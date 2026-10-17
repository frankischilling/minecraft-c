import java.io.*;
import java.lang.reflect.*;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Genuine original worlds, fresh original generators, and untouched numerical methods. */
public final class DensityProbe {
    private static final String[] CHANNELS = {"k", "l", "m", "n", "o", "a", "b", "c"};
    private static final String[] SCRATCH = {"d", "e", "f", "g", "h"};
    private static final String[] SPANS = {"d", "e", "f", "g", "h", "density", "blocks"};
    private final ClassLoader loader;
    private final boolean client;
    private final Path ownedRoot;
    private final Class<?> worldClass, providerClass, normalClass, storageClass, generatorClass, biomeClass;
    private final Method getManager, getSeed, managerGrid, densityMethod, baseMethod;
    private final IdentityHashMap<Object, String> biomeLabels = new IdentityHashMap<>();
    private final Field[] scratchFields = new Field[5];
    private Object world, manager, generator;
    private double[] density;
    private byte[] blocks;
    private Map<String, Object> climate;
    private Object currentBiomes;
    private int worldCount;
    private boolean matchingDensity;
    private int densityChunkX, densityChunkZ;

    private DensityProbe(ClassLoader loader, boolean client, Path ownedRoot) throws Exception {
        this.loader = loader;
        this.client = client;
        this.ownedRoot = ownedRoot;
        worldClass = Class.forName(client ? "fd" : "dj", true, loader);
        providerClass = Class.forName(client ? "xa" : "os", true, loader);
        normalClass = Class.forName(client ? "rh" : "kp", true, loader);
        storageClass = Class.forName(client ? "wt" : "om", true, loader);
        generatorClass = Class.forName(client ? "yf" : "pk", true, loader);
        biomeClass = Class.forName(client ? "kd" : "gs", true, loader);
        for (int i = 0; i < 13; i++) {
            String label = Character.toString((char) ('a' + i));
            Object identity = field(biomeClass, label).get(null);
            if (identity == null || biomeLabels.put(identity, label) != null) throw new IllegalStateException("biome catalogue");
        }
        Class<?> biomeArray = Array.newInstance(biomeClass, 0).getClass();
        Class<?> managerClass = Class.forName(client ? "xv" : "ph", true, loader);
        getManager = method(worldClass, "a");
        getSeed = method(worldClass, client ? "s" : "l");
        managerGrid = method(managerClass, "a", biomeArray, int.class, int.class, int.class, int.class);
        densityMethod = method(generatorClass, "a", double[].class, int.class, int.class, int.class, int.class, int.class, int.class);
        baseMethod = method(generatorClass, "a", int.class, int.class, byte[].class, biomeArray, double[].class);
        for (int i = 0; i < 5; i++) scratchFields[i] = field(generatorClass, SCRATCH[i]);
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

    private void initialize(long seed) throws Exception {
        if (++worldCount > 32) throw new IllegalArgumentException("world setup count limit");
        Path root = ownedRoot.resolve(String.format("seed-%03d", worldCount));
        PrintStream protocol = System.out;
        try {
            System.setOut(System.err);
            System.err.println("SETUP_BEGIN seed=" + seed + " root=" + root);
            if (!root.getParent().equals(ownedRoot) || Files.exists(root)) throw new IllegalStateException("world root must be new");
            Files.createDirectory(root);
            Object provider = normalClass.getDeclaredConstructor().newInstance();
            if (client) {
                Object storage = Class.forName("wy", true, loader).getDeclaredConstructor().newInstance();
                world = worldClass.getDeclaredConstructor(storageClass, String.class, providerClass, long.class)
                        .newInstance(storage, "density-reference", provider, seed);
            } else {
                Object storage = Class.forName("dq", true, loader).getDeclaredConstructor(File.class, String.class, boolean.class)
                        .newInstance(root.toFile(), "world", false);
                world = worldClass.getDeclaredConstructor(storageClass, String.class, long.class, providerClass)
                        .newInstance(storage, "density-reference", seed, provider);
            }
            if ((long) call(getSeed, world) != seed) throw new IllegalStateException("actual world seed differs");
            // Server spawn may already have sampled its provider's generator.
            // Measure a fresh genuine generator attached to this genuine world.
            generator = generatorClass.getDeclaredConstructor(worldClass, long.class).newInstance(world, seed);
            manager = call(getManager, world);
            System.err.println("SETUP_DONE seed=" + seed);
        } catch (Throwable failure) {
            System.err.println("UNAVAILABLE: genuine original world/generator setup failed for seed=" + seed);
            failure.printStackTrace(System.err);
            System.exit(77);
            return;
        } finally {
            System.setOut(protocol);
        }
        density = null;
        blocks = null;
        climate = null; // Unobserved, not a claim about spawn's manager cache nullness.
        currentBiomes = null;
        matchingDensity = false;
        for (Field scratch : scratchFields) if (scratch.get(generator) != null) throw new IllegalStateException("fresh generator scratch");
    }

    private static List<String> values(double[] array) {
        List<String> result = new ArrayList<>(array.length);
        for (double number : array) result.add(String.format("%016x", Double.doubleToRawLongBits(number)));
        return result;
    }

    private static String hex(byte[] array) {
        char[] alphabet = "0123456789abcdef".toCharArray();
        char[] result = new char[array.length * 2];
        for (int i = 0; i < array.length; i++) {
            result[i * 2] = alphabet[(array[i] & 255) >>> 4];
            result[i * 2 + 1] = alphabet[array[i] & 15];
        }
        return new String(result);
    }

    private List<String> continuation() throws Exception {
        Random original = (Random) field(generatorClass, "j").get(generator);
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream output = new ObjectOutputStream(bytes)) { output.writeObject(original); }
        Random copy;
        try (ObjectInputStream input = new ObjectInputStream(new ByteArrayInputStream(bytes.toByteArray()))) {
            copy = (Random) input.readObject();
        }
        List<String> result = new ArrayList<>(4);
        for (int i = 0; i < 4; i++) result.add(String.format("%016x", copy.nextLong()));
        return result;
    }

    private Map<String, Object> constructorState() throws Exception {
        Map<String, Object> result = new LinkedHashMap<>();
        for (String channel : CHANNELS) {
            Object octave = field(generatorClass, channel).get(generator);
            Object[] layers = (Object[]) field(octave.getClass(), "a").get(octave);
            List<Object> encoded = new ArrayList<>(layers.length);
            for (Object layer : layers) {
                Class<?> type = layer.getClass();
                Map<String, Object> entry = new LinkedHashMap<>();
                entry.put("offsets", values(new double[] {field(type, "a").getDouble(layer), field(type, "b").getDouble(layer), field(type, "c").getDouble(layer)}));
                int[] permutation = (int[]) field(type, "d").get(layer);
                if (permutation.length != 512) throw new IllegalStateException("permutation length");
                List<Integer> numbers = new ArrayList<>(512);
                for (int number : permutation) numbers.add(number);
                entry.put("permutation", numbers);
                encoded.add(entry);
            }
            result.put(channel, encoded);
        }
        return result;
    }

    private void refreshClimate(int x, int z) throws Exception {
        currentBiomes = call(managerGrid, manager, currentBiomes, x * 16, z * 16, 16, 16);
        Map<String, Object> result = new LinkedHashMap<>();
        String[] names = {"temperature", "rainfall", "auxiliary"};
        for (int i = 0; i < 3; i++) {
            double[] source = (double[]) field(manager.getClass(), Character.toString((char) ('a' + i))).get(manager);
            if (source.length < 256) throw new IllegalStateException("climate prefix length");
            result.put(names[i], values(Arrays.copyOf(source, 256)));
        }
        StringBuilder biomes = new StringBuilder(256);
        for (int i = 0; i < 256; i++) {
            String label = biomeLabels.get(Array.get(currentBiomes, i));
            if (label == null) throw new IllegalStateException("biome identity");
            biomes.append(label);
        }
        result.put("biomes", biomes.toString());
        climate = result;
    }

    private void sampleDensity(int x, int z) throws Exception {
        Object returned = call(densityMethod, generator, density, x * 4, 0, z * 4, 5, 17, 5);
        if (returned != density) throw new IllegalStateException("density replaced valid supplied output");
        densityChunkX = x;
        densityChunkZ = z;
        matchingDensity = true;
    }

    private void sampleBase(int x, int z) throws Exception {
        if (!matchingDensity || x != densityChunkX || z != densityChunkZ) throw new IllegalArgumentException("matching preceding density required");
        double[][] before = new double[5][];
        for (int i = 0; i < 5; i++) before[i] = ((double[]) scratchFields[i].get(generator)).clone();
        double[] temperature = (double[]) field(manager.getClass(), "a").get(manager);
        call(baseMethod, generator, x, z, blocks, currentBiomes, temperature);
        double[] recomputed = (double[]) field(generatorClass, "q").get(generator);
        if (recomputed.length != 425) throw new AssertionError("original q normal shape");
        for (int i = 0; i < 425; i++) {
            if (Double.doubleToRawLongBits(recomputed[i]) != Double.doubleToRawLongBits(density[i])) {
                throw new AssertionError("base recomputed density differs at " + i);
            }
        }
        for (int i = 0; i < 5; i++) {
            double[] after = (double[]) scratchFields[i].get(generator);
            if (after.length != before[i].length) throw new AssertionError("scratch recomputation length");
            for (int j = 0; j < after.length; j++) if (Double.doubleToRawLongBits(after[j]) != Double.doubleToRawLongBits(before[i][j])) {
                throw new AssertionError("scratch recomputation differs at " + i + ":" + j);
            }
        }
        System.err.println("CONTROL_BASE_RECOMPUTE q425_exact scratch_exact");
    }

    private Object[] spans() throws Exception {
        Object[] result = new Object[7];
        if (generator != null) for (int i = 0; i < 5; i++) result[i] = scratchFields[i].get(generator);
        result[5] = density;
        result[6] = blocks;
        return result;
    }

    private static Map<String, Object> descriptor(Object array) {
        if (array == null) return null;
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("length", Array.getLength(array));
        result.put("values", array instanceof byte[] ? hex((byte[]) array) : values((double[]) array));
        return result;
    }

    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1);
        String op = fields[0];
        int expected = op.equals("state") ? 1 : op.equals("init") ? 2 : op.equals("buffers") ? 8
                     : op.equals("density") || op.equals("base") || op.equals("generate") ? 3 : -1;
        if (fields.length != expected) throw new IllegalArgumentException("operation/field count");
        Object[] before = spans();
        if (op.equals("init")) initialize(Long.parseLong(fields[1]));
        else {
            if (generator == null) throw new IllegalArgumentException("initialize first");
            if (op.equals("buffers")) {
                for (int i = 0; i < 7; i++) {
                    int length = Integer.parseInt(fields[i + 1]);
                    int minimum = i < 3 || i == 5 ? 425 : i < 5 ? 25 : 32768;
                    int maximum = i < 3 || i == 5 ? 512 : i < 5 ? 64 : 32832;
                    if (length < minimum || length > maximum) throw new IllegalArgumentException("valid capacity required");
                    if (i == 6) {
                        blocks = new byte[length];
                        for (int j = 0; j < length; j++) blocks[j] = (byte) (j % 251);
                    } else {
                        double[] array = new double[length];
                        for (int j = 0; j < length; j++) array[j] = ((j % 11) - 5) * 0.125;
                        if (i < 5) scratchFields[i].set(generator, array);
                        else density = array;
                    }
                }
                matchingDensity = false;
            } else if (!op.equals("state")) {
                for (Object span : spans()) if (span == null) throw new IllegalArgumentException("fixture buffers required");
                int x = Integer.parseInt(fields[1]), z = Integer.parseInt(fields[2]);
                if (op.equals("density") || op.equals("generate")) {
                    refreshClimate(x, z);
                    sampleDensity(x, z);
                }
                if (op.equals("base") || op.equals("generate")) sampleBase(x, z);
            }
        }
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("state", op.equals("init") || op.equals("state") ? constructorState() : null);
        result.put("continuation", continuation());
        result.put("climate", climate);
        Object[] after = spans();
        StringBuilder retained = new StringBuilder(7);
        for (int i = 0; i < 7; i++) {
            result.put(SPANS[i], descriptor(after[i]));
            retained.append(!op.equals("init") && !op.equals("buffers") && before[i] != null && before[i] == after[i] ? '1' : '0');
        }
        result.put("retained", retained.toString());
        return "OK\t" + json(result);
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number) return value.toString();
        if (value instanceof String) return "\"" + ((String) value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        StringBuilder text = new StringBuilder(value instanceof Map ? "{" : "[");
        if (value instanceof Map) {
            for (Object object : ((Map<?, ?>) value).entrySet()) {
                Map.Entry<?, ?> entry = (Map.Entry<?, ?>) object;
                if (text.length() > 1) text.append(',');
                text.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
        } else for (Object object : (List<?>) value) {
            if (text.length() > 1) text.append(',');
            text.append(json(object));
        }
        return text.append(value instanceof Map ? '}' : ']').toString();
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

    public static void main(String[] args) throws Exception {
        if (args.length != 4 || !(args[0].equals("client") || args[0].equals("server"))) throw new IllegalArgumentException("Usage: DensityProbe side jar newOwnedRoot workspace");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toAbsolutePath().normalize();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c" : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!Files.isRegularFile(jar) || !hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar))).equals(expected)) {
            System.err.println("UNAVAILABLE: original JAR missing or SHA-256 mismatch"); System.exit(77);
        }
        Path workspace = Path.of(args[3]).toRealPath();
        Path allowed = workspace.resolve(".local/reference/density-setup").toRealPath();
        Path owned = Path.of(args[2]).toAbsolutePath().normalize();
        if (!allowed.startsWith(workspace) || !owned.getParent().toRealPath().equals(allowed) || Files.exists(owned)) throw new IllegalArgumentException("new owned world root must remain under workspace density-setup");
        Files.createDirectory(owned);
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            PrintStream protocol = System.out;
            DensityProbe probe;
            try {
                System.setOut(System.err);
                probe = new DensityProbe(loader, client, owned);
            } catch (Throwable failure) {
                System.err.println("UNAVAILABLE: original class setup failed"); failure.printStackTrace(System.err); System.exit(77); return;
            } finally { System.setOut(protocol); }
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String request; int count = 0;
            while ((request = line(reader)) != null) {
                if (++count > 1024) throw new IOException("request count limit");
                System.out.println(probe.request(request));
            }
        }
    }
}
