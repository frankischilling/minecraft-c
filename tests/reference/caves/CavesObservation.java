import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/**
 * Observe the original normal cave stage in fresh genuine world contexts.
 * Constructor setup is separate from the measured fresh generator's work.
 * Measured direct calls prepare actual base/surface, then invoke actual u's
 * public cave entry. Genuine server constructor setup may generate chunks.
 */
public final class CavesObservation {
    private static final int MAX_CASES = 12;
    private static final int MAX_LINE = 96;
    private static final int MAX_REQUEST_BYTES = 4096;
    private static final int MAX_CASE_OUTPUT = 4 * 1024 * 1024;
    private static final long MAX_OUTPUT = 64L * 1024L * 1024L;
    private static final int MIN_TARGET = Integer.MIN_VALUE + 8;
    private static final int MAX_TARGET = Integer.MAX_VALUE - 9;
    private static final String[] CHANNELS = {"k", "l", "m", "n", "o", "a", "b", "c"};
    private static final int[] LAYER_COUNTS = {16, 16, 8, 4, 4, 10, 16, 8};
    private static final String[] SCRATCH = {"d", "e", "f", "g", "h", "r", "s", "t"};
    private static final String[] IMMUTABLE = {"generator_continuation", "noise82", "q425", "scratch",
            "climate", "materials", "math_table_sha256_f32be"};

    private static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name); result.setAccessible(true); return result;
    }
    private static Method method(Class<?> type, String name, Class<?>... arguments) throws Exception {
        Method result = type.getDeclaredMethod(name, arguments); result.setAccessible(true); return result;
    }
    private static Object call(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            throw (Error) cause;
        }
    }
    private static String hex(byte[] bytes) {
        char[] chars = new char[bytes.length * 2]; char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; i++) {
            chars[i * 2] = alphabet[(bytes[i] & 255) >>> 4]; chars[i * 2 + 1] = alphabet[bytes[i] & 15];
        }
        return new String(chars);
    }
    private static String hash(Path path) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path)));
    }
    private static List<String> bits(double[] array) {
        List<String> result = new ArrayList<>(array.length);
        for (double number : array) result.add(String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(number)));
        return result;
    }
    private static Map<String, Object> descriptor(Object array) {
        if (array == null) return null;
        Map<String, Object> result = new LinkedHashMap<>(); int length = Array.getLength(array);
        if (length > 4096 && !(array instanceof byte[])) throw new IllegalStateException("observed array exceeds capture bound");
        result.put("length", length);
        result.put("values", array instanceof byte[] ? hex((byte[]) array) : bits((double[]) array));
        return result;
    }
    private static List<String> continuation(Random original) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream output = new ObjectOutputStream(bytes)) { output.writeObject(original); }
        Random copy;
        try (ObjectInputStream input = new ObjectInputStream(new ByteArrayInputStream(bytes.toByteArray()))) {
            copy = (Random) input.readObject();
        }
        List<String> result = new ArrayList<>(4);
        for (int i = 0; i < 4; i++) result.add(String.format(Locale.ROOT, "%016x", copy.nextLong()));
        return result;
    }
    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number || value instanceof Boolean) return value.toString();
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
    private static final class Request {
        final long seed;
        final int x, z;
        Request(long seed, int x, int z) { this.seed = seed; this.x = x; this.z = z; }
    }
    private static final class Context {
        Object world, generator, manager, cave, biomes;
        Random generatorRandom, caveRandom;
        byte[] blocks;
    }
    private static final class Snapshot {
        final Map<String, Object> record;
        final Object[] identities;
        Snapshot(Map<String, Object> record, Object[] identities) { this.record = record; this.identities = identities; }
    }

    private static final class Model {
        final ClassLoader loader;
        final boolean client;
        final Class<?> worldClass, providerClass, normalClass, storageClass, generatorClass;
        final Class<?> biomeClass, caveBaseClass, caveClass, generatorInterface, mathClass;
        final Method getSeed, getManager, grid, base, surface, caves;
        final Field mathField;
        final float[] mathTable;
        final int[] initialMathBits;
        final IdentityHashMap<Object, String> labels = new IdentityHashMap<>();

        Model(ClassLoader loader, boolean client) throws Exception {
            this.loader = loader; this.client = client;
            worldClass = Class.forName(client ? "fd" : "dj", true, loader);
            providerClass = Class.forName(client ? "xa" : "os", true, loader);
            normalClass = Class.forName(client ? "rh" : "kp", true, loader);
            storageClass = Class.forName(client ? "wt" : "om", true, loader);
            generatorClass = Class.forName(client ? "yf" : "pk", true, loader);
            biomeClass = Class.forName(client ? "kd" : "gs", true, loader);
            caveBaseClass = Class.forName(client ? "fv" : "dy", true, loader);
            caveClass = Class.forName(client ? "sq" : "lt", true, loader);
            generatorInterface = Class.forName(client ? "cl" : "bl", true, loader);
            mathClass = Class.forName(client ? "in" : "fq", true, loader);
            mathField = field(mathClass, "a"); mathTable = (float[]) mathField.get(null);
            if (mathTable.length != 65536) throw new AssertionError("actual math table shape");
            initialMathBits = new int[mathTable.length];
            for (int i = 0; i < mathTable.length; i++) initialMathBits[i] = Float.floatToRawIntBits(mathTable[i]);
            for (int i = 0; i < 13; i++) {
                String label = Character.toString((char) ('a' + i));
                Object identity = field(biomeClass, label).get(null);
                if (identity == null || labels.put(identity, label) != null) throw new AssertionError("actual biome catalogue");
            }
            Class<?> biomeArray = Array.newInstance(biomeClass, 0).getClass();
            Class<?> managerClass = Class.forName(client ? "xv" : "ph", true, loader);
            getSeed = method(worldClass, client ? "s" : "l"); getManager = method(worldClass, "a");
            grid = method(managerClass, "a", biomeArray, int.class, int.class, int.class, int.class);
            base = method(generatorClass, "a", int.class, int.class, byte[].class, biomeArray, double[].class);
            surface = method(generatorClass, "a", int.class, int.class, byte[].class, biomeArray);
            caves = method(caveBaseClass, "a", generatorInterface, worldClass, int.class, int.class, byte[].class);
        }
        Context setup(Request request, Path ownedRoot, int index) throws Exception {
            Context context = new Context(); Path root = ownedRoot.resolve(String.format(Locale.ROOT, "case-%03d", index));
            PrintStream protocol = System.out;
            try {
                System.setOut(System.err);
                System.err.println("SETUP_BEGIN seed=" + request.seed + " root=" + root);
                if (!root.getParent().equals(ownedRoot) || Files.exists(root) || Files.isSymbolicLink(root)) throw new IllegalStateException("case root must be new");
                Files.createDirectory(root);
                Object provider = normalClass.getDeclaredConstructor().newInstance();
                if (client) {
                    Object storage = Class.forName("wy", true, loader).getDeclaredConstructor().newInstance();
                    context.world = worldClass.getDeclaredConstructor(storageClass, String.class, providerClass, long.class)
                            .newInstance(storage, "caves-reference", provider, request.seed);
                } else {
                    Object storage = Class.forName("dq", true, loader).getDeclaredConstructor(File.class, String.class, boolean.class)
                            .newInstance(root.toFile(), "world", false);
                    context.world = worldClass.getDeclaredConstructor(storageClass, String.class, long.class, providerClass)
                            .newInstance(storage, "caves-reference", request.seed, provider);
                }
                if ((long) call(getSeed, context.world) != request.seed) throw new AssertionError("actual world seed");
                // Server spawn may already have used its provider generator.
                context.generator = generatorClass.getDeclaredConstructor(worldClass, long.class).newInstance(context.world, request.seed);
                context.manager = call(getManager, context.world);
                context.generatorRandom = (Random) field(generatorClass, "j").get(context.generator);
                context.cave = field(generatorClass, "u").get(context.generator);
                if (context.cave.getClass() != caveClass) throw new AssertionError("actual normal cave subclass");
                if (field(caveBaseClass, "a").getInt(context.cave) != 8) throw new AssertionError("actual default cave range");
                context.caveRandom = (Random) field(caveBaseClass, "b").get(context.cave);
                // Obtain the reference only. Do not inspect/serialize its unseeded state.
                System.err.println("SETUP_DONE seed=" + request.seed);
                return context;
            } catch (Throwable failure) {
                System.err.println("UNAVAILABLE: genuine original world/generator setup failed for seed=" + request.seed);
                failure.printStackTrace(System.err); return null;
            } finally { System.setOut(protocol); }
        }
        Map<String, Object> noise(Context context) throws Exception {
            Map<String, Object> result = new LinkedHashMap<>();
            for (int channel = 0; channel < CHANNELS.length; channel++) {
                Object octave = field(generatorClass, CHANNELS[channel]).get(context.generator);
                Object[] layers = (Object[]) field(octave.getClass(), "a").get(octave);
                if (layers.length != LAYER_COUNTS[channel]) throw new AssertionError("actual layer count");
                List<Object> encoded = new ArrayList<>(layers.length);
                for (Object layer : layers) {
                    Class<?> type = layer.getClass(); Map<String, Object> entry = new LinkedHashMap<>();
                    entry.put("offsets", bits(new double[] {field(type, "a").getDouble(layer), field(type, "b").getDouble(layer), field(type, "c").getDouble(layer)}));
                    int[] permutation = (int[]) field(type, "d").get(layer);
                    if (permutation.length != 512) throw new AssertionError("actual permutation length");
                    List<Integer> entries = new ArrayList<>(512);
                    for (int value : permutation) entries.add(value);
                    entry.put("permutation", entries); encoded.add(entry);
                }
                result.put(CHANNELS[channel], encoded);
            }
            return result;
        }
        String tableControl() throws Exception {
            if (mathField.get(null) != mathTable) throw new AssertionError("math table identity changed");
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            for (int i = 0; i < mathTable.length; i++) {
                int value = Float.floatToRawIntBits(mathTable[i]);
                if (value != initialMathBits[i]) throw new AssertionError("math table bits changed at " + i);
                digest.update((byte) (value >>> 24)); digest.update((byte) (value >>> 16));
                digest.update((byte) (value >>> 8)); digest.update((byte) value);
            }
            return hex(digest.digest());
        }
        String materialValues(Context context) throws Exception {
            if (context.biomes == null) return null;
            byte[] bytes = new byte[512]; Field top = field(biomeClass, "p"), filler = field(biomeClass, "q");
            for (int i = 0; i < 256; i++) {
                Object biome = Array.get(context.biomes, i); bytes[i * 2] = top.getByte(biome); bytes[i * 2 + 1] = filler.getByte(biome);
            }
            return hex(bytes);
        }
        Map<String, Object> climateValues(Context context) throws Exception {
            if (context.biomes == null) return null; // Unobserved, not manager-nullness after spawn.
            Map<String, Object> result = new LinkedHashMap<>(); String[] names = {"temperature", "rainfall", "auxiliary"};
            for (int i = 0; i < 3; i++) {
                Object values = field(context.manager.getClass(), Character.toString((char) ('a' + i))).get(context.manager);
                if (values == null || Array.getLength(values) < 256) throw new AssertionError("actual observed climate shape");
                result.put(names[i], descriptor(values));
            }
            if (Array.getLength(context.biomes) != 256) throw new AssertionError("actual selected biome shape");
            StringBuilder selected = new StringBuilder(256);
            for (int i = 0; i < 256; i++) {
                String label = labels.get(Array.get(context.biomes, i));
                if (label == null) throw new AssertionError("actual selected biome identity"); selected.append(label);
            }
            result.put("biomes", selected.toString()); return result;
        }
        Snapshot snapshot(Context context, boolean caveObserved) throws Exception {
            if (field(generatorClass, "j").get(context.generator) != context.generatorRandom
                    || field(generatorClass, "u").get(context.generator) != context.cave
                    || field(caveBaseClass, "b").get(context.cave) != context.caveRandom) throw new AssertionError("retained original Random/cave identity");
            Map<String, Object> result = new LinkedHashMap<>();
            result.put("generator_continuation", continuation(context.generatorRandom));
            result.put("cave_continuation", caveObserved ? continuation(context.caveRandom) : null);
            result.put("noise82", noise(context));
            Object q = field(generatorClass, "q").get(context.generator);
            if (q != null && Array.getLength(q) != 425) throw new AssertionError("actual normal q425");
            result.put("q425", descriptor(q));
            Map<String, Object> scratch = new LinkedHashMap<>(); List<Object> identities = new ArrayList<>();
            identities.add(q);
            for (String name : SCRATCH) {
                Object array = field(generatorClass, name).get(context.generator);
                scratch.put(name, descriptor(array)); identities.add(array);
            }
            result.put("scratch", scratch); result.put("climate", climateValues(context)); result.put("materials", materialValues(context));
            if (context.biomes != null) for (String name : new String[] {"a", "b", "c"}) identities.add(field(context.manager.getClass(), name).get(context.manager));
            identities.add(context.biomes); identities.add(mathTable);
            result.put("math_table_sha256_f32be", tableControl()); result.put("blocks", descriptor(context.blocks));
            return new Snapshot(result, identities.toArray());
        }
        void prefix(Context context, Request request, Snapshot constructor) throws Exception {
            long chunkSeed = (long) request.x * 341873128712L + (long) request.z * 132897987541L;
            context.generatorRandom.setSeed(chunkSeed);
            List<String> seeded = continuation(context.generatorRandom);
            context.biomes = call(grid, context.manager, null, request.x * 16, request.z * 16, 16, 16);
            Map<String, Object> climate = climateValues(context); String materials = materialValues(context);
            context.blocks = new byte[32771];
            for (int i = 0; i < context.blocks.length; i++) context.blocks[i] = (byte) (i % 251);
            call(base, context.generator, request.x, request.z, context.blocks, context.biomes,
                    (double[]) field(context.manager.getClass(), "a").get(context.manager));
            if (!seeded.equals(continuation(context.generatorRandom))) throw new AssertionError("base/climate consumed generator Random");
            Object density = field(generatorClass, "q").get(context.generator);
            if (density == null || Array.getLength(density) != 425) throw new AssertionError("actual base q425");
            Map<String, Object> q = descriptor(density);
            call(surface, context.generator, request.x, request.z, context.blocks, context.biomes);
            if (!json(constructor.record.get("noise82")).equals(json(noise(context)))) throw new AssertionError("prefix changed noise82");
            if (!json(q).equals(json(descriptor(field(generatorClass, "q").get(context.generator))))) throw new AssertionError("surface changed q425");
            if (!materials.equals(materialValues(context)) || !json(climate).equals(json(climateValues(context)))) throw new AssertionError("surface changed climate/materials");
            if (!hex(Arrays.copyOfRange(context.blocks, 32768, 32771)).equals("8a8b8c")) throw new AssertionError("prefix changed owned suffix");
            tableControl();
        }
        Map<String, Object> cavePass(Context context, Request request, String label, Snapshot prefix) throws Exception {
            byte[] beforeBlocks = context.blocks.clone(); Snapshot before = snapshot(context, false);
            call(caves, context.cave, context.generator, context.world, request.x, request.z, context.blocks);
            Snapshot after = snapshot(context, true);
            for (String key : IMMUTABLE) {
                if (!json(before.record.get(key)).equals(json(after.record.get(key)))) throw new AssertionError("caves changed " + key);
                if (!json(prefix.record.get(key)).equals(json(after.record.get(key)))) throw new AssertionError("caves changed prefix input " + key);
            }
            if (before.identities.length != after.identities.length) throw new AssertionError("observation identity cardinality");
            for (int i = 0; i < before.identities.length; i++) if (before.identities[i] != after.identities[i]) throw new AssertionError("caves replaced observed array");
            if (!Arrays.equals(Arrays.copyOfRange(beforeBlocks, 32768, beforeBlocks.length),
                    Arrays.copyOfRange(context.blocks, 32768, context.blocks.length))) throw new AssertionError("caves changed block suffix");
            int changes = 0;
            for (int i = 0; i < 32768; i++) if (beforeBlocks[i] != context.blocks[i]) changes++;
            Map<String, Object> result = new LinkedHashMap<>();
            result.put("label", label); result.put("input_blocks", descriptor(beforeBlocks)); result.put("after", after.record);
            result.put("changed_prefix_bytes", changes); result.put("suffix_unchanged", true);
            System.err.println("CONTROL_CAVES_STATE j_exact noise82_exact q425_exact scratch_exact climate_exact materials_exact table_exact suffix_exact");
            return result;
        }
        Map<String, Object> observe(Context context, Request request, int index) throws Exception {
            Snapshot constructor = snapshot(context, false);
            prefix(context, request, constructor); Snapshot prefix = snapshot(context, false);
            byte[] originalPrefix = context.blocks.clone();
            List<Object> passes = new ArrayList<>(3);
            passes.add(cavePass(context, request, "first", prefix));
            byte[] firstOutput = context.blocks.clone();
            System.arraycopy(originalPrefix, 0, context.blocks, 0, originalPrefix.length);
            passes.add(cavePass(context, request, "restored", prefix));
            byte[] restoredOutput = context.blocks.clone();
            passes.add(cavePass(context, request, "changed", prefix));
            Map<String, Object> result = new LinkedHashMap<>();
            result.put("case", index); result.put("seed", request.seed); result.put("chunk", Arrays.asList(request.x, request.z));
            long chunkSeed = (long) request.x * 341873128712L + (long) request.z * 132897987541L;
            result.put("chunk_seed", String.format(Locale.ROOT, "%016x", chunkSeed));
            result.put("constructor", constructor.record); result.put("prefix", prefix.record); result.put("passes", passes);
            // Observed relationships, not presumed idempotence acceptance rules.
            result.put("restored_output_equal_first", Arrays.equals(firstOutput, restoredOutput));
            result.put("reused_input_differs_from_prefix", !Arrays.equals(originalPrefix, restoredOutput));
            result.put("changed_output_equal_its_input", Arrays.equals(restoredOutput, context.blocks));
            return result;
        }
    }

    private static List<Request> readRequests(Path path) throws Exception {
        if (Files.size(path) > MAX_REQUEST_BYTES) throw new IllegalArgumentException("request byte limit");
        List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
        if (lines.isEmpty() || lines.size() > MAX_CASES) throw new IllegalArgumentException("request count limit");
        List<Request> result = new ArrayList<>(lines.size());
        for (String line : lines) {
            if (line.length() > MAX_LINE || !line.matches("case\\t-?(?:0|[1-9][0-9]*)\\t-?(?:0|[1-9][0-9]*)\\t-?(?:0|[1-9][0-9]*)")) {
                throw new IllegalArgumentException("bounded case TSV required");
            }
            String[] fields = line.split("\t", -1);
            long seed = Long.parseLong(fields[1]); int x = Integer.parseInt(fields[2]), z = Integer.parseInt(fields[3]);
            if (x < MIN_TARGET || x > MAX_TARGET || z < MIN_TARGET || z > MAX_TARGET) throw new IllegalArgumentException("ordinary scheduler domain required");
            result.add(new Request(seed, x, z));
        }
        return result;
    }
    private static int run(String[] args) throws Exception {
        if (args.length != 5 || !(args[0].equals("client") || args[0].equals("server"))) throw new IllegalArgumentException("Usage: CavesObservation side jar requests newOwnedRoot workspace");
        boolean client = args[0].equals("client"); Path jar = Path.of(args[1]).toRealPath();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!hash(jar).equals(expected)) { System.err.println("UNAVAILABLE: original JAR SHA-256 differs"); return 77; }
        List<Request> requests = readRequests(Path.of(args[2]).toRealPath());
        Path workspace = Path.of(args[4]).toRealPath();
        Path allowed = workspace.resolve(".local/reference/caves-setup").toRealPath();
        Path owned = Path.of(args[3]).toAbsolutePath().normalize();
        if (!allowed.startsWith(workspace) || !owned.getParent().toRealPath().equals(allowed)
                || Files.exists(owned) || Files.isSymbolicLink(owned)) throw new IllegalArgumentException("new owned world root required beneath workspace caves-setup");
        Files.createDirectory(owned);
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Model model; PrintStream protocol = System.out;
            try { System.setOut(System.err); model = new Model(loader, client); }
            catch (Throwable failure) {
                System.err.println("UNAVAILABLE: original class setup failed"); failure.printStackTrace(System.err); return 77;
            } finally { System.setOut(protocol); }
            long outputBytes = 0;
            for (int i = 0; i < requests.size(); i++) {
                Request request = requests.get(i); Context context = model.setup(request, owned, i + 1);
                if (context == null) return 77;
                System.err.println("MEASURE_BEGIN case=" + (i + 1) + " seed=" + request.seed);
                String line = "OK\t" + json(model.observe(context, request, i + 1));
                long size = line.getBytes(StandardCharsets.UTF_8).length + System.lineSeparator().getBytes(StandardCharsets.UTF_8).length;
                if (size > MAX_CASE_OUTPUT || outputBytes + size > MAX_OUTPUT) throw new IllegalStateException("capture output bound exceeded");
                outputBytes += size; System.out.println(line);
                System.err.println("MEASURE_DONE case=" + (i + 1) + " seed=" + request.seed);
            }
            return 0;
        } finally {
            if (!hash(jar).equals(expected)) throw new AssertionError("original JAR changed during observation");
        }
    }
    public static void main(String[] args) throws Exception {
        int status = run(args); if (status != 0) System.exit(status);
    }
}
