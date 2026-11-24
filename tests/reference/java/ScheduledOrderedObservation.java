import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.NoSuchElementException;
import java.util.TreeSet;

/** Public-API JDK collection observation; no original game classes or reflection. */
public final class ScheduledOrderedObservation {
    private static final int MAX_RECORDS = 128;
    private static final int MAX_OPERATIONS = 100000;

    private static final class Record implements Comparable<Record> {
        final int identity;
        final long sequence;
        long time;

        Record(int identity, long time, long sequence) {
            this.identity = identity;
            this.time = time;
            this.sequence = sequence;
        }

        public int compareTo(Record other) {
            if (time < other.time) return -1;
            if (time > other.time) return 1;
            if (sequence < other.sequence) return -1;
            if (sequence > other.sequence) return 1;
            return 0;
        }
    }

    private static Record requireRecord(Record[] records, String token) {
        int id = Integer.parseInt(token);
        if (id < 0 || id >= records.length || records[id] == null)
            throw new IllegalArgumentException("unknown record");
        return records[id];
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 0) throw new IllegalArgumentException("no arguments");
        Record[] records = new Record[MAX_RECORDS];
        TreeSet<Record> ordered = new TreeSet<>();
        BufferedReader input = new BufferedReader(new InputStreamReader(System.in, "US-ASCII"));
        int operations = 0;
        String line;
        while ((line = input.readLine()) != null) {
            if (++operations > MAX_OPERATIONS || line.length() > 160)
                throw new IllegalArgumentException("input limit");
            String[] words = line.split(" ", -1);
            String result = "-";
            switch (words[0]) {
                case "new":
                    if (words.length != 1) throw new IllegalArgumentException("new fields");
                    records = new Record[MAX_RECORDS];
                    ordered = new TreeSet<>();
                    break;
                case "record": {
                    if (words.length != 4) throw new IllegalArgumentException("record fields");
                    int id = Integer.parseInt(words[1]);
                    if (id < 0 || id >= records.length || records[id] != null)
                        throw new IllegalArgumentException("record identity");
                    records[id] = new Record(id, Long.parseLong(words[2]), Long.parseLong(words[3]));
                    break;
                }
                case "add":
                case "remove": {
                    if (words.length != 2) throw new IllegalArgumentException("mutation fields");
                    Record record = requireRecord(records, words[1]);
                    result = (words[0].equals("add") ? ordered.add(record) : ordered.remove(record))
                        ? "1" : "0";
                    break;
                }
                case "time":
                    if (words.length != 3) throw new IllegalArgumentException("time fields");
                    requireRecord(records, words[1]).time = Long.parseLong(words[2]);
                    break;
                case "shift": {
                    if (words.length != 2) throw new IllegalArgumentException("shift fields");
                    long delta = Long.parseLong(words[1]);
                    // Shift each externally held identity once, including absent records.
                    for (Record record : records) if (record != null) record.time += delta;
                    break;
                }
                case "first":
                    if (words.length != 1) throw new IllegalArgumentException("first fields");
                    break;
                default:
                    throw new IllegalArgumentException("unknown command");
            }
            String first;
            try {
                first = Integer.toString(ordered.first().identity);
            } catch (NoSuchElementException empty) {
                first = "empty";
            }
            System.out.println(words[0] + " " + result + " " + ordered.size() + " " + first);
        }
    }
}
