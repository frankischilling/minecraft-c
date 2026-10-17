import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Only the pinned original float table, sine/cosine lookups, and double-floor helper. */
public final class TrigMathObservation {
    private static String hex(byte[] bytes) {
        char[] result = new char[bytes.length * 2]; char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; i++) {
            result[i * 2] = alphabet[(bytes[i] & 255) >>> 4]; result[i * 2 + 1] = alphabet[bytes[i] & 15];
        }
        return new String(result);
    }
    private static String hash(Path path) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path)));
    }
    private static String tableHash(float[] table) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        for (float value : table) {
            int bits = Float.floatToRawIntBits(value);
            digest.update((byte) (bits >>> 24)); digest.update((byte) (bits >>> 16));
            digest.update((byte) (bits >>> 8)); digest.update((byte) bits);
        }
        return hex(digest.digest());
    }
    private static Object call(Method method, Object argument) throws Exception {
        try { return method.invoke(null, argument); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            throw (Error) cause;
        }
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 3 || !(args[0].equals("client") || args[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: TrigMathObservation side pinnedJar requests");
        }
        boolean client = args[0].equals("client"); Path jar = Path.of(args[1]).toRealPath();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!hash(jar).equals(expected)) {
            System.err.println("UNAVAILABLE: original JAR SHA-256 differs"); System.exit(77); return;
        }
        Path requestsPath = Path.of(args[2]).toRealPath();
        if (Files.size(requestsPath) > 32768) throw new IllegalArgumentException("request byte limit");
        List<String> requests = Files.readAllLines(requestsPath, StandardCharsets.UTF_8);
        if (requests.size() > 768) throw new IllegalArgumentException("request count limit");
        for (String request : requests) {
            if (request.length() > 32 || !request.matches("(?:sin|cos)\\t[0-9a-f]{8}|floor\\t[0-9a-f]{16}")) {
                throw new IllegalArgumentException("bounded method-specific input required");
            }
        }
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            String name = client ? "in" : "fq";
            Class<?> helper = Class.forName(name, true, loader);
            Field tableField = helper.getDeclaredField("a"); tableField.setAccessible(true);
            if (tableField.getType() != float[].class || !Modifier.isStatic(tableField.getModifiers())) throw new AssertionError("original table descriptor");
            float[] table = (float[]) tableField.get(null);
            if (table.length != 65536) throw new AssertionError("original table length");
            Method sin = helper.getDeclaredMethod("a", float.class), cos = helper.getDeclaredMethod("b", float.class);
            Method floor = helper.getDeclaredMethod("b", double.class);
            for (Method method : new Method[] {sin, cos, floor}) {
                if (!Modifier.isStatic(method.getModifiers()) || !Modifier.isPublic(method.getModifiers())) throw new AssertionError("original method modifiers");
            }
            if (sin.getReturnType() != float.class || cos.getReturnType() != float.class || floor.getReturnType() != int.class) throw new AssertionError("original return descriptors");
            System.err.println("INITIALIZED class=" + name + " table=a:[F length=65536");
            System.err.println("METHOD sin=a(F)F cos=b(F)F floor=b(D)I; no world or cave classes requested");
            System.out.println("TABLE_DESCRIPTOR\ta\t[F\t65536");
            System.out.println("METHOD_DESCRIPTOR\tsin\ta\t(F)F");
            System.out.println("METHOD_DESCRIPTOR\tcos\tb\t(F)F");
            System.out.println("METHOD_DESCRIPTOR\tfloor\tb\t(D)I");
            String before = tableHash(table);
            System.out.println("TABLE_SHA256_BEFORE\t" + before);
            for (int i = 0; i < table.length; i++) {
                System.out.printf(Locale.ROOT, "TABLE\t%d\t%08x%n", i, Float.floatToRawIntBits(table[i]));
            }
            for (String request : requests) {
                String[] fields = request.split("\t", -1); String operation = fields[0], input = fields[1];
                if (operation.equals("floor")) {
                    double number = Double.longBitsToDouble(Long.parseUnsignedLong(input, 16));
                    int value = (Integer) call(floor, Double.valueOf(number));
                    System.out.printf(Locale.ROOT, "CALL\tfloor\t(D)I\t%s\t%08x\t%d%n", input, value, value);
                } else {
                    float number = Float.intBitsToFloat(Integer.parseUnsignedInt(input, 16));
                    float value = (Float) call(operation.equals("sin") ? sin : cos, Float.valueOf(number));
                    System.out.printf(Locale.ROOT, "CALL\t%s\t(F)F\t%s\t%08x\t-%n", operation, input, Float.floatToRawIntBits(value));
                }
            }
            String after = tableHash((float[]) tableField.get(null));
            if (table != tableField.get(null) || !before.equals(after)) throw new AssertionError("original lookup/floor calls changed table");
            System.out.println("TABLE_SHA256_AFTER\t" + after);
            System.out.println("CALL_COUNT\t" + requests.size());
        } finally {
            if (!hash(jar).equals(expected)) throw new AssertionError("original JAR changed during observation");
        }
    }
}
