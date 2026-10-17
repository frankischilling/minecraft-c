import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Random;

/** Calls untouched original numerical noise classes; never opens a game world. */
public final class NoiseProbe {
    private static final int MAX_REQUESTS = 2048;
    private static final int MAX_LINE = 1024;
    private static final int MAX_GRID = 65536;
    private final Constructor<?> perlinConstructor;
    private final Constructor<?> octavesConstructor;
    private final Method point;
    private final Method sample2;
    private final Method add;
    private final Method fill;
    private final Field[] offsets;
    private final Field permutation;
    private final Field layers;

    private NoiseProbe(ClassLoader loader, boolean client) throws Exception {
        Class<?> perlin = Class.forName(client ? "am" : "ab", true, loader);
        Class<?> octaves = Class.forName(client ? "uf" : "mp", true, loader);
        perlinConstructor = perlin.getDeclaredConstructor(Random.class);
        octavesConstructor = octaves.getDeclaredConstructor(Random.class, int.class);
        perlinConstructor.setAccessible(true);
        octavesConstructor.setAccessible(true);
        point = method(perlin, double.class, double.class, double.class);
        sample2 = method(octaves, double.class, double.class);
        add = method(perlin, double[].class, double.class, double.class, double.class,
                     int.class, int.class, int.class, double.class, double.class, double.class, double.class);
        fill = method(octaves, double[].class, double.class, double.class, double.class,
                      int.class, int.class, int.class, double.class, double.class, double.class);
        offsets = new Field[] {field(perlin, "a"), field(perlin, "b"), field(perlin, "c")};
        permutation = field(perlin, "d");
        layers = field(octaves, "a");
    }

    private static Field field(Class<?> owner, String name) throws Exception {
        Field result = owner.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    private static Method method(Class<?> owner, Class<?>... arguments) throws Exception {
        Method result = owner.getDeclaredMethod("a", arguments);
        result.setAccessible(true);
        return result;
    }

    private static Object call(Method method, Object receiver, Object... arguments) throws Exception {
        try {
            return method.invoke(receiver, arguments);
        } catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            throw (Error) cause;
        }
    }

    private static int integer(String text, int limit) {
        int result = Integer.parseInt(text);
        if (result < 0 || result > limit) throw new IllegalArgumentException("integer outside probe bounds");
        return result;
    }

    private static double number(String text) {
        if (!text.matches("[0-9a-f]{16}")) throw new IllegalArgumentException("expected raw16hex double");
        return Double.longBitsToDouble(Long.parseUnsignedLong(text, 16));
    }

    private static void hex(StringBuilder text, long value) {
        String digits = Long.toHexString(value);
        for (int i = digits.length(); i < 16; i++) text.append('0');
        text.append(digits);
    }

    private static String bits(double[] values) {
        if (values.length == 0) return "-";
        StringBuilder result = new StringBuilder(values.length * 17);
        for (int i = 0; i < values.length; i++) {
            if (i != 0) result.append(',');
            hex(result, Double.doubleToRawLongBits(values[i]));
        }
        return result.toString();
    }

    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1);
        if (fields.length < 4) throw new IllegalArgumentException("short request");
        String op = fields[0];
        int expected = op.equals("state") ? 4 : op.equals("point") ? 7
                     : op.equals("sample2") ? 6 : op.equals("add") ? 14 : op.equals("fill") ? 13 : -1;
        if (fields.length != expected) throw new IllegalArgumentException("operation or field count");
        long seed = Long.parseLong(fields[1]);
        int skip = integer(fields[2], 128);
        int count = integer(fields[3], 128);
        boolean perlin = op.equals("point") || op.equals("add");
        if (perlin && count != 1) throw new IllegalArgumentException("Perlin requires count1");
        Random random = new Random(seed);
        for (int i = 0; i < skip; i++) perlinConstructor.newInstance(random);
        Object noise = perlin ? perlinConstructor.newInstance(random)
                              : octavesConstructor.newInstance(random, count);
        double[] output;
        StringBuilder permutations = new StringBuilder();
        if (op.equals("state")) {
            Object[] samplers = (Object[]) layers.get(noise);
            if (samplers.length != count) throw new AssertionError("original layer count");
            output = new double[count * 3];
            for (int layer = 0; layer < count; layer++) {
                for (int axis = 0; axis < 3; axis++) output[layer * 3 + axis] = offsets[axis].getDouble(samplers[layer]);
                int[] values = (int[]) permutation.get(samplers[layer]);
                if (values.length != 512) throw new AssertionError("original permutation length");
                for (int value : values) {
                    if (permutations.length() != 0) permutations.append(',');
                    permutations.append(value);
                }
            }
        } else if (op.equals("point")) {
            output = new double[] {(double) call(point, noise, number(fields[4]), number(fields[5]), number(fields[6]))};
        } else if (op.equals("sample2")) {
            output = new double[] {(double) call(sample2, noise, number(fields[4]), number(fields[5]))};
        } else {
            double x = number(fields[4]), y = number(fields[5]), z = number(fields[6]);
            int nx = integer(fields[7], MAX_GRID), ny = integer(fields[8], MAX_GRID), nz = integer(fields[9], MAX_GRID);
            long product = (long) nx * ny * nz;
            // Also bound work in the original's outer loops when an inner dimension is zero.
            if (product > MAX_GRID || (long) nx * Math.max(nz, 1) > MAX_GRID) {
                throw new IllegalArgumentException("grid product exceeds probe limit");
            }
            output = new double[(int) product + 3];
            for (int i = 0; i < output.length; i++) output[i] = ((i % 11) - 5) * 0.125;
            double sx = number(fields[10]), sy = number(fields[11]), sz = number(fields[12]);
            if (perlin) {
                call(add, noise, output, x, y, z, nx, ny, nz, sx, sy, sz, number(fields[13]));
            } else {
                Object returned = call(fill, noise, output, x, y, z, nx, ny, nz, sx, sy, sz);
                if (returned != output) throw new AssertionError("original replaced supplied array");
            }
        }
        StringBuilder continuation = new StringBuilder();
        for (int i = 0; i < 4; i++) {
            if (i != 0) continuation.append(',');
            hex(continuation, random.nextLong());
        }
        return "OK\t" + bits(output) + "\t" + continuation + "\t"
             + (permutations.length() == 0 ? "-" : permutations.toString());
    }

    private static String boundedLine(BufferedReader reader) throws IOException {
        StringBuilder result = new StringBuilder();
        int value;
        while ((value = reader.read()) != -1 && value != '\n') {
            if (result.length() == MAX_LINE) throw new IOException("request line limit");
            result.append((char) value);
        }
        if (value == -1 && result.length() == 0) return null;
        if (result.length() != 0 && result.charAt(result.length() - 1) == '\r') result.setLength(result.length() - 1);
        return result.toString();
    }

    private static String digest(Path path) throws Exception {
        byte[] bytes = MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path));
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            String digits = Integer.toHexString(value & 255);
            if (digits.length() == 1) result.append('0');
            result.append(digits);
        }
        return result.toString();
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 || !(arguments[0].equals("client") || arguments[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: NoiseProbe client|server original.jar");
        }
        boolean client = arguments[0].equals("client");
        Path jar = Path.of(arguments[1]).toAbsolutePath().normalize();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                                 : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!Files.isRegularFile(jar) || !digest(jar).equals(expected)) {
            System.err.println("UNAVAILABLE: original JAR missing or SHA-256 mismatch: " + jar);
            System.exit(77);
        }
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            NoiseProbe probe = new NoiseProbe(loader, client);
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String line;
            int requests = 0;
            while ((line = boundedLine(reader)) != null) {
                if (++requests > MAX_REQUESTS) throw new IOException("request count limit");
                System.out.println(probe.request(line));
            }
        }
    }
}
