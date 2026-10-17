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

/** Reflects untouched original 2D simplex classes; never opens game data. */
public final class SimplexProbe {
    private static final int MAX_REQUESTS = 2048;
    private static final int MAX_LINE = 1024;
    private static final int MAX_GRID = 65536;
    private final Constructor<?> perlinConstructor;
    private final Constructor<?> simplexConstructor;
    private final Constructor<?> octavesConstructor;
    private final Method add;
    private final Method fill;
    private final Method defaultFill;
    private final Field[] offsets;
    private final Field permutation;
    private final Field layers;
    private final Field skew;
    private final Field unskew;

    private SimplexProbe(ClassLoader loader, boolean client) throws Exception {
        Class<?> perlin = Class.forName(client ? "am" : "ab", true, loader);
        Class<?> simplex = Class.forName(client ? "cc" : "bi", true, loader);
        Class<?> octaves = Class.forName(client ? "ug" : "mq", true, loader);
        perlinConstructor = perlin.getDeclaredConstructor(Random.class);
        simplexConstructor = simplex.getDeclaredConstructor(Random.class);
        octavesConstructor = octaves.getDeclaredConstructor(Random.class, int.class);
        perlinConstructor.setAccessible(true);
        simplexConstructor.setAccessible(true);
        octavesConstructor.setAccessible(true);
        add = method(simplex, double[].class, double.class, double.class, int.class, int.class,
                     double.class, double.class, double.class);
        fill = method(octaves, double[].class, double.class, double.class, int.class, int.class,
                      double.class, double.class, double.class, double.class);
        defaultFill = method(octaves, double[].class, double.class, double.class, int.class, int.class,
                             double.class, double.class, double.class);
        offsets = new Field[] {field(simplex, "a"), field(simplex, "b"), field(simplex, "c")};
        permutation = field(simplex, "e");
        layers = field(octaves, "a");
        skew = field(simplex, "f");
        unskew = field(simplex, "g");
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
        int expected = op.equals("state") || op.equals("constants") ? 4 : op.equals("fill") ? 12
                     : op.equals("add") || op.equals("default") ? 11 : -1;
        if (fields.length != expected) throw new IllegalArgumentException("operation or field count");
        long seed = Long.parseLong(fields[1]);
        String prefix = fields[2];
        if (!prefix.equals("-") && !prefix.matches("[PS]{1,128}")) {
            throw new IllegalArgumentException("invalid constructor prefix");
        }
        int count = integer(fields[3], 128);
        if (op.equals("add") && count != 1) throw new IllegalArgumentException("add requires count1");
        if (op.equals("constants") && (seed != 0 || !prefix.equals("-") || count != 0)) {
            throw new IllegalArgumentException("constants requires seed0 prefix- count0");
        }
        Random random = new Random(seed);
        if (!prefix.equals("-")) {
            for (int i = 0; i < prefix.length(); i++) {
                (prefix.charAt(i) == 'P' ? perlinConstructor : simplexConstructor).newInstance(random);
            }
        }
        Object noise = op.equals("constants") ? null : op.equals("add") ? simplexConstructor.newInstance(random)
                                                                        : octavesConstructor.newInstance(random, count);
        double[] output;
        StringBuilder permutations = new StringBuilder();
        if (op.equals("constants")) {
            output = new double[] {skew.getDouble(null), unskew.getDouble(null)};
        } else if (op.equals("state")) {
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
        } else {
            double x = number(fields[4]), z = number(fields[5]);
            int nx = integer(fields[6], MAX_GRID), nz = integer(fields[7], MAX_GRID);
            long product = (long) nx * nz;
            if (product > MAX_GRID) throw new IllegalArgumentException("grid product exceeds probe limit");
            output = new double[(int) product + 3];
            for (int i = 0; i < output.length; i++) output[i] = ((i % 11) - 5) * 0.125;
            double sx = number(fields[8]), sz = number(fields[9]), factor = number(fields[10]);
            if (op.equals("add")) {
                call(add, noise, output, x, z, nx, nz, sx, sz, factor);
            } else {
                Object returned = op.equals("default")
                    ? call(defaultFill, noise, output, x, z, nx, nz, sx, sz, factor)
                    : call(fill, noise, output, x, z, nx, nz, sx, sz, factor, number(fields[11]));
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
            throw new IllegalArgumentException("Usage: SimplexProbe client|server original.jar");
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
            SimplexProbe probe = new SimplexProbe(loader, client);
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
