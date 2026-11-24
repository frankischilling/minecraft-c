import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored metadata operands around unchanged original World/Chunk/Nibble bodies. */
public final class MetadataSetObservation {
    static int network, nativeLinks, rejectedWrites, rows, events;
    static Path outputRoot;
    static fd world;
    static lm chunk;
    static byte[] fixed;
    static final int index = ((8 << 11) | (8 << 7) | 64) >> 1;
    static Map<Object,Object> cache;
    static List<Object> loaded;

    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }
    static long seed(Random random) throws Exception {
        return ((AtomicLong)field(Random.class, "seed").get(random)).get();
    }
    static Map<String,Object> map(Object... pairs) {
        Map<String,Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) result.put((String)pairs[i], pairs[i + 1]);
        return result;
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String)
            return "\"" + ((String)value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        if (value instanceof Map) {
            StringBuilder result = new StringBuilder("{");
            for (Object item : ((Map<?,?>)value).entrySet()) {
                Map.Entry<?,?> entry = (Map.Entry<?,?>)item;
                if (result.length() > 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return result.append('}').toString();
        }
        if (value instanceof List) {
            StringBuilder result = new StringBuilder("[");
            for (Object item : (List<?>)value) {
                if (result.length() > 1) result.append(',');
                result.append(json(item));
            }
            return result.append(']').toString();
        }
        return value.toString();
    }
    static final class Observer implements pm {
        public void a(String n,double x,double y,double z,float volume,float pitch) {++events;}
        public void a(sn entity) {++events;}
        public void a(String n,double x,double y,double z,double a,double b,double c) {++events;}
        public void b(sn entity) {++events;}
        public void a(int x,int y,int z) {++events;}
        public void b(int x,int y,int z,int a,int b,int c) {++events;}
        public void e() {++events;}
        public void a(String n,int x,int y,int z) {++events;}
        public void a(int x,int y,int z,ow entity) {++events;}
        public void a(gs p,int a,int b,int c,int d,int e) {++events;}
    }
    @SuppressWarnings("unchecked")
    static void prepare() throws Exception {
        world = new fd(new wy(), "MetadataSetReference", xa.a(0), 23L);
        Object provider = field(fd.class, "v").get(world);
        cache = (Map<Object,Object>)field(ok.class, "e").get(provider);
        loaded = (List<Object>)field(ok.class, "f").get(provider);
        cache.clear(); loaded.clear();
        for (int x = -1; x <= 1; ++x) for (int z = -1; z <= 1; ++z) {
            lm value = new lm(world, new byte[32768], x, z);
            value.n = true;
            cache.put(yy.a(x, z), value); loaded.add(value);
            if (x == 0 && z == 0) chunk = value;
        }
        need(cache.size() == 9 && loaded.size() == 9 && chunk != null, "nine genuine cached Chunks");
        fixed = chunk.e.a;
        need(fixed.length == 16384 && fixed[index] == 0, "original full metadata backing");
        world.r.setSeed(2468);
        world.a(new Observer());
    }
    static Integer selected(byte[] backing) {
        return backing != null && index < backing.length ? backing[index] & 255 : null;
    }
    static boolean unchangedOther(byte[] before, byte[] after) {
        if (before == null || after == null) return before == after;
        if (before.length != after.length) return false;
        for (int i = 0; i < before.length; ++i)
            if (i != index && before[i] != after[i]) return false;
        return true;
    }
    static void run(String profile, int x, int y, int z, int metadata) throws Exception {
        chunk.o = false;
        events = 0;
        wi nibble = chunk.e;
        byte[] backing = nibble == null ? null : nibble.a;
        byte[] backingBefore = backing == null ? null : backing.clone();
        byte[] fixedBefore = fixed.clone();
        Map<Object,Object> cacheBefore = new HashMap<>(cache);
        List<Object> loadedBefore = new ArrayList<>(loaded);
        long randomBefore = seed(world.r);
        boolean dirtyBefore = chunk.o, returned = false;
        Boolean answer = null;
        String exception = null;
        List<Object> frames = new ArrayList<>();
        String message = null;
        try {
            answer = world.e(x, y, z, metadata);
            returned = true;
        } catch (NullPointerException | ArrayIndexOutOfBoundsException failure) {
            exception = failure.getClass().getName().replace('.', '/');
            message = failure.getMessage();
            for (StackTraceElement frame : failure.getStackTrace()) {
                if (frame.getClassName().equals("fd") || frame.getClassName().equals("lm") ||
                    frame.getClassName().equals("wi") || frame.getClassName().equals("li"))
                    frames.add(map("class", frame.getClassName(), "method", frame.getMethodName(),
                                   "line", frame.getLineNumber()));
            }
        }
        System.out.println("METADATA_SET " + json(map(
            "profile", profile, "x", x, "y", y, "z", z, "metadata", metadata,
            "returned", returned, "result", answer, "exception", exception,
            "dirty_before", dirtyBefore, "dirty_after", chunk.o,
            "nibble_null", nibble == null, "backing_null", backing == null,
            "backing_length", backing == null ? null : backing.length,
            "before_byte", selected(backingBefore), "after_byte", selected(backing),
            "fixed_before", fixedBefore[index] & 255, "fixed_after", fixed[index] & 255,
            "fixed_unchanged", Arrays.equals(fixedBefore, fixed),
            "unselected_unchanged", unchangedOther(backingBefore, backing),
            "cache_membership_unchanged", cacheBefore.equals(cache) && loadedBefore.equals(loaded),
            "world_random_unchanged", randomBefore == seed(world.r), "events", events)));
        if (exception != null)
            System.out.println("METADATA_SET_FAILURE " + json(map("profile", profile,
                "exception", exception, "message", message, "frames", frames)));
        ++rows;
    }
    static void origin(Class<?> type, Path archive) throws Exception {
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),
             "unchanged original archive origin " + type.getName());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length == 2, "original JAR and private output root");
        Path jar = Path.of(args[0]).toRealPath();
        outputRoot = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h,int p) {++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p) {++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n) {++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p) {
                if (!Path.of(p).toAbsolutePath().normalize().startsWith(outputRoot)) {
                    ++rejectedWrites;throw new SecurityException("write outside private root denied");
                }
            }
            @Override public void checkDelete(String p) {++rejectedWrites;throw new SecurityException("delete denied");}
        });
        for (Class<?> type : new Class<?>[]{fd.class,wy.class,xa.class,gr.class,ok.class,lm.class,
                 li.class,wi.class,yy.class}) origin(type, jar);
        new gr("MetadataSetReference", "");
        prepare();
        lm normal = chunk;
        wi original = normal.e;
        byte[] replacement = new byte[index + 1];
        replacement[index] = (byte)0xa3;
        chunk.e = new wi(replacement);
        run("replacement-even18",8,64,8,18);
        run("replacement-odd-minus1",8,65,8,-1);
        chunk.e = null;
        run("e-null-even",8,64,8,18);
        run("e-null-odd",8,65,8,18);
        chunk.e = original;
        field(wi.class, "a").set(original, null);
        run("a-null-even",8,64,8,18);
        run("a-null-odd",8,65,8,18);
        field(wi.class, "a").set(original, new byte[0]);
        run("short-even",8,64,8,18);
        run("short-odd",8,65,8,18);
        field(wi.class, "a").set(original, fixed);
        chunk = new li(world, new byte[32768],0,0);
        chunk.e = null;
        cache.put(yy.a(0,0), chunk); loaded.set(loaded.indexOf(normal), chunk);
        run("blank-null-e",8,64,8,18);
        cache.put(yy.a(0,0), normal); loaded.set(loaded.indexOf(chunk), normal);
        chunk = normal;
        chunk.e = null;
        run("outside-y128",8,128,8,18);
        run("outside-y-minus1",8,-1,8,18);
        run("outside-x-upper",32000000,64,8,18);
        run("outside-z-upper",8,64,32000001,18);
        chunk.e = original;
        fixed[index] = (byte)0xa3;
        run("normal-even18",8,64,8,18);
        run("normal-odd-minus1",8,65,8,-1);
        need(rows == 15 && network == 0 && nativeLinks == 0 && rejectedWrites == 0,
             "finite observations and external-effect guards");
        System.out.println("Original metadata set: 15 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9");
    }
}
