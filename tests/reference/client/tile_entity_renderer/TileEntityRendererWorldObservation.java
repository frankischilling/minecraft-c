import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Authored original null-World callbacks and null-Living frame-prefix probe.
 * No reference operand fabrication, replacement class, native or draw. */
public final class TileEntityRendererWorldObservation {
    static int rows, networks, natives, writes;
    static ClassLoader loader;
    static Class<?> ll, je, cv;
    static Object manager, initialMap, sign, spawner, piston, signModel, spawnerMap;
    static Map<?, ?> map;
    static final int[] FLOAT_INPUTS = {0x3f800001, 0xbf400002};
    static final long[] DOUBLE_INPUTS = {0x3ff4000000000001L, 0xc004000000000002L, 0x400e000000000003L};

    static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Class<?> passive(String name) throws Exception { return Class.forName(name, false, loader); }
    static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name); field.setAccessible(true); return field;
    }
    static Object get(Object object, Class<?> type, String name) throws Exception { return field(type, name).get(object); }
    static void row(String text) { ++rows; System.out.println(text); }
    static String type(Object object) { return object == null ? "null" : object.getClass().getName(); }
    static String bits(float value) { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value)); }
    static String bits(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    static int same(Object a, Object b) { return a == b ? 1 : 0; }

    static Object blocks() throws Exception { return get(piston, passive("hy"), "b"); }

    static void state(String stage) throws Exception {
        StringBuilder result = new StringBuilder("TILE_WORLD_LL " + stage);
        for (String name : new String[]{"n", "e", "f", "g"}) result.append(' ').append(type(get(manager, ll, name)));
        for (String name : new String[]{"h", "i"}) result.append(' ').append(bits(field(ll, name).getFloat(manager)));
        for (String name : new String[]{"j", "k", "l"}) result.append(' ').append(bits(field(ll, name).getDouble(manager)));
        for (String name : new String[]{"b", "c", "d"}) result.append(' ').append(bits(field(ll, name).getDouble(null)));
        result.append(' ').append(map.size()).append(' ').append(same(get(manager, ll, "m"), initialMap))
              .append(' ').append(same(get(null, ll, "a"), manager));
        row(result.toString());
        for (String key : new String[]{"yk", "cy", "uk"}) {
            Object renderer = map.get(passive(key));
            Object initial = key.equals("yk") ? sign : key.equals("cy") ? spawner : piston;
            row("TILE_WORLD_JE " + stage + " " + key + " " + type(renderer) + " " + same(renderer, initial) + " " +
                same(get(renderer, je, "a"), manager));
        }
        Object currentSpawnerMap = get(spawner, passive("ag"), "b");
        row("TILE_WORLD_CHILDREN " + stage + " " + same(get(sign, passive("po"), "b"), signModel) + " " +
            same(currentSpawnerMap, spawnerMap) + " " + ((Map<?, ?>)currentSpawnerMap).size() + " " + type(blocks()));
    }

    static List<String> cvFields() throws Exception {
        List<String> names = new ArrayList<>(List.of("c", "d", "e", "f", "a", "b", "g", "h", "i", "j", "k", "l", "m"));
        for (char c = 'n'; c <= 'z'; ++c) names.add(String.valueOf(c));
        for (char c = 'A'; c <= 'N'; ++c) names.add(String.valueOf(c));
        names.add("O");
        for (char c = 'P'; c <= 'Z'; ++c) names.add(String.valueOf(c));
        names.add("aa");
        for (char c = 'b'; c <= 'm'; ++c) names.add("a" + c);
        Set<String> actual = new HashSet<>();
        for (Field field : cv.getDeclaredFields()) actual.add(field.getName());
        require(names.size() == 65 && names.size() == actual.size() && actual.equals(new HashSet<>(names)),
                "complete actual CV declared source field inventory");
        return names;
    }

    static void cvState(int step, Object blocks) throws Exception {
        require(blocks != null && blocks.getClass() == cv, "actual HY CV publication");
        for (String name : cvFields()) {
            Field field = field(cv, name);
            Class<?> type = field.getType();
            boolean isStatic = Modifier.isStatic(field.getModifiers());
            Object receiver = isStatic ? null : blocks;
            String descriptor, value;
            if (type == boolean.class) { descriptor = "Z"; value = field.getBoolean(receiver) ? "1" : "0"; }
            else if (type == int.class) { descriptor = "I"; value = Integer.toString(field.getInt(receiver)); }
            else if (type == float.class) { descriptor = "F"; value = bits(field.getFloat(receiver)); }
            else { descriptor = "L" + type.getName().replace('.', '/') + ";"; value = type(field.get(receiver)); }
            row("TILE_WORLD_FIELD " + step + " " + name + " " + (isStatic ? "S" : "I") + " " + descriptor + " " + value);
        }
    }

    static void callbacks() throws Exception {
        Method changeWorld = ll.getDeclaredMethod("a", passive("fd"));
        List<Object> retained = new ArrayList<>();
        Object previous = blocks();
        for (int step = 0; step < 3; ++step) {
            int ordinal = 0;
            for (Object renderer : map.values())
                System.out.println("TILE_WORLD_ORDER " + step + " " + ordinal++ + " " + type(renderer));
            changeWorld.invoke(manager, new Object[]{null});
            Object current = blocks();
            boolean distinct = true;
            for (Object older : retained) distinct &= older != current;
            row("TILE_WORLD_CV_ID " + step + " " + type(current) + " " + (previous == null ? 1 : 0) + " " +
                (current != previous ? 1 : 0) + " " + (distinct ? 1 : 0));
            cvState(step, current);
            state("world" + step);
            retained.add(current);
            previous = current;
        }
        require(retained.size() == 3, "retain all actual displaced CV identities through observation");
    }

    static void frameNull() throws Exception {
        Object previous = blocks();
        state("before_frame");
        Method frame = ll.getDeclaredMethod("a", passive("fd"), passive("ji"), passive("sj"), passive("ls"), float.class);
        try {
            frame.invoke(manager, new Object[]{null, null, null, null, Float.intBitsToFloat(0x3f000000)});
            throw new AssertionError("null Living frame returned");
        } catch (InvocationTargetException wrapper) {
            Throwable failure = wrapper.getCause();
            require(failure instanceof NullPointerException, "actual null Living failure type");
            StackTraceElement top = failure.getStackTrace()[0];
            require(top.getClassName().equals("ll") && top.getMethodName().equals("a"), "actual original LL frame failure");
            row("TILE_WORLD_FRAME_NULL " + failure.getClass().getName() + " " + top.getClassName() + " " +
                top.getMethodName() + " " + top.getLineNumber());
            System.out.println("TILE_WORLD_EXCEPTION " + failure);
        }
        Object after = blocks();
        row("TILE_WORLD_FRAME_ID " + (previous == null ? 1 : 0) + " " + (after == null ? 1 : 0) + " " + same(previous, after));
        state("after_frame");
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2 && Set.of("callbacks", "frame_null_cold").contains(args[1]), "JAR and profile required");
        Path jar = Path.of(args[0]).toRealPath();
        loader = TileEntityRendererWorldObservation.class.getClassLoader();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {
                if (permission instanceof RuntimePermission && permission.getName().equals("setSecurityManager"))
                    throw new SecurityException("guard replacement denied");
            }
            @Override public void checkConnect(String host, int port) { ++networks; throw new SecurityException("network denied"); }
            @Override public void checkListen(int port) { ++networks; throw new SecurityException("listener denied"); }
            @Override public void checkAccept(String host, int port) { ++networks; throw new SecurityException("accept denied"); }
            @Override public void checkLink(String library) { ++natives; throw new SecurityException("native denied: " + library); }
            @Override public void checkWrite(String file) { ++writes; throw new SecurityException("write denied"); }
            @Override public void checkDelete(String file) { ++writes; throw new SecurityException("delete denied"); }
            @Override public void checkExec(String command) { throw new SecurityException("process denied"); }
        });
        for (String name : new String[]{"ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt", "cv", "xp",
                                        "fd", "ji", "sj", "ls", "ow", "sk", "js", "eg", "az", "yk", "cy", "tn", "uk"})
            require(Path.of(passive(name).getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "original class provenance: " + name);
        ll = passive("ll"); je = passive("je"); cv = passive("cv");
        manager = get(null, ll, "a");
        require(manager != null && manager.getClass() == ll, "real published LL");
        initialMap = get(manager, ll, "m"); map = (Map<?, ?>)initialMap;
        sign = map.get(passive("yk")); spawner = map.get(passive("cy")); piston = map.get(passive("uk"));
        signModel = get(sign, passive("po"), "b"); spawnerMap = get(spawner, passive("ag"), "b");
        require(sign != null && spawner != null && piston != null && signModel != null && spawnerMap != null,
                "complete real singleton graph");
        field(ll, "h").setFloat(manager, Float.intBitsToFloat(FLOAT_INPUTS[0]));
        field(ll, "i").setFloat(manager, Float.intBitsToFloat(FLOAT_INPUTS[1]));
        for (int n = 0; n < 3; ++n) field(ll, String.valueOf((char)('j' + n))).setDouble(manager, Double.longBitsToDouble(DOUBLE_INPUTS[n]));
        row("TILE_WORLD_CASE " + args[1]);
        state("initial");
        if (args[1].equals("callbacks")) callbacks();
        frameNull();
        row("TILE_WORLD_DONE " + args[1] + " " + (args[1].equals("callbacks") ? 3 : 0) + " 1");
        require(networks == 0 && natives == 0 && writes == 0, "unexpected external work");
        System.out.println("Original TileEntityRenderer World: " + args[1] + "; rows=" + rows + "; network=0; native=0; writes=0");
    }
}
