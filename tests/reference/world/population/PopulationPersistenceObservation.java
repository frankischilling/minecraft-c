import java.io.PrintStream;
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

/** Genuine seed23 population followed by cache-only save and direct readback.
 * Compose the unchanged NaturalPopulationObservation, CacheCallbacksObservation
 * and ChunkObservation helpers. No original game field is assigned.
 * Random inspection requires --add-opens java.base/java.util=ALL-UNNAMED.
 */
public final class PopulationPersistenceObservation {
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final String SEED = "0000000000000017";
    private static final String PROFILE = "save-readback";
    private static boolean measuring;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Map<String, Object> object(Object... pairs) {
        require(pairs.length % 2 == 0, "complete object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int index = 0; index < pairs.length; index += 2) {
            require(pairs[index] instanceof String, "string object key");
            String key = (String) pairs[index];
            require(!result.containsKey(key), "distinct object key");
            result.put(key, pairs[index + 1]);
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

    private static Constructor<?> constructor(Class<?> owner, Class<?>... arguments) throws Exception {
        Constructor<?> value = owner.getDeclaredConstructor(arguments);
        require(value.getModifiers() == 0, "exact project-owned constructor " + owner.getName());
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

    private static Object construct(Constructor<?> constructor, Object... arguments) throws Exception {
        try { return constructor.newInstance(arguments); }
        catch (InvocationTargetException failure) { throw unwrap(failure); }
    }

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static Class<?> projectNested(String name, Class<?> enclosing) throws Exception {
        Class<?> value = Class.forName(name);
        require(value.getClassLoader() == PopulationPersistenceObservation.class.getClassLoader()
                && value.getDeclaringClass() == enclosing && value.getModifiers() == 26,
                "unchanged private static final project helper " + name);
        return value;
    }

    /** Read-only bridge to the actual existing RandomFields instance. */
    private static final class RandomBridge {
        final Object actual;
        final Method seedOwner;

        RandomBridge(Object actual, Class<?> type) throws Exception {
            require(actual != null && actual.getClass() == type,
                    "actual unchanged Natural RandomFields instance");
            this.actual = actual;
            seedOwner = method(type, "seedOwner", AtomicLong.class, 0, Random.class);
        }

        AtomicLong seedOwner(Random random) throws Exception {
            return (AtomicLong) invoke(seedOwner, actual, random);
        }
    }

    /** Borrow real helper fields; delegate every observation to its declaring body. */
    private static final class ObservationBridge {
        final Object actual, helper, world, dimension, storage, cache, generator, manager, cave, loaderOwner;
        final Class<?> helperType, worldType, chunkType;
        final URL jar;
        final Field generatorRandom, caveRandom, worldRandom;
        final RandomBridge randomFields;
        final boolean ring;
        final int[][] schedule, originSchedule;
        final Method original, origin, get, reference, state, oldSnapshot, observe;

        ObservationBridge(Object actual, Class<?> type, Object helper, Class<?> helperType,
                          Object randomFields, Class<?> randomType) throws Exception {
            require(actual != null && actual.getClass() == type,
                    "actual unchanged Natural Observation instance");
            this.actual = actual;
            this.helper = field(type, "helper", Object.class, 16).get(actual);
            this.helperType = (Class<?>) field(type, "helperType", Class.class, 16).get(actual);
            require(this.helper == helper && this.helperType == helperType,
                    "same actual callback Observer in the Natural Observation");
            world = field(type, "world", Object.class, 16).get(actual);
            dimension = field(type, "dimension", Object.class, 16).get(actual);
            storage = field(type, "storage", Object.class, 16).get(actual);
            cache = field(type, "cache", Object.class, 16).get(actual);
            generator = field(type, "generator", Object.class, 16).get(actual);
            manager = field(type, "manager", Object.class, 16).get(actual);
            cave = field(type, "cave", Object.class, 16).get(actual);
            loaderOwner = field(type, "loaderOwner", Object.class, 16).get(actual);
            worldType = (Class<?>) field(type, "worldType", Class.class, 16).get(actual);
            chunkType = (Class<?>) field(type, "chunkType", Class.class, 16).get(actual);
            jar = (URL) field(type, "jar", URL.class, 16).get(actual);
            generatorRandom = (Field) field(type, "generatorRandom", Field.class, 16).get(actual);
            caveRandom = (Field) field(type, "caveRandom", Field.class, 16).get(actual);
            worldRandom = (Field) field(type, "worldRandom", Field.class, 16).get(actual);
            Object actualRandom = field(type, "randomFields", randomType, 16).get(actual);
            require(actualRandom == randomFields, "same actual Natural RandomFields owner");
            this.randomFields = new RandomBridge(actualRandom, randomType);
            ring = field(type, "ring", boolean.class, 16).getBoolean(actual);
            schedule = (int[][]) field(type, "schedule", int[][].class, 16).get(actual);
            originSchedule = (int[][]) field(NaturalPopulationObservation.class, "SCHEDULE",
                    int[][].class, 26).get(null);
            require(!ring && schedule == originSchedule
                    && java.util.Arrays.deepEquals(originSchedule,
                            new int[][] {{1, 0}, {0, 1}, {1, 1}, {1, 1}}),
                    "actual unchanged Natural origin schedule");
            original = method(type, "original", Class.class, 0, String.class);
            origin = method(type, "origin", void.class, 0, Class.class);
            get = method(type, "get", Object.class, 0, String.class, Object.class);
            reference = method(type, "ref", Object.class, 0, Object.class);
            state = method(type, "state", Map.class, 0);
            oldSnapshot = method(type, "oldSnapshot", Object.class, 0, int.class, int.class);
            observe = method(type, "observe", Map.class, 0, String.class, String.class);
        }

        Class<?> original(String name) throws Exception {
            return (Class<?>) invoke(original, actual, name);
        }

        void origin(Class<?> type) throws Exception { invoke(origin, actual, type); }
        Object get(String name, Object receiver) throws Exception { return invoke(get, actual, name, receiver); }
        Object ref(Object value) throws Exception { return invoke(reference, actual, value); }
        Object state() throws Exception { return invoke(state, actual); }
        Object oldSnapshot(int x, int z) throws Exception { return invoke(oldSnapshot, actual, x, z); }
        Object observe(String side, String expected) throws Exception { return invoke(observe, actual, side, expected); }
    }

    private static final class Persistence {
        private static final int MAX_ENTRIES = 256;
        private static final int MAX_FILE = 4 * 1024 * 1024;
        private static final long MAX_SNAPSHOT_FILES = 8L * 1024 * 1024;
        private static final long MAX_RETAINED_FILES = 8L * 1024 * 1024;
        private static final int[][] READBACK = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
        private static final String[] INFO_ROLES = {
            "seed", "spawn_x", "spawn_y", "spawn_z", "time", "last_played",
            "size_on_disk", "player", "player_dimension", "level_name", "version",
            "raining", "rain_time", "thundering", "thunder_time"
        };
        final ObservationBridge observation;
        final boolean client;
        final Path root;
        final Object info, cacheMap, cacheList, pending, lightList;
        final Class<?> infoType;
        final Method setTime, time, save, load, fullChunk;
        final Field infoField;
        final List<Field> infoFields = new ArrayList<>();
        final Random providerRandom, caveRandom, worldRandom;
        final AtomicLong providerSeed, caveSeed, worldSeed;
        final Map<String, Object> bindings = new LinkedHashMap<>();
        final Map<String, Object> origins = new LinkedHashMap<>();
        final IdentityHashMap<Object, Boolean> loaded = new IdentityHashMap<>();
        long retainedFileBytes;
        Field normalizationField;
        byte[] normalizationOwner, initialNormalization;

        Persistence(ObservationBridge observation, boolean client, Path root) throws Exception {
            this.observation = observation; this.client = client; this.root = root;
            require(!observation.ring && observation.schedule == observation.originSchedule,
                    "save/readback follows exactly the unchanged origin profile");
            require(Files.isDirectory(root, LinkOption.NOFOLLOW_LINKS)
                    && !Files.isSymbolicLink(root) && root.toRealPath().equals(root),
                    "ordinary real owned save/readback root");
            Class<?> cacheType = original(client ? "ok" : "jb");
            Class<?> loaderType = original(client ? "ld" : "hb");
            require(observation.cache.getClass() == cacheType
                    && observation.loaderOwner.getClass() == loaderType,
                    "exact original cache and actual retained region loader");
            infoType = original(client ? "ei" : "ct");
            infoField = field(observation.worldType, "x", infoType, 4);
            info = infoField.get(observation.world);
            require(info != null && info.getClass() == infoType, "actual held WorldInfo owner");
            Class<?> compound = original(client ? "nu" : "iq");
            Class<?>[] infoTypes = {long.class, int.class, int.class, int.class, long.class,
                    long.class, long.class, compound, int.class, String.class, int.class,
                    boolean.class, int.class, boolean.class, int.class};
            require(infoType.getDeclaredFields().length == INFO_ROLES.length,
                    "complete original fifteen-field WorldInfo shape");
            for (int index = 0; index < INFO_ROLES.length; ++index)
                infoFields.add(field(infoType, Character.toString((char) ('a' + index)),
                        infoTypes[index], 2));
            setTime = method(observation.worldType, "a", void.class, 1, long.class);
            time = method(observation.worldType, client ? "t" : "m", long.class, 1);
            save = method(cacheType, "a", boolean.class, 1, boolean.class,
                    original(client ? "yb" : "pj"));
            load = method(loaderType, "a", observation.chunkType, 1,
                    observation.worldType, int.class, int.class);
            fullChunk = method(observation.helperType, "fullChunk", Object.class, 0, Object.class);
            bindings.put("world_time_set", binding(setTime));
            bindings.put("world_time_get", binding(time));
            bindings.put("cache_save", binding(save));
            bindings.put("region_load", binding(load));
            cacheMap = observation.get("cache.e", observation.cache);
            cacheList = observation.get("cache.f", observation.cache);
            pending = observation.get("cache.a", observation.cache);
            lightList = observation.get("world.C", observation.world);
            require(cacheMap != null && cacheMap.getClass() == java.util.HashMap.class
                    && cacheList != null && cacheList.getClass() == ArrayList.class
                    && pending != null && pending.getClass() == java.util.HashSet.class
                    && lightList != null && lightList.getClass() == ArrayList.class,
                    "actual original cache and light collection classes");
            providerRandom = (Random) observation.generatorRandom.get(observation.generator);
            caveRandom = (Random) observation.caveRandom.get(observation.cave);
            worldRandom = (Random) observation.worldRandom.get(observation.world);
            providerSeed = observation.randomFields.seedOwner(providerRandom);
            caveSeed = observation.randomFields.seedOwner(caveRandom);
            worldSeed = observation.randomFields.seedOwner(worldRandom);
            owners();
        }

        Class<?> original(String name) throws Exception {
            Class<?> result = observation.original(name);
            origins.put(name, observation.jar.toString());
            return result;
        }

        Object binding(Method value) throws Exception {
            observation.origin(value.getDeclaringClass());
            origins.put(value.getDeclaringClass().getName(), observation.jar.toString());
            StringBuilder descriptor = new StringBuilder("(");
            for (Class<?> type : value.getParameterTypes()) descriptor.append(type.descriptorString());
            descriptor.append(')').append(value.getReturnType().descriptorString());
            return object("class", value.getDeclaringClass().getName(), "method", value.getName(),
                    "descriptor", descriptor.toString(), "modifiers", value.getModifiers());
        }

        void owners() throws Exception {
            require(observation.get("world.t", observation.world) == observation.dimension
                    && observation.get("world.v", observation.world) == observation.cache
                    && observation.get("world.w", observation.world) == observation.storage
                    && observation.get("provider.a", observation.dimension) == observation.world
                    && observation.get("cache.g", observation.cache) == observation.world
                    && observation.get("cache.c", observation.cache) == observation.generator
                    && observation.get("cache.d", observation.cache) == observation.loaderOwner
                    && infoField.get(observation.world) == info
                    && observation.get("cache.e", observation.cache) == cacheMap
                    && observation.get("cache.f", observation.cache) == cacheList
                    && observation.get("cache.a", observation.cache) == pending
                    && observation.get("world.C", observation.world) == lightList,
                    "retained original World/cache/Info/collection owners during save/readback");
            require(observation.generatorRandom.get(observation.generator) == providerRandom
                    && observation.caveRandom.get(observation.cave) == caveRandom
                    && observation.worldRandom.get(observation.world) == worldRandom
                    && observation.randomFields.seedOwner(providerRandom) == providerSeed
                    && observation.randomFields.seedOwner(caveRandom) == caveSeed
                    && observation.randomFields.seedOwner(worldRandom) == worldSeed,
                    "retained actual Random and seed owners");
        }

        Object infoState() throws Exception {
            List<Object> fields = new ArrayList<>();
            for (int index = 0; index < infoFields.size(); ++index) {
                Field member = infoFields.get(index);
                Object value = member.get(info);
                if (member.getType() == long.class)
                    value = String.format(Locale.ROOT, "%016x", (Long) value);
                else if (index == 7) {
                    require(value == null, "genuine empty-player save/readback Info domain");
                    value = observation.ref(value);
                }
                fields.add(object("field", member.getName(), "role", INFO_ROLES[index],
                        "descriptor", member.getType().descriptorString(), "modifiers", member.getModifiers(),
                        "value", value));
            }
            return object("owner", observation.ref(info), "class", infoType.getName(), "fields", fields);
        }

        Object files() throws Exception {
            require(Files.isDirectory(root, LinkOption.NOFOLLOW_LINKS)
                    && !Files.isSymbolicLink(root) && root.toRealPath().equals(root),
                    "retained ordinary real save/readback root");
            List<Path> paths;
            try (var walk = Files.walk(root)) {
                paths = new ArrayList<>(walk.limit(MAX_ENTRIES + 1L).toList());
            }
            require(paths.size() <= MAX_ENTRIES, "complete save/readback file inventory entry budget");
            paths.sort(java.util.Comparator.comparing(path ->
                    root.relativize(path).toString().replace('\\', '/')));
            List<Object> entries = new ArrayList<>();
            long total = 0;
            int files = 0, directories = 0;
            for (Path path : paths) {
                require(path.normalize().startsWith(root) && !Files.isSymbolicLink(path),
                        "owned inventory path without symbolic links");
                var before = Files.readAttributes(path, java.nio.file.attribute.BasicFileAttributes.class,
                        LinkOption.NOFOLLOW_LINKS);
                String relative = path.equals(root) ? "." : root.relativize(path).toString().replace('\\', '/');
                if (before.isDirectory()) {
                    entries.add(object("path", relative, "kind", "directory"));
                    ++directories;
                    continue;
                }
                require(before.isRegularFile() && before.size() >= 0 && before.size() <= MAX_FILE
                        && total <= MAX_SNAPSHOT_FILES - before.size()
                        && retainedFileBytes <= MAX_RETAINED_FILES - before.size(),
                        "ordinary complete file within snapshot and retained-byte budgets");
                byte[] raw;
                try (var channel = Files.newByteChannel(path,
                        java.util.Set.<java.nio.file.OpenOption>of(StandardOpenOption.READ,
                                LinkOption.NOFOLLOW_LINKS));
                     var input = java.nio.channels.Channels.newInputStream(channel)) {
                    raw = input.readNBytes(MAX_FILE + 1);
                }
                var after = Files.readAttributes(path, java.nio.file.attribute.BasicFileAttributes.class,
                        LinkOption.NOFOLLOW_LINKS);
                require(raw.length == before.size() && raw.length <= MAX_FILE && after.isRegularFile()
                        && !Files.isSymbolicLink(path) && before.size() == after.size()
                        && before.lastModifiedTime().equals(after.lastModifiedTime())
                        && java.util.Objects.equals(before.fileKey(), after.fileKey()),
                        "complete stable ordinary raw file bytes");
                total += raw.length;
                retainedFileBytes += raw.length;
                entries.add(object("path", relative, "kind", "file", "bytes", raw.length,
                        "sha256", hash(raw), "hex", HexFormat.of().formatHex(raw)));
                ++files;
            }
            return object("complete", true, "entries", entries, "files", files,
                    "directories", directories, "bytes", total);
        }

        Object boundary(int x, int z) throws Exception {
            owners();
            Object cache = observation.oldSnapshot(x, z);
            Object numerical = observation.state();
            Object info = infoState();
            Object disk = files();
            owners();
            return object("cache", cache, "numerical", numerical, "info", info, "disk", disk);
        }

        Object blockMap() throws Exception {
            // Call only after a genuine successful region load has reached the
            // pinned original normal post-load body. No earlier active use of
            // vi/nn is introduced by this save/readback.
            require(!loaded.isEmpty(), "normalization observed only after genuine loader return");
            if (normalizationField == null) {
                Class<?> owner = original(client ? "vi" : "nn");
                normalizationField = field(owner, "a", byte[].class, 10);
                Method normalizer = method(owner, "a", void.class, 9, byte[].class);
                bindings.put("block_normalizer", binding(normalizer));
                Object value = normalizationField.get(null);
                require(value != null && value.getClass() == byte[].class && ((byte[]) value).length == 256,
                        "actual complete original post-load map");
                normalizationOwner = (byte[]) value;
                initialNormalization = normalizationOwner.clone();
            }
            require(normalizationField.get(null) == normalizationOwner
                    && java.util.Arrays.equals(normalizationOwner, initialNormalization),
                    "actual normalization map owner and bytes retained");
            return object("class", normalizationField.getDeclaringClass().getName(), "field", "a",
                    "descriptor", "[B", "modifiers", normalizationField.getModifiers(),
                    "owner", observation.ref(normalizationOwner), "length", normalizationOwner.length,
                    "bytes", normalizationOwner.length, "sha256", hash(normalizationOwner),
                    "values", HexFormat.of().formatHex(normalizationOwner),
                    "first_observation", "after-first-genuine-loader-return; no earlier observer active use");
        }

        Map<String, Object> observe() throws Exception {
            Object initial = boundary(0, 0);
            Object timeBefore = boundary(0, 0);
            long beforeTime = (Long) invoke(time, observation.world);
            long start = System.currentTimeMillis();
            invoke(setTime, observation.world, 1234L);
            long finish = System.currentTimeMillis();
            require(start <= finish && (Long) invoke(time, observation.world) == 1234L,
                    "actual time setter result and nondecreasing epoch interval");
            Object timeAfter = boundary(0, 0);
            Object timeAPI = object("binding", bindings.get("world_time_set"),
                    "receiver", observation.ref(observation.world), "value", 1234,
                    "previous", String.format(Locale.ROOT, "%016x", beforeTime),
                    "epoch_ms", List.of(start, finish), "before", timeBefore, "after", timeAfter);
            List<Object> saves = new ArrayList<>();
            for (int index = 0; index < 2; ++index) {
                Object before = boundary(0, 0);
                start = System.currentTimeMillis();
                Object result = invoke(save, observation.cache, true, null);
                finish = System.currentTimeMillis();
                require(start <= finish && result != null && result.getClass() == Boolean.class,
                        "actual Boolean cache-save result and epoch interval");
                Object after = boundary(0, 0);
                saves.add(object("index", index, "binding", bindings.get("cache_save"),
                        "receiver", observation.ref(observation.cache), "force", true,
                        "progress", null, "completed", result, "epoch_ms", List.of(start, finish),
                        "before", before, "after", after));
            }
            List<Object> loads = new ArrayList<>();
            Object firstMap = null;
            for (int index = 0; index < READBACK.length; ++index) {
                int x = READBACK[index][0], z = READBACK[index][1];
                Object before = boundary(x, z);
                start = System.currentTimeMillis();
                Object result = invoke(load, observation.loaderOwner, observation.world, x, z);
                finish = System.currentTimeMillis();
                require(start <= finish && result != null && result.getClass() == observation.chunkType
                        && observation.get("chunk.d", result) == observation.world
                        && (Integer) observation.get("chunk.j", result) == x
                        && (Integer) observation.get("chunk.k", result) == z
                        && loaded.put(result, true) == null,
                        "fresh direct original loader result at requested coordinates and actual World");
                for (Object cached : ((Map<?, ?>) cacheMap).values())
                    require(result != cached, "direct loader does not return a retained cached identity");
                for (Object cached : (List<?>) cacheList)
                    require(result != cached, "direct loader result is outside actual insertion list");
                Object map = blockMap();
                if (firstMap == null) firstMap = map;
                Object complete = invoke(fullChunk, observation.helper, result);
                Object after = boundary(x, z);
                loads.add(object("index", index, "binding", bindings.get("region_load"),
                        "receiver", observation.ref(observation.loaderOwner),
                        "world", observation.ref(observation.world), "coordinates", List.of(x, z),
                        "epoch_ms", List.of(start, finish), "result", observation.ref(result),
                        "chunk", complete, "normalization_map", map, "before", before, "after", after));
            }
            owners();
            return object("schema", 1, "scope", "original-game-population-chunk-save-readback",
                    "full_world_save", false, "unload", false,
                    "world_restart", false, "bindings", bindings, "class_origins", origins,
                    "initial", initial, "time_api", timeAPI, "cache_saves", saves,
                    "direct_loads", loads, "block_map", firstMap,
                    "limits", object("output_bytes", MAX_OUTPUT, "inventory_entries", MAX_ENTRIES,
                            "file_bytes", MAX_FILE, "snapshot_file_bytes", MAX_SNAPSHOT_FILES,
                            "retained_file_bytes", MAX_RETAINED_FILES,
                            "actual_retained_file_bytes", retainedFileBytes));
        }
    }

    public static void main(String[] args) {
        PrintStream originalOut = System.out;
        System.setOut(System.err);
        int status = 1;
        try {
            require(args.length == 5 || args.length == 6,
                    "jar side seedhex fresh_world_root output_json [save-readback]");
            String profile = args.length == 6 ? args[5] : PROFILE;
            require(profile.equals(PROFILE), "save-readback profile");
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
            ClassLoader project = PopulationPersistenceObservation.class.getClassLoader();
            require(NaturalPopulationObservation.class.getClassLoader() == project
                    && CacheCallbacksObservation.class.getClassLoader() == project
                    && ChunkObservation.class.getClassLoader() == project,
                    "project-owned unchanged helper classes");
            Class<?> randomType = projectNested("NaturalPopulationObservation$RandomFields",
                    NaturalPopulationObservation.class);
            Object randomFields = construct(constructor(randomType));
            Class<?> helperType = projectNested("CacheCallbacksObservation$Observer",
                    CacheCallbacksObservation.class);
            Constructor<?> helperConstructor = constructor(helperType, URL.class, ClassLoader.class,
                    Path.class, boolean.class, String.class);
            Class<?> observationType = projectNested("NaturalPopulationObservation$Observation",
                    NaturalPopulationObservation.class);
            Constructor<?> observationConstructor = constructor(observationType, Object.class,
                    Class.class, ClassLoader.class, URL.class, boolean.class, randomType, String.class);
            Method close = method(helperType, "close", void.class, 0);
            Method json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            URL jar = jarPath.toUri().toURL();
            Map<String, Object> record;
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                long constructionBegin = System.currentTimeMillis();
                Object helper = construct(helperConstructor, jar, loader, root, client, "population");
                long constructionEnd = System.currentTimeMillis();
                measuring = true;
                Throwable primary = null;
                try {
                    require(constructionBegin <= constructionEnd,
                            "nondecreasing genuine Observer construction epoch interval");
                    field(CacheCallbacksObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    field(ChunkObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    Object actual = construct(observationConstructor, helper, helperType, loader, jar,
                            client, randomFields, "origin");
                    ObservationBridge observation = new ObservationBridge(actual, observationType,
                            helper, helperType, randomFields, randomType);
                    Object population = observation.observe(args[1], expected);
                    Object persistence = new Persistence(observation, client, root).observe();
                    record = object("schema", 1, "scope", "original-game-population-save-readback",
                            "profile", PROFILE, "side", args[1], "seed_hex", SEED,
                            "jar_sha256", expected, "construction_epoch_ms", List.of(constructionBegin, constructionEnd),
                            "population", population, "persistence", persistence);
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
            require(encoded.length <= MAX_OUTPUT, "complete 64MiB population save/readback output budget");
            Files.write(output, encoded, StandardOpenOption.CREATE_NEW);
            status = 0;
        } catch (Throwable failure) {
            String name = failure.getClass().getName();
            boolean unavailable = !measuring && (name.equals("CacheCallbacksObservation$SetupUnavailable")
                    || name.equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine population save/readback setup"
                    : "FAILURE: population save/readback observation");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally {
            System.setOut(originalOut);
        }
        System.exit(status);
    }
}
