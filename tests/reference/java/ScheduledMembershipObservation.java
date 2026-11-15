import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;

/** JDK-only reached linked-bin histories, using qy's raw Comparable shape. */
public final class ScheduledMembershipObservation {
    @SuppressWarnings("rawtypes")
    private static final class Record implements Comparable {
        final int slot, x, y, z, block;
        final long sequence;
        long time;
        Record(int slot, int x, int y, int z, int block, long sequence, long time) {
            this.slot = slot; this.x = x; this.y = y; this.z = z;
            this.block = block; this.sequence = sequence; this.time = time;
        }
        @Override public int hashCode() { return ((x * 128 * 1024 + z * 128 + y) * 256) + block; }
        @Override public boolean equals(Object other) {
            return other instanceof Record r && x == r.x && y == r.y && z == r.z && block == r.block;
        }
        @Override public int compareTo(Object other) {
            Record r = (Record)other;
            return time < r.time ? -1 : time > r.time ? 1 : Long.compare(sequence, r.sequence);
        }
    }
    private static HashSet<Record> membership;
    private static Record[] records;

    private static String snapshot(Boolean result) {
        StringBuilder out = new StringBuilder("{\"result\":").append(result).append(",\"entries\":[");
        boolean first = true;
        for (Record record : membership) {
            if (!first) out.append(',');
            out.append(record.slot); first = false;
        }
        out.append("],\"records\":["); first = true;
        for (Record r : records) {
            if (r == null) continue;
            if (!first) out.append(',');
            out.append('[').append(r.slot).append(',').append(r.x).append(',').append(r.y).append(',')
                .append(r.z).append(',').append(r.block).append(',').append(r.time).append(',')
                .append(r.sequence).append(']'); first = false;
        }
        return out.append("]}").toString();
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 0) throw new IllegalArgumentException("no arguments");
        StringBuilder output = new StringBuilder();
        int count = 0;
        try (BufferedReader input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.US_ASCII))) {
            for (String line; (line = input.readLine()) != null;) {
                if (++count > 4096 || line.length() >= 192) throw new IllegalArgumentException("bound");
                String[] words = line.split(" ", -1);
                Boolean result = null;
                if (line.equals("new")) {
                    membership = new HashSet<>(); records = new Record[64];
                } else {
                    if (membership == null) throw new IllegalArgumentException("new first");
                    if (line.equals("snapshot")) {
                        // Public iteration only.
                    } else if (words[0].equals("shift") && words.length == 2) {
                        long delta = Long.parseLong(words[1]);
                        for (Record record : membership) record.time += delta;
                    } else {
                        int slot = Integer.parseInt(words[1]);
                        if (slot < 0 || slot >= 64) throw new IllegalArgumentException("slot");
                        if (words[0].equals("record") && words.length == 8) {
                            if (records[slot] != null) throw new IllegalArgumentException("owned slot");
                            records[slot] = new Record(slot, Integer.parseInt(words[2]), Integer.parseInt(words[3]),
                                Integer.parseInt(words[4]), Integer.parseInt(words[5]), Long.parseLong(words[6]),
                                Long.parseLong(words[7]));
                        } else {
                            Record record = records[slot];
                            if (record == null) throw new IllegalArgumentException("record first");
                            if (words[0].equals("time") && words.length == 3) record.time = Long.parseLong(words[2]);
                            else if (words[0].equals("add") && words.length == 2) result = membership.add(record);
                            else if (words[0].equals("remove") && words.length == 2) result = membership.remove(record);
                            else if (words[0].equals("contains") && words.length == 2) result = membership.contains(record);
                            else throw new IllegalArgumentException("request");
                        }
                    }
                }
                output.append(snapshot(result)).append('\n');
                if (output.length() > 4 * 1024 * 1024) throw new IllegalArgumentException("output bound");
            }
        }
        if (count == 0) throw new IllegalArgumentException("requests");
        System.out.write(output.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
    }
}
