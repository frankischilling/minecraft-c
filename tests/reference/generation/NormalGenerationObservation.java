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
import java.util.Arrays;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;

/** Observes sequential full normal generation without replacing original game state. */
public final class NormalGenerationObservation {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final int MAX_REGIONS = 32768;
    private static final int[][] COORDINATES = {{1, 0}, {0, 0}, {1, 0}, {-1, -1}, {32768, 0},
            {2000000, 0}, {-2000000, 0}, {Integer.MIN_VALUE + 8, 0}, {Integer.MAX_VALUE - 9, 0}};
    private static final String[] CHANNELS = {"k", "l", "m", "n", "o", "a", "b", "c"};
    private static final int[] LAYERS = {16, 16, 8, 4, 4, 10, 16, 8};
    private static final class OriginalDependencyUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        OriginalDependencyUnavailable(String message, LinkageError cause) { super(message, cause); }
    }

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private static Map<String, Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }
    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers) throws Exception {
        Field result = owner.getDeclaredField(name);
        require(result.getType() == type && result.getModifiers() == modifiers,
                "field descriptor/modifiers: " + owner.getName() + "." + name);
        result.setAccessible(true);
        return result;
    }
    private static Method method(Class<?> owner, String name, Class<?> result, int modifiers,
                                 Class<?>... arguments) throws Exception {
        Method value = owner.getDeclaredMethod(name, arguments);
        require(value.getReturnType() == result && value.getModifiers() == modifiers,
                "method descriptor/modifiers: " + owner.getName() + "." + name);
        value.setAccessible(true);
        return value;
    }
    private static Exception unwrap(InvocationTargetException failure) throws Exception {
        Throwable cause = failure.getCause();
        if (cause instanceof Exception) return (Exception) cause;
        if (cause instanceof Error) throw (Error) cause;
        return failure;
    }
    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) { throw unwrap(failure); }
    }
    private static boolean dependencyCause(Throwable failure) {
        for (int depth = 0; failure != null && depth < 16; ++depth, failure = failure.getCause())
            if (failure instanceof LinkageError) return true;
        return false;
    }
    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    private static void origin(Class<?> type, ClassLoader loader, URL jar, Map<String, Object> origins) {
        require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                "actual supplied archive class: " + type.getName());
        origins.put(type.getName(), jar.toString());
    }
    private static Object doubles(double[] array) {
        if (array == null) return null;
        StringBuilder result = new StringBuilder(array.length * 16);
        for (double value : array) result.append(String.format("%016x", Double.doubleToRawLongBits(value)));
        return object("length", array.length, "values", result.toString());
    }
    private static Map<String, Object> random(Random source) throws Exception {
        require(source != null && source.getClass() == Random.class, "actual java.util.Random");
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream stream = new ObjectOutputStream(bytes)) { stream.writeObject(source); }
        byte[] serialized = bytes.toByteArray();
        require(serialized.length <= 4096, "bounded Random serialization");
        Random copy;
        try (ObjectInputStream stream = new ObjectInputStream(new ByteArrayInputStream(serialized))) {
            copy = (Random) stream.readObject();
            require(stream.read() == -1, "complete Random serialization");
        }
        List<String> continuation = new ArrayList<>();
        for (int i = 0; i < 8; ++i) continuation.add(String.format("%016x", copy.nextLong()));
        return object("serialized_hex", HexFormat.of().formatHex(serialized), "next_longs", continuation);
    }

    // These helpers mark exact debugger boundaries and do not call the world.
    private static void begin(Object world, Object generator, int index) {
        System.err.println("NORMAL_GENERATION_BEGIN " + index);
    }
    private static void end(Object returnedChunk, int index) { System.err.println("NORMAL_GENERATION_END " + index); }

    private static final class Model {
        final Class<?> shared, modelType, worldType, chunkType, generatorType, managerType, biomeType;
        final Class<?> octaveType, layerType, caveBaseType, caveType;
        final Object model, world, cache, generator, manager, cave;
        final Method get, snapshot, worldSnapshot, local, identities, reference, json, generate;
        final Map<String, Field> fields = new LinkedHashMap<>();
        final IdentityHashMap<Object, String> biomeNames = new IdentityHashMap<>();
        final List<Object> returned = new ArrayList<>();
        final List<Object[]> returnedIdentities = new ArrayList<>();
        final List<Map<?, ?>> returnedLocal = new ArrayList<>();
        final Object initialManagerD;
        final Object initialI;
        final int[][] initialIRows;
        final Object initialW;

        Model(Object model, Class<?> shared, Class<?> modelType, ClassLoader loader, boolean client,
              long seed, URL jar, Map<String, Object> origins) throws Exception {
            this.model = model; this.shared = shared; this.modelType = modelType;
            worldType = (Class<?>) field(modelType, "worldType", Class.class, 16).get(model);
            chunkType = (Class<?>) field(modelType, "chunkType", Class.class, 16).get(model);
            generatorType = Class.forName(client ? "yf" : "pk", true, loader);
            managerType = Class.forName(client ? "xv" : "ph", true, loader);
            biomeType = Class.forName(client ? "kd" : "gs", true, loader);
            octaveType = Class.forName(client ? "uf" : "mp", true, loader);
            layerType = Class.forName(client ? "am" : "ab", true, loader);
            caveBaseType = Class.forName(client ? "fv" : "dy", true, loader);
            caveType = Class.forName(client ? "sq" : "lt", true, loader);
            for (Class<?> type : new Class<?>[] {generatorType, managerType, biomeType, octaveType, layerType,
                    caveBaseType, caveType}) origin(type, loader, jar, origins);
            get = method(modelType, "get", Object.class, 0, String.class, Object.class);
            snapshot = method(modelType, "snapshot", Map.class, 0, int.class, int.class, int.class);
            worldSnapshot = method(modelType, "worldSnapshot", Map.class, 0, int.class, int.class, int.class);
            local = method(modelType, "local", Map.class, 0);
            identities = method(modelType, "identities", Object[].class, 0, Object.class);
            reference = method(modelType, "reference", Map.class, 0, Object.class);
            json = method(shared, "json", String.class, 10, Object.class);
            world = field(modelType, "world", Object.class, 0).get(model);
            cache = field(modelType, "cache", Object.class, 0).get(model);
            manager = invoke(method(worldType, "a", managerType, 1), world);
            Constructor<?> constructor = generatorType.getDeclaredConstructor(worldType, long.class);
            require(constructor.getModifiers() == 1, "public genuine generator constructor");
            try { generator = constructor.newInstance(world, seed); }
            catch (InvocationTargetException failure) { throw unwrap(failure); }
            generate = method(generatorType, "b", chunkType, 1, int.class, int.class);
            bind("j", Random.class, 2); bind("p", worldType, 2); bind("u", caveBaseType, 2);
            for (String name : new String[] {"q", "r", "s", "t", "w"}) bind(name, double[].class, 2);
            for (String name : new String[] {"d", "e", "f", "g", "h"}) bind(name, double[].class, 0);
            bind("v", Array.newInstance(biomeType, 0).getClass(), 2); bind("i", int[][].class, 0);
            for (int i = 0; i < CHANNELS.length; ++i) bind(CHANNELS[i], octaveType, i < 5 ? 2 : 1);
            cave = fields.get("u").get(generator);
            require(cave.getClass() == caveType && fields.get("p").get(generator) == world, "genuine retained cave/world");
            for (char name = 'a'; name <= 'm'; ++name) {
                Field entry = field(biomeType, Character.toString(name), biomeType, 25);
                Object biome = entry.get(null);
                require(biome != null && biomeNames.put(biome, Character.toString(name)) == null, "distinct original biome identities");
                origin(biome.getClass(), loader, jar, origins);
            }
            initialManagerD = field(managerType, "d", Array.newInstance(biomeType, 0).getClass(), 1).get(manager);
            initialI = fields.get("i").get(generator);
            int[][] rows = (int[][]) initialI;
            require(rows.length == 32, "fresh unused matrix rows");
            initialIRows = rows.clone();
            for (int[] row : rows) require(row != null && row.length == 32, "fresh unused matrix columns");
            initialW = fields.get("w").get(generator);
            require(initialW == null && fields.get("q").get(generator) == null && fields.get("v").get(generator) == null,
                    "fresh generator nullable scratch");
            for (String name : new String[] {"d", "e", "f", "g", "h"})
                require(fields.get(name).get(generator) == null, "fresh density scratch " + name);
            IdentityHashMap<Object, Boolean> distinct = new IdentityHashMap<>();
            for (String name : new String[] {"r", "s", "t"}) {
                double[] values = (double[]) fields.get(name).get(generator);
                require(values.length == 256 && distinct.put(values, true) == null, "fresh distinct surface scratch");
                for (double value : values) require(Double.doubleToRawLongBits(value) == 0, "fresh positive-zero surface scratch");
            }
        }
        void bind(String name, Class<?> type, int modifiers) throws Exception {
            fields.put(name, field(generatorType, name, type, modifiers));
        }
        Map<?, ?> ref(Object value) throws Exception { return (Map<?, ?>) invoke(reference, model, value); }
        Object biomes(Object array) {
            if (array == null) return null;
            int length = Array.getLength(array);
            require(length <= 32768, "bounded biome array");
            StringBuilder values = new StringBuilder(length);
            for (int i = 0; i < length; ++i) {
                Object value = Array.get(array, i);
                String name = value == null ? "-" : biomeNames.get(value);
                require(name != null, "observed registered biome identity"); values.append(name);
            }
            return object("length", length, "values", values.toString());
        }
        String selectedMaterials() throws Exception {
            Object grid = fields.get("v").get(generator);
            if (grid == null) return null;
            byte[] values = new byte[Array.getLength(grid) * 2];
            Field top = field(biomeType, "p", byte.class, 1), filler = field(biomeType, "q", byte.class, 1);
            for (int i = 0; i < values.length / 2; ++i) {
                Object biome = Array.get(grid, i); require(biome != null, "selected material biome");
                values[i * 2] = top.getByte(biome); values[i * 2 + 1] = filler.getByte(biome);
            }
            return HexFormat.of().formatHex(values);
        }
        List<Object> materialTable() throws Exception {
            List<Object> result = new ArrayList<>();
            Field top = field(biomeType, "p", byte.class, 1), filler = field(biomeType, "q", byte.class, 1);
            for (char name = 'a'; name <= 'm'; ++name) {
                Object biome = field(biomeType, Character.toString(name), biomeType, 25).get(null);
                result.add(object("field", Character.toString(name), "top", top.getByte(biome) & 255,
                        "filler", filler.getByte(biome) & 255));
            }
            return result;
        }
        Map<String, Object> noise() throws Exception {
            Map<String, Object> result = new LinkedHashMap<>();
            Class<?> layersType = Array.newInstance(layerType, 0).getClass();
            for (int channel = 0; channel < CHANNELS.length; ++channel) {
                Object octave = fields.get(CHANNELS[channel]).get(generator);
                require(octave.getClass() == octaveType, "actual improved octave class");
                Object[] layers = (Object[]) field(octaveType, "a", layersType, 2).get(octave);
                require(layers.length == LAYERS[channel] && field(octaveType, "b", int.class, 2).getInt(octave) == layers.length,
                        "complete original channel layers");
                List<Object> entries = new ArrayList<>();
                for (Object layer : layers) {
                    require(layer.getClass() == layerType, "actual improved layer class");
                    List<String> offsets = new ArrayList<>();
                    for (String name : new String[] {"a", "b", "c"})
                        offsets.add(String.format("%016x", Double.doubleToRawLongBits(field(layerType, name, double.class, 1).getDouble(layer))));
                    int[] permutation = (int[]) field(layerType, "d", int[].class, 2).get(layer);
                    require(permutation.length == 512, "full permutation shape");
                    List<Integer> values = new ArrayList<>();
                    for (int value : permutation) { require(value >= 0 && value < 256, "permutation byte domain"); values.add(value); }
                    entries.add(object("offsets", offsets, "permutation", values));
                }
                result.put(CHANNELS[channel], entries);
            }
            return result;
        }
        Map<String, Object> generator(boolean constructor) throws Exception {
            Map<String, Object> scratch = new LinkedHashMap<>(), ids = new LinkedHashMap<>();
            for (String name : new String[] {"q", "r", "s", "t", "d", "e", "f", "g", "h"}) {
                double[] values = (double[]) fields.get(name).get(generator);
                scratch.put(name, doubles(values)); ids.put(name, ref(values));
            }
            Object grid = fields.get("v").get(generator), matrix = fields.get("i").get(generator);
            Object unused = fields.get("w").get(generator);
            require(matrix == initialI && unused == initialW, "unused matrix/w ownership retained");
            List<Object> rows = new ArrayList<>(), rowIds = new ArrayList<>();
            int[][] actual = (int[][]) matrix;
            for (int i = 0; i < actual.length; ++i) {
                require(actual[i] == initialIRows[i], "unused matrix row identity");
                List<Integer> values = new ArrayList<>();
                for (int value : actual[i]) { require(value == 0, "unused matrix unchanged"); values.add(value); }
                rows.add(values); rowIds.add(ref(actual[i]));
            }
            ids.put("v", ref(grid)); ids.put("i", ref(matrix)); ids.put("i_rows", rowIds); ids.put("w", ref(unused));
            ids.put("generator", ref(generator)); ids.put("world", ref(world)); ids.put("random", ref(fields.get("j").get(generator)));
            ids.put("cave", ref(cave)); ids.put("cave_random", ref(field(caveBaseType, "b", Random.class, 4).get(cave)));
            return object("noise", noise(), "scratch", scratch, "biomes", biomes(grid), "materials", selectedMaterials(),
                    "generator_random", random((Random) fields.get("j").get(generator)),
                    "cave_random", constructor ? object("omitted", "unconsumed default-seeded state before public cave reseed")
                            : random((Random) field(caveBaseType, "b", Random.class, 4).get(cave)),
                    "cave_range", field(caveBaseType, "a", int.class, 4).getInt(cave),
                    "original_only", object("i", rows, "w", doubles((double[]) unused)), "identities", ids);
        }
        Map<String, Object> manager() throws Exception {
            Map<String, Object> result = new LinkedHashMap<>(), ids = new LinkedHashMap<>();
            for (String name : new String[] {"a", "b", "c"}) {
                Object array = field(managerType, name, double[].class, 1).get(manager);
                result.put(name, doubles((double[]) array)); ids.put(name, ref(array));
            }
            Object cached = field(managerType, "d", Array.newInstance(biomeType, 0).getClass(), 1).get(manager);
            require(cached == initialManagerD, "direct generation preserves manager.d identity");
            result.put("d", biomes(cached)); ids.put("d", ref(cached)); ids.put("manager", ref(manager));
            result.put("identities", ids); return result;
        }
        void select(int index) throws Exception {
            Object chunk = returned.get(index);
            field(modelType, "chunk", Object.class, 0).set(model, chunk);
            field(modelType, "blocks", byte[].class, 0).set(model, invoke(get, model, "chunk.b", chunk));
            field(modelType, "localIdentities", Object[].class, 0).set(model, returnedIdentities.get(index));
        }
        List<Object> retained() throws Exception {
            List<Object> result = new ArrayList<>();
            for (int i = 0; i < returned.size(); ++i) {
                select(i); Map<?, ?> observed = (Map<?, ?>) invoke(local, model);
                require(invoke(json, null, observed).equals(invoke(json, null, returnedLocal.get(i))), "complete prior returned local state retained");
                result.add(object("index", i, "local", observed));
            }
            return result;
        }
    }

    public static void main(String[] args) {
        PrintStream output = System.out; System.setOut(System.err);
        boolean measuring = false; Class<?> unavailableType = null; int status = 1;
        try {
            require(args.length == 5, "arguments: jar side seed_hex new_world_root snapshots_json");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            require(args[2].matches("[0-9a-f]{16}"), "seed hex");
            long seed = Long.parseUnsignedLong(args[2], 16);
            require(seed == 23 || seed == 24 || seed == 26 || seed == 51, "qualifying fixed world seed");
            Path jarPath = Path.of(args[0]).toRealPath(), destination = Path.of(args[4]).toAbsolutePath().normalize();
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(Files.isRegularFile(jarPath) && hash(Files.readAllBytes(jarPath)).equals(expected), "pinned supplied original archive");
            require(!Files.exists(destination, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(destination)
                    && Files.isDirectory(destination.getParent()), "new bounded snapshot output");
            URL jar = jarPath.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                Class<?> shared = Class.forName("ChunkObservation"), modelType = Class.forName("ChunkObservation$Model");
                unavailableType = Class.forName("ChunkObservation$SetupUnavailable");
                Constructor<?> constructor = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
                require(constructor.getModifiers() == 0, "shared model constructor"); constructor.setAccessible(true);
                Object original;
                try { original = constructor.newInstance(loader, client); }
                catch (InvocationTargetException failure) {
                    if (failure.getCause() instanceof LinkageError)
                        throw new OriginalDependencyUnavailable("original class initialization", (LinkageError) failure.getCause());
                    throw unwrap(failure);
                }
                invoke(method(modelType, "setup", void.class, 0, Path.class, long.class), original,
                        Path.of(args[3]).toAbsolutePath().normalize(), seed);
                Object world = field(modelType, "world", Object.class, 0).get(original);
                Object cache = field(modelType, "cache", Object.class, 0).get(original);
                Class<?> worldType = (Class<?>) field(modelType, "worldType", Class.class, 16).get(original);
                Class<?> cacheType = (Class<?>) field(modelType, "cacheType", Class.class, 16).get(original);
                Class<?> chunkType = (Class<?>) field(modelType, "chunkType", Class.class, 16).get(original);
                Method get = method(modelType, "get", Object.class, 0, String.class, Object.class);
                Map<?, ?> map = (Map<?, ?>) invoke(get, original, "cache.e", cache);
                List<?> insertions = (List<?>) invoke(get, original, "cache.f", cache);
                Set<?> pending = (Set<?>) invoke(get, original, "cache.a", cache);
                List<?> queue = (List<?>) invoke(get, original, "world.C", world);
                Object metadata = field(worldType, "x", Class.forName(client ? "ei" : "ct", true, loader), 4).get(world);
                List<Integer> spawn = new ArrayList<>();
                for (String name : new String[] {"c", "d", "e"}) spawn.add((Integer) invoke(method(metadata.getClass(), name, int.class, 1), metadata));
                Map<String, Object> setupMetadata = object("spawn", spawn, "constructor_map_count", map.size(),
                        "constructor_insertion_count", insertions.size(), "constructor_pending_count", pending.size(),
                        "constructor_queue_count", queue.size());
                if (!client) require(spawn.equals(Arrays.asList(0, 64, 0)), "qualifying actual server spawn");
                else if (map.isEmpty()) invoke(method(cacheType, "c", chunkType, 1, int.class, int.class), cache, 0, 0);
                Object cachedOrigin = map.get(0);
                require(map.size() == 1 && cachedOrigin != null && cachedOrigin.getClass() == chunkType
                        && (int) invoke(get, original, "chunk.j", cachedOrigin) == 0 && (int) invoke(get, original, "chunk.k", cachedOrigin) == 0
                        && insertions.size() == 1 && insertions.get(0) == cachedOrigin && pending.isEmpty() && queue.isEmpty(),
                        "genuine matched origin cache baseline");
                Map<String, Object> origins = new LinkedHashMap<>();
                for (String name : new String[] {"worldType", "providerType", "storageType", "normalType", "chunkType", "nibbleType", "cacheType", "kindType", "regionType"})
                    origin((Class<?>) field(modelType, name, Class.class, 16).get(original), loader, jar, origins);
                for (String name : client ? new String[] {"ei", "cl", "bf", "uu", "yy", "wy"}
                        : new String[] {"ct", "bl", "an", "na", "qa", "dq"})
                    origin(Class.forName(name, true, loader), loader, jar, origins);
                Model observed = new Model(original, shared, modelType, loader, client, seed, jar, origins);
                Object setupWorld = invoke(observed.worldSnapshot, original, 0, 0, MAX_REGIONS);
                Map<String, Object> initial = object("generator", observed.generator(true), "manager", observed.manager(), "materials", observed.materialTable());
                String initialNoise = (String) invoke(observed.json, null, ((Map<?, ?>) initial.get("generator")).get("noise"));
                List<Object> calls = new ArrayList<>();
                measuring = true; field(shared, "measuring", boolean.class, 10).setBoolean(null, true);
                for (int index = 0; index < COORDINATES.length; ++index) {
                    int x = COORDINATES[index][0], z = COORDINATES[index][1];
                    List<Object> previous = observed.retained();
                    Object before = invoke(observed.worldSnapshot, original, x, z, MAX_REGIONS);
                    begin(world, observed.generator, index);
                    Object chunk = invoke(observed.generate, observed.generator, x, z);
                    end(chunk, index);
                    require(chunk != null && chunk.getClass() == chunkType && !observed.returned.contains(chunk)
                            && chunk != cachedOrigin, "fresh exact unpublished returned chunk identity");
                    require(!map.containsValue(chunk) && !insertions.contains(chunk), "direct generation does not publish returned chunk");
                    observed.returned.add(chunk); observed.returnedIdentities.add((Object[]) invoke(observed.identities, original, chunk));
                    observed.select(index); Map<?, ?> localState = (Map<?, ?>) invoke(observed.local, original);
                    observed.returnedLocal.add(localState);
                    Map<?, ?> after = (Map<?, ?>) invoke(observed.snapshot, original, x, z, MAX_REGIONS);
                    Map<String, Object> generator = observed.generator(false);
                    require(initialNoise.equals(invoke(observed.json, null, generator.get("noise"))), "all original noise82 state retained");
                    List<Object> retained = observed.retained(); observed.select(index);
                    Map<String, Object> complete = new LinkedHashMap<>();
                    for (Map.Entry<?, ?> entry : after.entrySet()) complete.put((String) entry.getKey(), entry.getValue());
                    complete.put("generator", generator); complete.put("manager", observed.manager());
                    complete.put("materials", observed.materialTable()); complete.put("retained", retained);
                    calls.add(object("index", index, "coordinates", Arrays.asList(x, z), "before_world", before,
                            "prior_retained", previous, "after", complete));
                }
                int[] opacity = (int[]) field(modelType, "opacity", int[].class, 16).get(original);
                int[] initialOpacity = (int[]) field(modelType, "initialOpacity", int[].class, 16).get(original);
                Field opacityField = (Field) field(modelType, "opacityField", Field.class, 16).get(original);
                Map<String, Object> record = object("schema", 1, "side", args[1], "seed_hex", args[2], "jar_sha256", expected,
                        "bindings", object("world", worldType.getName(), "chunk", chunkType.getName(), "generator", observed.generatorType.getName(),
                                "generate", "b", "finish", client ? "c" : "b"), "class_origins", origins,
                        "platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader(), "setup_metadata", setupMetadata,
                        "setup_world", setupWorld, "constructor", initial, "calls", calls,
                        "controls", object("noise82_unchanged", true, "manager_d_unchanged", true, "prior_returns_unchanged", true,
                                "fresh_unpublished_returns", true, "opacity_unchanged", opacityField.get(null) == opacity && Arrays.equals(opacity, initialOpacity)));
                byte[] encoded = (((String) invoke(observed.json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "complete normal generation snapshot budget");
                require(hash(Files.readAllBytes(jarPath)).equals(expected), "original archive preserved");
                Files.write(destination, encoded, StandardOpenOption.CREATE_NEW); status = 0;
            }
        } catch (Throwable failure) {
            boolean unavailable = !measuring && (failure instanceof OriginalDependencyUnavailable
                    || unavailableType != null && unavailableType.isInstance(failure)
                    && (failure.getCause() == null || dependencyCause(failure)));
            System.err.println(unavailable ? "UNAVAILABLE: genuine normal generation setup" : "FAILURE: normal generation observation");
            failure.printStackTrace(System.err); if (unavailable) status = 77;
        } finally { System.setOut(output); }
        System.exit(status);
    }
}
