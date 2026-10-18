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

/** Complete observations of genuine persistent-client / dedicated-constructor acquisition.
 * No original game fields are assigned; only private project observer bindings change.
 */
public final class CacheAcquisitionObservation {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static final String[] CHANNELS = {"k", "l", "m", "n", "o", "a", "b", "c"};
    private static final int[] LAYERS = {16, 16, 8, 4, 4, 10, 16, 8};
    private static boolean measuring;
    private static final class SetupUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupUnavailable(String message, Throwable cause) { super(message, cause); }
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


    // Ordinary helper boundaries: controller only reads target state.
    private static void begin(Object world, Object cache, int index) { }
    private static void end(Object result, int index) { }
    private static void identity(Object value, String reference) { }
    private static final class Retained {
        final Class<?> shared, modelType, worldType, chunkType, generatorType, managerType, biomeType;
        final Class<?> octaveType, layerType, caveBaseType, caveType;
        final Object model, world, cache, generator, manager, cave;
        final Method get, snapshot, worldSnapshot, local, identities, reference, json, generate;
        final Map<String, Field> fields = new LinkedHashMap<>();
        final IdentityHashMap<Object, String> biomeNames = new IdentityHashMap<>();
        final Object initialManagerD;
        final Object initialI;
        final int[][] initialIRows;
        final Object initialW;

        Retained(Object model, Class<?> shared, Class<?> modelType, ClassLoader loader, boolean client,
                 Object actualGenerator, URL jar, Map<String, Object> origins) throws Exception {
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
            generator = actualGenerator;
            require(generator != null && generator.getClass() == generatorType, "actual retained normal provider");
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
        Map<String, Object> generator() throws Exception {
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
                    "cave_random", random((Random) field(caveBaseType, "b", Random.class, 4).get(cave)),
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
            result.put("d", biomes(cached)); ids.put("d", ref(cached)); ids.put("manager", ref(manager));
            result.put("identities", ids); return result;
        }
    }

    private static Object blob(byte[] values) {
        return values == null ? null : object("length", values.length, "values", HexFormat.of().formatHex(values));
    }
    private static Constructor<?> constructor(Class<?> type, Class<?>... args) throws Exception {
        Constructor<?> result = type.getDeclaredConstructor(args);
        require(result.getModifiers() == 1, "exact public original constructor " + type.getName());
        return result;
    }
    private static Object construct(Constructor<?> constructor, Object... args) throws Exception {
        try { return constructor.newInstance(args); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (!measuring && (cause instanceof Exception || cause instanceof LinkageError))
                throw new SetupUnavailable("genuine original constructor failed", cause);
            throw unwrap(failure);
        } catch (LinkageError failure) { throw new SetupUnavailable("original constructor dependency", failure); }
    }
    private static final class Observer {
        final boolean client;
        final URL jar;
        final ClassLoader loader;
        final Class<?> modelType, worldType, chunkType, cacheType, infoType;
        final Object model, world, provider, storage, cache, owner, saveFormat, constructorOrigin;
        final Method get, worldSnapshot, reference, json, acquire, getChunk, markDrop, close, time;
        final Map<String, Object> origins = new LinkedHashMap<>();
        final Map<String, Class<?>> classes = new LinkedHashMap<>();
        final IdentityHashMap<Object, Boolean> tagged = new IdentityHashMap<>();
        final Map<String, Object> setupMetadata;
        final Retained retained;
        Class<?> type(String name) throws Exception {
            Class<?> result = Class.forName(name, true, loader); origin(result, loader, jar, origins);
            classes.put(name, result); return result;
        }
        void observerReference(String name, Object value) throws Exception {
            require(Arrays.asList("world", "provider", "storage", "cache").contains(name), "only observer reference bindings");
            require(modelType.getClassLoader() != loader, "helper-owned observer");
            field(modelType, name, Object.class, 0).set(model, value);
        }
        Object get(String name, Object receiver) throws Exception { return invoke(get, model, name, receiver); }
        Object ref(Object value) throws Exception {
            Object result = invoke(reference, model, value);
            if (value != null && !tagged.containsKey(value)) {
                require(tagged.size() < 1024, "bounded first-encounter identity tags");
                tagged.put(value, true);
                identity(value, (String) ((Map<?, ?>) result).get("ref"));
            }
            return result;
        }
        Observer(URL jar, ClassLoader loader, Path root, boolean client, long seed) throws Exception {
            this.jar = jar; this.loader = loader; this.client = client;
            modelType = Class.forName("ChunkObservation$Model");
            Constructor<?> shared = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
            require(shared.getModifiers() == 0, "shared model constructor"); shared.setAccessible(true);
            try { model = shared.newInstance(loader, client); }
            catch (InvocationTargetException failure) {
                if (failure.getCause() instanceof LinkageError)
                    throw new SetupUnavailable("original initialization dependency", failure.getCause());
                throw unwrap(failure);
            }
            get = method(modelType, "get", Object.class, 0, String.class, Object.class);
            worldSnapshot = method(modelType, "worldSnapshot", Map.class, 0, int.class, int.class, int.class);
            reference = method(modelType, "reference", Map.class, 0, Object.class);
            json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            worldType = type(client ? "fd" : "dj"); chunkType = type(client ? "lm" : "hi");
            cacheType = type(client ? "ok" : "he"); infoType = type(client ? "ei" : "ct");
            Class<?> storageType = type(client ? "wt" : "om"), providerType = type(client ? "xa" : "os");
            for (String name : client ? new String[] {"hi", "mx", "fm", "ld", "bf", "rh", "yf", "rj", "qj", "yy", "wi", "st", "eb", "uu", "li"}
                    : new String[] {"net.minecraft.server.MinecraftServer", "bt", "ie", "dq", "dp", "hb", "an", "kp", "pk", "kq", "jx", "qa", "ob", "lv", "co", "na", "hg"}) type(name);
            acquire = method(cacheType, "c", chunkType, 1, int.class, int.class);
            getChunk = method(cacheType, "b", chunkType, 1, int.class, int.class);
            markDrop = client ? null : method(cacheType, "d", void.class, 1, int.class, int.class);
            time = method(worldType, client ? "t" : "m", long.class, 1);
            Files.createDirectory(root);
            if (client) {
                owner = null; saveFormat = construct(constructor(classes.get("hi"), java.io.File.class), root.toFile());
                storage = invoke(method(classes.get("hi"), "a", storageType, 1, String.class, boolean.class), saveFormat, "world", false);
                require(storage.getClass() == classes.get("mx"), "actual persistent region save handler");
                world = construct(constructor(worldType, storageType, String.class, long.class), storage, "cache-acquisition-reference", seed);
                close = method(classes.get("hi"), "c", void.class, 1);
            } else {
                owner = construct(constructor(classes.get("net.minecraft.server.MinecraftServer")));
                saveFormat = null;
                storage = construct(constructor(classes.get("ie"), java.io.File.class, String.class, boolean.class), root.toFile(), "world", true);
                world = construct(constructor(classes.get("dp"), owner.getClass(), storageType, String.class, int.class, long.class),
                        owner, storage, "cache-acquisition-reference", 0, seed);
                close = method(classes.get("ie"), "e", void.class, 1);
                // Rebind only project-owned reflection descriptors. he uses one
                // leading force flag before the shared b..h cache fields.
                Method bind = method(modelType, "bind", Field.class, 0, String.class, Class.class, String.class, Class.class, int.class);
                String[] roles = {"a", "b", "c", "d", "e", "f", "g"};
                Class<?>[] expected = {Set.class, chunkType, type("bl"), classes.get("an"), Map.class, List.class, classes.get("dp")};
                for (int i = 0; i < roles.length; ++i)
                    invoke(bind, model, "cache." + roles[i], cacheType, Character.toString((char) ('b' + i)), expected[i], 2);
                field(modelType, "presenceMethod", Method.class, 16).set(model,
                        method(cacheType, "a", boolean.class, 1, int.class, int.class));
            }
            provider = get("world.t", world); cache = get("world.v", world);
            for (var pair : object("world", world, "provider", provider, "storage", storage, "cache", cache).entrySet()) observerReference(pair.getKey(), pair.getValue());
            require(world.getClass() == (client ? worldType : classes.get("dp")) && cache.getClass() == cacheType
                    && get("cache.g", cache) == world && get("provider.a", provider) == world
                    && get("world.w", world) == storage && get("cache.d", cache).getClass() == classes.get(client ? "ld" : "hb")
                    && get("cache.c", cache).getClass() == classes.get(client ? "yf" : "pk"), "actual retained world/provider/loader/cache route");
            require((long) invoke(method(worldType, client ? "s" : "l", long.class, 1), world) == seed, "actual seed");
            Object info = field(worldType, "x", infoType, 4).get(world); List<Integer> spawn = new ArrayList<>();
            for (String name : new String[] {"c", "d", "e"}) spawn.add((Integer) invoke(method(infoType, name, int.class, 1), info));
            require(spawn.equals(Arrays.asList(0, 64, 0)), "qualifying fixed origin setup");
            Map<?, ?> map = (Map<?, ?>) get("cache.e", cache); List<?> list = (List<?>) get("cache.f", cache);
            constructorOrigin = map.get(0);
            setupMetadata = object("spawn", spawn, "constructor_map_count", map.size(), "constructor_insertion_count", list.size(),
                    "constructor_pending_count", ((Set<?>) get("cache.a", cache)).size(), "constructor_queue_count", ((List<?>) get("world.C", world)).size(),
                    "constructor_origin", ref(constructorOrigin), "client_warmup", client && map.isEmpty());
            if (client && map.isEmpty()) invoke(acquire, cache, 0, 0);
            Object origin = map.get(0);
            require(map.size() == 1 && origin != null && origin.getClass() == chunkType
                    && (int) get("chunk.j", origin) == 0 && (int) get("chunk.k", origin) == 0
                    && list.size() == 1 && list.get(0) == origin && ((Set<?>) get("cache.a", cache)).isEmpty()
                    && ((List<?>) get("world.C", world)).isEmpty(), "matched ordinary origin cache baseline");
            require(constructorOrigin == null || constructorOrigin == origin, "constructor-cached origin identity retained");
            setupMetadata.put("baseline_origin", ref(origin));
            setupMetadata.put("constructor_origin_retained", constructorOrigin == null ? null : constructorOrigin == origin);
            setupMetadata.put("world_force_load", field(worldType, "y", boolean.class, 1).get(world));
            setupMetadata.put("provider_force_load", client ? null : field(cacheType, "a", boolean.class, 1).get(cache));
            if (!client) {
                require(field(classes.get("dp"), "C", cacheType, 1).get(world) == cache
                        && field(classes.get("dp"), "F", owner.getClass(), 2).get(world) == owner, "actual dedicated owner relationships");
                for (String name : new String[] {"c", "d", "e", "f", "p"})
                    require(field(owner.getClass(), name, owner.getClass().getDeclaredField(name).getType(), name.equals("p") ? 2 : 1).get(owner) == null, "owner service not started " + name);
            }
            retained = new Retained(model, ChunkObservation.class, modelType, loader, client, get("cache.c", cache), jar, origins);
        }
        Object fullChunk(Object value) throws Exception {
            require(value != null && value.getClass() == chunkType, "actual ordinary cached chunk");
            Map<String, Object> flags = new LinkedHashMap<>();
            for (String name : new String[] {"c", "n", "o", "p", "q"}) flags.put(name, get("chunk." + name, value));
            Map<?, ?> tiles = (Map<?, ?>) get("chunk.l", value);
            require(tiles.isEmpty(), "finite experiment has no tile objects");
            Object[] entities = (Object[]) get("chunk.m", value); require(entities.length == 8, "eight actual entity slices");
            List<Object> slices = new ArrayList<>(), identities = new ArrayList<>();
            for (Object slice : entities) {
                require(slice instanceof List<?> && ((List<?>) slice).isEmpty(), "finite experiment has empty entity slices");
                slices.add(object("object", ref(slice), "size", 0));
            }
            byte[] blocks = (byte[]) get("chunk.b", value), heights = (byte[]) get("chunk.h", value);
            require(blocks.length == 32768 && heights.length == 256, "complete ordinary chunk arrays");
            identities.add(ref(blocks));
            Map<String, Object> data = new LinkedHashMap<>(); data.put("blocks", blob(blocks));
            String[] names = {"e", "f", "g"}, roles = {"metadata", "skylight", "block_light"};
            for (int i = 0; i < names.length; ++i) {
                Object wrapper = get("chunk." + names[i], value); byte[] array = (byte[]) get("nibble.a", wrapper);
                require(array.length == 16384, "complete actual packed array");
                data.put(roles[i], blob(array)); identities.add(ref(wrapper)); identities.add(ref(array));
            }
            data.put("heightmap", blob(heights)); data.put("minimum_height", get("chunk.i", value)); identities.add(ref(heights));
            return object("object", ref(value), "world", ref(get("chunk.d", value)),
                    "coordinates", Arrays.asList(get("chunk.j", value), get("chunk.k", value)), "data", data,
                    "flags", flags, "time", String.format(java.util.Locale.ROOT, "%016x", (long) get("chunk.r", value)),
                    "array_references", identities, "tile_map", object("object", ref(tiles), "size", 0), "entity_slices", slices);
        }

        Object snapshot(int x, int z) throws Exception {
            Map<?, ?> map = (Map<?, ?>) get("cache.e", cache); List<?> list = (List<?>) get("cache.f", cache);
            require(map.size() <= 64 && list.size() <= 64 && ((Set<?>) get("cache.a", cache)).size() <= 64
                    && ((List<?>) get("world.C", world)).size() <= 32768, "bounded complete acquisition snapshot");
            ref(world); ref(cache); ref(get("cache.b", cache)); ref(get("cache.c", cache)); ref(get("cache.d", cache));
            ref(map); ref(list); ref(get("cache.a", cache)); ref(get("world.C", world));
            Object worldState = invoke(worldSnapshot, model, x, z, 32768);
            int[] opacity = (int[]) field(modelType, "opacity", int[].class, 16).get(model);
            int[] initialOpacity = (int[]) field(modelType, "initialOpacity", int[].class, 16).get(model);
            Field registry = (Field) field(modelType, "opacityField", Field.class, 16).get(model);
            boolean opacityIdentity = registry.get(null) == opacity, opacityValues = Arrays.equals(opacity, initialOpacity);
            require(opacityIdentity && opacityValues, "actual opacity registry identity and values retained");
            byte[] opacityBytes = (byte[]) invoke(method(modelType, "opacityBytes", byte[].class, 0), model);
            IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>(); List<Object> full = new ArrayList<>();
            List<Object> values = new ArrayList<>(map.values()); values.addAll(list); values.add(get("cache.b", cache));
            for (Object chunk : values) if (chunk != null && chunk.getClass() == chunkType && seen.put(chunk, true) == null) full.add(fullChunk(chunk));
            return object("world", worldState, "opacity", blob(opacityBytes), "opacity_identity_retained", opacityIdentity,
                    "opacity_values_retained", opacityValues, "cached_locals", full, "generator", retained.generator(), "manager", retained.manager(),
                    "materials", retained.materialTable(), "world_time", String.format("%016x", (long) invoke(time, world)),
                    "world_force_load", field(worldType, "y", boolean.class, 1).get(world),
                    "provider_force_load", client ? null : field(cacheType, "a", boolean.class, 1).get(cache));
        }
        void close() throws Exception { invoke(close, client ? saveFormat : storage); }
    }
    public static void main(String[] args) {
        PrintStream originalOut = System.out; System.setOut(System.err); int status = 1;
        try {
            require(args.length == 6 || args.length == 7, "jar side mode seed_hex new_owned_root snapshots_json [shared|dedicated-gates]");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            require(args[2].equals(client ? "persistent-client" : "dedicated-constructor"), "explicit actual constructor scope");
            require(args[3].equals("0000000000000017"), "initial fixed seed23");
            String scenario = args.length == 7 ? args[6] : "shared";
            require(scenario.equals("shared") || !client && scenario.equals("dedicated-gates"), "scenario");
            Path jarPath = Path.of(args[0]).toRealPath(), root = Path.of(args[4]).toAbsolutePath().normalize();
            Path output = Path.of(args[5]).toAbsolutePath().normalize();
            require(Files.isRegularFile(jarPath) && !Files.exists(root, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(root)
                    && Files.isDirectory(root.getParent()) && !Files.exists(output, LinkOption.NOFOLLOW_LINKS)
                    && !Files.isSymbolicLink(output) && Files.isDirectory(output.getParent()) && !output.startsWith(root), "fresh owned root and separate new output");
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                    : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(hash(Files.readAllBytes(jarPath)).equals(expected), "pinned supplied original archive");
            URL jar = jarPath.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                Observer observer = new Observer(jar, loader, root, client, 23L);
                Object initial = observer.snapshot(0, 0); List<Object> operations = new ArrayList<>();
                int[][] coordinates = scenario.equals("shared") ? new int[][] {{1,0},{0,0},{0,0},{32768,0},{-1,-1},{1,0}}
                        : new int[][] {{9,0},{9,0},{9,0},{9,0},{9,0},{9,0},{9,0},{0,0}};
                String[] methods = scenario.equals("shared") ? new String[] {"c","c","b","c","c","c"} : new String[] {"b","d","b","c","d","b","c","c"};
                measuring = true; field(ChunkObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                for (int index = 0; index < methods.length; ++index) {
                    int x = coordinates[index][0], z = coordinates[index][1]; Object before = observer.snapshot(x, z);
                    begin(observer.world, observer.cache, index);
                    Object result = invoke(methods[index].equals("c") ? observer.acquire : methods[index].equals("b") ? observer.getChunk : observer.markDrop, observer.cache, x, z);
                    end(result, index);
                    operations.add(object("index", index, "method", methods[index], "coordinates", Arrays.asList(x,z),
                            "result", observer.ref(result), "before", before, "after", observer.snapshot(x,z)));
                }
                observer.close();
                require(hash(Files.readAllBytes(jarPath)).equals(expected), "original archive preserved");
                Map<String, Object> record = object("schema", 1, "side", args[1], "mode", args[2], "scenario", scenario, "seed_hex", args[3],
                        "jar_sha256", expected, "platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader(),
                        "class_origins", observer.origins, "bindings", object("world", observer.world.getClass().getName(), "cache", observer.cacheType.getName(),
                                "storage", observer.storage.getClass().getName(), "loader", observer.get("cache.d", observer.cache).getClass().getName(),
                                "generator", observer.get("cache.c", observer.cache).getClass().getName()),
                        "setup_metadata", observer.setupMetadata, "initial", initial, "operations", operations);
                byte[] encoded = (((String) invoke(observer.json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_BYTES, "complete snapshot output bound");
                Files.write(output, encoded, StandardOpenOption.CREATE_NEW); status = 0;
            }
        } catch (Throwable failure) {
            boolean unavailable = !measuring && (failure instanceof SetupUnavailable || failure.getClass().getName().equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine acquisition setup" : "FAILURE: acquisition observation");
            failure.printStackTrace(System.err); if (unavailable) status = 77;
        } finally { System.setOut(originalOut); }
        System.exit(status);
    }
}
