import java.io.BufferedWriter;
import java.io.OutputStreamWriter;
import java.lang.management.ManagementFactory;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;

/** Finite reflection observations of original position keys and the selected JDK Set. */
public final class RedstoneSetProbe {
    private static final int MAX_HELD = 4096;
    private static final int MAX_CAPACITY = 1024;
    private static final int MAX_STEPS = 256;
    private static final class Probe implements AutoCloseable {
        final URLClassLoader loader;
        final Class<?> keyClass;
        final Constructor<?> constructor;
        final Field x, y, z, setMap, table, size, threshold, modCount, loadFactor;
        final Field nodeHash, nodeKey, nodeValue, nodeNext, parent, left, right, prev, red;
        final Field entryBefore, entryAfter, keySetView, elementData, listSize, listModCount;
        final HashSet<Object> set = new HashSet<>();
        // IDs deliberately use only strong ArrayList holds and reference equality.
        final ArrayList<Object> held = new ArrayList<>();
        final ArrayList<Object> allocatedKeys = new ArrayList<>();
        ArrayList<Object> snapshot;
        final BufferedWriter output;
        int steps;
        boolean terminal;

        Probe(Path jar, String side, BufferedWriter output) throws Exception {
            this.output = output;
            loader = new URLClassLoader(new java.net.URL[]{jar.toUri().toURL()}, null);
            keyClass = Class.forName(side.equals("client") ? "wf" : "oa", false, loader);
            require(keyClass.getSuperclass() == Object.class && keyClass.getInterfaces().length == 0,
                    "actual original position class shape");
            constructor = keyClass.getDeclaredConstructor(int.class, int.class, int.class);
            x = keyClass.getDeclaredField("a");
            y = keyClass.getDeclaredField("b");
            z = keyClass.getDeclaredField("c");
            require(keyClass.getDeclaredFields().length == 3, "exact three original fields");
            for (Field f : new Field[]{x, y, z})
                require(f.getType() == int.class && Modifier.isPublic(f.getModifiers()) &&
                        Modifier.isFinal(f.getModifiers()), "actual immutable coordinate field");
            setMap = field(HashSet.class, "map");
            table = field(java.util.HashMap.class, "table");
            size = field(java.util.HashMap.class, "size");
            threshold = field(java.util.HashMap.class, "threshold");
            modCount = field(java.util.HashMap.class, "modCount");
            loadFactor = field(java.util.HashMap.class, "loadFactor");
            Class<?> node = Class.forName("java.util.HashMap$Node");
            nodeHash = field(node, "hash"); nodeKey = field(node, "key");
            nodeValue = field(node, "value"); nodeNext = field(node, "next");
            Class<?> tree = Class.forName("java.util.HashMap$TreeNode");
            parent = field(tree, "parent"); left = field(tree, "left");
            right = field(tree, "right"); prev = field(tree, "prev"); red = field(tree, "red");
            Class<?> entry = Class.forName("java.util.LinkedHashMap$Entry");
            entryBefore = field(entry, "before"); entryAfter = field(entry, "after");
            keySetView = field(java.util.AbstractMap.class, "keySet");
            elementData = field(ArrayList.class, "elementData");
            listSize = field(ArrayList.class, "size");
            listModCount = field(java.util.AbstractList.class, "modCount");
        }

        int id(Object value) {
            if (value == null) return 0;
            for (int i = 0; i < held.size(); ++i)
                if (held.get(i) == value) return i + 1;
            require(!terminal && held.size() < MAX_HELD, "bounded passive held identity inventory");
            held.add(value);
            return held.size();
        }

        Object key(int a, int b, int c) throws Exception {
            require(!terminal && allocatedKeys.size() < 256, "bounded actual key constructions");
            Object value = constructor.newInstance(a, b, c);
            require(value.getClass() == keyClass, "actual original constructor result");
            allocatedKeys.add(value);
            id(value);
            return value;
        }

        String keyJson(Object key) throws Exception {
            require(key != null && key.getClass() == keyClass, "actual original key only");
            // This virtual hashCode is the original pure coordinate body, not identityHashCode.
            return "{\"id\":" + id(key) + ",\"x\":" + x.getInt(key) + ",\"y\":" +
                    y.getInt(key) + ",\"z\":" + z.getInt(key) + ",\"hash\":" + key.hashCode() + "}";
        }

        String keysJson(Iterable<?> values) throws Exception {
            StringBuilder b = new StringBuilder("[");
            boolean comma = false;
            int count = 0;
            for (Object value : values) {
                require(++count <= 256, "bounded complete key iteration");
                if (comma) b.append(','); comma = true;
                b.append(keyJson(value));
            }
            return b.append(']').toString();
        }

        String state() throws Exception {
            require(!terminal, "no collection observation after terminal identity reads");
            Object map = setMap.get(set);
            require(map != null && map.getClass() == java.util.HashMap.class, "actual HashSet backing map");
            Object[] buckets = (Object[])table.get(map);
            require(buckets == null || buckets.length <= MAX_CAPACITY, "bounded complete table");
            StringBuilder roots = new StringBuilder("["), nodes = new StringBuilder("[");
            ArrayList<Object> visited = new ArrayList<>();
            if (buckets != null) {
                for (int bucket = 0; bucket < buckets.length; ++bucket) {
                    if (bucket != 0) roots.append(',');
                    roots.append(id(buckets[bucket]));
                    for (Object n = buckets[bucket]; n != null; n = nodeNext.get(n)) {
                        require(visited.size() < 256, "bounded complete node graph");
                        for (Object earlier : visited) require(earlier != n, "no repeated bucket node");
                        visited.add(n);
                        if (visited.size() > 1) nodes.append(',');
                        boolean tree = n.getClass().getName().equals("java.util.HashMap$TreeNode");
                        require(tree || n.getClass().getName().equals("java.util.HashMap$Node"), "exact JDK node kinds");
                        nodes.append("{\"id\":").append(id(n)).append(",\"bucket\":").append(bucket)
                             .append(",\"class\":").append(quote(n.getClass().getName()))
                             .append(",\"hash\":").append(nodeHash.getInt(n))
                             .append(",\"key\":").append(keyJson(nodeKey.get(n)))
                             .append(",\"value_id\":").append(id(nodeValue.get(n)))
                             .append(",\"next\":").append(id(nodeNext.get(n)))
                             .append(",\"tree\":").append(tree);
                        if (tree) nodes.append(",\"parent\":").append(id(parent.get(n)))
                            .append(",\"left\":").append(id(left.get(n)))
                            .append(",\"right\":").append(id(right.get(n)))
                            .append(",\"prev\":").append(id(prev.get(n)))
                            .append(",\"before\":").append(id(entryBefore.get(n)))
                            .append(",\"after\":").append(id(entryAfter.get(n)))
                            .append(",\"red\":").append(red.getBoolean(n));
                        nodes.append('}');
                    }
                }
            }
            String iteration = keysJson(set);
            String saved = snapshot == null ? "null" : keysJson(snapshot);
            String storage = "null";
            if (snapshot != null) {
                Object[] array = (Object[])elementData.get(snapshot);
                require(array.length <= 256, "bounded complete snapshot backing array");
                StringBuilder elements = new StringBuilder("[");
                for (int i = 0; i < array.length; ++i) {
                    if (i != 0) elements.append(',');
                    elements.append(id(array[i]));
                }
                storage = "{\"list_id\":" + id(snapshot) + ",\"array_id\":" + id(array) +
                    ",\"size\":" + listSize.getInt(snapshot) + ",\"capacity\":" + array.length +
                    ",\"mod_count\":" + listModCount.getInt(snapshot) + ",\"elements\":" + elements.append(']') + "}";
            }
            require(visited.size() == set.size() && set.size() == size.getInt(map), "full observed node coverage");
            return "{\"set_id\":" + id(set) + ",\"map_id\":" + id(map) + ",\"table_id\":" + id(buckets) +
                    ",\"keyset_view_id\":" + id(keySetView.get(map)) + ",\"snapshot_storage\":" + storage +
                    ",\"size\":" + set.size() + ",\"map_size\":" + size.getInt(map) +
                    ",\"threshold\":" + threshold.getInt(map) + ",\"mod_count\":" + modCount.getInt(map) +
                    ",\"load_factor_bits\":" + Float.floatToRawIntBits(loadFactor.getFloat(map)) +
                    ",\"table_null\":" + (buckets == null) + ",\"capacity\":" +
                    (buckets == null ? 0 : buckets.length) + ",\"roots\":" + roots.append(']') +
                    ",\"nodes\":" + nodes.append(']') + ",\"iteration\":" + iteration +
                    ",\"snapshot\":" + saved + "}";
        }

        void step(String operation, Object key, Boolean result) throws Exception {
            require(!terminal && steps < MAX_STEPS, "bounded mutation/observation history");
            String record = "{\"kind\":\"step\",\"ordinal\":" + steps++ + ",\"operation\":" + quote(operation) +
                    ",\"argument\":" + (key == null ? "null" : keyJson(key)) + ",\"result\":" +
                    (result == null ? "null" : result.toString()) + ",\"state\":" + state() + "}";
            emit(record);
        }

        void add(int a, int b, int c) throws Exception {
            Object k = key(a, b, c);
            boolean value = set.add(k);
            step("add_fresh", k, value);
        }

        void same(int index) throws Exception {
            Object k = allocatedKeys.get(index);
            step("add_same_reference", k, set.add(k));
        }

        void remove(int a, int b, int c) throws Exception {
            Object k = key(a, b, c);
            step("remove_equal_fresh", k, set.remove(k));
        }

        void copy() throws Exception {
            snapshot = new ArrayList<>(set);
            step("snapshot_arraylist", null, null);
        }

        void clear() throws Exception {
            set.clear();
            step("clear", null, null);
        }

        void finish() throws Exception {
            // The final state is encoded BEFORE the first observer identity-hash call.
            step("terminal_before_identity_hash", null, null);
            terminal = true;
            StringBuilder b = new StringBuilder("{\"kind\":\"terminal\",\"all_mutations_finished\":true,")
                .append("\"observer_identity_hash_reads_are_post_terminal\":true,\"steps\":").append(steps)
                .append(",\"objects\":[");
            for (int i = 0; i < held.size(); ++i) {
                if (i != 0) b.append(',');
                Object value = held.get(i);
                b.append("{\"id\":").append(i + 1).append(",\"class\":").append(quote(value.getClass().getName()))
                 .append(",\"identity_hash_unsigned\":").append(quote(Integer.toUnsignedString(System.identityHashCode(value))))
                 .append('}');
            }
            emit(b.append("]}").toString());
            output.flush();
        }

        void emit(String value) throws Exception {
            require(value.length() <= 1024 * 1024, "bounded complete output record");
            output.write(value); output.newLine();
        }

        public void close() throws Exception { loader.close(); }
    }

    private static Field field(Class<?> owner, String name) throws Exception {
        Field value = owner.getDeclaredField(name); value.setAccessible(true); return value;
    }

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static String quote(String value) {
        StringBuilder b = new StringBuilder("\"");
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c >= 32 && c <= 126) b.append(c);
            else {
                b.append("\\u");
                String hex = Integer.toHexString(c);
                for (int pad = hex.length(); pad < 4; ++pad) b.append('0');
                b.append(hex);
            }
        }
        return b.append('"').toString();
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 3 && (args[1].equals("client") || args[1].equals("server")),
                "arguments: original_jar client|server case");
        String name = args[2];
        require(name.equals("lazy_duplicate_snapshot") || name.equals("equal_hash_tree") ||
                name.equals("resize") || name.equals("tree_split"), "known independent case");
        BufferedWriter out = new BufferedWriter(new OutputStreamWriter(System.out, StandardCharsets.UTF_8));
        try (Probe p = new Probe(Path.of(args[0]).toAbsolutePath(), args[1], out)) {
            p.emit("{\"kind\":\"header\",\"schema\":1,\"scope\":\"original-position-key-hashset-jdk17\"," +
                "\"side\":" + quote(args[1]) + ",\"case\":" + quote(name) + ",\"key_class\":" + quote(p.keyClass.getName()) +
                ",\"class_origin\":" + quote(p.keyClass.getProtectionDomain().getCodeSource().getLocation().toExternalForm()) +
                ",\"java_version\":" + quote(System.getProperty("java.version")) +
                ",\"runtime_version\":" + quote(System.getProperty("java.runtime.version")) +
                ",\"vm_version\":" + quote(System.getProperty("java.vm.version")) +
                ",\"vm_name\":" + quote(System.getProperty("java.vm.name")) +
                ",\"java_home\":" + quote(System.getProperty("java.home")) +
                ",\"selected_class_resources\":" + ClassResourcePins.json(p.keyClass) +
                ",\"vm_arguments\":" + quote(ManagementFactory.getRuntimeMXBean().getInputArguments().toString()) +
                ",\"observer_effects\":\"Class resource fingerprinting before mutations, reflected reads, strong linear ArrayList identity holds, original pure coordinate hash reads; Set iteration initializes its cached keySet view; no early observer identity hash; post-terminal identity reads can assign previously unused hashes; no uninstrumented equivalence claim\"}");
            p.step("initial", null, null);
            if (name.equals("lazy_duplicate_snapshot")) {
                p.add(7, 64, -3); p.same(0); p.add(7, 64, -3); p.add(8, 64, -3);
                p.copy(); p.clear(); p.add(7, 64, -3); p.remove(8, 64, -3);
            } else if (name.equals("equal_hash_tree")) {
                // Collection-API coordinates, explicitly not a reachable World geometry claim.
                for (int i = 0; i < 16; ++i) p.add(i, 0, -(i * 8976890));
                p.same(0); p.add(5, 0, -(5 * 8976890)); p.copy();
                p.remove(0, 0, 0); p.remove(4, 0, -(4 * 8976890)); p.remove(8, 0, -(8 * 8976890));
                p.clear(); p.add(5, 0, -(5 * 8976890)); p.add(6, 0, -(6 * 8976890));
            } else if (name.equals("resize")) {
                for (int i = 0; i < 64; ++i) p.add(0, 0, i);
                p.copy(); p.clear(); p.add(0, 0, 1); p.add(0, 0, 2);
            } else {
                for (int i = 0; i < 16; ++i) p.add(0, 0, i * 64);
                p.copy();
                for (int i = 1; i <= 48; ++i) p.add(0, 0, i);
                p.clear(); p.add(0, 0, 64); p.add(0, 0, 128);
            }
            p.finish();
        }
    }
}
