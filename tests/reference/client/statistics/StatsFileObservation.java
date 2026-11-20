import java.io.File;
import java.io.PrintStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.lang.management.ManagementFactory;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.Map;

/** Authored reflection adapter. All game receivers/bodies come from the supplied JAR. */
public final class StatsFileObservation {
    private static ClassLoader loader;
    private static Class<?> xi, cj, gr, vr, ny;
    private static Object inventory, child;
    private static Object field(Object object, Class<?> owner, String name) throws Exception {
        Field field = owner.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(object);
    }
    private static Object call(Object receiver, String name, Class<?>[] types, Object... args) throws Exception {
        Class<?> owner = receiver instanceof Class<?> ? (Class<?>)receiver : receiver.getClass();
        Method method = owner.getDeclaredMethod(name, types);
        method.setAccessible(true);
        try { return method.invoke(receiver instanceof Class<?> ? null : receiver, args); }
        catch (InvocationTargetException error) {
            if (error.getCause() instanceof Exception) throw (Exception)error.getCause();
            throw (Error)error.getCause();
        }
    }
    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static Object session(String username) throws Exception {
        return gr.getConstructor(String.class, String.class).newInstance(username, "-");
    }
    private static Object writer(Path directory) throws Exception {
        Files.createDirectories(directory);
        long before = ManagementFactory.getThreadMXBean().getTotalStartedThreadCount();
        Object writer = xi.getConstructor(gr, File.class).newInstance(session("Player123"), directory.toFile());
        waitWorkers(writer);
        require(ManagementFactory.getThreadMXBean().getTotalStartedThreadCount() > before, "actual constructor worker");
        return writer;
    }
    private static void waitWorkers(Object writer) throws Exception {
        Object syncher = field(writer, xi, "d");
        long deadline = System.nanoTime() + 10_000_000_000L;
        for (;;) {
            boolean active = false;
            for (Thread thread : Thread.getAllStackTraces().keySet()) {
                Class<?> type = thread.getClass();
                if (type.getClassLoader() == loader && (type.getName().equals("vk") || type.getName().equals("vj"))) {
                    active = true;
                    thread.join(100);
                }
            }
            if (!active && !(Boolean)field(syncher, cj, "a")) return;
            require(System.nanoTime() < deadline, "real worker join deadline");
            Thread.sleep(1);
        }
    }
    private static int bit(Object value) { return Boolean.TRUE.equals(value) ? 1 : 0; }
    private static void project(String label, Object writer) throws Exception {
        Object syncher = field(writer, xi, "d");
        System.out.println("STATS\t" + label + "\t" + ((Map<?, ?>)field(writer, xi, "a")).size() + "\t" +
            ((Map<?, ?>)field(writer, xi, "b")).size() + "\t" + bit(field(writer, xi, "c")) + "\t" +
            bit(call(writer, "a", new Class<?>[]{ny}, inventory)) + "\t" +
            call(writer, "a", new Class<?>[]{vr}, inventory) + "\t" + bit(field(syncher, cj, "a")) + "\t" +
            (field(syncher, cj, "b") == null ? 0 : 1) + "\t" + (field(syncher, cj, "c") == null ? 0 : 1) + "\t" + field(syncher, cj, "l"));
    }
    private static String serialize(Map<Object, Integer> map) throws Exception {
        return (String)call(xi, "a", new Class<?>[]{String.class, String.class, Map.class}, null, "local", map);
    }
    private static void normal(Path root) throws Exception {
        Object fresh = writer(root.resolve("fresh"));
        project("fresh", fresh);
        call(fresh, "a", new Class<?>[]{vr, int.class}, inventory, 0);
        project("zero", fresh);
        call(fresh, "a", new Class<?>[]{vr, int.class}, inventory, Integer.MAX_VALUE);
        call(fresh, "a", new Class<?>[]{vr, int.class}, inventory, 1);
        project("wrapped", fresh);
        call(fresh, "a", new Class<?>[]{vr, int.class}, inventory, -1);
        call(fresh, "c", new Class<?>[]{});
        project("saved", fresh);
        for (int i = 0; i < 100; ++i) call(fresh, "d", new Class<?>[]{});
        call(fresh, "d", new Class<?>[]{});
        waitWorkers(fresh);
        project("async", fresh);
        require(Files.isRegularFile(root.resolve("fresh/stats/stats_player123_unsent.dat")), "real save");
        Path loaded = root.resolve("loaded");
        Files.createDirectories(loaded.resolve("stats"));
        Map<Object, Integer> map = new LinkedHashMap<>();
        map.put(inventory, 3);
        Files.writeString(loaded.resolve("stats/stats_player123_unsent.dat"), serialize(map), Charset.defaultCharset());
        map.put(inventory, 40); map.put(child, -1);
        Files.writeString(loaded.resolve("stats/stats_player123.dat"), serialize(map), Charset.defaultCharset());
        Object reader = writer(loaded);
        project("pending", reader);
        Object syncher = field(reader, xi, "d"), pending = field(syncher, cj, "b");
        call(syncher, "a", new Class<?>[]{}); waitWorkers(reader);
        require(field(syncher, cj, "b") == pending, "refresh retains pending identity");
        call(reader, "d", new Class<?>[]{});
        project("serviced", reader);
        require((Integer)call(reader, "a", new Class<?>[]{vr}, child) == -1, "negative presence");
        call(reader, "a", new Class<?>[]{vr, int.class}, null, 0);
        require((Boolean)call(reader, "a", new Class<?>[]{ny}, (Object)null), "null identity key");
        // Authored map order avoids supplying or capturing JVM identity hashes.
        Files.writeString(root.resolve("single.json"), serialize(Map.of(inventory, -2147483648)), Charset.defaultCharset());
    }
    private static void codec(Path root) throws Exception {
        String[] cases = {"{", "{}", "{\"stats-change\":[]}", "{\"stats-change\":[{}]}",
            "{\"stats-change\":[[]]}", "{\"stats-change\":[{\"x\":1}]}",
            "{\"stats-change\":[{\"5242880\":null}]}", "{\"stats-change\":[{\"5242880\":1.5}]}",
            "1", "{\"stats-change\":[],}"};
        PrintStream previous = System.err;
        try (PrintStream captured = new PrintStream(Files.newOutputStream(root.resolve("source-caught.stderr")), true, "UTF-8")) {
            System.setErr(captured);
            for (int i = 0; i < cases.length; ++i) {
                String result;
                try {
                    Object map = call(xi, "a", new Class<?>[]{String.class}, cases[i]);
                    require(map instanceof Map<?, ?> && ((Map<?, ?>)map).isEmpty(), "malformed empty result " + i);
                    result = "OK";
                } catch (Exception error) {
                    result = error.getClass().getName();
                }
                System.out.println("CODEC\t" + i + "\t" + result);
            }
        } finally { System.setErr(previous); }
        Class<?> hf = Class.forName("hf", true, loader);
        String[] salts = {null, "", "", "local", "local"};
        String[] payloads = {"", "", "a", "\u00e9", "\ud800"};
        for (int i = 0; i < salts.length; ++i) {
            Object checksum = hf.getConstructor(String.class).newInstance(salts[i]);
            System.out.println("HASH\t" + i + "\t" + call(checksum, "a", new Class<?>[]{String.class}, payloads[i]));
        }
    }
    private static void provider(Path root) throws Exception {
        Path directory = root.resolve("provider");
        Files.createDirectories(directory);
        Path from = directory.resolve("from"), to = directory.resolve("to");
        Files.writeString(from, "from"); Files.writeString(to, "to");
        boolean replaced = from.toFile().renameTo(to.toFile());
        require(Files.readString(to).equals(replaced ? "from" : "to"), "actual File.renameTo outcome");
        Path empty = directory.resolve("empty");
        Files.createDirectory(empty);
        boolean rejected = false;
        try (FileReader reader = new FileReader(empty.toFile())) {
            throw new AssertionError("FileReader accepted a directory");
        } catch (FileNotFoundException error) { rejected = true; }
        require(rejected && empty.toFile().delete() && !Files.exists(empty), "actual directory open/delete provider");
        Files.writeString(root.resolve("provider.tsv"), "rename_existing\t" + (replaced ? 1 : 0) +
            "\ndirectory_reader\tjava.io.FileNotFoundException\ndelete_empty_directory\t1\n");
        if (File.separatorChar == '/') {
            File parent = new File(root.toFile(), "ending\\");
            Object actual = writer(parent.toPath());
            call(actual, "a", new Class<?>[]{vr, int.class}, inventory, 2);
            call(actual, "c", new Class<?>[]{});
            File expected = new File(new File(parent, "stats"), "stats_player123_unsent.dat");
            File stored = (File)field(field(actual, xi, "d"), cj, "e");
            require(stored.equals(expected) && Files.isRegularFile(expected.toPath()), "actual xi parent ending literal backslash");
            require(!Files.exists(Path.of(parent.toString() + "stats", "stats_player123_unsent.dat")), "no sibling stats directory");
            Files.writeString(root.resolve("posix-parent.tsv"), "expected\t" + expected + "\nactual\t" + stored + "\n");
        }
    }
    public static void main(String[] args) throws Exception {
        require(args.length == 3, "jar/root/binding");
        Path jar = Path.of(args[0]).toRealPath(), root = Path.of(args[1]).toAbsolutePath();
        try (URLClassLoader originals = new URLClassLoader(new URL[]{jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            loader = originals;
            xi = Class.forName("xi", true, loader); cj = Class.forName("cj", false, loader);
            gr = Class.forName("gr", true, loader); vr = Class.forName("vr", false, loader); ny = Class.forName("ny", false, loader);
            Class.forName("jl", true, loader);
            Class<?> ep = Class.forName("ep", true, loader);
            inventory = ep.getField("f").get(null); child = ep.getField("g").get(null);
            StringBuilder binding = new StringBuilder();
            for (String name : new String[]{"xi","cj","vk","vj","hf","hx","gr","jl","ep","vr","ny"}) {
                Class<?> type = Class.forName(name, false, loader);
                require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource().getLocation().toURI().equals(jar.toUri()), "original runtime origin " + name);
                byte[] data;
                try (var input = type.getResourceAsStream("/" + name + ".class")) { data = input.readAllBytes(); }
                binding.append(name).append('\t').append(HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data))).append('\t').append(jar.toUri()).append('\n');
            }
            Files.writeString(Path.of(args[2]), binding.toString());
            normal(root);
            codec(root);
            provider(root);
        }
    }
}
