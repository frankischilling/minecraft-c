import java.io.IOException;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.jar.Attributes;
import java.util.jar.JarFile;
import java.util.jar.Manifest;

/** Observes the original packed-nibble constructor and methods on one valid full-sized instance. */
public final class NibblesObservation {
    private static final int CELLS = 32768;
    private static final int PACKED = 16384;
    private static final int OUTPUT_LIMIT = 2 * 1024 * 1024;
    // Literal requests only. No expected getter values or packed setter results.
    private static final int[][] WRITES = {
        {0, 0, 0, 0}, {0, 1, 0, 15}, {15, 126, 15, 16}, {15, 127, 15, 31},
        {0, 126, 15, -1}, {0, 127, 15, -16}, {15, 0, 0, Integer.MIN_VALUE},
        {15, 1, 0, Integer.MAX_VALUE}, {3, 0, 11, 15}, {3, 1, 11, 0},
        {11, 126, 3, 31}, {11, 127, 3, 16}, {1, 64, 14, -16},
        {1, 65, 14, -1}, {14, 62, 1, Integer.MAX_VALUE}, {14, 63, 1, Integer.MIN_VALUE}
    };

    private NibblesObservation() { }

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static String hex(byte[] values) {
        char[] result = new char[values.length * 2];
        char[] digits = "0123456789abcdef".toCharArray();
        for (int i = 0; i < values.length; i++) {
            result[i * 2] = digits[(values[i] & 255) >>> 4];
            result[i * 2 + 1] = digits[values[i] & 15];
        }
        return new String(result);
    }

    private static String hash(Path path) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path)));
    }

    private static String quote(String value) {
        require(value.matches("[A-Za-z0-9_.$:;()\\[/-]+"), "unexpected binding/string characters");
        return "\"" + value + "\"";
    }

    private static void archiveClass(Class<?> owner, URLClassLoader loader, Path jar) throws Exception {
        require(owner.getClassLoader() == loader, "original class loader");
        require(owner.getProtectionDomain().getCodeSource() != null, "original code source");
        Path source = Path.of(owner.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
        require(Files.isSameFile(source, jar), "original archive origin");
    }

    private static byte[] readAll(Method getter, Object instance) throws Exception {
        byte[] values = new byte[CELLS];
        int cursor = 0;
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                for (int y = 0; y < 128; y++) {
                    int value = ((Integer) getter.invoke(instance, x, y, z)).intValue();
                    require(value >= 0 && value <= 15, "original getter result range");
                    values[cursor++] = (byte) value;
                }
            }
        }
        require(cursor == CELLS, "getter sweep cardinality");
        return values;
    }

    private static int unavailable(String reason) {
        System.err.println("UNAVAILABLE: nibbles setup " + reason);
        return 77;
    }

    private static int observe(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: NibblesObservation client|server pinnedJar");
        }
        boolean client = args[0].equals("client");
        String ownerName = client ? "wi" : "ob";
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        Path jar;
        try {
            jar = Path.of(args[1]).toRealPath();
            if (!Files.isRegularFile(jar) || Files.size(jar) > 2097152) return unavailable("archive size/type");
        } catch (IOException failure) {
            return unavailable("archive missing/unreadable");
        }
        String jarBefore = hash(jar);
        if (!jarBefore.equals(expected)) return unavailable("archive SHA-256 differs");
        try (JarFile archive = new JarFile(jar.toFile())) {
            Manifest manifest = archive.getManifest();
            String classPath = manifest == null ? null : manifest.getMainAttributes().getValue(Attributes.Name.CLASS_PATH);
            require(classPath == null || classPath.isEmpty(), "unexpected archive Class-Path");
        }
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Class<?> owner;
            try {
                owner = Class.forName(ownerName, false, loader);
                archiveClass(owner, loader, jar);
            } catch (ClassNotFoundException | LinkageError failure) {
                return unavailable("class load " + failure.getClass().getName());
            }
            Field arrayField = owner.getDeclaredField("a");
            require(arrayField.getType() == byte[].class &&
                    arrayField.getModifiers() == (Modifier.PUBLIC | Modifier.FINAL), "original a:[B binding");
            Constructor<?> constructor = owner.getDeclaredConstructor(int.class);
            require(constructor.getModifiers() == Modifier.PUBLIC, "original (I)V constructor");
            Method getter = owner.getDeclaredMethod("a", int.class, int.class, int.class);
            require(getter.getReturnType() == int.class && getter.getModifiers() == Modifier.PUBLIC, "original a(III)I getter");
            Method setter = owner.getDeclaredMethod("a", int.class, int.class, int.class, int.class);
            require(setter.getReturnType() == void.class && setter.getModifiers() == Modifier.PUBLIC, "original a(IIII)V setter");

            PrintStream protocol = System.out;
            System.err.println("NIBBLES_SETUP_BEGIN side=" + args[0] + " class=" + ownerName);
            try {
                System.setOut(System.err);
                Class.forName(ownerName, true, loader);
            } catch (ClassNotFoundException | LinkageError failure) {
                return unavailable("initialization " + failure.getClass().getName());
            } finally {
                System.setOut(protocol);
            }
            System.err.println("NIBBLES_SETUP_DONE side=" + args[0] + " class=" + ownerName);
            System.err.println("NIBBLES_MEASURE_BEGIN side=" + args[0]);
            StringBuilder output;
            try {
                System.setOut(System.err);
                Object instance = constructor.newInstance(CELLS);
                byte[] packed = (byte[]) arrayField.get(instance);
                require(packed != null && packed.length == PACKED, "original constructor array length");
                byte[] fresh = packed.clone();
                for (byte value : fresh) require(value == 0, "original fresh bytes zero");
                require(arrayField.get(instance) == packed, "original fresh array identity");
                // This arithmetic prepares bytes only; neither access method is reproduced here.
                for (int i = 0; i < packed.length; i++) {
                    packed[i] = (byte) (i * 73 + (i >>> 3) * 19 + (i >>> 8) * 151 + 0xa7);
                }
                byte[] prepared = packed.clone();
                byte[] initialGetters = readAll(getter, instance);
                require(arrayField.get(instance) == packed && Arrays.equals(prepared, packed), "initial getters changed storage");
                StringBuilder writes = new StringBuilder("[");
                for (int i = 0; i < WRITES.length; i++) {
                    int[] request = WRITES[i];
                    setter.invoke(instance, request[0], request[1], request[2], request[3]);
                    require(arrayField.get(instance) == packed, "setter replaced original array");
                    byte[] snapshot = packed.clone();
                    int observed = ((Integer) getter.invoke(instance, request[0], request[1], request[2])).intValue();
                    require(observed >= 0 && observed <= 15, "written getter range");
                    require(arrayField.get(instance) == packed && Arrays.equals(snapshot, packed), "written getter changed storage");
                    if (i != 0) writes.append(',');
                    writes.append("{\"x\":").append(request[0]).append(",\"y\":").append(request[1])
                            .append(",\"z\":").append(request[2]).append(",\"value\":").append(request[3])
                            .append(",\"observed\":").append(observed).append(",\"packed\":")
                            .append(quote(hex(snapshot))).append('}');
                }
                writes.append(']');
                byte[] finalPacked = packed.clone();
                byte[] finalGetters = readAll(getter, instance);
                require(arrayField.get(instance) == packed && Arrays.equals(finalPacked, packed), "final getters changed storage");
                archiveClass(instance.getClass(), loader, jar);
                String jarAfter = hash(jar);
                require(jarAfter.equals(jarBefore), "original archive changed");
                output = new StringBuilder("{\"schema\":1,\"side\":").append(quote(args[0]))
                        .append(",\"jar_sha256_before\":").append(quote(jarBefore))
                        .append(",\"jar_sha256_after\":").append(quote(jarAfter))
                        .append(",\"bindings\":{\"owner\":").append(quote(ownerName))
                        .append(",\"array\":\"a:[B\",\"constructor\":").append(quote(ownerName + "(I)V"))
                        .append(",\"getter\":\"a(III)I\",\"setter\":\"a(IIII)V\"}")
                        .append(",\"constructor_cells\":32768,\"packed_length\":16384,\"fresh_packed\":").append(quote(hex(fresh)))
                        .append(",\"prepared_packed\":").append(quote(hex(prepared)))
                        .append(",\"getter_order\":\"X/Z/Y\",\"initial_getters\":").append(quote(hex(initialGetters)))
                        .append(",\"writes\":").append(writes).append(",\"final_packed\":").append(quote(hex(finalPacked)))
                        .append(",\"final_getters\":").append(quote(hex(finalGetters)))
                        .append(",\"controls\":{\"fresh_zero_checked\":true,\"array_identity_unchanged\":true,")
                        .append("\"initial_getters_readonly\":true,\"written_getters_readonly\":true,\"final_getters_readonly\":true,")
                        .append("\"original_class_loader_verified\":true,\"jar_unchanged\":true}}");
            } finally {
                System.setOut(protocol);
            }
            byte[] bytes = output.append('\n').toString().getBytes(StandardCharsets.UTF_8);
            require(bytes.length < OUTPUT_LIMIT, "observation output limit");
            System.err.println("NIBBLES_CONTROL fresh_zero array_identity initial_getters_readonly written_getters_readonly final_getters_readonly");
            System.out.write(bytes);
            System.out.flush();
            require(!System.out.checkError(), "observation output write");
            System.err.println("NIBBLES_MEASURE_DONE side=" + args[0]);
            return 0;
        } finally {
            require(hash(jar).equals(jarBefore), "original archive changed during setup/observation");
        }
    }

    public static void main(String[] args) throws Exception {
        int status = observe(args);
        if (status != 0) System.exit(status);
    }
}
