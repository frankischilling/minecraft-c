import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
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
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Read-only numerical observations around genuine seed23 cache acquisitions.
 * Compile alongside the unchanged ChunkObservation and CacheCallbacksObservation.
 * Random inspection requires --add-opens java.base/java.util=ALL-UNNAMED.
 */
public final class NaturalPopulationObservation {
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final int MAX_ARRAY = 32768;
    private static final int CONTINUATION = 8;
    private static final String SEED = "0000000000000017";
    private static final String[] SCRATCH = {"q", "d", "e", "f", "g", "h", "r", "s", "t", "w"};
    private static final int[][] SCHEDULE = {{1, 0}, {0, 1}, {1, 1}, {1, 1}};
    private static boolean measuring;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Map<String, Object> object(Object... pairs) {
        require(pairs.length % 2 == 0, "complete object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) {
            require(pairs[i] instanceof String, "string object key");
            String key = (String) pairs[i];
            require(!result.containsKey(key), "distinct object key");
            result.put(key, pairs[i + 1]);
        }
        return result;
    }

    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers)
            throws Exception {
        Field result = owner.getDeclaredField(name);
        require(result.getType() == type && result.getModifiers() == modifiers,
                "exact field " + owner.getName() + "." + name + ":" + type.descriptorString());
        result.setAccessible(true);
        return result;
    }

    private static Method method(Class<?> owner, String name, Class<?> result, int modifiers,
                                 Class<?>... arguments) throws Exception {
        Method value = owner.getDeclaredMethod(name, arguments);
        require(value.getReturnType() == result && value.getModifiers() == modifiers,
                "exact method " + owner.getName() + "." + name);
        value.setAccessible(true);
        return value;
    }

    private static Exception unwrap(InvocationTargetException failure) {
        Throwable cause = failure.getCause();
        if (cause instanceof Error) throw (Error) cause;
        if (cause instanceof Exception) return (Exception) cause;
        return failure;
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) { throw unwrap(failure); }
    }

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static String bits(double value) {
        return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value));
    }

    private static final class RandomFields {
        final Field seed = field(Random.class, "seed", AtomicLong.class, 18);
        final Field gaussian = field(Random.class, "haveNextNextGaussian", boolean.class, 2);
        final Field nextGaussian = field(Random.class, "nextNextGaussian", double.class, 2);

        RandomFields() throws Exception {
            require(Random.class.getClassLoader() == null && AtomicLong.class.getClassLoader() == null,
                    "bootstrap Random and AtomicLong owners");
        }

        AtomicLong seedOwner(Random random) throws Exception {
            require(random != null && random.getClass() == Random.class, "actual java.util.Random");
            Object value = seed.get(random);
            require(value != null && value.getClass() == AtomicLong.class, "actual AtomicLong seed");
            return (AtomicLong) value;
        }

        Map<String, Object> snapshot(Random random) throws Exception {
            AtomicLong owner = seedOwner(random);
            long initialSeed = owner.get();
            boolean initialGaussian = gaussian.getBoolean(random);
            long initialBits = Double.doubleToRawLongBits(nextGaussian.getDouble(random));
            require(initialSeed >= 0 && initialSeed < (1L << 48), "raw48 Random seed");
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            try (ObjectOutputStream stream = new ObjectOutputStream(bytes)) {
                stream.writeObject(random);
            }
            byte[] serialized = bytes.toByteArray();
            require(serialized.length <= 4096, "bounded bootstrap Random serialization");
            Random copy;
            try (ObjectInputStream stream = new ObjectInputStream(new ByteArrayInputStream(serialized))) {
                Object value = stream.readObject();
                require(value != null && value.getClass() == Random.class && stream.read() == -1,
                        "complete actual Random copy");
                copy = (Random) value;
            }
            require(seedOwner(copy).get() == initialSeed && gaussian.getBoolean(copy) == initialGaussian
                    && Double.doubleToRawLongBits(nextGaussian.getDouble(copy)) == initialBits,
                    "copy preserves all actual Random state");
            List<String> continuation = new ArrayList<>();
            for (int i = 0; i < CONTINUATION; ++i)
                continuation.add(String.format(Locale.ROOT, "%016x", copy.nextLong()));
            require(seedOwner(random) == owner && owner.get() == initialSeed
                    && gaussian.getBoolean(random) == initialGaussian
                    && Double.doubleToRawLongBits(nextGaussian.getDouble(random)) == initialBits,
                    "actual Random owner and state unchanged by observation");
            return object("seed48", String.format(Locale.ROOT, "%012x", initialSeed),
                    "have_next_next_gaussian", initialGaussian,
                    "next_next_gaussian_bits", String.format(Locale.ROOT, "%016x", initialBits),
                    "next_longs", continuation);
        }
    }

    private static final class Observation {
        final Object helper, world, dimension, storage, cache, generator, manager, cave, loaderOwner;
        final Class<?> helperType, worldType, chunkType, generatorType, managerType, biomeType;
        final ClassLoader loader;
        final URL jar;
        final Method snapshot, reference, helperGet, acquire;
        final Field generatorWorld, generatorRandom, caveRandom, providerBiomes, managerBiomes;
        final Field worldRandom, drainDepth, chunkLight, falling;
        final Map<String, Field> scratch = new LinkedHashMap<>();
        final Map<String, Field> climate = new LinkedHashMap<>();
        final IdentityHashMap<Object, String> biomeNames = new IdentityHashMap<>();
        final Map<String, Object> biomeStatics = new LinkedHashMap<>();
        final RandomFields randomFields;

        Observation(Object helper, Class<?> helperType, ClassLoader loader, URL jar,
                    boolean client, RandomFields randomFields) throws Exception {
            this.helper = helper; this.helperType = helperType; this.loader = loader;
            this.jar = jar; this.randomFields = randomFields;
            require(helper.getClass() == helperType && helperType.getModifiers() == 26,
                    "unchanged private static final callback Observer");
            worldType = (Class<?>) field(helperType, "worldType", Class.class, 16).get(helper);
            chunkType = (Class<?>) field(helperType, "chunkType", Class.class, 16).get(helper);
            world = field(helperType, "world", Object.class, 16).get(helper);
            dimension = field(helperType, "provider", Object.class, 16).get(helper);
            storage = field(helperType, "storage", Object.class, 16).get(helper);
            cache = field(helperType, "cache", Object.class, 16).get(helper);
            require(world.getClass() == worldType && worldType == original(client ? "fd" : "dj")
                    && chunkType == original(client ? "lm" : "hi"), "genuine original World/Chunk");
            Class<?> modelType = (Class<?>) field(helperType, "modelType", Class.class, 16).get(helper);
            Object model = field(helperType, "model", Object.class, 16).get(helper);
            require(modelType == Class.forName("ChunkObservation$Model") && model.getClass() == modelType
                    && modelType.getClassLoader() == NaturalPopulationObservation.class.getClassLoader(),
                    "unchanged project-owned ChunkObservation Model");
            snapshot = method(helperType, "snapshot", Object.class, 0, int.class, int.class);
            reference = method(helperType, "ref", Object.class, 0, Object.class);
            helperGet = method(helperType, "get", Object.class, 0, String.class, Object.class);
            acquire = (Method) field(helperType, "acquire", Method.class, 16).get(helper);
            require(acquire.equals(method(original(client ? "ok" : "jb"), "c", chunkType, 1,
                    int.class, int.class)), "actual cache acquisition entry");
            generator = get("cache.c", cache);
            loaderOwner = get("cache.d", cache);
            generatorType = original(client ? "yf" : "pk");
            managerType = original(client ? "xv" : "ph");
            biomeType = original(client ? "kd" : "gs");
            Class<?> dimensionType = original(client ? "xa" : "os");
            require(dimension.getClass() == original(client ? "rh" : "kp")
                    && get("world.t", world) == dimension && get("world.v", world) == cache
                    && get("world.w", world) == storage && get("provider.a", dimension) == world
                    && get("cache.g", cache) == world && generator.getClass() == generatorType
                    && loaderOwner.getClass() == original(client ? "ld" : "hb"),
                    "actual retained original owner graph");
            manager = field(dimensionType, "b", managerType, 1).get(dimension);
            require(manager != null && manager.getClass() == managerType, "actual normal climate manager");
            generatorWorld = field(generatorType, "p", worldType, 2);
            generatorRandom = field(generatorType, "j", Random.class, 2);
            Class<?> caveBase = original(client ? "fv" : "dy");
            cave = field(generatorType, "u", caveBase, 2).get(generator);
            require(cave != null && cave.getClass() == original(client ? "sq" : "lt"),
                    "actual retained cave owner");
            caveRandom = field(caveBase, "b", Random.class, 4);
            for (String name : SCRATCH) {
                int modifiers = "defgh".contains(name) ? 0 : 2;
                scratch.put(name, field(generatorType, name, double[].class, modifiers));
            }
            Class<?> biomeArray = Array.newInstance(biomeType, 0).getClass();
            providerBiomes = field(generatorType, "v", biomeArray, 2);
            managerBiomes = field(managerType, "d", biomeArray, 1);
            for (String name : new String[] {"a", "b", "c"})
                climate.put(name, field(managerType, name, double[].class, 1));
            worldRandom = field(worldType, "r", Random.class, 1);
            drainDepth = field(worldType, "M", int.class, 2);
            chunkLight = field(chunkType, "a", boolean.class, 9);
            falling = field(original(client ? "gk" : "ei"), "a", boolean.class, 9);
            for (char label = 'a'; label <= 'm'; ++label) {
                String name = Character.toString(label);
                Object value = field(biomeType, name, biomeType, 25).get(null);
                require(value != null && biomeType.isInstance(value)
                        && biomeNames.put(value, name) == null, "distinct actual biome static " + name);
                origin(value.getClass());
                biomeStatics.put(name, ref(value));
            }
        }

        Class<?> original(String name) throws Exception {
            Class<?> type = Class.forName(name, false, loader);
            origin(type);
            return type;
        }

        void origin(Class<?> type) {
            require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                    && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                    "exact supplied archive origin: " + type.getName());
        }

        Object get(String name, Object receiver) throws Exception {
            return invoke(helperGet, helper, name, receiver);
        }

        Object ref(Object value) throws Exception {
            return invoke(reference, helper, value);
        }

        Object doubles(Object value) {
            if (value == null) return null;
            require(value.getClass() == double[].class, "actual double array");
            double[] array = (double[]) value;
            require(array.length <= MAX_ARRAY, "bounded complete double array");
            StringBuilder encoded = new StringBuilder(array.length * 16);
            for (double item : array) encoded.append(bits(item));
            return object("length", array.length, "values", encoded.toString());
        }

        Object biomes(Object array) {
            if (array == null) return null;
            require(array.getClass() == providerBiomes.getType(), "actual registered-biome array");
            int length = Array.getLength(array);
            require(length <= MAX_ARRAY, "bounded complete biome array");
            StringBuilder encoded = new StringBuilder(length);
            for (int i = 0; i < length; ++i) {
                Object value = Array.get(array, i);
                String name = value == null ? "-" : biomeNames.get(value);
                require(name != null, "actual registered biome element");
                encoded.append(name);
            }
            return object("length", length, "values", encoded.toString());
        }

        Map<String, Object> state() throws Exception {
            require(generatorWorld.get(generator) == world && get("cache.c", cache) == generator
                    && get("provider.b", dimension) == manager && get("cache.d", cache) == loaderOwner,
                    "retained numerical owners");
            Random providerRandom = (Random) generatorRandom.get(generator);
            Random actualCaveRandom = (Random) caveRandom.get(cave);
            Random actualWorldRandom = (Random) worldRandom.get(world);
            Map<String, Object> ids = object("generator", ref(generator), "world", ref(world),
                    "random", ref(providerRandom), "cave", ref(cave),
                    "cave_random", ref(actualCaveRandom),
                    "random_seed", ref(randomFields.seedOwner(providerRandom)),
                    "cave_random_seed", ref(randomFields.seedOwner(actualCaveRandom)));
            Map<String, Object> values = new LinkedHashMap<>();
            for (String name : SCRATCH) {
                Object array = scratch.get(name).get(generator);
                values.put(name, doubles(array));
                ids.put(name, ref(array));
            }
            Object grid = providerBiomes.get(generator);
            ids.put("v", ref(grid));
            Map<String, Object> providerState = object("scratch", values, "biomes", biomes(grid),
                    "generator_random", randomFields.snapshot(providerRandom),
                    "cave_random", randomFields.snapshot(actualCaveRandom), "identities", ids);
            Map<String, Object> managerIds = object("manager", ref(manager));
            Map<String, Object> managerState = new LinkedHashMap<>();
            for (String name : new String[] {"a", "b", "c"}) {
                Object array = climate.get(name).get(manager);
                managerState.put(name, doubles(array));
                managerIds.put(name, ref(array));
            }
            Object cached = managerBiomes.get(manager);
            managerState.put("d", biomes(cached));
            managerIds.put("d", ref(cached));
            managerState.put("identities", managerIds);
            Map<String, Object> aux = object("manager", managerState,
                    "world_random", randomFields.snapshot(actualWorldRandom),
                    "world_random_attribution", "per-run-constructor-input; never replaced or reseeded",
                    "world_random_identity", ref(actualWorldRandom),
                    "world_random_seed_identity", ref(randomFields.seedOwner(actualWorldRandom)),
                    "drain_depth", drainDepth.getInt(world), "chunk_static_a", chunkLight.getBoolean(null),
                    "falling", falling.getBoolean(null), "identities", object("world", ref(world),
                    "dimension", ref(dimension), "cache", ref(cache), "storage", ref(storage),
                    "loader", ref(loaderOwner)), "biome_statics", biomeStatics);
            return object("provider", providerState, "aux", aux);
        }

        Object oldSnapshot(int x, int z) throws Exception {
            return invoke(snapshot, helper, x, z);
        }

        Map<String, Object> observe(String side, String expected) throws Exception {
            Object initial = oldSnapshot(0, 0);
            List<Object> states = new ArrayList<>();
            states.add(state());
            List<Object> operations = new ArrayList<>();
            for (int index = 0; index < SCHEDULE.length; ++index) {
                int x = SCHEDULE[index][0], z = SCHEDULE[index][1];
                Object before = oldSnapshot(x, z);
                Object result = invoke(acquire, cache, x, z);
                Object resultReference = ref(result);
                Object after = oldSnapshot(x, z);
                operations.add(object("index", index, "method", "c", "coordinates", List.of(x, z),
                        "result", resultReference, "before", before, "after", after));
                states.add(state());
            }
            Object origins = field(helperType, "origins", Map.class, 16).get(helper);
            Object setup = field(helperType, "setupMetadata", Map.class, 16).get(helper);
            Map<String, Object> callbacks = object("schema", 1, "scope", "cache-callback-boundary",
                    "side", side, "scenario", "population", "seed_hex", SEED, "jar_sha256", expected,
                    "platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader(),
                    "class_origins", origins, "bindings", object("world", worldType.getName(),
                    "cache", cache.getClass().getName(), "storage", storage.getClass().getName(),
                    "loader", loaderOwner.getClass().getName(), "generator", generatorType.getName()),
                    "setup_metadata", setup, "time_api", null, "initial", initial,
                    "operations", operations, "saved", null, "external_effects",
                    "Original loader, generation and terrain-feature internals remain opaque; only cache coordinator transitions are compared.");
            return object("schema", 1, "scope", "original-game-normal-population", "side", side,
                    "seed_hex", SEED, "jar_sha256", expected, "callbacks", callbacks, "states", states);
        }
    }

    public static void main(String[] args) {
        PrintStream originalOut = System.out;
        System.setOut(System.err);
        int status = 1;
        try {
            require(args.length == 5, "jar side seedhex fresh_world_root output_json");
            require(Runtime.version().feature() >= 17, "Java17 or newer");
            boolean client = args[1].equals("client");
            require(client || args[1].equals("server"), "client or server");
            require(args[2].equals(SEED), "fixed genuine seed23 profile");
            Path jarPath = Path.of(args[0]).toRealPath();
            Path root = Path.of(args[3]).toAbsolutePath().normalize();
            Path output = Path.of(args[4]).toAbsolutePath().normalize();
            require(root.getParent() != null && output.getParent() != null
                    && Files.isDirectory(root.getParent()) && Files.isDirectory(output.getParent())
                    && !Files.exists(root, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(root)
                    && !Files.exists(output, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(output)
                    && !output.startsWith(root), "fresh owned root and separate new output");
            String expected = client
                    ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(Files.isRegularFile(jarPath) && Files.size(jarPath) <= 2 * 1024 * 1024
                    && hash(Files.readAllBytes(jarPath)).equals(expected), "pinned supplied original archive");
            RandomFields randomFields = new RandomFields();
            Class<?> helperType = Class.forName("CacheCallbacksObservation$Observer");
            require(helperType.getClassLoader() == NaturalPopulationObservation.class.getClassLoader()
                    && CacheCallbacksObservation.class.getClassLoader() == helperType.getClassLoader()
                    && ChunkObservation.class.getClassLoader() == helperType.getClassLoader(),
                    "project-owned unchanged helper classes");
            Constructor<?> constructor = helperType.getDeclaredConstructor(URL.class, ClassLoader.class,
                    Path.class, boolean.class, String.class);
            require(constructor.getModifiers() == 0, "unchanged callback Observer constructor");
            constructor.setAccessible(true);
            Method close = method(helperType, "close", void.class, 0);
            Method json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            URL jar = jarPath.toUri().toURL();
            Map<String, Object> record;
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                Object helper;
                try { helper = constructor.newInstance(jar, loader, root, client, "population"); }
                catch (InvocationTargetException failure) { throw unwrap(failure); }
                measuring = true;
                Throwable primary = null;
                try {
                    field(CacheCallbacksObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    field(ChunkObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    Observation observation = new Observation(helper, helperType, loader, jar, client, randomFields);
                    record = observation.observe(args[1], expected);
                } catch (Throwable failure) {
                    primary = failure;
                    throw failure;
                } finally {
                    try { invoke(close, helper); }
                    catch (Throwable failure) {
                        if (primary == null) throw failure;
                        primary.addSuppressed(failure);
                    }
                }
            }
            require(hash(Files.readAllBytes(jarPath)).equals(expected), "original archive preserved");
            byte[] encoded = (((String) invoke(json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
            require(encoded.length <= MAX_OUTPUT, "complete 64MiB natural-population output budget");
            Files.write(output, encoded, StandardOpenOption.CREATE_NEW);
            status = 0;
        } catch (Throwable failure) {
            String name = failure.getClass().getName();
            boolean unavailable = !measuring && (name.equals("CacheCallbacksObservation$SetupUnavailable")
                    || name.equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine natural-population setup"
                    : "FAILURE: natural-population observation");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally {
            System.setOut(originalOut);
        }
        System.exit(status);
    }
}
