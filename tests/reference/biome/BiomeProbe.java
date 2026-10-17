import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.lang.reflect.Array;
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
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

/** Untouched classifiers and explicit-seed numerical manager fixtures, without worlds. */
public final class BiomeProbe {
    private static final int MAX_ARRAY = 4096;
    private static final String[] SLOTS = {"a", "b", "c", "d", "t", "o"};
    private final boolean client;
    private final Class<?> biomeClass;
    private final Constructor<?> managerConstructor;
    private final Constructor<?> octaveConstructor;
    private final Constructor<?> chunkConstructor;
    private final Field[] cacheFields = new Field[4];
    private final Field[] channelFields = new Field[3];
    private final Object[] identities = new Object[13];
    private final List<String> names = new ArrayList<>();
    private final IdentityHashMap<Object, String> labels = new IdentityHashMap<>();
    private final String table;
    private final Method classifyFloat;
    private final Method classifyDouble;
    private final Method temperature;
    private final Method biomes;
    private final Method cached;
    private final Method point;
    private final Method chunk;
    private final Method raw;
    private Object manager;
    private double[] externalTemperature;
    private Object externalBiomes;

    private BiomeProbe(ClassLoader loader, boolean client) throws Exception {
        this.client = client;
        biomeClass = Class.forName(client ? "kd" : "gs", true, loader);
        Field name = field(biomeClass, "n");
        for (int i = 0; i < 13; i++) {
            String label = Character.toString((char) ('a' + i));
            identities[i] = field(biomeClass, label).get(null);
            if (identities[i] == null || labels.put(identities[i], label) != null) {
                throw new IllegalStateException("original identity catalogue is not distinct");
            }
            names.add((String) name.get(identities[i]));
        }
        Object entries = field(biomeClass, "x").get(null);
        if (Array.getLength(entries) != 4096) throw new IllegalStateException("original table length");
        StringBuilder text = new StringBuilder(4096);
        for (int i = 0; i < 4096; i++) text.append(label(Array.get(entries, i)));
        table = text.toString();
        classifyFloat = method(biomeClass, "a", float.class, float.class);
        classifyDouble = method(biomeClass, "a", double.class, double.class);
        Class<?> managerClass = Class.forName(client ? "xv" : "ph", true, loader);
        Class<?> octaveClass = Class.forName(client ? "ug" : "mq", true, loader);
        Class<?> chunkClass = Class.forName(client ? "yy" : "qa", true, loader);
        Class<?> biomeArray = Array.newInstance(biomeClass, 0).getClass();
        managerConstructor = managerClass.getDeclaredConstructor();
        managerConstructor.setAccessible(true);
        octaveConstructor = octaveClass.getDeclaredConstructor(Random.class, int.class);
        octaveConstructor.setAccessible(true);
        chunkConstructor = chunkClass.getDeclaredConstructor(int.class, int.class);
        chunkConstructor.setAccessible(true);
        for (int i = 0; i < 4; i++) cacheFields[i] = field(managerClass, SLOTS[i]);
        for (int i = 0; i < 3; i++) channelFields[i] = field(managerClass, Character.toString((char) ('e' + i)));
        temperature = method(managerClass, "a", double[].class, int.class, int.class, int.class, int.class);
        biomes = method(managerClass, "a", biomeArray, int.class, int.class, int.class, int.class);
        cached = method(managerClass, "a", int.class, int.class, int.class, int.class);
        point = method(managerClass, "a", int.class, int.class);
        chunk = method(managerClass, "a", chunkClass);
        raw = client ? method(managerClass, "b", int.class, int.class) : null;
        // Establish the actual protected-constructor fixture while failures are
        // still setup failures. Discard this independent seed-zero fixture;
        // protocol init commands construct their own fresh explicit-seed state.
        initialize(0);
        for (Field cache : cacheFields) {
            if (cache.get(manager) != null) throw new IllegalStateException("fixture cache is not initially null");
        }
        manager = null;
    }

    private static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    private static Method method(Class<?> type, String name, Class<?>... arguments) throws Exception {
        Method result = type.getDeclaredMethod(name, arguments);
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

    private String label(Object biome) {
        if (biome == null) return "-";
        String result = labels.get(biome);
        if (result == null) throw new IllegalStateException("unrecognized original biome identity");
        return result;
    }

    private static String hex(double value) {
        return String.format("%016x", Double.doubleToRawLongBits(value));
    }

    private static float floatValue(String text) {
        if (!text.matches("[0-9a-f]{8}")) throw new IllegalArgumentException("expected float bits");
        float result = Float.intBitsToFloat(Integer.parseUnsignedInt(text, 16));
        if (!Float.isFinite(result) || result < 0 || result > 1) throw new IllegalArgumentException("unit interval required");
        return result;
    }

    private static double doubleValue(String text) {
        if (!text.matches("[0-9a-f]{16}")) throw new IllegalArgumentException("expected double bits");
        double result = Double.longBitsToDouble(Long.parseUnsignedLong(text, 16));
        if (!Double.isFinite(result) || result < 0 || result > 1) throw new IllegalArgumentException("unit interval required");
        return result;
    }

    private static int dimension(String text) {
        int result = Integer.parseInt(text);
        if (result < 0 || result > MAX_ARRAY) throw new IllegalArgumentException("dimension limit");
        return result;
    }

    private static int length(String text) {
        int result = Integer.parseInt(text);
        if (result < -1 || result > MAX_ARRAY) throw new IllegalArgumentException("array limit");
        return result;
    }

    private void initialize(long seed) throws Exception {
        manager = managerConstructor.newInstance();
        long[] factors = {9871L, 39811L, 543321L};
        int[] counts = {4, 4, 2};
        for (int i = 0; i < 3; i++) {
            channelFields[i].set(manager, octaveConstructor.newInstance(new Random(seed * factors[i]), counts[i]));
        }
        externalTemperature = null;
        externalBiomes = null;
    }

    private Object[] arrays() throws Exception {
        Object[] result = new Object[6];
        if (manager != null) for (int i = 0; i < 4; i++) result[i] = cacheFields[i].get(manager);
        result[4] = externalTemperature;
        result[5] = externalBiomes;
        return result;
    }

    private Object descriptor(Object array, boolean numeric) {
        if (array == null) return null;
        Map<String, Object> result = new LinkedHashMap<>();
        int length = Array.getLength(array);
        if (length > MAX_ARRAY) throw new IllegalStateException("original array exceeds fixture limit");
        result.put("length", length);
        if (numeric) {
            List<String> values = new ArrayList<>(length);
            for (double value : (double[]) array) values.add(hex(value));
            result.put("values", values);
        } else {
            StringBuilder values = new StringBuilder(length);
            for (int i = 0; i < length; i++) values.append(label(Array.get(array, i)));
            result.put("values", values.toString());
        }
        return result;
    }

    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1);
        String op = fields[0];
        int expected = op.equals("catalog") || op.equals("table") ? 1 : op.equals("init") ? 2
                     : op.equals("buffers") ? 7 : op.equals("temperature") ? 5
                     : op.equals("biomes") || op.equals("cached") ? 4
                     : op.equals("float") || op.equals("double") || op.equals("point")
                       || op.equals("chunk") || op.equals("raw") ? 3 : -1;
        if (fields.length != expected) throw new IllegalArgumentException("operation/field count");
        Object[] before = arrays();
        Object returned = null;
        String biome = null, number = null;
        if (op.equals("init")) {
            initialize(Long.parseLong(fields[1]));
        } else if (op.equals("float")) {
            biome = label(call(classifyFloat, null, floatValue(fields[1]), floatValue(fields[2])));
        } else if (op.equals("double")) {
            biome = label(call(classifyDouble, null, doubleValue(fields[1]), doubleValue(fields[2])));
        } else if (!op.equals("catalog") && !op.equals("table")) {
            if (manager == null) throw new IllegalArgumentException("initialize manager first");
            if (op.equals("buffers")) {
                for (int i = 0; i < 6; i++) {
                    int size = length(fields[i + 1]);
                    Object array = null;
                    if (size >= 0 && i != 3 && i != 5) {
                        double[] values = new double[size];
                        for (int j = 0; j < size; j++) values[j] = ((j % 11) - 5) * 0.125;
                        array = values;
                    } else if (size >= 0) {
                        array = Array.newInstance(biomeClass, size);
                        for (int j = 0; j < size; j++) Array.set(array, j, identities[j % 13]);
                    }
                    if (i < 4) cacheFields[i].set(manager, array);
                    else if (i == 4) externalTemperature = (double[]) array;
                    else externalBiomes = array;
                }
            } else {
                int x = Integer.parseInt(fields[1]), z = Integer.parseInt(fields[2]);
                if (op.equals("temperature")) {
                    int width = dimension(fields[3]), height = dimension(fields[4]);
                    if ((long) width * height > MAX_ARRAY) throw new IllegalArgumentException("grid limit");
                    returned = call(temperature, manager, externalTemperature, x, z, width, height);
                    externalTemperature = (double[]) returned;
                } else if (op.equals("biomes") || op.equals("cached")) {
                    int size = dimension(fields[3]);
                    if ((long) size * size > MAX_ARRAY) throw new IllegalArgumentException("square limit");
                    if (op.equals("biomes")) {
                        returned = call(biomes, manager, externalBiomes, x, z, size, size);
                        externalBiomes = returned;
                    } else {
                        returned = call(cached, manager, x, z, size, size);
                    }
                } else if (op.equals("point")) {
                    biome = label(call(point, manager, x, z));
                } else if (op.equals("chunk")) {
                    biome = label(call(chunk, manager, chunkConstructor.newInstance(x, z)));
                } else if (op.equals("raw")) {
                    if (!client) throw new IllegalArgumentException("raw method is client-only");
                    number = hex((double) call(raw, manager, x, z));
                }
            }
        }
        Map<String, Object> response = new LinkedHashMap<>();
        response.put("catalog", op.equals("catalog") ? names : null);
        response.put("table", op.equals("table") ? table : null);
        response.put("biome", biome);
        response.put("number", number);
        Object[] after = arrays();
        StringBuilder retained = new StringBuilder(6), returnedMask = new StringBuilder(6), aliases = new StringBuilder(15);
        for (int i = 0; i < 6; i++) {
            response.put(SLOTS[i], descriptor(after[i], i != 3 && i != 5));
            // Fixture-only replacements have a fixed mask; actual methods retain
            // meaningful object identity, independent of host allocation addresses.
            retained.append(!op.equals("init") && !op.equals("buffers") && before[i] != null && before[i] == after[i] ? '1' : '0');
            returnedMask.append(returned != null && returned == after[i] ? '1' : '0');
            for (int j = i + 1; j < 6; j++) aliases.append(after[i] != null && after[i] == after[j] ? '1' : '0');
        }
        response.put("retained", retained.toString());
        response.put("returned", returnedMask.toString());
        response.put("aliases", aliases.toString());
        return "OK\t" + json(response);
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number) return value.toString();
        if (value instanceof String) {
            return "\"" + ((String) value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        }
        StringBuilder result = new StringBuilder(value instanceof Map ? "{" : "[");
        if (value instanceof Map) {
            for (Object entryObject : ((Map<?, ?>) value).entrySet()) {
                Map.Entry<?, ?> entry = (Map.Entry<?, ?>) entryObject;
                if (result.length() != 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
        } else {
            for (Object item : (List<?>) value) {
                if (result.length() != 1) result.append(',');
                result.append(json(item));
            }
        }
        return result.append(value instanceof Map ? '}' : ']').toString();
    }

    private static String line(BufferedReader reader) throws IOException {
        StringBuilder result = new StringBuilder();
        int value;
        while ((value = reader.read()) != -1 && value != '\n') {
            if (result.length() == 256) throw new IOException("request line limit");
            result.append((char) value);
        }
        if (value == -1 && result.length() == 0) return null;
        if (result.length() != 0 && result.charAt(result.length() - 1) == '\r') result.setLength(result.length() - 1);
        return result.toString();
    }

    private static String digest(Path path) throws Exception {
        StringBuilder result = new StringBuilder();
        for (byte value : MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path))) {
            result.append(String.format("%02x", value & 255));
        }
        return result.toString();
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 || !(arguments[0].equals("client") || arguments[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: BiomeProbe client|server original.jar");
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
            PrintStream protocol = System.out;
            BiomeProbe probe;
            try {
                System.setOut(System.err);
                probe = new BiomeProbe(loader, client);
            } catch (Throwable failure) {
                System.err.println("UNAVAILABLE: isolated original classifier/manager fixture setup failed");
                failure.printStackTrace(System.err);
                System.exit(77);
                return;
            } finally {
                System.setOut(protocol);
            }
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String request;
            int requests = 0;
            while ((request = line(reader)) != null) {
                if (++requests > 8192) throw new IOException("request count limit");
                System.out.println(probe.request(request));
            }
        }
    }
}
