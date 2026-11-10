import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

/** Observe unchanged original World/Arrow water calls. Reflection only reads
 * values; no Source field, Random or pool reset supplies a returned result. */
public final class ArrowWaterOriginalObservation {
    static final String JAR_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final IdentityHashMap<Object, Integer> ids = new IdentityHashMap<>();
    static final List<Object> retained = new ArrayList<>();
    static int rows, snapshots, calls, network, nativeLinks, execs, rejectedWrites;
    static Path archive, root;
    static Set<Long> initialThreads;

    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }
    static Object get(Class<?> type, String name, Object owner) throws Exception {
        return field(type, name).get(owner);
    }
    static String sha(byte[] value) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));
    }
    static int id(Object value) {
        if (value == null) return 0;
        Integer old = ids.get(value);
        if (old != null) return old;
        need(retained.size() < 8192, "observer identity capacity");
        int result = retained.size() + 1;
        ids.put(value, result);
        retained.add(value);
        return result;
    }
    static void row(Object... values) {
        need(++rows <= 8192, "observer row capacity");
        StringBuilder line = new StringBuilder();
        for (Object value : values) {
            if (line.length() != 0) line.append('\t');
            String text = String.valueOf(value);
            need(text.indexOf('\t') < 0 && text.indexOf('\n') < 0 && text.indexOf('\r') < 0,
                 "unescaped observer value");
            line.append(text);
        }
        need(line.length() <= 4096, "observer row extent");
        System.out.println(line);
        System.out.flush();
        need(!System.out.checkError(), "complete observer transport");
    }
    static void origin(Class<?> type) throws Exception {
        need(type.getProtectionDomain().getCodeSource() != null &&
             Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                 .toRealPath().equals(archive), "foreign game class " + type.getName());
        row("ORIGIN", type.getName());
    }
    static String scalar(Class<?> type, Object value) {
        if (type == double.class)
            return String.format(java.util.Locale.ROOT, "%016x", Double.doubleToRawLongBits((Double)value));
        if (type == float.class)
            return String.format(java.util.Locale.ROOT, "%08x", Float.floatToRawIntBits((Float)value));
        if (type.isPrimitive()) return String.valueOf(value);
        return id(value) + ":" + (value == null ? "null" : value.getClass().getName());
    }
    static void fields(int snapshot, Class<?> owner, Object object) throws Exception {
        Field[] fields = owner.getDeclaredFields();
        Arrays.sort(fields, Comparator.comparing(Field::getName));
        for (Field member : fields) {
            if (Modifier.isStatic(member.getModifiers())) continue;
            member.setAccessible(true);
            row("FIELD", snapshot, id(object), owner.getName(), member.getName(),
                member.getType().getName(), scalar(member.getType(), member.get(object)));
        }
    }
    static void random(int snapshot, String role, Random object) throws Exception {
        row("RANDOM", snapshot, role, id(object),
            String.format(java.util.Locale.ROOT, "%016x", ((AtomicLong)get(Random.class,"seed",object)).get()),
            get(Random.class,"haveNextNextGaussian",object),
            String.format(java.util.Locale.ROOT, "%016x", Double.doubleToRawLongBits(
                (Double)get(Random.class,"nextNextGaussian",object))));
    }
    static void list(int snapshot, String role, List<?> object) {
        need(object != null && object.size() <= 128, "bounded actual list " + role);
        row("LIST", snapshot, role, id(object), object.size());
        for (int i = 0; i < object.size(); ++i)
            row("LIST_ITEM", snapshot, role, i, id(object.get(i)));
    }
    static void snapshot(String stage, fd world, sl arrow) throws Exception {
        int index = snapshots++;
        row("SNAPSHOT", index, stage, calls, id(world), id(arrow));
        fields(index, sn.class, arrow);
        fields(index, sl.class, arrow);
        fields(index, eq.class, arrow.aW);
        fields(index, fd.class, world);
        random(index, "arrow.bs", (Random)get(sn.class,"bs",arrow));
        random(index, "world.r", world.r);
        list(index, "world.b", world.b);
        list(index, "world.D", (List<?>)get(fd.class,"D",world));
        list(index, "world.e", world.e);
        list(index, "world.R", (List<?>)get(fd.class,"R",world));
        Object cache = get(fd.class,"v",world);
        List<?> chunks = (List<?>)get(ok.class,"f",cache);
        need(chunks.size() == 25, "actual provider insertion list remains 25");
        for (int i = 0; i < chunks.size(); ++i) {
            Object chunk = chunks.get(i);
            row("CHUNK", index, i, id(chunk), get(lm.class,"j",chunk), get(lm.class,"k",chunk),
                id(get(lm.class,"d",chunk)));
            for (int slice = 0; slice < 8; ++slice)
                list(index, "chunk." + i + "." + slice, ((List<?>[])get(lm.class,"m",chunk))[slice]);
        }
        noNewLiveThreads();
    }
    static void noNewLiveThreads() {
        for (Thread thread : Thread.getAllStackTraces().keySet())
            need(!thread.isAlive() || initialThreads.contains(thread.getId()),
                 "unqualified new live thread " + thread.getName());
    }

    static void prepareWater(fd world, int block) {
        for (int x=6;x<=14;++x) for(int y=64;y<=68;++y) for(int z=6;z<=10;++z) {
            boolean changed = world.a(x,y,z,block,0);
            row("PREPARE_QUIET_SET",x,y,z,block,0,changed);
            need(changed,"actual quiet water setter");
        }
    }
    static final class Observer implements pm {
        final sl arrow;
        int events;
        Observer(sl value) { arrow=value; }
        String bits(double value) { return String.format(java.util.Locale.ROOT,"%016x",Double.doubleToRawLongBits(value)); }
        String bits(float value) { return String.format(java.util.Locale.ROOT,"%08x",Float.floatToRawIntBits(value)); }
        void event(boolean sound,String name,double x,double y,double z,double vx,double vy,double vz,float volume,float pitch) {
            need(++events<=64,"bounded observed callbacks");
            try {
                Random random=(Random)get(sn.class,"bs",arrow);
                row("WATER_EVENT",calls,events,sound,name,bits(x),bits(y),bits(z),bits(vx),bits(vy),bits(vz),bits(volume),bits(pitch),
                    arrow.bt,arrow.bx,((AtomicLong)get(Random.class,"seed",random)).get(),
                    get(Random.class,"haveNextNextGaussian",random),bits((Double)get(Random.class,"nextNextGaussian",random)));
            } catch(Exception error) { throw new AssertionError(error); }
        }
        public void a(String n,double x,double y,double z,float volume,float pitch) { event(true,n,x,y,z,0,0,0,volume,pitch); }
        public void a(String n,double x,double y,double z,double vx,double vy,double vz) { event(false,n,x,y,z,vx,vy,vz,0,0); }
        public void a(int x,int y,int z) { throw new AssertionError("unexpected observer cell"); }
        public void b(int a,int b,int c,int d,int e,int f) { throw new AssertionError("unexpected observer range"); }
        public void a(sn e) { throw new AssertionError("unexpected observer entity added"); }
        public void b(sn e) { throw new AssertionError("unexpected observer entity removed"); }
        public void e() { throw new AssertionError("unexpected observer sky"); }
        public void a(String n,int x,int y,int z) { throw new AssertionError("unexpected observer record"); }
        public void a(int x,int y,int z,ow e) { throw new AssertionError("unexpected observer tile"); }
        public void a(gs p,int a,int b,int c,int d,int e) { throw new AssertionError("unexpected observer auxiliary"); }
    }
    @SuppressWarnings("removal")
    static final class Guard extends SecurityManager {
        public void checkPermission(Permission permission) {}
        public void checkConnect(String host, int port) { ++network; throw new SecurityException("network denied"); }
        public void checkListen(int port) { ++network; throw new SecurityException("listen denied"); }
        public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native library denied"); }
        public void checkExec(String command) { ++execs; throw new SecurityException("exec denied"); }
        public void checkWrite(String path) {
            if (!Path.of(path).toAbsolutePath().normalize().startsWith(root)) {
                ++rejectedWrites;
                throw new SecurityException("outside isolated fixture write");
            }
        }
        public void checkDelete(String path) { checkWrite(path); }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Throwable {
        need(args.length == 3, "JAR MODE PREAUTHORED_ROOT");
        archive = Path.of(args[0]).toRealPath();
        need(sha(Files.readAllBytes(archive)).equals(JAR_SHA), "unchanged original archive");
        String mode = args[1];
        need(Set.of("water-first","water-multi","dry-to-water","water-to-air","null-callback").contains(mode), "finite water mode");
        root = Path.of(args[2]).toRealPath();
        need(Files.isDirectory(root) && Files.isRegularFile(root.resolve("world0/level.dat")),
             "preauthored saved World required");
        initialThreads = new HashSet<>();
        for (Thread thread : Thread.getAllStackTraces().keySet()) initialThreads.add(thread.getId());
        System.setSecurityManager(new Guard());
        row("ARROW_WATER_ORIGINAL_BEGIN", 1, mode);
        for (Class<?> type : new Class<?>[]{fd.class,mx.class,fm.class,wt.class,xa.class,rh.class,
                ok.class,ld.class,lm.class,li.class,sn.class,sl.class,eq.class,bt.class,vf.class,
                in.class,uu.class,to.class,as.class,nu.class,rj.class,qj.class}) origin(type);
        try {
            mx saving = new mx(root.toFile(), "world0", false);
            /* The SAME load-or-create descriptor used by the C fixture:
             * (Lwt;Ljava/lang/String;JLxa;)V, NOT the direct-metadata overload. */
            fd world = new fd(saving, "Tick", -99L, xa.a(0));
            need(world.getClass() == fd.class && !world.B, "ordinary saved client World");
            Object cache = get(fd.class,"v",world);
            need(cache.getClass() == ok.class && get(ok.class,"g",cache) == world &&
                 get(ok.class,"d",cache).getClass() == ld.class, "genuine provider/loader");
            // Actual Native setup calls center once, then remaining coordinates
            // in lexicographic order. This is input order, not a cache repair.
            List<int[]> loadOrder = new ArrayList<>();
            loadOrder.add(new int[]{0,0});
            for (int x = -2; x <= 2; ++x) for (int z = -2; z <= 2; ++z)
                if (x != 0 || z != 0) loadOrder.add(new int[]{x,z});
            for (int[] coordinate : loadOrder) {
                int x = coordinate[0], z = coordinate[1];
                lm chunk = world.c(x,z);
                need(chunk.getClass() == lm.class && chunk.d == world && chunk.j == x && chunk.k == z,
                     "normal stored Chunk, no Blank/subclass");
                byte[] blocks = (byte[])get(lm.class,"b",chunk);
                need(blocks.length == 32768, "stored block extent");
                for (byte value : blocks) need(value == 0, "authored stored air only");
                for (List<?> slice : chunk.m) need(slice.isEmpty(), "no authored actor");
            }
            need(((Map<?,?>)get(ok.class,"e",cache)).size() == 25 &&
                 ((List<?>)get(ok.class,"f",cache)).size() == 25, "exact 25 loaded records");
            row("SETUP", id(world), id(cache), 25, world.B);

            sl arrow = new sl(world,8.5,66.0,8.5);
            snapshot("constructed",world,arrow);
            arrow.a(1.0,0.0,0.0,0.6f,0.0f);
            /* Do not inspect/activate Math's random holder: IN's real clinit
             * fills its sine table with Math.sin, not Math.random. */
            snapshot("headed",world,arrow);
            boolean inserted = world.b(arrow);
            need(inserted && world.b.size() == 1 && world.b.get(0) == arrow && arrow.bF,
                 "genuine Source insertion/membership");
            snapshot("inserted",world,arrow);
            if (mode.equals("dry-to-water")) {
                row("CALL",calls,"fd.g()V","before-dry"); ++calls; world.g();
                row("OUTCOME",calls,"returned"); snapshot("dry",world,arrow);
            }
            prepareWater(world,9);
            Observer observer = new Observer(arrow);
            world.a(observer);
            if (mode.equals("null-callback")) world.a((pm)null);
            int count = mode.equals("water-multi") ? 8 : mode.equals("water-to-air") ? 2 : 1;
            for (int i = 0; i < count; ++i) {
                if (mode.equals("water-to-air") && i == 1) {
                    world.b(observer); prepareWater(world,0); world.a(observer);
                }
                row("CALL", calls, "fd.g()V", "before");
                ++calls;
                try { world.g(); }
                catch (Throwable failure) {
                    row("OUTCOME", calls, "threw", failure.getClass().getName());
                    snapshot("failed",world,arrow);
                    need(mode.equals("null-callback") && failure.getClass() == NullPointerException.class,
                         "only the authored null observer may throw");
                    break;
                }
                row("OUTCOME", calls, "returned");
                snapshot("returned",world,arrow);
                need(arrow.bt == calls, "actual Arrow Base reached each World call");
            }
            need(!mode.equals("null-callback") || observer.events == 1,"null callback stops first bubble");
            row("WATER_CALLBACKS",observer.events);
            row("ARROW_WATER_ORIGINAL_SOURCE_COMPLETE", mode, calls, snapshots);
        } finally {
            rj.a(); // Actual Source Region cache close/flush, no save or getter replay.
            row("ARROW_WATER_ORIGINAL_REGION_CLOSED", mode);
        }
        noNewLiveThreads();
        need(network == 0 && nativeLinks == 0 && execs == 0 && rejectedWrites == 0,
             "unqualified denied side effect attempt");
        row("GUARD", network, nativeLinks, execs, rejectedWrites);
        row("ARROW_WATER_ORIGINAL_COMPLETE", mode, calls, snapshots, rows + 1);
    }
}
