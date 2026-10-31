import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Authored observer of unchanged Beta TileEntityRenderer constructors/lookups.
 * No replacement class, copied constructor, receiver fabrication or GL call. */
public final class TileEntityRendererObservation {
    static final String[] TILE_NAMES = {"sk", "js", "eg", "az", "yk", "cy", "tn", "uk", "ow"};
    static int networkAttempts, nativeAttempts, writeAttempts, rows;
    static ClassLoader loader;
    static Class<?> ll, je, ow;
    static Object singleton;
    static Map<?, ?> cache;

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static void row(String value) { ++rows; System.out.println(value); }
    static Class<?> passive(String name) throws Exception { return Class.forName(name, false, loader); }
    static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    static Object get(Object object, Class<?> type, String name) throws Exception {
        return field(type, name).get(object);
    }
    static String type(Object object) { return object == null ? "null" : object.getClass().getName(); }
    static String f(Object object, Class<?> owner, String name) throws Exception {
        return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(field(owner, name).getFloat(object)));
    }
    static String d(Object object, Class<?> owner, String name) throws Exception {
        return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(field(owner, name).getDouble(object)));
    }
    static int bool(Object object, Class<?> owner, String name) throws Exception {
        return field(owner, name).getBoolean(object) ? 1 : 0;
    }
    static Set<Object> identities() { return Collections.newSetFromMap(new IdentityHashMap<>()); }

    static void mapRows(String stage) throws Exception {
        require(cache.getClass() == HashMap.class, "actual original HashMap required");
        Object table = field(HashMap.class, "table").get(cache);
        int length = table == null ? 0 : Array.getLength(table), nulls = 0;
        List<String> entries = new ArrayList<>();
        int ordinal = 0;
        for (Map.Entry<?, ?> entry : cache.entrySet()) {
            require(entry.getKey() instanceof Class<?>, "lookup must retain Class keys");
            String key = ((Class<?>)entry.getKey()).getName(), renderer = type(entry.getValue());
            if (entry.getValue() == null) ++nulls;
            else require(get(entry.getValue(), je, "a") == singleton, "actual JE backpointer");
            entries.add("TILE_ENTRY " + stage + " " + key + " " + renderer);
            // Physical iteration and bucket/hash order are genuine diagnostic evidence,
            // intentionally separate from the sorted logical finite row inventory.
            System.out.println("TILE_ITERATION " + stage + " " + ordinal++ + " " + key + " " + renderer);
        }
        row("TILE_MAP " + stage + " " + cache.size() + " " + length + " " +
            field(HashMap.class, "threshold").getInt(cache) + " " + nulls + " " +
            (cache.containsKey(ow) ? 1 : 0) + " " + (cache.containsKey(null) ? 1 : 0));
        Collections.sort(entries);
        for (String entry : entries) row(entry);
        int nodes = 0;
        for (int bucket = 0; bucket < length; ++bucket) {
            Object node = Array.get(table, bucket);
            for (int depth = 0; node != null; ++depth) {
                require(depth < 64, "bounded actual HashMap chain");
                Class<?> nodeType = node.getClass();
                Object key = get(node, nodeType, "key"), value = get(node, nodeType, "value");
                require(key instanceof Class<?>, "actual bucket key class");
                System.out.printf(Locale.ROOT, "TILE_BUCKET %s %d %d %08x %s %s%n", stage,
                    bucket, depth, field(nodeType, "hash").getInt(node), ((Class<?>)key).getName(), type(value));
                node = get(node, nodeType, "next");
                ++nodes;
            }
        }
        require(nodes == cache.size(), "complete physical bucket inventory");
        System.out.println("TILE_MAP_RECEIPT " + stage + " " + ordinal + " " + nodes);
    }

    static void constructorRows() throws Exception {
        Set<Object> rendererIdentities = identities();
        int backpointers = 0;
        for (String key : new String[]{"yk", "cy", "uk"}) {
            Object renderer = cache.get(passive(key));
            require(renderer != null && rendererIdentities.add(renderer), "three actual distinct renderers");
            boolean same = get(renderer, je, "a") == singleton;
            if (same) ++backpointers;
            row("TILE_RENDERER " + key + " " + type(renderer) + " " + (same ? 1 : 0));
        }
        Object spawner = cache.get(passive("cy")), piston = cache.get(passive("uk"));
        Map<?, ?> spawnerMap = (Map<?, ?>)get(spawner, passive("ag"), "b");
        row("TILE_CTOR " + type(cache) + " " + cache.size() + " " +
            (get(null, ll, "a") == singleton ? 1 : 0) + " " + rendererIdentities.size() + " " +
            backpointers + " " + type(spawnerMap) + " " + spawnerMap.size() + " " +
            (get(piston, passive("hy"), "b") == null ? 1 : 0));
        StringBuilder defaults = new StringBuilder("TILE_DEFAULTS");
        for (String name : new String[]{"n", "e", "f", "g"})
            defaults.append(' ').append(get(singleton, ll, name) == null ? 1 : 0);
        for (String name : new String[]{"h", "i"}) defaults.append(' ').append(f(singleton, ll, name));
        for (String name : new String[]{"j", "k", "l"}) defaults.append(' ').append(d(singleton, ll, name));
        for (String name : new String[]{"b", "c", "d"}) defaults.append(' ').append(d(null, ll, name));
        row(defaults.toString());
    }

    static void signGeometry() throws Exception {
        Class<?> rf = passive("rf"), ps = passive("ps"), ib = passive("ib"),
                 tz = passive("tz"), bt = passive("bt");
        Object sign = cache.get(passive("yk")), model = get(sign, passive("po"), "b");
        Object[] parts = {get(model, rf, "a"), get(model, rf, "b")};
        require(parts[0] != parts[1], "two distinct real Sign parts");
        row("TILE_SIGN " + type(model) + " " + type(parts[0]) + " " + type(parts[1]) + " 1");
        Set<Object> positions = identities(), vertices = identities(), graph = identities();
        require(graph.add(model), "model identity");
        int originalsCount = 0, quadCount = 0, quadVertexCount = 0;
        for (int p = 0; p < parts.length; ++p) {
            Object part = parts[p];
            require(graph.add(part), "distinct part identity");
            StringBuilder state = new StringBuilder("TILE_PART " + p + " " + get(part, ps, "l") + " " + get(part, ps, "m"));
            for (String name : new String[]{"g", "h", "i", "n"}) state.append(' ').append(bool(part, ps, name));
            state.append(' ').append(get(part, ps, "o"));
            for (String name : new String[]{"a", "b", "c", "d", "e", "f"}) state.append(' ').append(f(part, ps, name));
            row(state.toString());
            Object[] originals = (Object[])get(part, ps, "j"), quads = (Object[])get(part, ps, "k");
            require(originals.length == 8 && quads.length == 6 && graph.add(originals) && graph.add(quads),
                    "complete actual box arrays");
            for (int v = 0; v < originals.length; ++v) {
                Object vertex = originals[v], position = get(vertex, ib, "a");
                require(vertices.add(vertex) && positions.add(position) && graph.add(vertex) && graph.add(position),
                        "distinct original vertex/position");
                StringBuilder value = new StringBuilder("TILE_VERTEX " + p + " " + v);
                for (String name : new String[]{"a", "b", "c"}) value.append(' ').append(d(position, bt, name));
                value.append(' ').append(f(vertex, ib, "b")).append(' ').append(f(vertex, ib, "c"));
                row(value.toString());
                ++originalsCount;
            }
            for (int q = 0; q < quads.length; ++q) {
                Object quad = quads[q];
                Object[] corners = (Object[])get(quad, tz, "a");
                require(corners.length == 4 && graph.add(quad) && graph.add(corners), "complete actual quad");
                StringBuilder value = new StringBuilder("TILE_QUAD " + p + " " + q + " " + get(quad, tz, "b") + " " + bool(quad, tz, "c"));
                for (Object corner : corners) {
                    int index = -1;
                    boolean copy = true;
                    for (int v = 0; v < originals.length; ++v) {
                        if (get(corner, ib, "a") == get(originals[v], ib, "a")) index = v;
                        if (corner == originals[v]) copy = false;
                    }
                    require(index >= 0 && vertices.add(corner) && graph.add(corner), "actual UV-copy alias");
                    value.append(' ').append(index).append(' ').append(copy ? 1 : 0)
                         .append(' ').append(f(corner, ib, "b")).append(' ').append(f(corner, ib, "c"));
                    ++quadVertexCount;
                }
                row(value.toString());
                ++quadCount;
            }
        }
        row("TILE_GRAPH " + parts.length + " " + originalsCount + " " + quadCount + " " +
            quadVertexCount + " " + vertices.size() + " " + positions.size() + " " + graph.size());
    }

    static void lookups(boolean reverse) throws Exception {
        Method lookup = ll.getDeclaredMethod("a", Class.class);
        List<String> order = new ArrayList<>(List.of(TILE_NAMES));
        if (reverse) Collections.reverse(order);
        for (int step = 0; step < order.size(); ++step) {
            Class<?> key = passive(order.get(step));
            int before = cache.size();
            Object first = lookup.invoke(singleton, key);
            int after = cache.size();
            Object node = null;
            for (Map.Entry<?, ?> entry : cache.entrySet()) if (entry.getKey() == key) node = entry;
            Object repeated = lookup.invoke(singleton, key);
            Object repeatedNode = null;
            for (Map.Entry<?, ?> entry : cache.entrySet()) if (entry.getKey() == key) repeatedNode = entry;
            row("TILE_LOOKUP " + step + " " + key.getName() + " " + type(first) + " " + before + " " + after +
                " " + type(repeated) + " " + cache.size() + " " + (first == repeated ? 1 : 0) + " " +
                (cache.containsKey(key) ? 1 : 0) + " " + (node == repeatedNode ? 1 : 0));
            mapRows(String.format(Locale.ROOT, "lookup%02d", step));
        }
    }

    static void nulls() throws Exception {
        Method tile = ll.getDeclaredMethod("b", ow), eligible = ll.getDeclaredMethod("a", ow),
               lookup = ll.getDeclaredMethod("a", Class.class);
        int before = cache.size();
        Object renderer = tile.invoke(singleton, new Object[]{null});
        Object hasRenderer = eligible.invoke(singleton, new Object[]{null});
        row("TILE_NULL_TILE " + type(renderer) + " " + ((Boolean)hasRenderer ? 1 : 0) + " " + before + " " + cache.size());
        try {
            lookup.invoke(singleton, new Object[]{null});
            throw new AssertionError("null Class lookup returned");
        } catch (InvocationTargetException wrapper) {
            Throwable failure = wrapper.getCause();
            require(failure instanceof NullPointerException, "actual null-Class failure type");
            StackTraceElement top = failure.getStackTrace()[0];
            require(top.getClassName().equals("ll") && top.getMethodName().equals("a"), "actual original failure frame");
            row("TILE_NULL_CLASS " + failure.getClass().getName() + " " + top.getClassName() + " " +
                top.getMethodName() + " " + top.getLineNumber() + " " + before + " " + cache.size() + " " +
                (cache.containsKey(null) ? 1 : 0));
            System.out.println("TILE_EXCEPTION " + failure);
        }
        mapRows("after_nulls");
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2 && Set.of("constructor", "forward", "reverse", "nulls").contains(args[1]),
                "expected original JAR and complete-profile name");
        Path jar = Path.of(args[0]).toRealPath();
        loader = TileEntityRendererObservation.class.getClassLoader();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {
                if (permission instanceof RuntimePermission && permission.getName().equals("setSecurityManager"))
                    throw new SecurityException("observer guard cannot be replaced");
            }
            @Override public void checkConnect(String host, int port) { ++networkAttempts; throw new SecurityException("network denied"); }
            @Override public void checkListen(int port) { ++networkAttempts; throw new SecurityException("listener denied"); }
            @Override public void checkAccept(String host, int port) { ++networkAttempts; throw new SecurityException("accept denied"); }
            @Override public void checkLink(String library) { ++nativeAttempts; throw new SecurityException("native link denied: " + library); }
            @Override public void checkWrite(String file) { ++writeAttempts; throw new SecurityException("file write denied"); }
            @Override public void checkDelete(String file) { ++writeAttempts; throw new SecurityException("file delete denied"); }
            @Override public void checkExec(String command) { throw new SecurityException("process execution denied"); }
        });
        row("TILE_CASE " + args[1]);
        for (String name : new String[]{"ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt",
                                        "sk", "js", "eg", "az", "yk", "cy", "tn", "uk", "ow"}) {
            Class<?> type = passive(name);
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "original class provenance: " + name);
        }
        ll = passive("ll"); je = passive("je"); ow = passive("ow");
        for (String name : TILE_NAMES) {
            Class<?> type = passive(name), parent = type.getSuperclass();
            row("TILE_LITERAL " + name + " " + (parent == null ? "null" : parent.getName()) + " " + (parent == ow ? 1 : 0));
        }
        // This is the first active use of LL. Field.get executes its unchanged
        // <clinit>, complete constructor graph and JE callbacks before returning.
        singleton = get(null, ll, "a");
        require(singleton != null && singleton.getClass() == ll, "published actual LL singleton");
        cache = (Map<?, ?>)get(singleton, ll, "m");
        constructorRows();
        mapRows("initial");
        if (args[1].equals("constructor")) signGeometry();
        else if (args[1].equals("forward")) lookups(false);
        else if (args[1].equals("reverse")) lookups(true);
        else nulls();
        require(networkAttempts == 0 && nativeAttempts == 0 && writeAttempts == 0, "unexpected external work");
        System.out.println("Original TileEntityRenderer: " + args[1] + "; rows=" + rows + "; network=0; native=0; writes=0");
    }
}
