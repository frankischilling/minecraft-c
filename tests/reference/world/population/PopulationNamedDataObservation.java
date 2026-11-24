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

/** Local original-only experiment using actual named MapData and full World saves.
 * Compose the five unchanged public observer sources; copy no original bodies.
 * Random inspection requires --add-opens java.base/java.util=ALL-UNNAMED.
 */
public final class PopulationNamedDataObservation {
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final int MAX_COLLECTION = 256;
    private static final String SEED = "0000000000000017";
    private static final String PROFILE = "named-map-data";
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

    private static Constructor<?> constructor(Class<?> owner, int modifiers, Class<?>... arguments)
            throws Exception {
        Constructor<?> value = owner.getDeclaredConstructor(arguments);
        require(value.getModifiers() == modifiers, "exact constructor " + owner.getName());
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
        require(value.getClassLoader() == PopulationNamedDataObservation.class.getClassLoader()
                && value.getDeclaringClass() == enclosing && value.getModifiers() == 26,
                "unchanged private static final project helper " + name);
        return value;
    }

    private static Object signature(Class<?> owner, String name, Class<?> result, int modifiers,
                                    Class<?>... arguments) {
        StringBuilder descriptor = new StringBuilder("(");
        for (Class<?> argument : arguments) descriptor.append(argument.descriptorString());
        descriptor.append(')').append(result.descriptorString());
        return object("class", owner.getName(), "method", name,
                "descriptor", descriptor.toString(), "modifiers", modifiers);
    }

    /** Borrow checked members and owners, but do not call the empty-profile graph reader. */
    private static final class NamedData {
        final Object adapter, persistence, world, storage, players, savedData;
        final Class<?> adapterType, worldType, savedType, baseType, mapType;
        final URL jar;
        final ClassLoader loader;
        final Method original, origin, reference, checkOwners, baseBoundary;
        final Method time, setTime, save, dirtyMark, worldPut, managerLoad;
        final Constructor<?> mapConstructor, managerConstructor;
        final Field savedStorage, namedField, orderedField, idField, retainedBytes;
        final Field name, dirty, centerX, centerZ, dimension, scale, colors, updates, trackers, decorations, trackerMap;
        final Map<String, Object> bindings = new LinkedHashMap<>();
        final Map<String, Object> fieldBindings = new LinkedHashMap<>();
        final Map<String, Object> origins = new LinkedHashMap<>();
        final List<Object> values = new ArrayList<>();
        final IdentityHashMap<Object, Boolean> distinctValues = new IdentityHashMap<>();
        final Manager worldManager;
        Manager reader;
        final int maxEntries, maxFile;
        final long maxSnapshot, maxRetained;

        NamedData(Object adapter, Class<?> adapterType, Object persistence, Class<?> persistenceType,
                  ClassLoader loader, URL jar, boolean client) throws Exception {
            require(adapter != null && adapter.getClass() == adapterType
                    && field(adapterType, "persistence", Object.class, 16).get(adapter) == persistence,
                    "actual unchanged FullSave adapter and boundary owner");
            this.adapter = adapter; this.adapterType = adapterType; this.persistence = persistence;
            this.loader = loader; this.jar = jar;
            require(field(adapterType, "jar", URL.class, 16).get(adapter).equals(jar)
                    && field(adapterType, "originalLoader", ClassLoader.class, 16).get(adapter) == loader,
                    "same original archive and isolated loader");
            world = field(adapterType, "world", Object.class, 16).get(adapter);
            storage = field(adapterType, "storage", Object.class, 16).get(adapter);
            players = field(adapterType, "players", Object.class, 16).get(adapter);
            savedData = field(adapterType, "savedData", Object.class, 16).get(adapter);
            worldType = (Class<?>) field(adapterType, "worldType", Class.class, 16).get(adapter);
            savedType = (Class<?>) field(adapterType, "savedType", Class.class, 16).get(adapter);
            original = method(adapterType, "original", Class.class, 0, String.class);
            origin = method(adapterType, "origin", void.class, 0, Class.class);
            reference = method(adapterType, "ref", Object.class, 0, Object.class);
            checkOwners = method(adapterType, "owners", void.class, 0);
            baseBoundary = method(persistenceType, "boundary", Object.class, 0, int.class, int.class);
            time = (Method) field(adapterType, "time", Method.class, 16).get(adapter);
            setTime = (Method) field(adapterType, "setTime", Method.class, 16).get(adapter);
            save = (Method) field(adapterType, "save", Method.class, 16).get(adapter);
            savedStorage = (Field) field(adapterType, "savedStorage", Field.class, 16).get(adapter);
            namedField = (Field) field(adapterType, "namedField", Field.class, 16).get(adapter);
            orderedField = (Field) field(adapterType, "orderedField", Field.class, 16).get(adapter);
            idField = (Field) field(adapterType, "idField", Field.class, 16).get(adapter);
            retainedBytes = (Field) field(adapterType, "retainedBytes", Field.class, 16).get(adapter);
            maxEntries = field(adapterType, "maxEntries", int.class, 16).getInt(adapter);
            maxFile = field(adapterType, "maxFile", int.class, 16).getInt(adapter);
            maxSnapshot = field(adapterType, "maxSnapshot", long.class, 16).getLong(adapter);
            maxRetained = field(adapterType, "maxRetained", long.class, 16).getLong(adapter);
            copyMap(adapterType, "bindings", bindings);
            copyMap(adapterType, "fieldBindings", fieldBindings);
            copyMap(adapterType, "origins", origins);
            require(world.getClass() == original(client ? "fd" : "dj")
                    && savedType == original(client ? "hc" : "es"), "actual source World and manager");
            baseType = original(client ? "hm" : "fa");
            mapType = original(client ? "iu" : "fv");
            require(baseType.getModifiers() == 1025 && baseType.getDeclaredFields().length == 2
                    && mapType.getModifiers() == 1 && mapType.getSuperclass() == baseType
                    && mapType.getDeclaredFields().length == 9, "complete base SavedData and MapData field shapes");
            name = originalField("value_name", baseType, "a", String.class, 17);
            dirty = originalField("value_dirty", baseType, "b", boolean.class, 2);
            centerX = originalField("map_x_center", mapType, "b", int.class, 1);
            centerZ = originalField("map_z_center", mapType, "c", int.class, 1);
            dimension = originalField("map_dimension", mapType, "d", byte.class, 1);
            scale = originalField("map_scale", mapType, "e", byte.class, 1);
            colors = originalField("map_colors", mapType, "f", byte[].class, 1);
            updates = originalField("map_update_counter", mapType, "g", int.class, 1);
            trackers = originalField("map_trackers", mapType, "h", List.class, 1);
            decorations = originalField("map_decorations", mapType, "i", List.class, 1);
            trackerMap = originalField("map_tracker_cache", mapType, "j", Map.class, 2);
            dirtyMark = originalMethod("value_mark_dirty", baseType, "a", void.class, 1);
            originalMethod("value_set_dirty", baseType, "a", void.class, 1, boolean.class);
            originalMethod("value_get_dirty", baseType, "b", boolean.class, 1);
            Class<?> compound = original(client ? "nu" : "iq");
            originalMethod("value_read", baseType, "a", void.class, 1025, compound);
            originalMethod("value_write", baseType, "b", void.class, 1025, compound);
            originalMethod("map_read", mapType, "a", void.class, 1, compound);
            originalMethod("map_write", mapType, "b", void.class, 1, compound);
            worldPut = originalMethod("world_put", worldType, "a", void.class, 1, String.class, baseType);
            originalMethod("world_load", worldType, "a", baseType, 1, Class.class, String.class);
            originalMethod("manager_put", savedType, "a", void.class, 1, String.class, baseType);
            managerLoad = originalMethod("manager_load", savedType, "a", baseType, 1, Class.class, String.class);
            originalMethod("manager_write", savedType, "a", void.class, 2, baseType);
            mapConstructor = originalConstructor("map_constructor", mapType, String.class);
            managerConstructor = originalConstructor("manager_constructor", savedType,
                    original(client ? "wt" : "om"));
            originalConstructor("base_constructor", baseType, String.class);
            worldManager = new Manager(savedData);
            require(worldManager.named == field(adapterType, "namedCache", Object.class, 16).get(adapter)
                    && worldManager.ordered == field(adapterType, "orderedList", Object.class, 16).get(adapter)
                    && worldManager.ids == field(adapterType, "idMap", Object.class, 16).get(adapter),
                    "same World-owned manager backings as the checked FullSave adapter");
            owners();
        }

        void copyMap(Class<?> type, String member, Map<String, Object> target) throws Exception {
            Object actual = field(type, member, Map.class, 16).get(adapter);
            require(actual != null && actual.getClass() == LinkedHashMap.class,
                    "actual unchanged project member map " + member);
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) actual).entrySet()) {
                require(entry.getKey() instanceof String, "string project binding role");
                target.put((String) entry.getKey(), entry.getValue());
            }
        }

        Class<?> original(String value) throws Exception {
            Class<?> type = (Class<?>) invoke(original, adapter, value);
            origin(type);
            return type;
        }

        void origin(Class<?> type) throws Exception {
            invoke(origin, adapter, type);
            require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                    && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                    "all measured game members from the pinned original archive");
            origins.put(type.getName(), jar.toString());
        }

        Object ref(Object value) throws Exception { return invoke(reference, adapter, value); }

        Field originalField(String role, Class<?> owner, String member, Class<?> type, int modifiers)
                throws Exception {
            origin(owner);
            Field value = field(owner, member, type, modifiers);
            require(!fieldBindings.containsKey(role), "distinct field role");
            fieldBindings.put(role, object("class", owner.getName(), "field", member,
                    "descriptor", type.descriptorString(), "modifiers", modifiers));
            return value;
        }

        Method originalMethod(String role, Class<?> owner, String member, Class<?> result,
                              int modifiers, Class<?>... arguments) throws Exception {
            origin(owner);
            Method value = method(owner, member, result, modifiers, arguments);
            require(!bindings.containsKey(role), "distinct method role");
            bindings.put(role, signature(owner, member, result, modifiers, arguments));
            return value;
        }

        Constructor<?> originalConstructor(String role, Class<?> owner, Class<?>... arguments)
                throws Exception {
            origin(owner);
            Constructor<?> value = constructor(owner, 1, arguments);
            require(!bindings.containsKey(role), "distinct constructor role");
            bindings.put(role, signature(owner, "<init>", void.class, 1, arguments));
            return value;
        }

        Object string(Object value) throws Exception {
            require(value == null || value.getClass() == String.class, "actual nullable String");
            if (value == null) return object("object", ref(null), "utf16", null);
            String actual = (String) value;
            require(actual.length() <= MAX_COLLECTION, "complete bounded String input");
            List<Object> units = new ArrayList<>();
            for (int index = 0; index < actual.length(); ++index) units.add((int) actual.charAt(index));
            return object("object", ref(value), "utf16", units);
        }

        Object list(Object value) throws Exception {
            require(value != null && value.getClass() == ArrayList.class, "actual bootstrap ArrayList");
            List<?> actual = (List<?>) value;
            int size = actual.size();
            require(size >= 0 && size <= MAX_COLLECTION, "complete bounded actual List");
            List<Object> entries = new ArrayList<>();
            for (int index = 0; index < size; ++index)
                entries.add(object("index", index, "value", ref(actual.get(index))));
            require(actual.size() == size, "stable complete List occurrences");
            return object("object", ref(value), "size", size, "entries", entries);
        }

        Object map(Object value) throws Exception {
            require(value != null && value.getClass() == HashMap.class, "actual bootstrap HashMap");
            Map<?, ?> actual = (Map<?, ?>) value;
            int size = actual.size();
            require(size >= 0 && size <= MAX_COLLECTION, "complete bounded actual Map");
            List<Object> entries = new ArrayList<>();
            for (Map.Entry<?, ?> entry : actual.entrySet()) {
                Object key = entry.getKey();
                entries.add(object("ordinal", entries.size(), "key", ref(key),
                        "key_string", key == null || key.getClass() == String.class ? string(key) : null,
                        "value", ref(entry.getValue())));
            }
            require(actual.size() == size && entries.size() == size, "stable complete HashMap entries");
            return object("object", ref(value), "size", size, "entries", entries);
        }

        final class Manager {
            final Object value, named, ordered, ids;

            Manager(Object value) throws Exception {
                require(value != null && value.getClass() == savedType, "actual original SavedData manager");
                this.value = value;
                named = namedField.get(value); ordered = orderedField.get(value); ids = idField.get(value);
                require(named != null && named.getClass() == HashMap.class
                        && ordered != null && ordered.getClass() == ArrayList.class
                        && ids != null && ids.getClass() == HashMap.class
                        && named != ids && savedStorage.get(value) == storage,
                        "genuine distinct manager backings and same actual storage handler");
            }

            void owners() throws Exception {
                require(namedField.get(value) == named && orderedField.get(value) == ordered
                        && idField.get(value) == ids && savedStorage.get(value) == storage,
                        "retained actual manager fields and backing owners");
            }

            Object snapshot() throws Exception {
                owners();
                Object result = object("object", ref(value), "storage", ref(savedStorage.get(value)),
                        "named_cache", map(named), "ordered_list", list(ordered), "id_map", map(ids));
                owners();
                return result;
            }
        }

        void owners() throws Exception {
            invoke(checkOwners, adapter);
            worldManager.owners();
            require(players != null && players.getClass() == ArrayList.class && ((List<?>) players).isEmpty(),
                    "genuine empty Player List in the named-data experiment");
            if (reader != null) {
                reader.owners();
                require(reader.value != worldManager.value && reader.named != worldManager.named
                        && reader.ids != worldManager.ids && reader.ordered != worldManager.ordered
                        && reader.named != worldManager.ids && reader.ids != worldManager.named
                        && reader.ordered != players, "separate genuine reader manager and collection owners");
            }
        }

        void addValue(Object value) throws Exception {
            require(value != null && value.getClass() == mapType, "actual original MapData return");
            if (distinctValues.put(value, true) == null) values.add(value);
            require(values.size() <= MAX_COLLECTION, "bounded distinct observed MapData values");
        }

        Object mapData(Object value) throws Exception {
            require(value != null && value.getClass() == mapType, "actual original MapData instance");
            Object actualColors = colors.get(value);
            require(actualColors != null && actualColors.getClass() == byte[].class,
                    "actual constructed or loaded MapData colors backing");
            byte[] actual = (byte[]) actualColors;
            require(actual.length <= maxFile, "complete bounded colors backing");
            byte[] copy = actual.clone();
            Object h = trackers.get(value), i = decorations.get(value), j = trackerMap.get(value);
            require(h != null && i != null && h != i && h != players && i != players
                    && h != worldManager.ordered && i != worldManager.ordered
                    && j != worldManager.named && j != worldManager.ids,
                    "distinct genuine MapData tracker collections");
            Object result = object("object", ref(value), "name", string(name.get(value)),
                    "dirty", dirty.getBoolean(value), "x_center", centerX.getInt(value),
                    "z_center", centerZ.getInt(value), "dimension", (int) dimension.getByte(value),
                    "scale", (int) scale.getByte(value), "update_counter", updates.getInt(value),
                    "colors", object("object", ref(actualColors), "bytes", copy.length,
                            "sha256", hash(copy), "hex", HexFormat.of().formatHex(copy)),
                    "trackers", list(h), "decorations", list(i), "tracker_cache", map(j));
            require(colors.get(value) == actualColors && trackers.get(value) == h
                    && decorations.get(value) == i && trackerMap.get(value) == j
                    && java.util.Arrays.equals(actual, copy), "stable complete MapData observation");
            return result;
        }

        Object boundary() throws Exception {
            owners();
            Object state = invoke(baseBoundary, persistence, 0, 0);
            Object collections = object("world", ref(world), "players", list(players),
                    "saved_data", worldManager.snapshot(),
                    "reader_manager", reader == null ? null : reader.snapshot());
            List<Object> maps = new ArrayList<>();
            for (Object value : values) maps.add(mapData(value));
            owners();
            return object("state", state, "collections", collections, "map_data", maps);
        }

        List<Long> interval(long start) {
            long finish = System.currentTimeMillis();
            require(start <= finish, "nondecreasing measured original API epoch interval");
            return List.of(start, finish);
        }

        Map<String, Object> voidOperation(String role, Method api, Object receiver, Object... arguments)
                throws Exception {
            Object before = boundary();
            long start = System.currentTimeMillis();
            Object result = invoke(api, receiver, arguments);
            List<Long> epoch = interval(start);
            require(result == null, "actual void original API returned normally");
            return object("binding", bindings.get(role), "receiver", ref(receiver),
                    "returned_void", true, "epoch_ms", epoch, "before", before, "after", boundary());
        }

        Object load(String valueName, Object expectedHit) throws Exception {
            Object before = boundary();
            long start = System.currentTimeMillis();
            Object result = invoke(managerLoad, reader.value, mapType, valueName);
            List<Long> epoch = interval(start);
            require(expectedHit == null || result == expectedHit, "actual manager returned its cached loaded owner");
            addValue(result);
            return object("binding", bindings.get("manager_load"), "receiver", ref(reader.value),
                    "requested_class", mapType.getName(), "requested_class_object", ref(mapType),
                    "name", string(valueName), "result", ref(result), "epoch_ms", epoch,
                    "before", before, "after", boundary());
        }

        Object observe() throws Exception {
            Object initial = boundary();
            long previous = (Long) invoke(time, world);
            Map<String, Object> timeAPI = voidOperation("world_time_set", setTime, world, 1234L);
            timeAPI.put("value", 1234);
            timeAPI.put("previous", String.format(Locale.ROOT, "%016x", previous));
            require((Long) invoke(time, world) == 1234L, "actual public World time setter");

            String suppliedName = "named_map_value", lookupKey = "named_map_lookup";
            require(!suppliedName.equals(lookupKey), "distinct actual constructor name and World lookup key");
            Object before = boundary();
            long start = System.currentTimeMillis();
            Object value = construct(mapConstructor, suppliedName);
            List<Long> epoch = interval(start);
            addValue(value);
            require(name.get(value) == suppliedName, "actual base constructor retained supplied String owner");
            Object construction = object("binding", bindings.get("map_constructor"),
                    "name", string(suppliedName), "result", ref(value), "epoch_ms", epoch,
                    "before", before, "after", boundary());
            Object mark = voidOperation("value_mark_dirty", dirtyMark, value);
            Map<String, Object> put = voidOperation("world_put", worldPut, world, lookupKey, value);
            put.put("key", string(lookupKey)); put.put("value", ref(value));

            List<Object> saves = new ArrayList<>();
            for (int index = 0; index < 2; ++index) {
                Map<String, Object> saved = voidOperation("world_save", save, world, true, null);
                saved.put("index", index); saved.put("force", true); saved.put("progress", null);
                saves.add(saved);
            }

            before = boundary();
            start = System.currentTimeMillis();
            Object loadedManager = construct(managerConstructor, storage);
            epoch = interval(start);
            reader = new Manager(loadedManager);
            Object readerConstruction = object("binding", bindings.get("manager_constructor"),
                    "storage", ref(storage), "result", ref(loadedManager), "epoch_ms", epoch,
                    "before", before, "after", boundary());
            String actualName = (String) name.get(value);
            require(actualName != null, "genuine value name for separate manager readback");
            Object loaded = load(actualName, null);
            require(values.size() == 2 && values.get(1) != value
                    && colors.get(values.get(1)) != colors.get(value),
                    "genuine fresh readback MapData and colors owner");
            Object hit = load(actualName, values.get(1));
            require(values.size() == 2, "actual manager cache hit retained the loaded object");
            owners();
            return object("schema", 1, "scope", "original-game-held-world-named-map-data-experiment",
                    "full_world_save", true, "unload", false, "world_restart", false,
                    "bindings", bindings, "field_bindings", fieldBindings, "class_origins", origins,
                    "initial", initial, "time_api", timeAPI, "map_constructor", construction,
                    "dirty_mark", mark, "put", put, "saves", saves,
                    "reader_constructor", readerConstruction, "reader_load", loaded, "reader_cache_hit", hit,
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
                    "pinnedJar side seedhex freshRoot output [named-map-data]");
            String profile = args.length == 6 ? args[5] : PROFILE;
            require(profile.equals(PROFILE), "only named-map-data observation profile");
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
            ClassLoader project = PopulationNamedDataObservation.class.getClassLoader();
            Class<?>[] publicHelpers = {ChunkObservation.class, CacheCallbacksObservation.class,
                    NaturalPopulationObservation.class, PopulationPersistenceObservation.class,
                    PopulationWorldSaveObservation.class};
            List<Object> sourceHelpers = new ArrayList<>();
            String[] sources = {"tests/reference/chunk/ChunkObservation.java",
                    "tests/reference/cache/CacheCallbacksObservation.java",
                    "tests/reference/world/population/NaturalPopulationObservation.java",
                    "tests/reference/world/population/PopulationPersistenceObservation.java",
                    "tests/reference/world/population/PopulationWorldSaveObservation.java"};
            for (int index = 0; index < publicHelpers.length; ++index) {
                Class<?> helper = publicHelpers[index];
                require(helper.getClassLoader() == project && helper.getModifiers() == 17,
                        "five unchanged public final observer helpers in the project loader");
                sourceHelpers.add(object("source", sources[index], "class", helper.getName(),
                        "modifiers", helper.getModifiers()));
            }
            Class<?> randomType = nested("NaturalPopulationObservation$RandomFields", NaturalPopulationObservation.class);
            Class<?> helperType = nested("CacheCallbacksObservation$Observer", CacheCallbacksObservation.class);
            Class<?> naturalType = nested("NaturalPopulationObservation$Observation", NaturalPopulationObservation.class);
            Class<?> bridgeType = nested("PopulationPersistenceObservation$ObservationBridge", PopulationPersistenceObservation.class);
            Class<?> persistenceType = nested("PopulationPersistenceObservation$Persistence", PopulationPersistenceObservation.class);
            Class<?> fullType = nested("PopulationWorldSaveObservation$FullSave", PopulationWorldSaveObservation.class);
            Object randomFields = construct(constructor(randomType, 0));
            Constructor<?> helperConstructor = constructor(helperType, 0, URL.class, ClassLoader.class,
                    Path.class, boolean.class, String.class);
            Constructor<?> naturalConstructor = constructor(naturalType, 0, Object.class, Class.class,
                    ClassLoader.class, URL.class, boolean.class, randomType, String.class);
            Constructor<?> bridgeConstructor = constructor(bridgeType, 0, Object.class, Class.class,
                    Object.class, Class.class, Object.class, Class.class);
            Constructor<?> persistenceConstructor = constructor(persistenceType, 0, bridgeType, boolean.class, Path.class);
            Constructor<?> fullConstructor = constructor(fullType, 0, Object.class, Class.class,
                    Object.class, Class.class, ClassLoader.class, URL.class, boolean.class, Method.class);
            Method naturalObserve = method(naturalType, "observe", Map.class, 0, String.class, String.class);
            Method close = method(helperType, "close", void.class, 0);
            Method json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            Object projectBindings = object(
                    "random_fields_constructor", signature(randomType, "<init>", void.class, 0),
                    "observer_constructor", signature(helperType, "<init>", void.class, 0,
                            helperConstructor.getParameterTypes()),
                    "natural_constructor", signature(naturalType, "<init>", void.class, 0,
                            naturalConstructor.getParameterTypes()),
                    "bridge_constructor", signature(bridgeType, "<init>", void.class, 0,
                            bridgeConstructor.getParameterTypes()),
                    "persistence_constructor", signature(persistenceType, "<init>", void.class, 0,
                            persistenceConstructor.getParameterTypes()),
                    "full_save_adapter_constructor", signature(fullType, "<init>", void.class, 0,
                            fullConstructor.getParameterTypes()),
                    "natural_observe", signature(naturalType, "observe", Map.class, 0, String.class, String.class),
                    "base_boundary", signature(persistenceType, "boundary", Object.class, 0, int.class, int.class),
                    "observer_close", signature(helperType, "close", void.class, 0),
                    "json", signature(ChunkObservation.class, "json", String.class, 10, Object.class));
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
                    Object full = construct(fullConstructor, bridge, bridgeType, persistence,
                            persistenceType, loader, jar, client, originalClose);
                    NamedData observation = new NamedData(full, fullType, persistence,
                            persistenceType, loader, jar, client);
                    Object named = observation.observe();
                    record = object("schema", 1, "scope", "original-game-population-named-map-data-experiment",
                            "profile", PROFILE, "experiment", true, "native_comparison", false,
                            "tested_commit", null, "side", args[1], "seed_hex", SEED,
                            "jar_sha256", expected, "construction_epoch_ms", List.of(begin, end),
                            "source_helpers", sourceHelpers, "project_bindings", projectBindings,
                            "population", population, "named_data", named);
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
            require(encoded.length <= MAX_OUTPUT, "complete 64MiB named-data experiment output budget");
            Files.write(output, encoded, StandardOpenOption.CREATE_NEW);
            status = 0;
        } catch (Throwable failure) {
            String name = failure.getClass().getName();
            boolean unavailable = !measuring && (name.equals("CacheCallbacksObservation$SetupUnavailable")
                    || name.equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine named-data experiment construction"
                    : "FAILURE: original-only named MapData experiment");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally {
            System.setOut(originalOut);
        }
        System.exit(status);
    }
}
