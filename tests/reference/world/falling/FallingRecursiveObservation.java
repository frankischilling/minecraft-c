import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.ObjectOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;

/** Reflection-only observation of unchanged original methods on an authored save copy. */
public final class FallingRecursiveObservation {
    private static final String CLIENT_SHA256 =
        "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Class<?> original(URLClassLoader loader, Path archive, String name)
            throws Exception {
        Class<?> type = Class.forName(name, true, loader);
        require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
            && Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(archive), "supplied original archive origin: " + name);
        return type;
    }

    private static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    private static Object call(Method method, Object owner, Object... args) throws Exception {
        try { return method.invoke(owner, args); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Error) throw (Error) cause;
            if (cause instanceof Exception) throw (Exception) cause;
            throw failure;
        }
    }

    private static byte[] randomState(Random random) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream output = new ObjectOutputStream(bytes)) {
            output.writeObject(random);
        }
        return bytes.toByteArray();
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 2, "supplied client JAR and copied authored World directory");
        Path archive = Path.of(args[0]).toRealPath(), copy = Path.of(args[1]).toRealPath();
        require(Files.isRegularFile(archive) && Files.isDirectory(copy)
            && Files.isRegularFile(copy.resolve("level.dat"))
            && Files.isRegularFile(copy.resolve("region/r.0.0.mcr")), "ordinary supplied inputs");
        String sha256 = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256")
            .digest(Files.readAllBytes(archive)));
        require(sha256.equals(CLIENT_SHA256), "unchanged original client JAR pin");
        try (URLClassLoader loader = new URLClassLoader(
                new java.net.URL[] {archive.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Class<?> worldType = original(loader, archive, "fd");
            Class<?> blockType = original(loader, archive, "uu");
            Class<?> fallingType = original(loader, archive, "gk");
            Class<?> tickType = original(loader, archive, "qy");
            Class<?> storageType = original(loader, archive, "mx");
            Class<?> storageInterface = original(loader, archive, "wt");
            Object handler = storageType.getConstructor(File.class, String.class, boolean.class)
                .newInstance(copy.getParent().toFile(), copy.getFileName().toString(), false);
            Object world = worldType.getConstructor(storageInterface, String.class, long.class)
                .newInstance(handler, "", 23L);
            require(world.getClass() == worldType, "genuine original World");
            Method get = worldType.getMethod("a", int.class, int.class, int.class);
            for (int x = 0; x < 2; ++x)
                for (int z = 0; z < 2; ++z)
                    call(get, world, x * 16, 64, z * 16);
            Object provider = field(worldType, "v").get(world);
            Map<?, ?> cache = (Map<?, ?>)field(provider.getClass(), "e").get(provider);
            require(cache.size() == 4, "four genuine loaded Chunks");
            for (int y = 6; y <= 80; ++y)
                require((Integer)call(get, world, 20, y, 20) == 13, "authored initial gravel");
            require((Integer)call(get, world, 20, 4, 20) == 1
                && (Integer)call(get, world, 20, 5, 20) == 0, "authored floor/gap");
            Field immediate = field(worldType, "a"), falling = field(fallingType, "a");
            boolean previousImmediate = immediate.getBoolean(world);
            boolean previousFalling = falling.getBoolean(null);
            Field sequence = field(tickType, "f");
            long before = sequence.getLong(null);
            Set<?> ordered = (Set<?>)field(worldType, "E").get(world);
            Set<?> membership = (Set<?>)field(worldType, "F").get(world);
            int oldE = ordered.size(), oldF = membership.size();
            Random random = (Random)field(worldType, "r").get(world);
            byte[] randomBefore = randomState(random);
            Object gravel = ((Object[])field(blockType, "m").get(null))[13];
            Method neighbor = fallingType.getMethod("b", worldType,
                int.class, int.class, int.class, int.class);
            try {
                immediate.setBoolean(world, true);
                falling.setBoolean(null, true);
                call(neighbor, gravel, world, 20, 6, 20, Integer.MIN_VALUE);
            } finally {
                immediate.setBoolean(world, previousImmediate);
                falling.setBoolean(null, previousFalling);
            }
            List<Integer> heights = new ArrayList<>();
            for (int y = 5; y <= 80; ++y) {
                int actual = (Integer)call(get, world, 20, y, 20);
                require(actual == 0 || actual == 13, "only source air/gravel in final column");
                if (actual == 13) heights.add(y);
            }
            long requests = sequence.getLong(null) - before;
            require(ordered.size() == oldE && membership.size() == oldF, "no queued records");
            require(Arrays.equals(randomBefore, randomState(random)), "unchanged World Random");
            System.out.println("ORIGINAL_FALLING jar=" + sha256 + " java="
                + System.getProperty("java.runtime.version") + " survivors=" + heights.size()
                + " scheduled=" + requests + " heights=" + heights
                + " queue_unchanged=true random_unchanged=true");
        }
    }
}
