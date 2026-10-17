import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Direct measured base/surface calls; genuine server setup may generate chunks. */
public final class SurfaceProbe {
    private static final String[] CHANNELS = {"k", "l", "m", "n", "o", "a", "b", "c"};
    private static final String[] SCRATCH = {"r", "s", "t"};
    private static final String[] SPANS = {"sand", "gravel", "thickness", "blocks"};
    private final ClassLoader loader;
    private final boolean client;
    private final Path ownedRoot;
    private final Class<?> worldClass, providerClass, normalClass, storageClass, generatorClass, biomeClass;
    private final Method getManager, getSeed, managerGrid, baseMethod, surfaceMethod;
    private final Field[] scratchFields = new Field[3];
    private final IdentityHashMap<Object, String> biomeLabels = new IdentityHashMap<>();
    private Object world, manager, generator, currentBiomes;
    private Random random;
    private Map<String, Object> initialState, climate, density;
    private List<String> constructorContinuation;
    private String materials, chunkSeed;
    private byte[] blocks;
    private boolean matchingBase, matchingNoise;
    private int baseX, baseZ, worldCount;

    private SurfaceProbe(ClassLoader loader, boolean client, Path ownedRoot) throws Exception {
        this.loader = loader; this.client = client; this.ownedRoot = ownedRoot;
        worldClass = Class.forName(client ? "fd" : "dj", true, loader);
        providerClass = Class.forName(client ? "xa" : "os", true, loader);
        normalClass = Class.forName(client ? "rh" : "kp", true, loader);
        storageClass = Class.forName(client ? "wt" : "om", true, loader);
        generatorClass = Class.forName(client ? "yf" : "pk", true, loader);
        biomeClass = Class.forName(client ? "kd" : "gs", true, loader);
        for (int i = 0; i < 13; i++) {
            String label = Character.toString((char) ('a' + i));
            Object identity = field(biomeClass, label).get(null);
            if (identity == null || biomeLabels.put(identity, label) != null) throw new AssertionError("biome catalogue identities");
        }
        verifyDefaults();
        Class<?> biomeArray = Array.newInstance(biomeClass, 0).getClass();
        Class<?> managerClass = Class.forName(client ? "xv" : "ph", true, loader);
        getManager = method(worldClass, "a");
        getSeed = method(worldClass, client ? "s" : "l");
        managerGrid = method(managerClass, "a", biomeArray, int.class, int.class, int.class, int.class);
        baseMethod = method(generatorClass, "a", int.class, int.class, byte[].class, biomeArray, double[].class);
        surfaceMethod = method(generatorClass, "a", int.class, int.class, byte[].class, biomeArray);
        for (int i = 0; i < 3; i++) scratchFields[i] = field(generatorClass, SCRATCH[i]);
        Class<?> registry = Class.forName(client ? "uu" : "na", true, loader);
        String[] fields = {"u", "v", "w", "A", "C", "F", "G", "R", "aU"};
        int[] ids = {1, 2, 3, 7, 9, 12, 13, 24, 79};
        for (int i = 0; i < fields.length; i++) {
            Object block = field(registry, fields[i]).get(null);
            if (field(registry, "bn").getInt(block) != ids[i]) throw new AssertionError("actual initialized block binding " + fields[i]);
        }
    }
    private static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name); result.setAccessible(true); return result;
    }
    private static Method method(Class<?> type, String name, Class<?>... parameters) throws Exception {
        Method result = type.getDeclaredMethod(name, parameters); result.setAccessible(true); return result;
    }
    private static Object call(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            throw (Error) cause;
        }
    }
    private void verifyDefaults() throws Exception {
        for (Map.Entry<Object, String> entry : biomeLabels.entrySet()) {
            boolean sand = entry.getValue().equals("h") || entry.getValue().equals("j");
            if ((field(biomeClass, "p").getByte(entry.getKey()) & 255) != (sand ? 12 : 2)
                    || (field(biomeClass, "q").getByte(entry.getKey()) & 255) != (sand ? 12 : 3)) {
                throw new AssertionError("actual initialized material defaults " + entry.getValue());
            }
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
                        .newInstance(storage, "surface-reference", provider, seed);
            } else {
                Object storage = Class.forName("dq", true, loader).getDeclaredConstructor(File.class, String.class, boolean.class)
                        .newInstance(root.toFile(), "world", false);
                world = worldClass.getDeclaredConstructor(storageClass, String.class, long.class, providerClass)
                        .newInstance(storage, "surface-reference", seed, provider);
            }
            if ((long) call(getSeed, world) != seed) throw new AssertionError("actual world seed differs");
            // Spawn may have used the provider generator; measure a new genuine one.
            generator = generatorClass.getDeclaredConstructor(worldClass, long.class).newInstance(world, seed);
            manager = call(getManager, world);
            verifyDefaults();
            System.err.println("SETUP_DONE seed=" + seed);
        } catch (Throwable failure) {
            System.err.println("UNAVAILABLE: genuine original world/generator setup failed for seed=" + seed);
            failure.printStackTrace(System.err); System.exit(77); return;
        } finally { System.setOut(protocol); }
        random = (Random) field(generatorClass, "j").get(generator);
        constructorContinuation = continuation();
        initialState = constructorState();
        climate = null; density = null; materials = null; chunkSeed = null;
        blocks = null; currentBiomes = null; matchingBase = false; matchingNoise = false;
        if (field(generatorClass, "q").get(generator) != null) throw new AssertionError("fresh generator q");
        for (Field scratch : scratchFields) {
            double[] values = (double[]) scratch.get(generator);
            if (values == null || values.length != 256) throw new AssertionError("constructor surface scratch shape");
            for (double value : values) if (Double.doubleToRawLongBits(value) != 0L) throw new AssertionError("constructor surface scratch zero");
        }
    }
    private static List<String> values(double[] array) {
        List<String> result = new ArrayList<>(array.length);
        for (double number : array) result.add(String.format("%016x", Double.doubleToRawLongBits(number)));
        return result;
    }
    private static String hex(byte[] bytes) {
        char[] result = new char[bytes.length * 2]; char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; i++) {
            result[i * 2] = alphabet[(bytes[i] & 255) >>> 4]; result[i * 2 + 1] = alphabet[bytes[i] & 15];
        }
        return new String(result);
    }
    private List<String> continuation() throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream output = new ObjectOutputStream(bytes)) { output.writeObject(random); }
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
                Class<?> type = layer.getClass(); Map<String, Object> entry = new LinkedHashMap<>();
                entry.put("offsets", values(new double[] {field(type, "a").getDouble(layer), field(type, "b").getDouble(layer), field(type, "c").getDouble(layer)}));
                int[] permutation = (int[]) field(type, "d").get(layer);
                if (permutation.length != 512) throw new AssertionError("permutation length");
                List<Integer> numbers = new ArrayList<>(512);
                for (int number : permutation) numbers.add(number);
                entry.put("permutation", numbers); encoded.add(entry);
            }
            result.put(channel, encoded);
        }
        return result;
    }
    private String selectedLabels() {
        StringBuilder result = new StringBuilder(256);
        for (int i = 0; i < 256; i++) {
            String label = biomeLabels.get(Array.get(currentBiomes, i));
            if (label == null) throw new AssertionError("actual biome identity"); result.append(label);
        }
        return result.toString();
    }
    private String selectedMaterials() throws Exception {
        byte[] result = new byte[512]; Field top = field(biomeClass, "p"), filler = field(biomeClass, "q");
        for (int i = 0; i < 256; i++) {
            Object biome = Array.get(currentBiomes, i); result[i * 2] = top.getByte(biome); result[i * 2 + 1] = filler.getByte(biome);
        }
        return hex(result);
    }
    private Map<String, Object> actualClimate() throws Exception {
        Map<String, Object> result = new LinkedHashMap<>(); String[] names = {"temperature", "rainfall", "auxiliary"};
        for (int i = 0; i < 3; i++) {
            double[] source = (double[]) field(manager.getClass(), Character.toString((char) ('a' + i))).get(manager);
            if (source.length < 256) throw new AssertionError("climate shape");
            result.put(names[i], values(Arrays.copyOf(source, 256)));
        }
        result.put("biomes", selectedLabels()); return result;
    }
    private Map<String, Object> actualDensity() throws Exception {
        double[] source = (double[]) field(generatorClass, "q").get(generator);
        if (source == null) return null;
        if (source.length != 425) throw new AssertionError("normal q425 shape");
        return descriptor(source);
    }
    private void sampleBase(int x, int z) throws Exception {
        List<String> before = continuation();
        currentBiomes = call(managerGrid, manager, currentBiomes, x * 16, z * 16, 16, 16);
        climate = actualClimate(); materials = selectedMaterials();
        call(baseMethod, generator, x, z, blocks, currentBiomes, (double[]) field(manager.getClass(), "a").get(manager));
        density = actualDensity();
        if (!before.equals(continuation())) throw new AssertionError("base consumed live Random");
        baseX = x; baseZ = z; matchingBase = true; matchingNoise = false;
    }
    private void sampleSurface(int x, int z, boolean apply) throws Exception {
        if (!matchingBase || x != baseX || z != baseZ || materials == null || (apply && !matchingNoise)) {
            throw new IllegalArgumentException("matching base/materials and apply fields required");
        }
        if (!json(density).equals(json(actualDensity()))) throw new AssertionError("q changed before surface");
        if (!materials.equals(selectedMaterials())) throw new AssertionError("selected materials changed before surface");
        double[][] before = new double[3][];
        Object[] identities = new Object[3];
        for (int i = 0; i < 3; i++) { double[] array = (double[]) scratchFields[i].get(generator); before[i] = array.clone(); identities[i] = array; }
        byte[] suffix = Arrays.copyOfRange(blocks, 32768, blocks.length);
        call(surfaceMethod, generator, x, z, blocks, currentBiomes);
        if (!json(initialState).equals(json(constructorState()))) throw new AssertionError("surface changed noise82");
        if (!materials.equals(selectedMaterials())) throw new AssertionError("surface changed selected material fields");
        if (!json(climate).equals(json(actualClimate()))) throw new AssertionError("surface changed climate/biomes");
        if (!json(density).equals(json(actualDensity()))) throw new AssertionError("surface changed q425");
        if (!Arrays.equals(suffix, Arrays.copyOfRange(blocks, 32768, blocks.length))) throw new AssertionError("surface changed block suffix");
        for (int i = 0; i < 3; i++) {
            double[] after = (double[]) scratchFields[i].get(generator);
            if (after != identities[i] || after.length != before[i].length) throw new AssertionError("scratch retention");
            for (int j = 256; j < after.length; j++) if (Double.doubleToRawLongBits(after[j]) != 0L) throw new AssertionError("scratch suffix not +0");
            if (apply) for (int j = 0; j < after.length; j++) {
                if (Double.doubleToRawLongBits(after[j]) != Double.doubleToRawLongBits(before[i][j])) throw new AssertionError("apply recomputed fields differ");
            }
        }
        matchingNoise = true;
        System.err.println("CONTROL_SURFACE_STATE noise82_exact materials_exact density_exact suffix_exact");
        if (apply) System.err.println("CONTROL_APPLY_FIELDS scratch_exact");
    }
    private Object[] spans() throws Exception {
        Object[] result = new Object[4];
        if (generator != null) for (int i = 0; i < 3; i++) result[i] = scratchFields[i].get(generator);
        result[3] = blocks; return result;
    }
    private static Map<String, Object> descriptor(Object array) {
        if (array == null) return null;
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("length", Array.getLength(array)); result.put("values", array instanceof byte[] ? hex((byte[]) array) : values((double[]) array)); return result;
    }
    private String request(String line) throws Exception {
        String[] fields = line.split("\t", -1); String op = fields[0];
        int expected = op.equals("state") ? 1 : op.equals("init") ? 2 : op.equals("buffers") ? 5
                     : op.equals("base") || op.equals("seed") || op.equals("surface") || op.equals("apply") ? 3 : -1;
        if (fields.length != expected) throw new IllegalArgumentException("operation/field count");
        Object[] before = spans();
        if (op.equals("init")) initialize(Long.parseLong(fields[1]));
        else {
            if (generator == null) throw new IllegalArgumentException("initialize first");
            if (op.equals("buffers")) {
                int[] lengths = new int[4];
                for (int i = 0; i < 4; i++) {
                    lengths[i] = Integer.parseInt(fields[i + 1]);
                    if (lengths[i] < (i == 3 ? 32768 : 256) || lengths[i] > (i == 3 ? 32832 : 512)) throw new IllegalArgumentException("valid bounded capacities required");
                }
                for (int i = 0; i < 3; i++) {
                    double[] array = new double[lengths[i]];
                    for (int j = 0; j < array.length; j++) array[j] = ((j % 11) - 5) * .125;
                    scratchFields[i].set(generator, array);
                }
                blocks = new byte[lengths[3]];
                for (int j = 0; j < blocks.length; j++) blocks[j] = (byte) (j % 251);
                matchingBase = false; matchingNoise = false;
            } else if (!op.equals("state")) {
                int x = Integer.parseInt(fields[1]), z = Integer.parseInt(fields[2]);
                if (op.equals("seed")) {
                    long seed = (long) x * 341873128712L + (long) z * 132897987541L;
                    random.setSeed(seed); chunkSeed = String.format("%016x", seed);
                } else {
                    if (blocks == null) throw new IllegalArgumentException("fixture buffers required");
                    if (op.equals("base")) sampleBase(x, z); else sampleSurface(x, z, op.equals("apply"));
                }
            }
        }
        Map<String, Object> observedState = constructorState();
        if (!json(initialState).equals(json(observedState))) throw new AssertionError("noise82 changed");
        if (!json(density).equals(json(actualDensity()))) throw new AssertionError("observed q425 changed");
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("state", op.equals("init") || op.equals("state") ? observedState : null);
        result.put("constructor_continuation", constructorContinuation); result.put("continuation", continuation());
        result.put("climate", climate); result.put("materials", materials); result.put("chunk_seed", chunkSeed);
        Object[] after = spans(); StringBuilder retained = new StringBuilder(4);
        for (int i = 0; i < 3; i++) {
            result.put(SPANS[i], descriptor(after[i]));
        }
        for (int i = 0; i < 4; i++) {
            retained.append(!op.equals("init") && !op.equals("buffers") && before[i] != null && before[i] == after[i] ? '1' : '0');
        }
        result.put("density", density); result.put("blocks", descriptor(blocks)); result.put("retained", retained.toString());
        return "OK\t" + json(result);
    }
    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number) return value.toString();
        if (value instanceof String) return "\"" + ((String) value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        StringBuilder result = new StringBuilder(value instanceof Map ? "{" : "[");
        if (value instanceof Map) for (Object item : ((Map<?, ?>) value).entrySet()) {
            Map.Entry<?, ?> entry = (Map.Entry<?, ?>) item;
            if (result.length() > 1) result.append(','); result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
        } else for (Object item : (List<?>) value) {
            if (result.length() > 1) result.append(','); result.append(json(item));
        }
        return result.append(value instanceof Map ? '}' : ']').toString();
    }
    private static String line(BufferedReader reader) throws IOException {
        StringBuilder result = new StringBuilder(); int value;
        while ((value = reader.read()) != -1 && value != '\n') {
            if (result.length() == 256) throw new IOException("request line limit"); result.append((char) value);
        }
        if (value == -1 && result.length() == 0) return null;
        if (result.length() != 0 && result.charAt(result.length() - 1) == '\r') result.setLength(result.length() - 1);
        return result.toString();
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 4 || !(args[0].equals("client") || args[0].equals("server"))) throw new IllegalArgumentException("Usage: SurfaceProbe side jar newOwnedRoot workspace");
        boolean client = args[0].equals("client"); Path jar = Path.of(args[1]).toAbsolutePath().normalize();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c" : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!Files.isRegularFile(jar) || !hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar))).equals(expected)) {
            System.err.println("UNAVAILABLE: original JAR missing or SHA-256 mismatch"); System.exit(77); return;
        }
        Path workspace = Path.of(args[3]).toRealPath();
        Path allowed = workspace.resolve(".local/reference/surface-setup").toRealPath();
        Path owned = Path.of(args[2]).toAbsolutePath().normalize();
        if (!allowed.startsWith(workspace) || !owned.getParent().toRealPath().equals(allowed) || Files.exists(owned)) throw new IllegalArgumentException("new owned world root must remain under workspace surface-setup");
        Files.createDirectory(owned);
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            SurfaceProbe probe; PrintStream protocol = System.out;
            try {
                System.setOut(System.err); probe = new SurfaceProbe(loader, client, owned);
            } catch (Throwable failure) {
                System.err.println("UNAVAILABLE: original class setup failed"); failure.printStackTrace(System.err); System.exit(77); return;
            } finally { System.setOut(protocol); }
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
            String request; int count = 0;
            while ((request = line(reader)) != null) {
                if (++count > 1024) throw new IOException("request count limit"); System.out.println(probe.request(request));
            }
        }
    }
}
