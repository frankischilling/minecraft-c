import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

/** Genuine complete TH constructor, declared Statistics-first prerequisite,
 * original Pig operand and unchanged awake frame/null-prefix observations. */
public final class RenderManagerFrameObservation {
    static int rows, networks, natives, writes;
    static ClassLoader loader;
    static Class<?> th, bw, sn, ko;
    static Object manager, mapIdentity, raIdentity;
    static Map<?, ?> roster;
    static final IdentityHashMap<Object, Object> rosterEntries = new IdentityHashMap<>();
    static final String[] FLOAT_FIELDS = {"aU", "aS", "aV", "aT"};
    static final String[] DOUBLE_FIELDS = {"bl", "bm", "bn", "aM", "aN", "aO"};
    static final List<Slot> unchanged = new ArrayList<>();
    static final List<ArraySlot> unchangedArrays = new ArrayList<>();
    static final List<Object> rendererRandoms = new ArrayList<>();
    static final IdentityHashMap<Object, Integer> ids = new IdentityHashMap<>();
    static final class Slot {
        final Object receiver, value;
        final Field field;
        Slot(Object receiver, Field field) throws Exception { this.receiver = receiver; this.field = field; this.value = field.get(receiver); }
        boolean same() throws Exception {
            Object now = field.get(receiver);
            if (field.getType() == float.class) return Float.floatToRawIntBits((Float)now) == Float.floatToRawIntBits((Float)value);
            if (field.getType() == double.class) return Double.doubleToRawLongBits((Double)now) == Double.doubleToRawLongBits((Double)value);
            return field.getType().isPrimitive() ? now.equals(value) : now == value;
        }
    }
    static final class ArraySlot {
        final Object array;
        final Object[] elements;
        ArraySlot(Object array) { this.array = array; this.elements = ((Object[])array).clone(); }
        boolean same() {
            if (Array.getLength(array) != elements.length) return false;
            for (int i = 0; i < elements.length; ++i) if (Array.get(array, i) != elements[i]) return false;
            return true;
        }
    }
    static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Class<?> passive(String name) throws Exception { return Class.forName(name, false, loader); }
    static Field field(Class<?> owner, String name) throws Exception { Field f = owner.getDeclaredField(name); f.setAccessible(true); return f; }
    static Object get(Object object, Class<?> owner, String name) throws Exception { return field(owner, name).get(object); }
    static void row(String value) { ++rows; System.out.println(value); }
    static String type(Object object) { return object == null ? "null" : object.getClass().getName(); }
    static String f(float value) { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value)); }
    static String d(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    static int identity(Object object) { if (object == null) return 0; if (!ids.containsKey(object)) ids.put(object, ids.size() + 1); return ids.get(object); }
    static List<Field> fields(Class<?> owner) {
        List<Field> result = new ArrayList<>();
        for (Class<?> c = owner; c != Object.class; c = c.getSuperclass())
            for (Field f : c.getDeclaredFields()) if (!Modifier.isStatic(f.getModifiers())) { f.setAccessible(true); result.add(f); }
        result.sort(Comparator.comparing(f -> f.getDeclaringClass().getName() + "." + f.getName()));
        return result;
    }
    static String pose(Object actor) throws Exception {
        StringBuilder value = new StringBuilder();
        for (String name : FLOAT_FIELDS) value.append(' ').append(f(field(sn, name).getFloat(actor)));
        for (String name : DOUBLE_FIELDS) value.append(' ').append(d(field(sn, name).getDouble(actor)));
        return value.toString();
    }
    static String random(Object object) throws Exception {
        require(object != null && object.getClass() == Random.class, "actual ordinary Random");
        return ((AtomicLong)get(object, Random.class, "seed")).get() + ":" +
               d(field(Random.class, "nextNextGaussian").getDouble(object)) + ":" + field(Random.class, "haveNextNextGaussian").getBoolean(object);
    }
    static long unique() throws Exception { return ((AtomicLong)get(null, Random.class, "seedUniquifier")).get(); }
    static String rendererRandomState() throws Exception { StringBuilder value = new StringBuilder(); for (Object r : rendererRandoms) value.append(random(r)).append(';'); return value.toString(); }
    static String origins() throws Exception { return d(field(th, "b").getDouble(null)) + " " + d(field(th, "c").getDouble(null)) + " " + d(field(th, "d").getDouble(null)); }
    static void seedScalars() throws Exception {
        field(th, "i").setFloat(manager, Float.intBitsToFloat(0x3f012345));
        field(th, "j").setFloat(manager, Float.intBitsToFloat(0xbf054321));
        for (int i = 0; i < 3; ++i) field(th, String.valueOf((char)('l' + i))).setDouble(manager, Double.longBitsToDouble(0x3ff123456789abcdL + i));
    }
    static String scalarWords() throws Exception {
        StringBuilder value = new StringBuilder();
        for (String name : new String[]{"i", "j"}) value.append(' ').append(f(field(th, name).getFloat(manager)));
        for (String name : new String[]{"l", "m", "n"}) value.append(' ').append(d(field(th, name).getDouble(manager)));
        return value.toString();
    }
    static void failure(String tag, Throwable error) {
        int depth = 0;
        for (Throwable cause = error; cause != null; cause = cause.getCause()) {
            require(depth < 16, "bounded actual exception chain");
            StackTraceElement top = cause.getStackTrace().length == 0 ? null : cause.getStackTrace()[0];
            row(tag + " " + depth++ + " " + cause.getClass().getName() + " " +
                (top == null ? "- - -1" : top.getClassName() + " " + top.getMethodName() + " " + top.getLineNumber()));
            System.out.println("RM_EXCEPTION_DETAIL " + cause);
        }
    }

    static void prepare() throws Exception {
        // Exact original Minecraft constructor's invokestatic jl.a() at186,
        // declared here as a prerequisite, not execution of the App constructor.
        Method entry = passive("jl").getDeclaredMethod("a");
        try { entry.invoke(null); }
        catch (Throwable error) { failure("RM_PREREQ_THROW", error); throw new AssertionError("real Statistics-first prerequisite failed", error); }
        Class<?> stats = passive("jl");
        StringBuilder values = new StringBuilder("RM_PREP jl a normal");
        values.append(' ').append(((Map<?, ?>)get(null, stats, "a")).size());
        for (String name : new String[]{"b", "c", "d", "e"}) values.append(' ').append(((List<?>)get(null, stats, name)).size());
        for (String name : new String[]{"C", "D", "E", "F"}) {
            Object array = get(null, stats, name);
            values.append(' ').append(array == null ? -1 : Array.getLength(array));
        }
        values.append(' ').append(field(stats, "G").getBoolean(null) ? 1 : 0).append(' ').append(field(stats, "H").getBoolean(null) ? 1 : 0);
        row(values.toString());
    }

    static void modelSlots(Object object, Set<Object> seen) throws Exception {
        if (object == null || !seen.add(object)) return;
        require(seen.size() < 32768, "bounded actual model alias graph");
        Class<?> type = object.getClass();
        if (type.isArray()) {
            if (!type.getComponentType().isPrimitive()) {
                unchangedArrays.add(new ArraySlot(object));
                for (int i = 0; i < Array.getLength(object); ++i) modelSlots(Array.get(object, i), seen);
            }
            return;
        }
        // Model, PS and CV fields/array identities are checked. Vertex/quad
        // coordinates are not exported or given new geometry qualification.
        if (!ko.isAssignableFrom(type) && type != passive("ps") && type != passive("cv")) return;
        for (Field field : fields(type)) {
            unchanged.add(new Slot(object, field));
            Object child = field.get(object);
            if (!field.getType().isPrimitive() && (child == null || child.getClass() != String.class)) modelSlots(child, seen);
        }
    }

    static void roster() throws Exception {
        roster = (Map<?, ?>)get(manager, th, "o"); mapIdentity = roster; raIdentity = get(manager, th, "f");
        require(roster.size() == 28, "complete unchanged TH constructor, no partial roster");
        List<Map.Entry<?, ?>> entries = new ArrayList<>(roster.entrySet());
        entries.sort(Comparator.comparing(entry -> ((Class<?>)entry.getKey()).getName()));
        Set<Object> rendererIds = Collections.newSetFromMap(new IdentityHashMap<>()), models = Collections.newSetFromMap(new IdentityHashMap<>());
        int backpointers = 0, ordinal = 0;
        for (Map.Entry<?, ?> entry : roster.entrySet())
            System.out.println("RM_MAP_ORDER " + ordinal++ + " " + ((Class<?>)entry.getKey()).getName() + " " + type(entry.getValue()));
        for (Map.Entry<?, ?> entry : entries) {
            Class<?> key = (Class<?>)entry.getKey(); Object renderer = entry.getValue();
            rosterEntries.put(key, renderer);
            require(renderer != null && rendererIds.add(renderer) && bw.isAssignableFrom(renderer.getClass()), "all actual distinct BW renderers");
            boolean back = get(renderer, bw, "b") == manager; if (back) ++backpointers;
            Object baseModel = get(renderer, bw, "a"), blocks = get(renderer, bw, "e");
            require(baseModel != null && blocks != null && ko.isInstance(baseModel), "real BW model and CV children");
            row("RM_ROSTER " + key.getName() + " " + type(renderer) + " " + (back ? 1 : 0) + " " + type(baseModel) + " " + type(blocks) + " " + identity(baseModel) + " " + identity(blocks));
            for (Field field : fields(renderer.getClass())) {
                unchanged.add(new Slot(renderer, field));
                if (!field.getType().isPrimitive()) {
                    Object value = field.get(renderer);
                    row("RM_REF " + key.getName() + " " + field.getDeclaringClass().getName() + "." + field.getName() + " " + type(value) + " " + identity(value));
                    if (value != null && value.getClass() == Random.class) rendererRandoms.add(value);
                    modelSlots(value, models);
                }
            }
        }
        require(backpointers == 28, "all actual BW callbacks completed before static publication");
        row("RM_CTOR " + type(manager) + " " + type(roster) + " " + roster.size() + " " + rendererIds.size() + " " + backpointers + " " + type(raIdentity) + " " + unchanged.size() + " " + unchangedArrays.size());
    }

    static boolean aliases() throws Exception {
        if (get(null, th, "a") != manager || get(manager, th, "o") != mapIdentity || roster.size() != 28 || get(manager, th, "f") != raIdentity) return false;
        for (Map.Entry<?, ?> entry : roster.entrySet()) if (!rosterEntries.containsKey(entry.getKey()) || rosterEntries.get(entry.getKey()) != entry.getValue()) return false;
        for (Slot slot : unchanged) if (!slot.same()) return false;
        for (ArraySlot array : unchangedArrays) if (!array.same()) return false;
        return true;
    }
    static Object pig() throws Exception {
        Object actor = passive("wh").getDeclaredConstructor(passive("fd")).newInstance(new Object[]{null});
        require(actor.getClass() == passive("wh") && get(actor, sn, "aI") == null, "genuine original Pig(NULL)");
        row("RM_PIG " + type(actor) + " " + field(sn, "aD").getInt(actor) + " " + field(sn, "a").getInt(null) + " " +
            ((Map<?, ?>)get(get(actor, sn, "bD"), passive("ud"), "b")).size());
        return actor;
    }
    static Object mathRandom() throws Exception {
        Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
        return get(null, holder, "randomNumberGenerator"); // Only AFTER real Pig constructor's Math calls.
    }
    static Method frame() throws Exception { return th.getDeclaredMethod("a", passive("fd"), passive("ji"), passive("sj"), passive("ls"), passive("kv"), float.class); }

    static void awake(Object actor, String input) throws Exception {
        String[] cells = input.split(" "); require(cells.length == 13, "complete raw input row");
        float partial = Float.intBitsToFloat(Integer.parseUnsignedInt(cells[2], 16));
        for (int i = 0; i < 4; ++i) field(sn, FLOAT_FIELDS[i]).setFloat(actor, Float.intBitsToFloat(Integer.parseUnsignedInt(cells[3 + i], 16)));
        for (int i = 0; i < 6; ++i) field(sn, DOUBLE_FIELDS[i]).setDouble(actor, Double.longBitsToDouble(Long.parseUnsignedLong(cells[7 + i], 16)));
        seedScalars();
        Method sleeping = actor.getClass().getMethod("N");
        String beforePose = pose(actor), beforeOrigins = origins(), beforePrivate = random(get(actor, sn, "bs")), beforeMath = random(mathRandom()), beforeRender = rendererRandomState();
        long beforeUnique = unique(); int beforeId = field(sn, "a").getInt(null);
        Map<?, ?> watched = (Map<?, ?>)get(get(actor, sn, "bD"), passive("ud"), "b"); int watchSize = watched.size();
        boolean asleep = (Boolean)sleeping.invoke(actor);
        row("RM_INPUT " + cells[0] + " " + cells[1] + " " + f(partial) + beforePose);
        row("RM_N " + cells[0] + " " + sleeping.getDeclaringClass().getName() + " " + (asleep ? 1 : 0));
        require(!asleep && sleeping.getDeclaringClass() == passive("ls"), "real unchanged default Living.N false");
        frame().invoke(manager, new Object[]{null, null, null, actor, null, partial});
        row("RM_FRAME " + cells[0] + " " + (get(manager, th, "g") == null ? 1 : 0) + " " + (get(manager, th, "e") == null ? 1 : 0) + " " +
            (get(manager, th, "k") == null ? 1 : 0) + " " + (get(manager, th, "h") == actor ? 1 : 0) + " " + (get(manager, th, "p") == null ? 1 : 0) + scalarWords());
        row("RM_SIDE " + cells[0] + " " + (unique() == beforeUnique ? 1 : 0) + " " + (random(get(actor, sn, "bs")).equals(beforePrivate) ? 1 : 0) + " " +
            (random(mathRandom()).equals(beforeMath) ? 1 : 0) + " " + (rendererRandomState().equals(beforeRender) ? 1 : 0) + " " +
            (field(sn, "a").getInt(null) == beforeId ? 1 : 0) + " " + (pose(actor).equals(beforePose) ? 1 : 0) + " " +
            (watched.size() == watchSize ? 1 : 0) + " " + (aliases() ? 1 : 0) + " " + (origins().equals(beforeOrigins) ? 1 : 0));
    }

    static void nullLiving(Object actor, String ordinary) throws Exception {
        awake(actor, ordinary); // h is a genuine Pig occurrence before h -> NULL.
        seedScalars(); String before = scalarWords();
        String beforePose = pose(actor), beforeOrigins = origins(), beforePrivate = random(get(actor, sn, "bs")), beforeMath = random(mathRandom()), beforeRender = rendererRandomState();
        long beforeUnique = unique(); int beforeId = field(sn, "a").getInt(null);
        Map<?, ?> watched = (Map<?, ?>)get(get(actor, sn, "bD"), passive("ud"), "b"); int watchSize = watched.size();
        row("RM_NULL_BEFORE" + before);
        try { frame().invoke(manager, new Object[]{null, null, null, null, null, 0.5f}); throw new AssertionError("NULL Living returned"); }
        catch (InvocationTargetException wrapper) {
            Throwable failure = wrapper.getCause(); StackTraceElement top = failure.getStackTrace()[0];
            require(failure instanceof NullPointerException && top.getClassName().equals("th") && top.getMethodName().equals("a"), "actual TH N29 null failure");
            row("RM_NULL_THROW " + failure.getClass().getName() + " " + top.getClassName() + " " + top.getMethodName() + " " + top.getLineNumber());
            System.out.println("RM_EXCEPTION_DETAIL " + failure);
        }
        StringBuilder refs = new StringBuilder("RM_NULL_AFTER");
        for (String name : new String[]{"g", "e", "k", "h", "p"}) refs.append(' ').append(get(manager, th, name) == null ? 1 : 0);
        row(refs + scalarWords());
        row("RM_NULL_STABLE " + (scalarWords().equals(before) ? 1 : 0) + " " + (aliases() ? 1 : 0));
        row("RM_NULL_SIDE " + (unique() == beforeUnique ? 1 : 0) + " " + (random(get(actor, sn, "bs")).equals(beforePrivate) ? 1 : 0) + " " +
            (random(mathRandom()).equals(beforeMath) ? 1 : 0) + " " + (rendererRandomState().equals(beforeRender) ? 1 : 0) + " " +
            (field(sn, "a").getInt(null) == beforeId ? 1 : 0) + " " + (pose(actor).equals(beforePose) ? 1 : 0) + " " +
            (watched.size() == watchSize ? 1 : 0) + " " + (aliases() ? 1 : 0) + " " + (origins().equals(beforeOrigins) ? 1 : 0));
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 3 && Set.of("cold_th", "prepared_awake", "prepared_null").contains(args[1]), "JAR/profile/raw TSV required");
        Path jar = Path.of(args[0]).toRealPath(); List<String> inputs = Files.readAllLines(Path.of(args[2]));
        loader = RenderManagerFrameObservation.class.getClassLoader();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) { if (p instanceof RuntimePermission && p.getName().equals("setSecurityManager")) throw new SecurityException("guard replacement denied"); }
            @Override public void checkConnect(String h, int p) { ++networks; throw new SecurityException("network denied"); }
            @Override public void checkListen(int p) { ++networks; throw new SecurityException("listen denied"); }
            @Override public void checkAccept(String h, int p) { ++networks; throw new SecurityException("accept denied"); }
            @Override public void checkLink(String name) { ++natives; throw new SecurityException("native denied: " + name); }
            @Override public void checkWrite(String file) { ++writes; throw new SecurityException("write denied"); }
            @Override public void checkDelete(String file) { ++writes; throw new SecurityException("delete denied"); }
            @Override public void checkExec(String command) { throw new SecurityException("process denied"); }
        });
        th = passive("th"); bw = passive("bw"); sn = passive("sn"); ko = passive("ko");
        for (String name : new String[]{"th", "bw", "ra", "ko", "fh", "ps", "ib", "tz", "bt", "cv", "jl", "gm", "uu", "wh", "bg", "ii", "ls", "sn", "eq", "ud", "ma"})
            require(Path.of(passive(name).getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "original class provenance: " + name);
        row("RM_CASE " + args[1]);
        if (!args[1].equals("cold_th")) prepare();
        try { manager = get(null, th, "a"); }
        catch (Throwable error) {
            failure("RM_COLD_THROW", error);
            if (!args[1].equals("cold_th")) throw new AssertionError("prepared genuine TH constructor failed", error);
        }
        if (manager != null) {
            require(manager.getClass() == th, "actual TH publication");
            row("RM_TH_NORMAL 1"); roster(); row("RM_ORIGINS " + origins());
            if (!args[1].equals("cold_th")) {
                Object actor = pig();
                if (args[1].equals("prepared_awake")) for (String input : inputs) awake(actor, input);
                else nullLiving(actor, inputs.get(0));
                require(aliases(), "complete original roster/model/RA/backpointer stability");
            }
        } else require(args[1].equals("cold_th"), "prepared profile requires full actual TH");
        require(networks == 0 && natives == 0 && writes == 0, "unexpected external work");
        System.out.println("Original RenderManager frame: " + args[1] + "; rows=" + rows + "; network=0; native=0; writes=0");
    }
}
