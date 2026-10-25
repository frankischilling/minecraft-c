import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Original game methods remain unchanged. World.r.setSeed is an explicit
 * post-constructor input; no replacement Random, World, provider or queue.
 */
public final class WorldPropagationProbe {
    static Object world, cache, generator, cave, sky, block;
    static Class<?> fd, lm;
    static Method getBlock, drain;
    static DataOutputStream queues;
    static Path output;
    static int event;
    static final MessageDigest digest;
    static {
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (Exception error) {
            throw new ExceptionInInitializerError(error);
        }
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field f = type.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static Object get(Class<?> type, Object receiver, String name) throws Exception {
        return field(type, name).get(receiver);
    }

    static Object call(Method method, Object receiver, Object... args) throws Exception {
        try {
            return method.invoke(receiver, args);
        } catch (InvocationTargetException error) {
            Throwable c = error.getCause();
            if (c instanceof Exception)
                throw (Exception)c;
            if (c instanceof Error)
                throw (Error)c;
            throw error;
        }
    }

    static List<?> queue() throws Exception {
        return (List<?>)get(fd, world, "C");
    }

    static Map<?, ?> chunks() throws Exception {
        return (Map<?, ?>)get(cache.getClass(), cache, "e");
    }

    static Random worldRandom() throws Exception {
        return (Random)get(fd, world, "r");
    }

    static byte[] bytes(Object chunk, String name) throws Exception {
        return (byte[])get(lm, chunk, name);
    }

    static byte[] nibbles(Object chunk, String name) throws Exception {
        Object n = get(lm, chunk, name);
        return (byte[])get(n.getClass(), n, "a");
    }

    static byte[][] arrays(Object chunk) throws Exception {
        return new byte[][] {bytes(chunk, "b"), nibbles(chunk, "e"), nibbles(chunk, "f"),
                             nibbles(chunk, "g"), bytes(chunk, "h")};
    }

    static String hex(byte[] bytes) {
        return HexFormat.of().formatHex(bytes);
    }

    static String random(Random random) throws Exception {
        long seed = ((AtomicLong)get(Random.class, random, "seed")).get();
        boolean have = (Boolean)get(Random.class, random, "haveNextNextGaussian");
        long gaussian =
            Double.doubleToRawLongBits((Double)get(Random.class, random, "nextNextGaussian"));
        return String.format(Locale.ROOT, "[\"%012x\",%s,\"%016x\"]", seed, have, gaussian);
    }

    static void observe(String phase, int code, int point, int call, boolean more)
        throws Exception {
        List<?> regions = queue();
        Set<?> f = (Set<?>)get(fd, world, "F"), e = (Set<?>)get(fd, world, "E");
        int m = field(fd, "M").getInt(world);
        System.out.println(
            "{\"event\":" + event + ",\"phase\":\"" + phase + "\",\"point\":" + point +
            ",\"call\":" + call + ",\"more\":" + more + ",\"C\":" + regions.size() +
            ",\"M\":" + m + ",\"F\":" + f.size() + ",\"E\":" + e.size() +
            ",\"cache\":" + chunks().size() + ",\"world_rng\":" + random(worldRandom()) +
            ",\"generator_rng\":" + random((Random)get(generator.getClass(), generator, "j")) +
            ",\"cave_rng\":" + random((Random)get(Class.forName("fv"), cave, "b")) + "}");
        long eventBytes = 20L + regions.size() * 28L;
        if (queues.size() + eventBytes > 512L * 1024 * 1024)
            throw new IOException("queue capture byte budget");
        queues.writeInt(event++);
        queues.writeInt(code);
        queues.writeInt(point);
        queues.writeInt(call);
        queues.writeInt(regions.size());
        Class<?> st = Class.forName("st");
        for (Object region : regions) {
            Object type = get(st, region, "a");
            queues.writeInt(type == sky ? 0 : type == block ? 1 : 2);
            for (String name : new String[] {"b", "c", "d", "e", "f", "g"})
                queues.writeInt(field(st, name).getInt(region));
        }
    }

    static void snapshot(String name) throws Exception {
        TreeMap<String, Object> sorted = new TreeMap<>();
        for (Object c : chunks().values())
            sorted.put(field(lm, "j").getInt(c) + "," + field(lm, "k").getInt(c), c);
        try (DataOutputStream out = new DataOutputStream(
                 new BufferedOutputStream(Files.newOutputStream(output.resolve(name))))) {
            out.writeInt(0x4c495431);
            out.writeInt(sorted.size());
            for (Object c : sorted.values()) {
                out.writeInt(field(lm, "j").getInt(c));
                out.writeInt(field(lm, "k").getInt(c));
                for (byte[] a : arrays(c))
                    out.write(a);
            }
        }
    }

    static long[] record(Object tick) throws Exception {
        Class<?> t = Class.forName("qy");
        return new long[] {field(t, "a").getInt(tick),  field(t, "b").getInt(tick),
                           field(t, "c").getInt(tick),  field(t, "d").getInt(tick),
                           field(t, "e").getLong(tick), field(t, "g").getLong(tick)};
    }

    static void writeRecord(DataOutputStream out, long[] t) throws Exception {
        for (int i = 0; i < 4; i++)
            out.writeInt((int)t[i]);
        out.writeLong(t[4]);
        out.writeLong(t[5]);
    }

    static void scheduler() throws Exception {
        Set<?> f = (Set<?>)get(fd, world, "F"), e = (Set<?>)get(fd, world, "E");
        List<long[]> rows = new ArrayList<>();
        IdentityHashMap<Object, Boolean> identity = new IdentityHashMap<>();
        for (Object t : f) {
            rows.add(record(t));
            identity.put(t, Boolean.TRUE);
        }
        rows.sort((a, b) -> {
            for (int i = 0; i < 6; i++) {
                int c = Long.compare(a[i], b[i]);
                if (c != 0)
                    return c;
            }
            return 0;
        });
        try (DataOutputStream out = new DataOutputStream(new BufferedOutputStream(
                 Files.newOutputStream(output.resolve("scheduler.bin"))))) {
            out.writeInt(0x53434831);
            out.writeInt(f.size());
            out.writeInt(e.size());
            for (long[] r : rows)
                writeRecord(out, r);
            for (Object t : e) {
                writeRecord(out, record(t));
                out.writeInt(identity.containsKey(t) ? 1 : 0);
            }
        }
    }

    static void drainWorld(int point) throws Exception {
        int calls = 0;
        boolean more;
        do {
            if (++calls > 10000)
                throw new IllegalStateException("observer drain budget");
            more = (Boolean)call(drain, world);
            observe("drain", 11, point, calls, more);
        } while (more);
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2 ||
            (!args[1].equals("view") && !args[1].equals("fresh") && !args[1].equals("warm")))
            throw new IllegalArgumentException("new output directory view|fresh|warm");
        output = Path.of(args[0]);
        if (Files.exists(output))
            throw new IllegalArgumentException("fresh output required");
        Files.createDirectories(output);
        fd = Class.forName("fd");
        lm = Class.forName("lm");
        Path jar = Path.of(fd.getProtectionDomain().getCodeSource().getLocation().toURI());
        if (!hex(digest.digest(Files.readAllBytes(jar)))
                 .equals("af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"))
            throw new IllegalStateException("original JAR pin");
        String origin =
            fd.getProtectionDomain().getCodeSource().getLocation().toURI().toASCIIString();
        System.out.println("{\"runtime_origin\":true,\"fd_jar_uri\":\"" + origin +
                           ("\",\"client_jar_sha256\":" +
                            "\"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c\"" +
                            ",\"java_runtime_version\":\"") +
                           System.getProperty("java.runtime.version") + "\",\"java_vm_name\":\"" +
                           System.getProperty("java.vm.name") + "\"}");
        Object storage = Class.forName("mx")
                             .getConstructor(File.class, String.class, boolean.class)
                             .newInstance(output.toFile(), "world", false);
        world = fd.getConstructor(Class.forName("wt"), String.class, long.class)
                    .newInstance(storage, "", 23L);
        cache = get(fd, world, "v");
        generator = get(cache.getClass(), cache, "c");
        cave = get(generator.getClass(), generator, "u");
        Class<?> eb = Class.forName("eb");
        sky = get(eb, null, "a");
        block = get(eb, null, "b");
        getBlock = fd.getMethod("a", int.class, int.class, int.class);
        drain = fd.getMethod("j");
        // This is the explicit diagnostic input, AFTER all genuine constructor effects.
        worldRandom().setSeed(0x0123456789abcdefL);
        Object spawn = call(fd.getMethod("u"), world);
        Class<?> br = spawn.getClass();
        int sx = field(br, "a").getInt(spawn), sy = field(br, "b").getInt(spawn),
            sz = field(br, "c").getInt(spawn);
        System.out.println("{\"inputs\":true,\"world_seed\":23,\"controlled_world_random_seed\":" +
                           "\"0123456789abcdef\",\"spawn\":[" +
                           sx + "," + sy + "," + sz + "]}");
        try (DataOutputStream q = new DataOutputStream(
                 new BufferedOutputStream(Files.newOutputStream(output.resolve("queues.bin"))))) {
            queues = q;
            q.writeInt(0x51433031);
            observe("constructed", 0, 0, 0, false);
            snapshot("constructed.chunks");
            if (!args[1].equals("fresh")) {
                Constructor<?> ew = Class.forName("ew").getConstructor(
                    fd, int.class, int.class, int.class, int.class, int.class, int.class);
                int cx = (sx >> 4) * 16, cz = (sz >> 4) * 16;
                for (int x = -1; x <= 1; x++)
                    for (int z = -1; z <= 1; z++)
                        for (int y = 0; y < 128; y += 16)
                            ew.newInstance(world, cx + x * 16 - 1, y - 1, cz + z * 16 - 1,
                                           cx + x * 16 + 17, y + 17, cz + z * 16 + 17);
                observe("view-undrained", 1, 0, 0, false);
                snapshot("view-undrained.chunks");
                drainWorld(0);
                snapshot("view-drained.chunks");
            }
            int point = 0;
            if (!args[1].equals("view"))
                for (int x = -128; x <= 128; x += 16)
                    for (int z = -128; z <= 128; z += 16) {
                        call(getBlock, world, sx + x, 64, sz + z);
                        ++point;
                        observe("acquired", 10, point, 0, false);
                        drainWorld(point);
                    }
            snapshot("final.chunks");
            scheduler();
            observe("complete", 12, point, 0, false);
        }
    }
}
