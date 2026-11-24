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
import java.util.HashMap;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Observe actual full World saves on a held, naturally populated original World.
 * Compose the four unchanged public observer sources; copy no original bodies.
 * Random inspection requires --add-opens java.base/java.util=ALL-UNNAMED.
 */
public final class PopulationWorldSaveObservation {
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final int MAX_COLLECTION = 256;
    private static final String SEED = "0000000000000017";
    private static final String PROFILE = "full-world-save";
    private static boolean measuring;

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
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
        Field value = owner.getDeclaredField(name);
        require(value.getType() == type && value.getModifiers() == modifiers,
                "exact field " + owner.getName() + "." + name + ":" + type.descriptorString());
        value.setAccessible(true);
        return value;
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
        require(value.getModifiers() == 0, "unchanged project-owned constructor " + owner.getName());
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

    private static Class<?> nested(String name, Class<?> enclosing) throws Exception {
        Class<?> value = Class.forName(name);
        require(value.getClassLoader() == PopulationWorldSaveObservation.class.getClassLoader()
                && value.getDeclaringClass() == enclosing && value.getModifiers() == 26,
                "unchanged private static final project helper " + name);
        return value;
    }

    private static final class FullSave {
        final Object bridge, persistence, world, storage, info, players, savedData;
        final Object namedCache, orderedList, idMap;
        final Class<?> worldType, savedType;
        final Field playersField, savedField, savedStorage, namedField, orderedField, idField;
        final Field retainedBytes;
        final Method original, origin, reference, baseBoundary, time, setTime, save;
        final Map<String, Object> bindings = new LinkedHashMap<>();
        final Map<String, Object> fieldBindings = new LinkedHashMap<>();
        final Map<String, Object> origins = new LinkedHashMap<>();
        final URL jar;
        final ClassLoader originalLoader;
        final int maxEntries, maxFile;
        final long maxSnapshot, maxRetained;

        FullSave(Object bridge, Class<?> bridgeType, Object persistence, Class<?> persistenceType,
                   ClassLoader loader, URL jar, boolean client, Method close) throws Exception {
            require(bridge != null && bridge.getClass() == bridgeType
                    && persistence != null && persistence.getClass() == persistenceType
                    && field(persistenceType, "observation", bridgeType, 16).get(persistence) == bridge,
                    "same actual unchanged observation bridge and boundary owner");
            this.bridge = bridge; this.persistence = persistence;
            this.jar = jar; this.originalLoader = loader;
            world = field(bridgeType, "world", Object.class, 16).get(bridge);
            storage = field(bridgeType, "storage", Object.class, 16).get(bridge);
            worldType = (Class<?>) field(bridgeType, "worldType", Class.class, 16).get(bridge);
            require(field(bridgeType, "jar", URL.class, 16).get(bridge).equals(jar),
                    "same exact original archive URL");
            original = method(bridgeType, "original", Class.class, 0, String.class);
            origin = method(bridgeType, "origin", void.class, 0, Class.class);
            reference = method(bridgeType, "ref", Object.class, 0, Object.class);
            baseBoundary = method(persistenceType, "boundary", Object.class, 0, int.class, int.class);
            retainedBytes = field(persistenceType, "retainedFileBytes", long.class, 0);
            maxEntries = field(persistenceType, "MAX_ENTRIES", int.class, 26).getInt(null);
            maxFile = field(persistenceType, "MAX_FILE", int.class, 26).getInt(null);
            maxSnapshot = field(persistenceType, "MAX_SNAPSHOT_FILES", long.class, 26).getLong(null);
            maxRetained = field(persistenceType, "MAX_RETAINED_FILES", long.class, 26).getLong(null);
            require(maxEntries == 256 && maxFile == 4 * 1024 * 1024
                    && maxSnapshot == 8L * 1024 * 1024 && maxRetained == 8L * 1024 * 1024,
                    "unchanged complete raw-file observation limits");
            require(world.getClass() == original(client ? "fd" : "dj")
                    && world.getClass() == worldType
                    && storage.getClass() == original(client ? "mx" : "ie"),
                    "genuine original base World and McRegion handler");
            Class<?> infoType = original(client ? "ei" : "ct");
            info = field(persistenceType, "info", Object.class, 16).get(persistence);
            require(info != null && info.getClass() == infoType, "actual unchanged boundary Info owner");
            Class<?> progress = original(client ? "yb" : "pj");
            Class<?> compound = original(client ? "nu" : "iq");
            Class<?> storageInterface = original(client ? "wt" : "om");
            savedType = original(client ? "hc" : "es");
            require(savedType.getModifiers() == 1 && savedType.getDeclaredFields().length == 4,
                    "actual complete original SavedData class shape");
            playersField = originalField("world_players", worldType, "d", List.class, 1);
            savedField = originalField("world_saved_data", worldType, "z", savedType, 1);
            originalField("world_info", worldType, "x", infoType, 4);
            originalField("world_storage", worldType, "w", storageInterface, 20);
            savedStorage = originalField("saved_storage", savedType, "a", storageInterface, 2);
            namedField = originalField("saved_named_cache", savedType, "b", Map.class, 2);
            orderedField = originalField("saved_ordered_list", savedType, "c", List.class, 2);
            idField = originalField("saved_id_map", savedType, "d", Map.class, 2);
            players = playersField.get(world);
            savedData = savedField.get(world);
            require(savedData != null && savedData.getClass() == savedType,
                    "actual World-owned SavedData instance");
            namedCache = namedField.get(savedData);
            orderedList = orderedField.get(savedData);
            idMap = idField.get(savedData);
            require(ArrayList.class.getClassLoader() == null && HashMap.class.getClassLoader() == null,
                    "bootstrap collection implementations");
            IdentityHashMap<Object, Boolean> distinct = new IdentityHashMap<>();
            for (Object value : new Object[] {players, namedCache, orderedList, idMap})
                require(value != null && distinct.put(value, true) == null,
                        "four genuine distinct Player/SavedData collection owners");
            require(players.getClass() == ArrayList.class && orderedList.getClass() == ArrayList.class
                    && namedCache.getClass() == HashMap.class && idMap.getClass() == HashMap.class,
                    "actual Player ArrayList and SavedData HashMap/ArrayList classes");
            time = originalMethod("world_time_get", worldType, client ? "t" : "m", long.class, 1);
            setTime = originalMethod("world_time_set", worldType, "a", void.class, 1, long.class);
            save = originalMethod("world_save", worldType, "a", void.class, 1, boolean.class, progress);
            originalMethod("world_metadata", worldType, client ? "D" : "w", void.class, 2);
            originalMethod("world_session", worldType, client ? "r" : "k", void.class, 1);
            originalMethod("handler_list_save", storage.getClass(), "a", void.class, 1, infoType, List.class);
            originalMethod("handler_base_list_save", original(client ? "fm" : "dq"),
                    "a", void.class, 1, infoType, List.class);
            originalMethod("info_list", infoType, "a", compound, 1, List.class);
            originalMethod("info_common_writer", infoType, "a", void.class, 2, compound, compound);
            originalMethod("saved_data_save", savedType, "a", void.class, 1);
            originalMethod("info_version_set", infoType, client ? "d" : "a", void.class, 1, int.class);
            bindings.put("region_close", methodBinding(close));
            owners();
        }

        Class<?> original(String name) throws Exception {
            Class<?> type = (Class<?>) invoke(original, bridge, name);
            origin(type);
            return type;
        }

        void origin(Class<?> type) throws Exception {
            invoke(origin, bridge, type);
            require(type.getClassLoader() == originalLoader && type.getProtectionDomain().getCodeSource() != null
                    && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                    "all measured game members from the pinned original archive");
            origins.put(type.getName(), jar.toString());
        }

        Object ref(Object value) throws Exception { return invoke(reference, bridge, value); }

        Object methodBinding(Method value) throws Exception {
            origin(value.getDeclaringClass());
            StringBuilder descriptor = new StringBuilder("(");
            for (Class<?> argument : value.getParameterTypes()) descriptor.append(argument.descriptorString());
            descriptor.append(')').append(value.getReturnType().descriptorString());
            return object("class", value.getDeclaringClass().getName(), "method", value.getName(),
                    "descriptor", descriptor.toString(), "modifiers", value.getModifiers());
        }

        Method originalMethod(String role, Class<?> owner, String name, Class<?> result,
                              int modifiers, Class<?>... arguments) throws Exception {
            Method value = method(owner, name, result, modifiers, arguments);
            bindings.put(role, methodBinding(value));
            return value;
        }

        Field originalField(String role, Class<?> owner, String name, Class<?> type, int modifiers)
                throws Exception {
            origin(owner);
            Field value = field(owner, name, type, modifiers);
            fieldBindings.put(role, object("class", owner.getName(), "field", name,
                    "descriptor", type.descriptorString(), "modifiers", modifiers));
            return value;
        }

        void owners() throws Exception {
            require(playersField.get(world) == players && savedField.get(world) == savedData
                    && savedStorage.get(savedData) == storage
                    && namedField.get(savedData) == namedCache && orderedField.get(savedData) == orderedList
                    && idField.get(savedData) == idMap,
                    "retained actual Player and SavedData fields and backing owners");
        }

        Object list(Object value) throws Exception {
            require(value != null && value.getClass() == ArrayList.class, "actual bootstrap ArrayList");
            List<?> actual = (List<?>) value;
            int size = actual.size();
            require(size >= 0 && size <= MAX_COLLECTION, "bounded complete actual List");
            List<Object> entries = new ArrayList<>();
            for (Object entry : actual) entries.add(ref(entry));
            require(actual.size() == size && entries.size() == size, "stable complete List entries");
            require(size == 0, "genuine full-save observation requires actual empty Player/SavedData Lists");
            return object("object", ref(value), "size", size, "entries", entries);
        }

        Object map(Object value) throws Exception {
            require(value != null && value.getClass() == HashMap.class, "actual bootstrap HashMap");
            Map<?, ?> actual = (Map<?, ?>) value;
            int size = actual.size();
            require(size >= 0 && size <= MAX_COLLECTION, "bounded complete actual Map");
            List<Object> entries = new ArrayList<>();
            for (Map.Entry<?, ?> entry : actual.entrySet())
                entries.add(object("key", ref(entry.getKey()), "value", ref(entry.getValue())));
            require(actual.size() == size && entries.size() == size, "stable complete HashMap entries");
            require(size == 0, "genuine full-save observation requires actual empty named/ID Maps");
            return object("object", ref(value), "size", size, "entries", entries);
        }

        Object collections() throws Exception {
            owners();
            Object result = object("world", ref(world), "players", list(players),
                    "saved_data", object("object", ref(savedData), "storage", ref(savedStorage.get(savedData)),
                            "named_cache", map(namedCache), "ordered_list", list(orderedList), "id_map", map(idMap)));
            owners();
            return result;
        }

        Object boundary() throws Exception {
            owners();
            Object state = invoke(baseBoundary, persistence, 0, 0);
            Object collections = collections();
            owners();
            return object("state", state, "collections", collections);
        }

        Object observe() throws Exception {
            Object initial = boundary();
            Object before = boundary();
            long previous = (Long) invoke(time, world);
            long start = System.currentTimeMillis();
            invoke(setTime, world, 1234L);
            long finish = System.currentTimeMillis();
            require(start <= finish && (Long) invoke(time, world) == 1234L,
                    "actual public time setter and nondecreasing measured interval");
            Object timeAPI = object("binding", bindings.get("world_time_set"), "receiver", ref(world),
                    "value", 1234, "previous", String.format(Locale.ROOT, "%016x", previous),
                    "epoch_ms", List.of(start, finish), "before", before, "after", boundary());
            List<Object> saves = new ArrayList<>();
            for (int index = 0; index < 2; ++index) {
                before = boundary();
                start = System.currentTimeMillis();
                Object result = invoke(save, world, true, null);
                finish = System.currentTimeMillis();
                require(start <= finish && result == null,
                        "actual full World void save returned and measured interval is nondecreasing");
                Object after = boundary();
                saves.add(object("index", index, "binding", bindings.get("world_save"),
                        "receiver", ref(world), "force", true, "progress", null,
                        "returned_void", true, "epoch_ms", List.of(start, finish), "before", before, "after", after));
            }
            owners();
            return object("schema", 1, "scope", "original-game-held-world-full-save",
                    "full_world_save", true, "unload", false, "world_restart", false,
                    "bindings", bindings, "field_bindings", fieldBindings, "class_origins", origins,
                    "initial", initial, "time_api", timeAPI, "saves", saves,
                    "limits", object("output_bytes", MAX_OUTPUT, "collection_entries", MAX_COLLECTION,
                            "inventory_entries", maxEntries, "file_bytes", maxFile,
                            "snapshot_file_bytes", maxSnapshot, "retained_file_bytes", maxRetained,
                            "actual_retained_file_bytes", retainedBytes.getLong(persistence)));
        }
    }

    public static void main(String[] args) {
        PrintStream originalOut = System.out;
        System.setOut(System.err);
        int status = 1;
        try {
            require(args.length == 5 || args.length == 6,
                    "pinnedJar side seedhex freshRoot output [full-world-save]");
            String profile = args.length == 6 ? args[5] : PROFILE;
            require(profile.equals(PROFILE), "only full-world-save observation profile");
            require(Runtime.version().feature() >= 17, "Java17 or newer");
            boolean client = args[1].equals("client");
            require(client || args[1].equals("server"), "client or server");
            require(args[2].equals(SEED), "fixed genuine seed23 origin profile");
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
            ClassLoader project = PopulationWorldSaveObservation.class.getClassLoader();
            require(ChunkObservation.class.getClassLoader() == project
                    && CacheCallbacksObservation.class.getClassLoader() == project
                    && NaturalPopulationObservation.class.getClassLoader() == project
                    && PopulationPersistenceObservation.class.getClassLoader() == project,
                    "four unchanged public observer helpers in the project loader");
            Class<?> randomType = nested("NaturalPopulationObservation$RandomFields", NaturalPopulationObservation.class);
            Class<?> helperType = nested("CacheCallbacksObservation$Observer", CacheCallbacksObservation.class);
            Class<?> naturalType = nested("NaturalPopulationObservation$Observation", NaturalPopulationObservation.class);
            Class<?> bridgeType = nested("PopulationPersistenceObservation$ObservationBridge", PopulationPersistenceObservation.class);
            Class<?> persistenceType = nested("PopulationPersistenceObservation$Persistence", PopulationPersistenceObservation.class);
            Object randomFields = construct(constructor(randomType));
            Constructor<?> helperConstructor = constructor(helperType, URL.class, ClassLoader.class,
                    Path.class, boolean.class, String.class);
            Constructor<?> naturalConstructor = constructor(naturalType, Object.class, Class.class,
                    ClassLoader.class, URL.class, boolean.class, randomType, String.class);
            Constructor<?> bridgeConstructor = constructor(bridgeType, Object.class, Class.class,
                    Object.class, Class.class, Object.class, Class.class);
            Constructor<?> persistenceConstructor = constructor(persistenceType, bridgeType, boolean.class, Path.class);
            Method naturalObserve = method(naturalType, "observe", Map.class, 0, String.class, String.class);
            Method close = method(helperType, "close", void.class, 0);
            Method json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            URL jar = jarPath.toUri().toURL();
            Map<String, Object> record;
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                long begin = System.currentTimeMillis();
                Object helper = construct(helperConstructor, jar, loader, root, client, "population");
                long end = System.currentTimeMillis();
                measuring = true;
                Throwable primary = null;
                try {
                    require(begin <= end, "nondecreasing genuine constructor epoch interval");
                    field(CacheCallbacksObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    field(ChunkObservation.class, "measuring", boolean.class, 10).setBoolean(null, true);
                    Object natural = construct(naturalConstructor, helper, helperType, loader, jar,
                            client, randomFields, "origin");
                    Object population = invoke(naturalObserve, natural, args[1], expected);
                    Object bridge = construct(bridgeConstructor, natural, naturalType, helper,
                            helperType, randomFields, randomType);
                    Object persistence = construct(persistenceConstructor, bridge, client, root);
                    Method originalClose = (Method) field(helperType, "close", Method.class, 16).get(helper);
                    FullSave observation = new FullSave(bridge, bridgeType, persistence,
                            persistenceType, loader, jar, client, originalClose);
                    Object saved = observation.observe();
                    record = object("schema", 1, "scope", "original-game-population-full-world-save",
                            "profile", PROFILE, "side", args[1], "seed_hex", SEED,
                            "jar_sha256", expected, "construction_epoch_ms", List.of(begin, end),
                            "population", population, "world_save", saved);
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
            require(hash(Files.readAllBytes(jarPath)).equals(expected), "supplied original archive preserved");
            byte[] encoded = (((String) invoke(json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
            require(encoded.length <= MAX_OUTPUT, "complete 64MiB full-World observation output budget");
            Files.write(output, encoded, StandardOpenOption.CREATE_NEW);
            status = 0;
        } catch (Throwable failure) {
            String name = failure.getClass().getName();
            boolean unavailable = !measuring && (name.equals("CacheCallbacksObservation$SetupUnavailable")
                    || name.equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine full-World observation construction"
                    : "FAILURE: original-only full-World save observation");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally {
            System.setOut(originalOut);
        }
        System.exit(status);
    }
}
