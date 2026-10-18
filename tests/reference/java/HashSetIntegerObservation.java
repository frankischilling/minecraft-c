import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Project-owned JDK-only API observation. It reads no HashSet internals. */
public final class HashSetIntegerObservation {
    private HashSetIntegerObservation() {}

    private static Map<String, Object> object(Object... pairs) {
        if ((pairs.length & 1) != 0) throw new IllegalArgumentException("pairs");
        Map<String, Object> out = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) {
            if (!(pairs[i] instanceof String key) || out.containsKey(key)) {
                throw new IllegalArgumentException("unique string key");
            }
            out.put(key, pairs[i + 1]);
        }
        return out;
    }

    private static List<Integer> ordered(HashSet<Integer> set) {
        List<Integer> values = new ArrayList<>();
        for (Integer value : set) values.add(value);
        return values;
    }

    private static Integer first(HashSet<Integer> set) {
        Iterator<Integer> iterator = set.iterator();
        return iterator.hasNext() ? iterator.next() : null;
    }

    private static Map<String, Object> snapshot(HashSet<Integer> set) {
        return object("size", set.size(), "iteration", ordered(set), "fresh_first", first(set));
    }

    private static final class CaseTrace {
        private final String name;
        private final HashSet<Integer> set = new HashSet<>();
        private final List<Map<String, Object>> operations = new ArrayList<>();
        private final List<Map<String, Object>> checkpoints = new ArrayList<>();
        private final Map<String, Object> initial = snapshot(set);
        private int maximumSize;

        private CaseTrace(String name) { this.name = name; }

        private void mutate(String action, Integer argument, Integer batch, Integer step) {
            if (operations.size() >= 1024 || set.size() > 512) {
                throw new IllegalStateException("bounded cases");
            }
            Map<String, Object> before = snapshot(set);
            Integer selected = null;
            Boolean result;
            Boolean empty = null;
            if (action.equals("add")) {
                result = set.add(argument);
            } else if (action.equals("remove")) {
                result = set.remove(argument);
            } else if (action.equals("fresh_first_set_remove")) {
                empty = set.isEmpty();
                if (empty) {
                    result = null;
                } else {
                    selected = set.iterator().next();
                    result = set.remove(selected);
                }
            } else {
                throw new IllegalArgumentException("action");
            }
            maximumSize = Math.max(maximumSize, set.size());
            operations.add(object("index", operations.size(), "action", action,
                                  "argument", argument, "batch", batch, "step", step,
                                  "before", before, "empty_before_action", empty,
                                  "selected", selected, "result", result, "after", snapshot(set)));
        }

        private void add(int value) { mutate("add", value, null, null); }
        private void remove(int value) { mutate("remove", value, null, null); }
        private void checkpoint(String label) { checkpoints.add(object("label", label, "state", snapshot(set))); }
        private void take(int batch, int count) {
            if (count < 0 || count > 100) throw new IllegalArgumentException("bounded batch");
            for (int step = 0; step < count; step++) {
                mutate("fresh_first_set_remove", null, batch, step);
            }
        }
        private Map<String, Object> record() {
            return object("name", name, "initial", initial, "operations", operations,
                          "checkpoints", checkpoints, "maximum_size", maximumSize,
                          "final", snapshot(set));
        }
    }

    /** Test-data construction only: no private hash/capacity value is observed. */
    private static int keyFromConstructionBits(int bits) { return bits ^ (bits >>> 16); }

    private static List<CaseTrace> cases() {
        List<CaseTrace> traces = new ArrayList<>();
        CaseTrace fresh = new CaseTrace("survivors-fresh");
        traces.add(fresh);
        fresh.add(16); fresh.add(32); fresh.checkpoint("survivors"); fresh.take(0, 4);

        CaseTrace grown = new CaseTrace("survivors-after-growth");
        traces.add(grown);
        grown.add(16); grown.add(32);
        for (int value = 65; value <= 75; value++) grown.add(value);
        for (int value = 65; value <= 75; value++) grown.remove(value);
        grown.checkpoint("survivors"); grown.take(0, 4);

        CaseTrace repeated = new CaseTrace("duplicate-remove-readd");
        traces.add(repeated);
        repeated.add(16); repeated.add(32); repeated.add(16); repeated.remove(160);
        repeated.remove(16); repeated.add(16); repeated.add(48); repeated.remove(32); repeated.add(32);
        repeated.checkpoint("after-readd"); repeated.take(0, 6);

        CaseTrace collisions = new CaseTrace("collision-candidates");
        traces.add(collisions);
        for (int index = 0; index < 16; index++) collisions.add(index * 64);
        collisions.checkpoint("after-collision-keys"); collisions.take(0, 20);

        CaseTrace signed = new CaseTrace("signed-collision-candidates");
        traces.add(signed);
        List<Integer> signedKeys = new ArrayList<>();
        for (int index = 0; index < 8; index++) {
            int negative = keyFromConstructionBits(Integer.MIN_VALUE + index * 64);
            int positive = keyFromConstructionBits(index * 64);
            signedKeys.add(negative); signedKeys.add(positive);
            signed.add(negative); signed.add(positive);
        }
        int selectedKey = signedKeys.get(3);
        signed.remove(selectedKey); signed.add(selectedKey);
        signed.remove(keyFromConstructionBits(0x40000000));
        signed.checkpoint("after-signed-keys"); signed.take(0, 20);

        CaseTrace split = new CaseTrace("collision-resize-candidates");
        traces.add(split);
        for (int index = 0; index < 16; index++) split.add(index * 64);
        for (int value = 1; value <= 40; value++) split.add(value);
        split.remove(0); split.remove(128); split.remove(256); split.add(0);
        split.checkpoint("after-growth-and-removals"); split.take(0, 60);

        CaseTrace large = new CaseTrace("thresholds-and-two-100-step-batches");
        traces.add(large);
        for (int value = 0; value < 130; value++) {
            large.add(value);
            int size = value + 1;
            if (size == 12 || size == 13 || size == 24 || size == 25 || size == 48 ||
                size == 49 || size == 96 || size == 97) large.checkpoint("size-" + size);
        }
        large.checkpoint("before-first-100"); large.take(0, 100);
        large.checkpoint("after-first-100"); large.take(1, 100); large.checkpoint("after-second-100");
        return traces;
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Integer || value instanceof Boolean) return value.toString();
        if (value instanceof String text) {
            StringBuilder out = new StringBuilder("\"");
            for (int index = 0; index < text.length(); index++) {
                char unit = text.charAt(index);
                if (unit == '"' || unit == '\\') out.append('\\').append(unit);
                else if (unit < 32 || unit > 126) {
                    out.append(String.format("\\u%04x", (int) unit));
                } else out.append(unit);
            }
            return out.append('"').toString();
        }
        if (value instanceof List<?> list) {
            StringBuilder out = new StringBuilder("[");
            boolean comma = false;
            for (Object item : list) {
                if (comma) out.append(',');
                comma = true; out.append(json(item));
            }
            return out.append(']').toString();
        }
        if (value instanceof Map<?, ?> map) {
            StringBuilder out = new StringBuilder("{");
            boolean comma = false;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                if (!(entry.getKey() instanceof String)) throw new IllegalArgumentException("JSON key");
                if (comma) out.append(',');
                comma = true; out.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return out.append('}').toString();
        }
        throw new IllegalArgumentException("JSON value");
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1 || !(args[0].equals("observed") || args[0].equals("control"))) {
            throw new IllegalArgumentException("observed|control");
        }
        List<Map<String, Object>> records = new ArrayList<>();
        int count = 0;
        for (CaseTrace trace : cases()) {
            records.add(trace.record()); count += trace.operations.size();
        }
        Map<String, Object> result = object("schema", 1, "scope", "JDK-only Integer HashSet public-API traces",
            "internal_state_read", false, "original_game_classes_used", false,
            "case_count", records.size(), "operation_count", count,
            "limits", object("case_count", 7, "operations_per_case", 1024, "set_members", 512,
                             "fresh_remove_batch", 100, "output_bytes", 4 * 1024 * 1024), "cases", records);
        byte[] output = (json(result) + "\n").getBytes(StandardCharsets.UTF_8);
        if (output.length > 4 * 1024 * 1024) throw new IllegalStateException("bounded output");
        System.out.write(output); System.out.flush();
    }
}
