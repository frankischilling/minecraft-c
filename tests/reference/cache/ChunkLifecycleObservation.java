import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.ObjectOutputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
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
import java.util.Collection;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.TreeMap;
import java.util.HashSet;
import java.util.zip.ZipFile;

/** Original constructor/API observations in fresh, helper-owned world copies. */
public final class ChunkLifecycleObservation {
    static final String CLIENT = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final String SERVER = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    static final int MAX_RECORD = 32 * 1024 * 1024;
    static boolean started;
    static final class SetupUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupUnavailable(Throwable cause) { super("original constructor dependency", cause); }
    }
    static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    static Map<String,Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String,Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) {
            require(values[i] instanceof String && !result.containsKey(values[i]), "unique string keys");
            result.put((String) values[i], values[i + 1]);
        }
        return result;
    }
    static String hash(byte[] raw) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(raw));
    }
    static String hex(long value) { return String.format(Locale.ROOT, "%016x", value); }
    static String descriptor(Class<?> type) {
        if (type.isArray()) return type.getName().replace('.', '/');
        if (!type.isPrimitive()) return "L" + type.getName().replace('.', '/') + ";";
        if (type == void.class) return "V";
        if (type == boolean.class) return "Z";
        if (type == byte.class) return "B";
        if (type == short.class) return "S";
        if (type == char.class) return "C";
        if (type == int.class) return "I";
        if (type == long.class) return "J";
        if (type == float.class) return "F";
        if (type == double.class) return "D";
        throw new IllegalStateException("primitive type");
    }
    static String signature(Class<?> result, Class<?>... args) {
        StringBuilder text = new StringBuilder("(");
        for (Class<?> type : args) text.append(descriptor(type));
        return text.append(')').append(descriptor(result)).toString();
    }
    static Object call(Method method, Object receiver, Object... args) throws Exception {
        try { return method.invoke(receiver, args); }
        catch (InvocationTargetException failure) {
            if (failure.getCause() instanceof Exception e) throw e;
            if (failure.getCause() instanceof Error e) throw e;
            throw failure;
        }
    }
    static void json(Object value, StringBuilder out) {
        if (value == null) { out.append("null"); return; }
        if (value instanceof String text) {
            out.append('"');
            for (char c : text.toCharArray()) {
                if (c == '"' || c == '\\') out.append('\\').append(c);
                else if (c < 32 || c > 126) out.append(String.format(Locale.ROOT, "\\u%04x", (int) c));
                else out.append(c);
            }
            out.append('"'); return;
        }
        if (value instanceof Number || value instanceof Boolean) { out.append(value); return; }
        if (value instanceof Map<?,?> map) {
            out.append('{'); boolean comma = false;
            for (var entry : map.entrySet()) {
                require(entry.getKey() instanceof String, "JSON string key");
                if (comma) out.append(','); comma = true;
                json(entry.getKey(), out); out.append(':'); json(entry.getValue(), out);
            }
            out.append('}'); return;
        }
        if (value instanceof Iterable<?> list) {
            out.append('['); boolean comma = false;
            for (Object item : list) { if (comma) out.append(','); comma = true; json(item, out); }
            out.append(']'); return;
        }
        throw new IllegalStateException("JSON value " + value.getClass());
    }
    static byte[] encoded(Object value) {
        StringBuilder out = new StringBuilder(); json(value, out); out.append('\n');
        return out.toString().getBytes(StandardCharsets.UTF_8);
    }
    private static void setup(Object world, Object cache, Object storage, Object owner) { }
    private static void begin(Object world, Object cache, int index, String action) { }
    private static void end(Object result, int index, String exceptionClass) { }
    private static void identity(Object value, int reference) { }

    @SuppressWarnings("try")
    static final class Observer implements AutoCloseable {
        final Path archive, parent;
        final boolean client, dedicated, nonempty;
        final String context;
        final URL jar;
        final URLClassLoader loader;
        final Map<String,Object> origins = object(), bindings = new TreeMap<>(), fields = new TreeMap<>();
        final Map<String,Object> blobs = new LinkedHashMap<>();
        final Map<String,byte[]> blobBytes = new LinkedHashMap<>();
        final IdentityHashMap<byte[],String> arrayHashes = new IdentityHashMap<>();
        final Map<String,String> fileHashes = new LinkedHashMap<>();
        final IdentityHashMap<Object,Integer> ids = new IdentityHashMap<>();
        final List<Object> chunks = new ArrayList<>(), operations = new ArrayList<>();
        final List<Object> chunkStates = new ArrayList<>();
        final Map<String,Integer> chunkStateIndices = new LinkedHashMap<>();
        final List<Object> constructionTrace = new ArrayList<>();
        final List<Object> externalObjects = new ArrayList<>(), constructedNBT = new ArrayList<>();
        final IdentityHashMap<Random,byte[]> externalRandomStates = new IdentityHashMap<>();
        boolean readingExternal;
        final Class<?> worldType, chunkType, cacheType, storageType, compoundType;
        final Object world, cache, storage, format, owner, generator, chunkLoader, info, fallback;
        Object competingStorage;
        final Method acquire, save, worldSave, timeSetter, dirty, eligibility;
        boolean closed;
        int index;
        long decodedBlobBytes;
        Observer(Path archive, String context, Path parent, boolean fresh, boolean nonempty) throws Exception {
            this.archive = archive; this.context = context; this.parent = parent; this.nonempty = nonempty;
            client = context.equals("client"); dedicated = context.equals("dedicated");
            jar = archive.toUri().toURL();
            loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader());
            worldType = type(client ? "fd" : "dj"); chunkType = type(client ? "lm" : "hi");
            cacheType = type(dedicated ? "he" : client ? "ok" : "jb");
            storageType = type(client ? "wt" : "om"); compoundType = type(client ? "nu" : "iq");
            if (client) {
                format = construct(type("hi"), new Class<?>[] {File.class}, parent.toFile());
                storage = construct(type("mx"), new Class<?>[] {File.class, String.class, boolean.class}, parent.toFile(), "world", false);
            } else {
                format = null;
                storage = construct(type("ie"), new Class<?>[] {File.class, String.class, boolean.class}, parent.toFile(), "world", false);
            }
            require(storage.getClass() == type(client ? "mx" : "ie"), "actual region save handler");
            if (dedicated) {
                owner = construct(type("net.minecraft.server.MinecraftServer"), new Class<?>[] {});
                world = construct(type("dp"), new Class<?>[] {owner.getClass(), storageType, String.class, int.class, long.class},
                                  owner, storage, "chunk-lifecycle-reference", 0, 23L);
            } else {
                owner = null;
                Object provider = construct(type(client ? "rh" : "kp"), new Class<?>[] {});
                world = construct(worldType, new Class<?>[] {storageType, String.class, long.class, type(client ? "xa" : "os")},
                                  storage, "chunk-lifecycle-reference", 23L, provider);
            }
            cache = get(worldType, world, "v"); info = get(worldType, world, "x");
            require(cache.getClass() == cacheType && get(cacheType, cache, dedicated ? "h" : "g") == world
                    && get(worldType, world, "w") == storage, "actual cache/world/storage ownership");
            generator = get(cacheType, cache, dedicated ? "d" : "c");
            chunkLoader = get(cacheType, cache, dedicated ? "e" : "d");
            fallback = get(cacheType, cache, dedicated ? "c" : "b");
            require(chunkLoader.getClass() == type(client ? "ld" : "hb") && generator.getClass() == type(client ? "yf" : "pk"), "persistent normal path");
            // Eagerly load the finite watched declaring classes before the JDI setup
            // marker.  Merely resolving ld/hb does not load the separate schema
            // class, and copied-world constructors need not open a region yet.
            String[] watchedClasses = client
                ? new String[] {"fd", "lm", "ok", "li", "ld", "to", "mx", "fm", "hc", "yf", "qj"}
                : new String[] {"dj", "hi", dedicated ? "he" : "jb", "hg", "hb", "mg", "ie", "dq", "es", "pk", "jx"};
            for (String watched : watchedClasses) type(watched);
            type(client ? "us" : "my");
            if (nonempty) for (String name : client ? new String[] {"ow", "yk", "sn", "fz"} : new String[] {"jh", "pq", "lq", "eb"}) type(name);
            if (dedicated) require(get(type("dp"), world, "C") == cache && get(type("dp"), world, "F") == owner, "dedicated owners");
            require(get(info.getClass(), info, "a").equals(23L), "actual seed23");
            require(get(info.getClass(), info, "b").equals(0) && get(info.getClass(), info, "c").equals(64)
                    && get(info.getClass(), info, "d").equals(0), "actual spawn");
            require(get(worldType, world, "s").equals(fresh), "genuine fresh/load-aware constructor");
            require(map().size() == (fresh ? 1 : 0) && list().size() == (fresh ? 1 : 0)
                    && pending().isEmpty() && ((List<?>) get(worldType, world, "C")).isEmpty(), "constructor cache/pending/queue baseline");
            acquire = method(cacheType, "c", chunkType, 1, int.class, int.class);
            save = method(cacheType, "a", boolean.class, 1, boolean.class, type(client ? "yb" : "pj"));
            worldSave = method(worldType, "a", void.class, 1, boolean.class, type(client ? "yb" : "pj"));
            timeSetter = method(worldType, "a", void.class, 1, long.class);
            dirty = method(chunkType, client ? "g" : "f", void.class, 1);
            eligibility = method(chunkType, "a", boolean.class, 1, boolean.class);
        }
        Class<?> type(String name) throws Exception {
            Class<?> value = Class.forName(name, true, loader);
            require(value.getClassLoader() == loader && value.getProtectionDomain().getCodeSource() != null
                    && value.getProtectionDomain().getCodeSource().getLocation().equals(jar), "supplied original class " + name);
            origins.put(name, jar.toExternalForm()); return value;
        }
        Field field(Class<?> declaring, String name) throws Exception {
            Field value = declaring.getDeclaredField(name); value.setAccessible(true);
            require(value.getDeclaringClass() == declaring && declaring.getClassLoader() == loader, "declared original field");
            fields.put(declaring.getName() + "." + name, object("class", declaring.getName(), "name", name,
                       "signature", descriptor(value.getType()), "modifiers", value.getModifiers()));
            return value;
        }
        Object get(Class<?> declaring, Object receiver, String name) throws Exception { return field(declaring, name).get(receiver); }
        Method method(Class<?> declaring, String name, Class<?> result, int modifiers, Class<?>... args) throws Exception {
            Method value = declaring.getDeclaredMethod(name, args);
            require(value.getDeclaringClass() == declaring && value.getReturnType() == result && value.getModifiers() == modifiers, "exact original method");
            value.setAccessible(true); String signature = signature(result, args);
            bindings.put(declaring.getName() + "." + name + signature,
                         object("class", declaring.getName(), "method", name, "signature", signature, "modifiers", modifiers));
            return value;
        }
        Object construct(Class<?> declaring, Class<?>[] args, Object... values) throws Exception {
            Constructor<?> value = declaring.getDeclaredConstructor(args);
            require(value.getModifiers() == 1, "exact public original constructor");
            String signature = signature(void.class, args);
            bindings.put(declaring.getName() + ".<init>" + signature,
                         object("class", declaring.getName(), "method", "<init>", "signature", signature, "modifiers", 1));
            List<Object> arguments = new ArrayList<>(); for (Object item : values) arguments.add(argument(item));
            long start = System.currentTimeMillis();
            try {
                Object result = value.newInstance(values); long finish = System.currentTimeMillis();
                constructionTrace.add(object("kind", "constructor", "class", declaring.getName(), "method", "<init>", "signature", signature,
                    "arguments", arguments, "interval_ms", Arrays.asList(start, finish),
                    "result", object("reference", ref(result), "class", result.getClass().getName())));
                return result;
            }
            catch (InvocationTargetException failure) {
                if (!started && failure.getCause() instanceof LinkageError) throw new SetupUnavailable(failure.getCause());
                if (failure.getCause() instanceof Exception e) throw e;
                if (failure.getCause() instanceof Error e) throw e;
                throw failure;
            }
        }
        Integer ref(Object value) {
            if (value == null) return null;
            Integer number = ids.get(value);
            if (number == null) {
                require(ids.size() < 16384, "bounded helper identity inventory");
                number = ids.size() + 1; ids.put(value, number); if (started) identity(value, number);
            }
            return number;
        }
        void publishConstructionIdentities() {
            ids.entrySet().stream().sorted(Map.Entry.comparingByValue()).forEach(entry -> identity(entry.getKey(), entry.getValue()));
        }
        Object argument(Object value) {
            if (value == null || value instanceof Boolean || value instanceof Integer || value instanceof String) return value;
            if (value instanceof Long number) return object("long", hex(number));
            if (value instanceof Double number) return object("binary64", hex(Double.doubleToRawLongBits(number)));
            if (value instanceof Float number) return object("binary32", String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(number)));
            if (value instanceof File file) return object("reference", ref(file), "path", file.toString());
            return object("reference", ref(value));
        }
        Object blob(byte[] raw) throws Exception {
            Map<String,Object> result = storedBlob(raw, arrayHashes.get(raw));
            arrayHashes.put(raw, (String) result.get("sha256")); return result;
        }
        Map<String,Object> storedBlob(byte[] raw, String known) throws Exception {
            require(raw.length <= 8 * 1024 * 1024, "bounded lossless blob");
            if (known != null && Arrays.equals(blobBytes.get(known), raw)) return object("bytes", raw.length, "sha256", known);
            String sha = hash(raw);
            byte[] old = blobBytes.get(sha);
            if (old == null) {
                require(blobs.size() < 4096 && decodedBlobBytes <= 64L * 1024 * 1024 - raw.length, "bounded blob dictionary/decoded bytes");
                decodedBlobBytes += raw.length; blobBytes.put(sha, raw.clone());
                blobs.put(sha, object("bytes", raw.length, "hex", HexFormat.of().formatHex(raw)));
            } else require(Arrays.equals(old, raw), "lossless dictionary/hash collision");
            return object("bytes", raw.length, "sha256", sha);
        }
        Map<?,?> map() throws Exception { return (Map<?,?>) get(cacheType, cache, dedicated ? "f" : "e"); }
        List<?> list() throws Exception { return (List<?>) get(cacheType, cache, dedicated ? "g" : "f"); }
        Set<?> pending() throws Exception { return (Set<?>) get(cacheType, cache, dedicated ? "b" : "a"); }
        void retain(Object chunk) {
            require(chunk != null && (chunk.getClass() == chunkType || chunk == fallback), "actual chunk identity");
            for (Object old : chunks) if (old == chunk) return;
            require(chunks.size() < 128, "bounded retained chunks"); chunks.add(chunk);
        }
        Map<String,Object> chunk(Object value) throws Exception {
            retain(value); Map<String,Object> arrays = object(), flags = object();
            String[] names = {"b", "e", "f", "g", "h"};
            String[] roles = {"blocks", "metadata", "skylight", "block_light", "heightmap"};
            int[] lengths = {32768,16384,16384,16384,256};
            for (int i = 0; i < 5; i++) {
                Object wrapper = get(chunkType, value, names[i]);
                byte[] raw = (byte[]) (i > 0 && i < 4 ? get(type(client ? "wi" : "ob"), wrapper, "a") : wrapper);
                require(raw != null && raw.length == lengths[i], "complete actual array");
                arrays.put(roles[i], object("owner", ref(wrapper), "array", ref(raw), "data", blob(raw)));
            }
            for (String name : new String[] {"c","n","o","p","q"}) flags.put(name, get(chunkType, value, name));
            Map<?,?> tiles = (Map<?,?>) get(chunkType, value, "l");
            require(nonempty ? tiles.size() <= 1 : tiles.isEmpty(), "finite tile-map scope");
            List<Object> tileEntries = new ArrayList<>();
            for (var entry : tiles.entrySet()) {
                retainExternal(entry.getValue()); Class<?> keyClass = type(entry.getKey().getClass().getName());
                Map<String,Object> keyFields = new TreeMap<>();
                for (Field member : keyClass.getDeclaredFields()) if (!Modifier.isStatic(member.getModifiers())) {
                    require(member.getType() == int.class, "finite integer tile position key");
                    keyFields.put(member.getName(), get(keyClass, entry.getKey(), member.getName()));
                }
                tileEntries.add(object("key", ref(entry.getKey()), "key_class", keyClass.getName(), "key_fields", keyFields,
                    "value", ref(entry.getValue())));
            }
            Object[] slices = (Object[]) get(chunkType, value, "m"); require(slices.length == 8, "eight entity slices");
            List<Object> entities = new ArrayList<>();
            for (Object slice : slices) {
                require(slice instanceof List<?> && ((List<?>) slice).size() <= 1, "bounded membership-only entities");
                List<Object> members = new ArrayList<>(); for (Object member : (List<?>) slice) {
                    if (nonempty) retainExternal(member); members.add(entity(member));
                }
                entities.add(object("id", ref(slice), "members", members));
            }
            Map<String,Object> state = object("id", ref(value), "class", value.getClass().getName(), "world", ref(get(chunkType, value, "d")),
                          "coordinates", Arrays.asList(get(chunkType, value, "j"), get(chunkType, value, "k")),
                          "blank", value == fallback, "flags", flags, "last_save", hex((long) get(chunkType, value, "r")),
                          "minimum", get(chunkType, value, "i"), "arrays", arrays,
                          "tiles", object("id", ref(tiles), "entries", tileEntries), "slices_owner", ref(slices), "entities", entities);
            String key = new String(encoded(state), StandardCharsets.US_ASCII);
            Integer index = chunkStateIndices.get(key);
            if (index == null) {
                require(chunkStates.size() < 4096, "bounded lossless chunk-state dictionary");
                index = chunkStates.size(); chunkStateIndices.put(key, index); chunkStates.add(state);
            }
            return object("chunk_reference", index);
        }
        Object entity(Object value) throws Exception {
            Class<?> base = type(client ? "sn" : "lq");
            List<Object> position = new ArrayList<>(), membership = new ArrayList<>();
            for (String name : client ? new String[] {"aM","aN","aO"} : new String[] {"aP","aQ","aR"})
                position.add(hex(Double.doubleToRawLongBits((double) get(base, value, name))));
            for (String name : client ? new String[] {"bF","bG","bH","bI"} : new String[] {"bG","bH","bI","bJ"})
                membership.add(get(base, value, name));
            return object("id", ref(value), "class", value.getClass().getName(), "position_bits", position, "membership", membership);
        }
        Object metadata() throws Exception {
            String[] names = {"seed","spawn_x","spawn_y","spawn_z","time","last_played","size_on_disk","player","player_dimension","level_name","version","raining","rain_time","thundering","thunder_time"};
            Map<String,Object> result = object();
            for (int i = 0; i < names.length; i++) {
                Object value = get(info.getClass(), info, String.valueOf((char) ('a' + i)));
                if (value instanceof Long number) value = hex(number);
                if (i == 7) require(value == null, "empty player state");
                result.put(names[i], value);
            }
            return result;
        }
        Object handler(Object value) throws Exception {
            Class<?> declaring = type(client ? "fm" : "dq");
            return object("id", ref(value), "class", value.getClass().getName(), "token", hex((long) get(declaring, value, "e")),
                "world_root", ((File) get(declaring, value, "b")).toString(),
                "players_root", ((File) get(declaring, value, "c")).toString(),
                "data_root", ((File) get(declaring, value, "d")).toString());
        }
        Object registryMap() throws Exception {
            byte[] table = (byte[]) get(type(client ? "vi" : "nn"), null, "a");
            Object[] entries = (Object[]) get(type(client ? "uu" : "na"), null, "m");
            require(table.length == 256 && entries.length == 256, "actual postload block map/registry lengths");
            for (int i = 0; i < 256; i++) require((table[i] & 255) == (i == 0 || entries[i] != null ? i : 0), "actual postload registry mapping");
            return blob(table);
        }
        Object numeric(Object value, IdentityHashMap<Object,Boolean> seen, int depth) throws Exception {
            if (value == null || value instanceof Boolean || value instanceof Integer || value instanceof String) return value;
            if (value instanceof Byte number) return object("byte", number.intValue());
            if (value instanceof Short number) return object("short", number.intValue());
            if (value instanceof Character unit) return object("char", (int) unit);
            if (value instanceof Long number) return object("long", hex(number));
            if (value instanceof Double number) return object("binary64", hex(Double.doubleToRawLongBits(number)));
            if (value instanceof Float number) return object("binary32", String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(number)));
            if (value == world || value == cache || value == storage) return object("borrowed", ref(value));
            require(depth <= 12 && (!readingExternal || seen.containsKey(value) || seen.size() < 128), "bounded numerical ownership graph");
            if (seen.put(value, true) != null) return object("reference", ref(value));
            Map<String,Object> result = object("id", ref(value), "class", value.getClass().getName());
            if (value instanceof Class<?> constant) {
                require(constant.getClassLoader() == loader && constant.getProtectionDomain().getCodeSource() != null
                        && constant.getProtectionDomain().getCodeSource().getLocation().equals(jar), "original class literal origin");
                origins.put(constant.getName(), jar.toExternalForm()); result.put("class_literal", constant.getName()); return result;
            }
            if (value instanceof Map<?,?> map) {
                require(map.size() <= 16, "bounded externally owned map"); List<Object> entries = new ArrayList<>();
                for (var entry : map.entrySet()) entries.add(object("key", numeric(entry.getKey(), seen, depth + 1),
                    "value", numeric(entry.getValue(), seen, depth + 1)));
                result.put("entries", entries); return result;
            }
            if (value instanceof Collection<?> collection) {
                require(collection.size() <= (readingExternal ? 16 : 256), "bounded original-owned collection");
                List<Object> items = new ArrayList<>();
                for (Object item : collection) items.add(numeric(item, seen, depth + 1));
                result.put("values", items); return result;
            }
            if (value instanceof Random random) {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                try (ObjectOutputStream stream = new ObjectOutputStream(bytes)) { stream.writeObject(random); }
                byte[] raw = bytes.toByteArray();
                if (readingExternal) {
                    byte[] previous = externalRandomStates.putIfAbsent(random, raw.clone());
                    require(previous == null || Arrays.equals(previous, raw), "external Random unchanged within VM");
                }
                result.put("serialized", blob(raw)); return result;
            }
            if (value.getClass().isArray()) {
                int length = java.lang.reflect.Array.getLength(value); require(length <= (readingExternal ? 256 : 32768), "bounded numerical array");
                List<Object> values = new ArrayList<>();
                for (int i = 0; i < length; i++) values.add(numeric(java.lang.reflect.Array.get(value, i), seen, depth + 1));
                result.put("values", values); return result;
            }
            require(value.getClass().getClassLoader() == loader, "original-owned numerical object " + value.getClass().getName());
            type(value.getClass().getName()); Map<String,Object> members = new TreeMap<>();
            for (Class<?> ownerType = value.getClass(); ownerType.getClassLoader() == loader; ownerType = ownerType.getSuperclass()) {
                for (Field member : ownerType.getDeclaredFields()) if (!Modifier.isStatic(member.getModifiers()))
                    members.put(ownerType.getName() + "." + member.getName(), numeric(get(ownerType, value, member.getName()), seen, depth + 1));
            }
            result.put("fields", members); return result;
        }
        void retainExternal(Object value) throws Exception {
            require(nonempty && (value.getClass() == type(client ? "fz" : "eb") || value.getClass() == type(client ? "yk" : "pq")),
                "finite original Boat/Sign external body");
            for (Object old : externalObjects) if (old == value) return;
            require(externalObjects.size() < 8, "finite externally retained objects"); externalObjects.add(value); ref(value);
        }
        Object externalState() throws Exception {
            require(!readingExternal, "nonrecursive external graph read"); readingExternal = true;
            try {
                IdentityHashMap<Object,Boolean> seen = new IdentityHashMap<>(); List<Object> values = new ArrayList<>();
                for (Object value : externalObjects) values.add(numeric(value, seen, 0));
                Object registry = dedicated ? numeric(get(type("dp"), world, "G"), seen, 0) : null;
                Object result = object("encoding", "owned-objects-v1", "objects", values, "dedicated_registry", registry,
                    "random_representation", "actual-java-serialization; raw per-VM state retained", "within_vm_random_unchanged", true);
                require(seen.size() <= 128 && encoded(result).length <= 256 * 1024, "bounded complete external graph"); return result;
            } finally { readingExternal = false; }
        }
        Object files() throws Exception {
            Path root = parent.resolve("world"); Map<String,Object> result = new TreeMap<>(); long total = 0;
            try (var walk = Files.walk(root)) {
                List<Path> paths = walk.sorted().toList(); require(paths.size() <= 1024, "bounded world paths");
                for (Path path : paths) {
                    require(!Files.isSymbolicLink(path), "no owned-world symlinks");
                    if (!Files.isRegularFile(path, LinkOption.NOFOLLOW_LINKS)) continue;
                    long size = Files.size(path); total += size;
                    require(size <= 8 * 1024 * 1024 && total <= 64 * 1024 * 1024 && result.size() < 256, "bounded complete files");
                    String name = root.relativize(path).toString().replace('\\', '/');
                    Map<String,Object> value = storedBlob(Files.readAllBytes(path), fileHashes.get(name));
                    fileHashes.put(name, (String) value.get("sha256")); result.put(name, value);
                }
            }
            return result;
        }
        Object snapshot() throws Exception {
            require(map().size() <= 128 && list().size() <= 256 && pending().size() <= 256, "bounded complete cache");
            List<Object> entries = new ArrayList<>(), insertions = new ArrayList<>();
            for (var entry : map().entrySet()) { retain(entry.getValue()); entries.add(Arrays.asList(entry.getKey(), ref(entry.getValue()))); }
            for (Object value : list()) { retain(value); insertions.add(ref(value)); }
            retain(fallback); List<Object> locals = new ArrayList<>();
            for (Object value : new ArrayList<>(chunks)) locals.add(chunk(value));
            Map<String,Object> registrations = object();
            for (String name : new String[] {"b","c","d","e","D","G"}) {
                List<?> values = (List<?>) get(worldType, world, name);
                require(nonempty ? values.size() <= 2 : values.isEmpty(), "finite world registrations " + name);
                List<Object> members = new ArrayList<>(); for (Object value : values) { retainExternal(value); members.add(ref(value)); }
                registrations.put(name, object("id", ref(values), "values", members));
            }
            List<?> queue = (List<?>) get(worldType, world, "C"); require(queue.isEmpty(), "loaded-only unchanged empty light queue");
            Object provider = get(worldType, world, "t");
            Map<String,Object> state = object("world", ref(world), "cache", ref(cache), "storage", ref(storage), "info", ref(info),
                "provider", ref(provider), "generator", ref(generator), "loader", ref(chunkLoader), "fallback", ref(fallback),
                "map", object("id", ref(map()), "entries", entries), "insertions", object("id", ref(list()), "values", insertions),
                "pending", object("id", ref(pending()), "values", new ArrayList<>(pending())),
                "chunks", locals, "queue", object("id", ref(queue), "values", new ArrayList<>()),
                "registrations", registrations, "metadata", metadata(),
                "world_force_load", get(worldType, world, "y"), "save_off", dedicated ? get(type("dp"), world, "E") : false,
                "provider_force_load", dedicated ? get(cacheType, cache, "a") : null,
                "handler_token", hex((long) get(type(client ? "fm" : "dq"), storage, "e")),
                "handler", handler(storage), "competing_handler", competingStorage == null ? null : handler(competingStorage),
                "files", files());
            if (nonempty) state.put("external_state", externalState());
            if (dedicated) for (String name : new String[] {"c","d","e","f","p"})
                require(get(type("net.minecraft.server.MinecraftServer"), owner, name) == null, "uninitialized owner service retained");
            return state;
        }
        Object invoke(String action, Object receiver, Method target, Object... args) throws Exception {
            require(index < 512, "bounded measured operations");
            Object before = snapshot(); List<Object> arguments = new ArrayList<>();
            for (Object arg : args) arguments.add(argument(arg));
            begin(world, cache, index, action); long start = System.currentTimeMillis();
            boolean fatal = action.equals("fatal_cache_save") || action.equals("fatal_world_save");
            Object result = null, exception = null; String exceptionClass = null;
            try { result = call(target, receiver, args); }
            catch (RuntimeException failure) {
                if (!fatal || failure.getClass() != type(client ? "us" : "my")) throw failure;
                require("The save is being accessed from another location, aborting".equals(failure.getMessage()), "exact actual session conflict");
                exceptionClass = failure.getClass().getName(); exception = object("id", ref(failure), "class", exceptionClass, "message", failure.getMessage());
            }
            long finish = System.currentTimeMillis(); require(!fatal || exception != null, "required actual fatal session exception");
            if (result != null && chunkType.isInstance(result)) retain(result);
            end(result, index, exceptionClass);
            Object projected = result == null || result instanceof Boolean || result instanceof Integer ? result : object("reference", ref(result));
            operations.add(object("index", index++, "action", action, "receiver", ref(receiver),
                "class", target.getDeclaringClass().getName(), "method", target.getName(),
                "signature", signature(target.getReturnType(), target.getParameterTypes()), "arguments", arguments,
                "interval_ms", Arrays.asList(start, finish), "result", projected, "exception", exception, "before", before, "after", snapshot()));
            return result;
        }
        void time(long value) throws Exception { invoke("time", world, timeSetter, value); }
        void mark(int count) throws Exception { for (int i = 0; i < count; i++) invoke("mark_dirty", list().get(i), dirty); }
        Object acquire(int x) throws Exception { return invoke("acquire", cache, acquire, x, 0); }
        void prepare() throws Exception {
            Object template = list().get(0); retain(template); require(!(boolean) get(chunkType, template, "n"), "unpopulated original template");
            Class<?> schema = type(client ? "to" : "mg");
            Method encode = method(schema, "a", void.class, 9, chunkType, worldType, compoundType);
            Method decode = method(schema, "a", chunkType, 9, worldType, compoundType);
            Method coordinate = method(compoundType, "a", void.class, 1, String.class, int.class);
            Method write = method(chunkLoader.getClass(), "a", void.class, 1, worldType, chunkType);
            time(1000L);
            for (int i = 0; i <= 101; i++) {
                Object level = construct(compoundType, new Class<?>[] {});
                call(encode, null, template, world, level); call(coordinate, level, "xPos", i * 16); call(coordinate, level, "zPos", 0);
                Object cloned = call(decode, null, world, level); retain(cloned);
                invoke("constructed_storage_clone_write", chunkLoader, write, world, cloned);
            }
            invoke("prepare_world_save_first", world, worldSave, true, null);
            invoke("prepare_world_save_second", world, worldSave, true, null);
        }
        Object freshNBT() throws Exception { return construct(compoundType, new Class<?>[] {}); }
        void putNBT(Object nbt, String name, Class<?> kind, Object value) throws Exception {
            call(method(compoundType, "a", void.class, 1, String.class, kind), nbt, name, value);
        }
        Object numberNBT(Class<?> kind, Object value) throws Exception {
            Object temp = freshNBT(); putNBT(temp, "v", kind, value);
            return ((Map<?,?>) get(compoundType, temp, "a")).get("v");
        }
        Object listNBT(Object... values) throws Exception {
            Class<?> list = type(client ? "sp" : "lr"), tag = type(client ? "ij" : "fo");
            Object result = construct(list, new Class<?>[] {}); Method add = method(list, "a", void.class, 1, tag);
            for (Object value : values) call(add, result, value); return result;
        }
        Object actorNBT(boolean tile) throws Exception {
            Object result = freshNBT(); putNBT(result, "id", String.class, tile ? "Sign" : "Boat");
            Method setTag = method(compoundType, "a", void.class, 1, String.class, type(client ? "ij" : "fo"));
            if (tile) {
                putNBT(result, "x", int.class, 257); putNBT(result, "y", int.class, 64); putNBT(result, "z", int.class, 1);
                for (int i = 1; i <= 4; i++) putNBT(result, "Text" + i, String.class, i == 1 ? "owned detach" : i == 2 ? "NUL\u0000" : "");
            } else {
                call(setTag, result, "Pos", listNBT(numberNBT(double.class, 258.5), numberNBT(double.class, 65.0), numberNBT(double.class, 2.5)));
                call(setTag, result, "Motion", listNBT(numberNBT(double.class, 0.0), numberNBT(double.class, 0.0), numberNBT(double.class, 0.0)));
                call(setTag, result, "Rotation", listNBT(numberNBT(float.class, 0.0f), numberNBT(float.class, 0.0f)));
                putNBT(result, "FallDistance", float.class, 0.0f); putNBT(result, "Fire", short.class, (short) 0);
                putNBT(result, "Air", short.class, (short) 300); putNBT(result, "OnGround", byte.class, (byte) 1);
            }
            return result;
        }
        void rawNBT(String stage, Object value) throws Exception {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            call(method(type(client ? "as" : "ag"), "a", void.class, 9, compoundType, DataOutput.class), null,
                value, (DataOutput) new DataOutputStream(bytes));
            require(bytes.size() <= 1024 * 1024, "finite typed NBT"); constructedNBT.add(object("stage", stage, "raw", blob(bytes.toByteArray())));
        }
        void prepareDetach() throws Exception {
            require(nonempty && !dedicated, "finite base preparation route"); Object template = list().get(0); retain(template);
            Class<?> schema = type(client ? "to" : "mg");
            Method encode = method(schema, "a", void.class, 9, chunkType, worldType, compoundType);
            Object level = freshNBT(); call(encode, null, template, world, level);
            putNBT(level, "xPos", int.class, 16); putNBT(level, "zPos", int.class, 0);
            byte[] blocks = ((byte[]) call(method(compoundType, "j", byte[].class, 1, String.class), level, "Blocks")).clone();
            require(blocks.length == 32768, "complete constructed blocks"); blocks[(1 << 11) | (1 << 7) | 64] = 63;
            putNBT(level, "Blocks", byte[].class, blocks);
            Method setTag = method(compoundType, "a", void.class, 1, String.class, type(client ? "ij" : "fo"));
            call(setTag, level, "Entities", listNBT(actorNBT(false))); call(setTag, level, "TileEntities", listNBT(actorNBT(true)));
            rawNBT("constructed_level", level);
            Object cloned = call(method(schema, "a", chunkType, 9, worldType, compoundType), null, world, level); retain(cloned);
            Object encoded = freshNBT(); call(encode, null, cloned, world, encoded); rawNBT("factory_reserialized_level", encoded);
            time(1000L); invoke("constructed_external_storage_write", chunkLoader,
                method(chunkLoader.getClass(), "a", void.class, 1, worldType, chunkType), world, cloned);
            invoke("prepare_world_save_first", world, worldSave, true, null); invoke("prepare_world_save_second", world, worldSave, true, null);
        }
        void detachOwned() throws Exception {
            require(nonempty, "finite external detach profile"); time(6000L); Object target = acquire(16);
            require(((Map<?,?>) get(chunkType, target, "l")).size() == 1 && externalObjects.size() == 2,
                "genuine loaded Boat and Sign");
            if (dedicated) {
                invoke("drop_external", cache, method(cacheType, "d", void.class, 1, int.class, int.class), 16, 0);
                invoke("unload_external", cache, method(cacheType, "a", boolean.class, 1));
            } else invoke("direct_detach_external", target, method(chunkType, client ? "f" : "e", void.class, 1));
        }
        void cohort() throws Exception {
            time(1000L); for (int i = 0; i < 24; i++) acquire(16 * i);
            Object target = list().get(0);
            invoke("clean_eligibility", target, eligibility, false); invoke("clean_eligibility", target, eligibility, true);
            invoke("blank_eligibility", fallback, method(fallback.getClass(), "a", boolean.class, 1, boolean.class), false);
            invoke("blank_eligibility", fallback, method(fallback.getClass(), "a", boolean.class, 1, boolean.class), true);
            mark(24); invoke("exact24_incremental", cache, save, false, null);
            acquire(384); mark(25); time(2000L);
            invoke("save25_cutoff", cache, save, false, null); invoke("save25_continuation", cache, save, false, null);
            invoke("clean_forced", cache, save, true, null);
            invoke("mark_dirty", target, dirty); time(3000L); invoke("world_forced", world, worldSave, true, null);
            mark(25); time(4000L); invoke("world_incremental", world, worldSave, false, null);
            invoke("world_continuation", cache, save, false, null);
            if (client) {
                mark(24); Method incremental = method(worldType, "a", boolean.class, 1, int.class);
                invoke("client_nonzero_pass", world, incremental, 1); invoke("client_zero_pass", world, incremental, 0);
            }
            time(Long.MAX_VALUE - 300); invoke("mark_dirty", target, dirty); invoke("stamp_overflow_base", cache, save, false, null);
            int x = (int) get(chunkType, target, "j");
            Object boat = construct(type(client ? "fz" : "eb"), new Class<?>[] {worldType, double.class, double.class, double.class}, world, x * 16.0 + 0.5, 64.0, 0.5);
            Class<?> entity = type(client ? "sn" : "lq");
            invoke("api_owned_membership_add", target, method(chunkType, "a", void.class, 1, entity), boat);
            invoke("api_owned_membership_remove", target, method(chunkType, "b", void.class, 1, entity), boat);
            invoke("overflow_equal_forced", target, eligibility, true); invoke("overflow_equal_incremental", target, eligibility, false);
            time(Long.MIN_VALUE + 298); invoke("overflow_before_deadline", target, eligibility, false);
            time(Long.MIN_VALUE + 299); invoke("overflow_at_deadline", target, eligibility, false);
            invoke("forced_distinct", target, eligibility, true); time(5000L); invoke("refresh_empty_serializer_q", cache, save, true, null);
            invoke("empty_maintenance", cache, method(cacheType, "a", boolean.class, 1));
            if (dedicated) {
                invoke("drop", cache, method(cacheType, "d", void.class, 1, int.class, int.class), 16, 0);
                invoke("unload_one", cache, method(cacheType, "a", boolean.class, 1)); acquire(16);
            }
            time(8000L); invoke("mark_dirty", target, dirty); compete(); invoke("fatal_cache_save", cache, save, false, null);
        }
        void compete() throws Exception {
            require(competingStorage == null && index < 512, "one actual competing handler");
            Object before = snapshot(); File directory = parent.toFile(); ref(directory);
            begin(world, cache, index, "competing_handler_constructor"); long start = System.currentTimeMillis();
            competingStorage = construct(type(client ? "mx" : "ie"), new Class<?>[] {File.class, String.class, boolean.class}, directory, "world", false);
            long finish = System.currentTimeMillis();
            require(!get(type(client ? "fm" : "dq"), competingStorage, "e").equals(get(type(client ? "fm" : "dq"), storage, "e")), "genuine distinct handler clock tokens");
            end(competingStorage, index, null);
            operations.add(object("index", index++, "action", "competing_handler_constructor", "receiver", ref(competingStorage),
                "class", competingStorage.getClass().getName(), "method", "<init>", "signature", "(Ljava/io/File;Ljava/lang/String;Z)V",
                "arguments", Arrays.asList(argument(directory), "world", false), "interval_ms", Arrays.asList(start, finish),
                "result", object("reference", ref(competingStorage)), "exception", null, "before", before, "after", snapshot()));
        }
        void sessionWorld() throws Exception {
            time(6000L); Object target = acquire(0); invoke("mark_dirty", target, dirty); time(8000L); compete();
            invoke("fatal_world_save", world, worldSave, true, null);
        }
        void unload101() throws Exception {
            require(dedicated, "dedicated natural pending producer"); time(6000L);
            batch("batch_acquire101", acquire, true);
            Method drop = method(cacheType, "d", void.class, 1, int.class, int.class);
            batch("batch_drop101", drop, false);
            invoke("unload100", cache, method(cacheType, "a", boolean.class, 1));
            invoke("unload_last", cache, method(cacheType, "a", boolean.class, 1)); acquire(16); acquire(1616);
        }
        void batch(String action, Method target, boolean admission) throws Exception {
            require(index < 512, "bounded batch operation"); Object before = snapshot(); List<Object> calls = new ArrayList<>();
            begin(world, cache, index, action); long start = System.currentTimeMillis();
            for (int i = 1; i <= 101; i++) {
                Object result = call(target, cache, i * 16, 0);
                if (admission) { require(result != null && chunkType.isInstance(result), "actual batch acquisition result"); retain(result); }
                else require(result == null, "actual drop void result");
                calls.add(object("index", i - 1, "arguments", Arrays.asList(i * 16, 0), "result", result == null ? null : object("reference", ref(result))));
            }
            long finish = System.currentTimeMillis(); end(null, index, null);
            operations.add(object("index", index++, "action", action, "receiver", ref(cache), "class", target.getDeclaringClass().getName(),
                "method", target.getName(), "signature", signature(target.getReturnType(), target.getParameterTypes()),
                "arguments", object("first", 1, "last", 101, "multiplier", 16, "z", 0), "calls", calls,
                "interval_ms", Arrays.asList(start, finish), "result", null, "exception", null, "before", before, "after", snapshot(),
                "observation_scope", "Complete outer batch payloads; every actual call individually traced with all headers and affected target payloads."));
        }
        @Override public void close() throws Exception {
            if (closed) return;
            if (client) call(method(type("hi"), "c", void.class, 1), format);
            else call(method(type("ie"), "e", void.class, 1), storage);
            require(((Map<?,?>) get(type(client ? "rj" : "kq"), null, "a")).isEmpty(), "actual region-cache close");
            closed = true; loader.close();
        }
    }
    static Object threads(ClassLoader loader, Object owner, Set<String> originals) throws Exception {
        List<Object> result = new ArrayList<>(); var inventory = Thread.getAllStackTraces();
        require(inventory.size() <= 64, "bounded thread inventory"); int inert = 0;
        for (var entry : inventory.entrySet()) {
            Thread thread = entry.getKey(); require(entry.getValue().length <= 128, "bounded thread frames");
            List<Object> frames = new ArrayList<>(); boolean original = false;
            for (StackTraceElement frame : entry.getValue()) {
                frames.add(frame.toString());
                original |= originals.contains(frame.getClassName());
                if (originals.contains(frame.getClassName())) require(frame.getClassName().equals("bt"), "no unexpected original thread frame");
            }
            if (original) {
                require(owner != null && thread.getClass().getName().equals("bt") && thread.getClass().getClassLoader() == loader
                        && thread.isDaemon() && entry.getValue().length >= 2, "exact inert dedicated daemon");
                Field stored = thread.getClass().getDeclaredField("a"); stored.setAccessible(true);
                require(stored.get(thread) == owner, "inert daemon retained owner");
                for (StackTraceElement frame : entry.getValue()) require(frame.getClassName().equals("bt") && frame.getMethodName().equals("run")
                        || frame.getClassName().equals("java.lang.Thread") && frame.getMethodName().startsWith("sleep"), "inert daemon sleep-only frames");
                inert++;
            } else require(thread == Thread.currentThread() || thread.isDaemon(), "only measured main non-daemon");
            result.add(object("id", thread.getId(), "name", thread.getName(), "daemon", thread.isDaemon(), "state", thread.getState().name(),
                              "original_frame", original, "frames", frames));
        }
        require(inert == (owner == null ? 0 : 1), "exact original inert daemon count"); return result;
    }
    @SuppressWarnings("try")
    public static void main(String[] args) {
        int status = 1; System.setOut(System.err);
        try {
            require(args.length == 6, "jar client|base|dedicated prepare|prepare-detach|cohort|unload101|session-world|detach-owned seed_hex owned_parent snapshot_json");
            String context = args[1], mode = args[2];
            require(Arrays.asList("client","base","dedicated").contains(context) && Arrays.asList("prepare","prepare-detach","cohort","unload101","session-world","detach-owned").contains(mode)
                    && args[3].equals("0000000000000017") && (!(mode.equals("prepare") || mode.equals("prepare-detach")) || !context.equals("dedicated")), "fixed context/scenario/seed");
            Path archive = Path.of(args[0]).toRealPath(), parent = Path.of(args[4]).toRealPath(), output = Path.of(args[5]).toAbsolutePath().normalize();
            require(!Files.isSymbolicLink(parent) && Files.isDirectory(parent) && Files.isDirectory(output.getParent())
                    && !Files.exists(output, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(output) && !output.startsWith(parent), "owned separate create-new output");
            require((mode.equals("prepare") || mode.equals("prepare-detach")) ? !Files.exists(parent.resolve("world")) : Files.isRegularFile(parent.resolve("world/level.dat")), "new preparation or complete saved copy");
            String expected = context.equals("client") ? CLIENT : SERVER;
            require(hash(Files.readAllBytes(archive)).equals(expected), "pinned original archive");
            Set<String> originals = new HashSet<>();
            try (ZipFile zip = new ZipFile(archive.toFile())) {
                var entries = zip.entries();
                while (entries.hasMoreElements()) {
                    String name = entries.nextElement().getName();
                    if (name.endsWith(".class")) originals.add(name.substring(0, name.length() - 6).replace('/', '.'));
                }
            }
            require(originals.size() <= 4096, "bounded original class-name inventory");
            try (Observer observer = new Observer(archive, context, parent, mode.equals("prepare") || mode.equals("prepare-detach"), mode.equals("prepare-detach") || mode.equals("detach-owned"))) {
                new HashSet<Integer>().iterator();
                started = true; setup(observer.world, observer.cache, observer.storage, observer.owner); observer.publishConstructionIdentities();
                Object blockMap = observer.registryMap();
                Object beforeThreads = threads(observer.loader, observer.owner, originals), initial = observer.snapshot();
                Object initialProvider = observer.numeric(observer.generator, new IdentityHashMap<>(), 0);
                if (mode.equals("prepare")) observer.prepare(); else if (mode.equals("prepare-detach")) observer.prepareDetach();
                else if (mode.equals("detach-owned")) observer.detachOwned(); else if (mode.equals("cohort")) observer.cohort();
                else if (mode.equals("session-world")) observer.sessionWorld(); else observer.unload101();
                Object finalState = observer.snapshot(), finalProvider = observer.numeric(observer.generator, new IdentityHashMap<>(), 0);
                require(blockMap.equals(observer.registryMap()), "actual normalization map retained");
                observer.close();
                Map<String,Object> record = object("schema", 1, "scope", "original-chunk-lifecycle-constructor-api", "context", context, "mode", mode,
                    "seed_hex", args[3], "jar_sha256", expected, "class_origins", observer.origins, "bindings", observer.bindings, "fields", observer.fields,
                    "construction", (mode.equals("prepare") || mode.equals("prepare-detach")) ? "original-derived storage clones via real schema/NBT/region APIs; no generated-at-target terrain claim" :
                    "fresh copy of original-only API-constructed storage; actual world/cache/loader; temporary entity add/remove is API-owned membership setup",
                    "initial", initial, "operations", observer.operations, "final", finalState,
                    "provider_state_initial", initialProvider, "provider_state_final", finalProvider,
                    "provider_state_unchanged", initialProvider.equals(finalProvider), "blobs", observer.blobs,
                    "state_encoding", "chunks-indexed-v1", "chunk_states", observer.chunkStates,
                    "construction_trace", observer.constructionTrace,
                    "constructor_route", context.equals("client") ? "direct-public-mx-and-fd; real-hi-close" : context.equals("base") ? "direct-public-ie-and-dj" : "direct-public-owner-ie-dp; no-server-start",
                    "block_map", blockMap,
                    "threads_before", beforeThreads, "threads_after", threads(observer.loader, observer.owner, originals),
                    "limitations", observer.nonempty ? "Finite API-constructed Boat/Sign storage and actual activation/detach bodies only; no entity tick/feature simulation, general cleanup, or external-view retirement claim. Raw per-VM unconsumed entity Random retained; no seeded replacement." :
                    "Dedicated owner constructor only; E=true/base pending injection/nonempty feature bodies not measured. Session failure ends original-world API work; no recovery claim.");
                if (observer.nonempty) record.put("constructed_nbt", observer.constructedNBT);
                require(hash(Files.readAllBytes(archive)).equals(expected), "original archive unchanged");
                byte[] raw = encoded(record); require(raw.length <= MAX_RECORD, "complete snapshot budget");
                Files.write(output, raw, StandardOpenOption.CREATE_NEW); status = 0;
            }
        } catch (Throwable failure) {
            boolean unavailable = !started && failure instanceof SetupUnavailable;
            System.err.println(unavailable ? "UNAVAILABLE: original constructor dependency" : "FAILURE: chunk lifecycle observation");
            failure.printStackTrace(System.err); if (unavailable) status = 77;
        }
        System.exit(status);
    }
}
