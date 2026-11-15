import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Authored JDK-only public-API probe; no game classes or collection internals. */
public final class HashSetOrderObservation {
    private static final int MAX_REQUESTS = 4096, MAX_MEMBERS = 1024;
    private static final int MAX_OUTPUT = 4 * 1024 * 1024;
    private static HashSet<Integer> integers;
    private static HashSet<Key> objects;

    /** The prepared packed coordinate keys have distinct complete hashes.
     * This non-Comparable peer also checks the World.P key-type boundary. */
    private static final class Key {
        final int value;
        Key(int value) { this.value = value; }
        @Override public int hashCode() { return value; }
        @Override public boolean equals(Object other) {
            return other instanceof Key key && value == key.value;
        }
    }

    private static void need(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static String snapshot() {
        List<Integer> left = new ArrayList<>(integers), right = new ArrayList<>();
        for (Key key : objects) right.add(key.value);
        need(left.equals(right), "Integer/non-Comparable complete traversal differs");
        need(left.size() <= MAX_MEMBERS, "member bound");
        Integer first = integers.isEmpty() ? null : integers.iterator().next();
        Integer peer = objects.isEmpty() ? null : objects.iterator().next().value;
        need(first == null ? peer == null : first.equals(peer), "fresh iterator first differs");
        StringBuilder out = new StringBuilder("{\"size\":").append(left.size())
            .append(",\"iteration\":[");
        for (int i = 0; i < left.size(); ++i) {
            if (i != 0) out.append(',');
            out.append(left.get(i));
        }
        return out.append("],\"fresh_first\":").append(first).append('}').toString();
    }

    public static void main(String[] args) throws Exception {
        need(args.length == 0, "no arguments");
        StringBuilder output = new StringBuilder();
        int requests = 0;
        try (BufferedReader input = new BufferedReader(
                new InputStreamReader(System.in, StandardCharsets.US_ASCII))) {
            for (String line; (line = input.readLine()) != null;) {
                need(++requests <= MAX_REQUESTS && line.length() <= 32, "request bound");
                if (line.equals("new")) {
                    integers = new HashSet<>();
                    objects = new HashSet<>();
                    output.append(snapshot()).append('\n');
                } else {
                    need(integers != null, "new before operations");
                    if (line.equals("snapshot")) {
                        output.append(snapshot()).append('\n');
                    } else {
                        String before = snapshot();
                        boolean take = line.equals("take"), empty = integers.isEmpty();
                        Integer selected = null;
                        Boolean changed = null, peer = null;
                        if (line.startsWith("add ")) {
                            int key = Integer.parseInt(line.substring(4));
                            changed = integers.add(key);
                            peer = objects.add(new Key(key));
                        } else if (line.startsWith("remove ")) {
                            int key = Integer.parseInt(line.substring(7));
                            changed = integers.remove(key);
                            peer = objects.remove(new Key(key));
                        } else if (line.equals("clear")) {
                            integers.clear();
                            objects.clear();
                        } else if (take) {
                            if (!empty) {
                                selected = integers.iterator().next();
                                changed = integers.remove(selected);
                                peer = objects.remove(new Key(selected));
                            }
                        } else throw new IllegalArgumentException("request");
                        need(changed == null ? peer == null : changed.equals(peer), "mutation return differs");
                        output.append("{\"before\":").append(before)
                            .append(",\"empty_before_action\":").append(take ? Boolean.toString(empty) : "null")
                            .append(",\"selected\":").append(selected)
                            .append(",\"result\":").append(changed)
                            .append(",\"after\":").append(snapshot()).append("}\n");
                    }
                }
                need(output.length() <= MAX_OUTPUT, "output bound");
            }
        }
        need(requests != 0, "nonempty requests");
        System.out.write(output.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
    }
}
