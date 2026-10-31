import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Actual wh(NULL) supplies the Living operand to unchanged LL frame binding.
 * No subclass, Unsafe, serialization constructor, fake World or GL operation. */
public final class TileEntityRendererFrameObservation {
    static int rows, networks, natives, writes;
    static ClassLoader loader;
    static Class<?> ll, sn;
    static final String[] FLOAT_FIELDS = {"aU", "aS", "aV", "aT"};
    static final String[] DOUBLE_FIELDS = {"bl", "bm", "bn", "aM", "aN", "aO"};
    static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Class<?> passive(String name) throws Exception { return Class.forName(name, false, loader); }
    static Field field(Class<?> type, String name) throws Exception { Field f = type.getDeclaredField(name); f.setAccessible(true); return f; }
    static Object get(Object object, Class<?> type, String name) throws Exception { return field(type, name).get(object); }
    static String bits(float value) { return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value)); }
    static String bits(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    static float f(String text) { return Float.intBitsToFloat(Integer.parseUnsignedInt(text, 16)); }
    static double d(String text) { return Double.longBitsToDouble(Long.parseUnsignedLong(text, 16)); }
    static void row(String text) { ++rows; System.out.println(text); }
    static String pose(Object actor) throws Exception {
        StringBuilder value = new StringBuilder();
        for (String name : FLOAT_FIELDS) value.append(' ').append(bits(field(sn, name).getFloat(actor)));
        for (String name : DOUBLE_FIELDS) value.append(' ').append(bits(field(sn, name).getDouble(actor)));
        return value.toString();
    }
    static String random(Object owner) throws Exception {
        require(owner != null && owner.getClass() == Random.class, "actual ordinary Random owner");
        return ((AtomicLong)get(owner, Random.class, "seed")).get() + ":" +
            bits(field(Random.class, "nextNextGaussian").getDouble(owner)) + ":" +
            field(Random.class, "haveNextNextGaussian").getBoolean(owner);
    }
    static long uniquifier() throws Exception { return ((AtomicLong)get(null, Random.class, "seedUniquifier")).get(); }
    static String unrelated(Object manager) throws Exception {
        StringBuilder value = new StringBuilder();
        for (String name : new String[]{"n", "e", "f"}) value.append(get(manager, ll, name) == null).append(':');
        for (String name : new String[]{"b", "c", "d"}) value.append(bits(field(ll, name).getDouble(null))).append(':');
        return value.toString();
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "original JAR and authored TSV inputs required");
        Path jar = Path.of(args[0]).toRealPath();
        List<String> inputs = Files.readAllLines(Path.of(args[1]));
        loader = TileEntityRendererFrameObservation.class.getClassLoader();
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
        for (String name : new String[]{"ll", "je", "po", "ag", "hy", "rf", "ps", "ib", "tz", "bt", "wh", "bg", "ii", "ls", "sn", "eq", "ud", "ma", "fd", "ji", "sj"})
            require(Path.of(passive(name).getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "original class provenance: " + name);
        ll = passive("ll"); sn = passive("sn");
        Object manager = get(null, ll, "a");
        Map<?, ?> map = (Map<?, ?>)get(manager, ll, "m");
        Object piston = map.get(passive("uk")), initialCV = get(piston, passive("hy"), "b");
        Object actor;
        try {
            actor = passive("wh").getDeclaredConstructor(passive("fd")).newInstance(new Object[]{null});
        } catch (InvocationTargetException wrapper) {
            Throwable failure = wrapper.getCause(); StackTraceElement top = failure.getStackTrace()[0];
            System.out.println("TILE_FRAME_CONSTRUCTOR_EXCEPTION " + failure.getClass().getName() + " " + top.getClassName() + " " + top.getMethodName() + " " + top.getLineNumber());
            throw wrapper; // No arithmetic qualification when the real constructor fails.
        }
        require(actor.getClass() == passive("wh") && get(actor, sn, "aI") == null, "actual original Pig with null World");
        Object watcher = get(actor, sn, "bD"), actorRandom = get(actor, sn, "bs");
        Map<?, ?> watched = (Map<?, ?>)get(watcher, passive("ud"), "b");
        // AFTER original LS constructor returned; its four Math.random calls
        // have already reached the real JDK holder. Never warm it beforehand.
        Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
        Object mathRandom = get(null, holder, "randomNumberGenerator");
        row("TILE_FRAME_CTOR " + actor.getClass().getName() + " 1 " + (get(actor, sn, "aI") == null ? 1 : 0) + " " +
            field(sn, "aD").getInt(actor) + " " + field(sn, "a").getInt(null) + " " + watched.size() + " " +
            (actorRandom != mathRandom ? 1 : 0));
        Method frame = ll.getDeclaredMethod("a", passive("fd"), passive("ji"), passive("sj"), passive("ls"), float.class);
        int completed = 0;
        for (String input : inputs) {
            String[] cells = input.split(" ");
            require(cells.length == 13 && Integer.parseInt(cells[0]) == completed, "ordered complete authored input shape");
            float partial = f(cells[2]);
            for (int i = 0; i < FLOAT_FIELDS.length; ++i) field(sn, FLOAT_FIELDS[i]).setFloat(actor, f(cells[3 + i]));
            for (int i = 0; i < DOUBLE_FIELDS.length; ++i) field(sn, DOUBLE_FIELDS[i]).setDouble(actor, d(cells[7 + i]));
            field(ll, "h").setFloat(manager, Float.intBitsToFloat(0x3f012345));
            field(ll, "i").setFloat(manager, Float.intBitsToFloat(0xbf054321));
            for (int i = 0; i < 3; ++i)
                field(ll, String.valueOf((char)('j' + i))).setDouble(manager, Double.longBitsToDouble(0x3ff123456789abcdL + i));
            String poseBefore = pose(actor), actorBefore = random(actorRandom), mathBefore = random(mathRandom), otherBefore = unrelated(manager);
            long uniqueBefore = uniquifier(); int idBefore = field(sn, "a").getInt(null), watchedBefore = watched.size();
            row("TILE_FRAME_DECLARED " + input);
            row("TILE_FRAME_INPUT " + cells[0] + " " + cells[1] + " " + bits(partial) + poseBefore);
            Throwable failure = null;
            try { frame.invoke(manager, new Object[]{null, null, null, actor, partial}); }
            catch (InvocationTargetException wrapper) { failure = wrapper.getCause(); }
            StringBuilder result = new StringBuilder("TILE_FRAME_RESULT " + cells[0] + " " + (failure == null ? 1 : 0));
            result.append(' ').append(get(manager, ll, "e") == null ? 1 : 0).append(' ').append(get(manager, ll, "f") == null ? 1 : 0)
                  .append(' ').append(get(manager, ll, "g") == actor ? 1 : 0).append(' ').append(get(manager, ll, "n") == null ? 1 : 0)
                  .append(' ').append(get(manager, ll, "m") == map ? 1 : 0).append(' ').append(get(null, ll, "a") == manager ? 1 : 0)
                  .append(' ').append(get(piston, passive("hy"), "b") == initialCV ? 1 : 0);
            for (String name : new String[]{"h", "i"}) result.append(' ').append(bits(field(ll, name).getFloat(manager)));
            for (String name : new String[]{"j", "k", "l"}) result.append(' ').append(bits(field(ll, name).getDouble(manager)));
            row(result.toString());
            row("TILE_FRAME_SIDE " + cells[0] + " " + (uniquifier() == uniqueBefore ? 1 : 0) + " " +
                (random(actorRandom).equals(actorBefore) ? 1 : 0) + " " + (random(mathRandom).equals(mathBefore) ? 1 : 0) + " " +
                (field(sn, "a").getInt(null) == idBefore ? 1 : 0) + " " + (pose(actor).equals(poseBefore) ? 1 : 0) + " " +
                (unrelated(manager).equals(otherBefore) ? 1 : 0) + " " + (watched.size() == watchedBefore ? 1 : 0));
            if (failure != null) {
                StackTraceElement top = failure.getStackTrace()[0];
                System.out.println("TILE_FRAME_EXCEPTION " + cells[0] + " " + failure.getClass().getName() + " " + top.getClassName() + " " + top.getMethodName() + " " + top.getLineNumber());
                throw new AssertionError("actual original frame failed", failure);
            }
            ++completed;
        }
        require(networks == 0 && natives == 0 && writes == 0 && completed == inputs.size(), "unexpected external work/incomplete frames");
        System.out.println("Original TileEntityRenderer frame: cases=" + completed + "; rows=" + rows + "; network=0; native=0; writes=0");
    }
}
