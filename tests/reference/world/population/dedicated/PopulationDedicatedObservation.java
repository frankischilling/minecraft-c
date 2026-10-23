import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Array;
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
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.atomic.AtomicLong;
import java.util.zip.ZipFile;

/** Original-only preparation: real dedicated construction and natural terrain.
 * No original field assignments, prepared Chunk records, native comparison or
 * execution-equivalence claim. Compile with the four unchanged project helpers
 * listed in the adjacent notes. The runner owns fresh directories and processes.
 */
public final class PopulationDedicatedObservation {
    private static final String SEED = "0000000000000017";
    private static final String PROFILE = "dedicated-natural-unload";
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final int MAX_STATE = 8 * 1024 * 1024;
    private static final int MAX_OBJECTS = 8192;
    private static final int MAX_ARRAY = 32768;
    private static final int MAX_COLLECTION = 32768;
    private static final String TITLE = "dedicated-population-reference";
    private static final int[][] ORIGIN = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {1, 1}};
    private static final int[][] FAR = {{16, 0}, {17, 0}, {16, 1}, {17, 1}};

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Map<String, Object> object(Object... pairs) {
        return ChunkLifecycleObservation.object(pairs);
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception exception) throw exception;
            if (cause instanceof Error error) throw error;
            throw failure;
        }
    }

    private static String utf16(String value) {
        if (value == null) return null;
        StringBuilder result = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i)
            result.append(String.format(Locale.ROOT, "%04x", (int) value.charAt(i)));
        return result.toString();
    }

    private static Object scalar(Class<?> type, Object value) {
        if (type == long.class) return ChunkLifecycleObservation.hex((Long) value);
        if (type == double.class)
            return ChunkLifecycleObservation.hex(Double.doubleToRawLongBits((Double) value));
        if (type == float.class)
            return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits((Float) value));
        if (type == char.class) return (int) (Character) value;
        return value;
    }

    /** Use the unchanged copied-Random observer without drawing the real owner. */
    private static final class RandomReader {
        final Object receiver;
        final Method snapshot, seedOwner;

        RandomReader() throws Exception {
            Class<?> type = Class.forName("NaturalPopulationObservation$RandomFields");
            require(type.getClassLoader() == PopulationDedicatedObservation.class.getClassLoader()
                    && type.getDeclaringClass() == NaturalPopulationObservation.class
                    && type.getModifiers() == 26, "unchanged project RandomFields helper");
            Constructor<?> constructor = type.getDeclaredConstructor();
            require(constructor.getModifiers() == 0, "unchanged RandomFields constructor");
            constructor.setAccessible(true);
            receiver = constructor.newInstance();
            snapshot = type.getDeclaredMethod("snapshot", Random.class);
            seedOwner = type.getDeclaredMethod("seedOwner", Random.class);
            require(snapshot.getReturnType() == Map.class && snapshot.getModifiers() == 0
                    && seedOwner.getReturnType() == AtomicLong.class && seedOwner.getModifiers() == 0,
                    "unchanged passive Random observer signatures");
            snapshot.setAccessible(true);
            seedOwner.setAccessible(true);
        }

        Object snapshot(Random random) throws Exception { return invoke(snapshot, receiver, random); }
        AtomicLong seed(Random random) throws Exception { return (AtomicLong) invoke(seedOwner, receiver, random); }
    }

    /** Same original public constructor/API route as the lifecycle helper.
     * Its Observer constructor cannot be reused: it asserts cache/queue sizes.
     * This bridge deliberately observes whatever construction actually leaves.
     */
    private static final class Original implements AutoCloseable {
        final Path root;
        final URL jar;
        final URLClassLoader loader;
        final Class<?> worldType, chunkType, cacheType;
        Object world, cache, storage, owner, info, fallback, preparingHandler;
        Method acquire, worldSave, timeSetter;
        final Map<String, Object> origins = new TreeMap<>(), bindings = new TreeMap<>(), fields = new TreeMap<>();
        final Map<String, Object> blobs = new LinkedHashMap<>();
        final Map<String, byte[]> blobBytes = new LinkedHashMap<>();
        final IdentityHashMap<Object, Integer> ids = new IdentityHashMap<>();
        final List<Object> chunks = new ArrayList<>(), constructionTrace = new ArrayList<>(), preparationCalls = new ArrayList<>();
        Object preparation;
        long constructionLow, constructionHigh;
        long decodedBlobBytes;
        boolean closed;

        Original(Path archive, Path root) throws Exception {
            this.root = root; jar = archive.toUri().toURL();
            loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader());
            worldType = type("dj"); chunkType = type("hi"); cacheType = type("he");
        }

        void constructWorld() throws Exception {
            Class<?> handlerType = type("ie"), infoType = type("ct");
            Object preparing = construct(handlerType, new Class<?>[] {File.class, String.class, boolean.class}, root.toFile(), "world", false);
            preparingHandler = preparing;
            Object preparedInfo = construct(infoType, new Class<?>[] {long.class, String.class}, 23L, TITLE);
            preparationCall("actual_info_spawn", preparedInfo, method(infoType, "a", void.class, 1, int.class, int.class, int.class), 0, 64, 0);
            List<Object> emptyPlayers = new ArrayList<>();
            preparationCall("actual_handler_metadata_save", preparing,
                    method(handlerType, "a", void.class, 1, infoType, List.class), preparedInfo, emptyPlayers);
            Object readback = preparationCall("actual_prepared_info_readback", preparing,
                    method(type("dq"), "c", infoType, 1));
            require(readback != null && readback.getClass() == infoType
                    && get(infoType, readback, "a").equals(23L) && get(infoType, readback, "b").equals(0)
                    && get(infoType, readback, "c").equals(64) && get(infoType, readback, "d").equals(0),
                    "actual metadata read API confirms prepared seed/spawn");
            preparationCall("actual_preparation_handler_close", preparing, method(handlerType, "e", void.class, 1));
            Path level = root.resolve("world/level.dat"), session = root.resolve("world/session.lock");
            require(Files.isRegularFile(level, LinkOption.NOFOLLOW_LINKS) && Files.size(level) <= 4 * 1024 * 1024
                    && Files.isRegularFile(session, LinkOption.NOFOLLOW_LINKS) && Files.size(session) == 8,
                    "actual metadata API produced complete existing-world inputs");
            preparation = object("scope", "actual original metadata APIs only; no prepared Chunk or World mutation",
                    "handler", reference(preparing), "info", infoFields(preparedInfo),
                    "readback_info", infoFields(readback),
                    "supplied_players", object("object", reference(emptyPlayers), "size", emptyPlayers.size()),
                    "operations", preparationCalls, "level_dat", storedBlob(Files.readAllBytes(level), null),
                    "session_lock", storedBlob(Files.readAllBytes(session), null));
            constructionLow = System.currentTimeMillis();
            storage = construct(handlerType, new Class<?>[] {File.class, String.class, boolean.class}, root.toFile(), "world", false);
            owner = construct(type("net.minecraft.server.MinecraftServer"), new Class<?>[] {});
            world = construct(type("dp"), new Class<?>[] {owner.getClass(), type("om"), String.class, int.class, long.class},
                    owner, storage, TITLE, 0, 23L);
            constructionHigh = System.currentTimeMillis();
            require(constructionHigh >= constructionLow, "ordered real dedicated construction epoch");
            cache = get(worldType, world, "v"); info = get(worldType, world, "x");
            require(cache != null && cache.getClass() == cacheType && get(cacheType, cache, "h") == world
                    && get(type("dp"), world, "C") == cache && get(worldType, world, "w") == storage,
                    "actual dedicated/base cache and handler correspondence");
            require(get(worldType, world, "s").equals(false), "actual dp constructor used existing metadata");
            fallback = get(cacheType, cache, "c");
            require(fallback != null && fallback.getClass() == type("hg"), "actual dedicated fallback owner");
            acquire = method(cacheType, "c", chunkType, 1, int.class, int.class);
            worldSave = method(worldType, "a", void.class, 1, boolean.class, type("pj"));
            timeSetter = method(worldType, "a", void.class, 1, long.class);
        }

        Class<?> type(String name) throws Exception {
            Class<?> value = Class.forName(name, false, loader);
            require(value.getClassLoader() == loader && value.getProtectionDomain().getCodeSource() != null
                    && jar.equals(value.getProtectionDomain().getCodeSource().getLocation()), "exact original class " + name);
            origins.put(name, jar.toExternalForm()); return value;
        }

        Field field(Class<?> declaring, String name) throws Exception {
            Field value = declaring.getDeclaredField(name); value.setAccessible(true);
            fields.put(declaring.getName() + "." + name,
                    object("class", declaring.getName(), "field", name, "descriptor", value.getType().descriptorString(),
                            "modifiers", value.getModifiers()));
            return value;
        }

        Object get(Class<?> declaring, Object receiver, String name) throws Exception { return field(declaring, name).get(receiver); }

        Method method(Class<?> declaring, String name, Class<?> result, int modifiers, Class<?>... arguments) throws Exception {
            Method value = declaring.getDeclaredMethod(name, arguments);
            require(value.getReturnType() == result && value.getModifiers() == modifiers, "exact original method " + declaring.getName() + "." + name);
            value.setAccessible(true);
            String signature = ChunkLifecycleObservation.signature(result, arguments);
            bindings.put(declaring.getName() + "." + name + signature,
                    object("class", declaring.getName(), "method", name, "descriptor", signature, "modifiers", modifiers));
            return value;
        }

        Object construct(Class<?> declaring, Class<?>[] parameters, Object... arguments) throws Exception {
            Constructor<?> constructor = declaring.getDeclaredConstructor(parameters);
            require(constructor.getModifiers() == 1, "exact public original constructor " + declaring.getName());
            List<Object> args = new ArrayList<>();
            for (Object value : arguments) args.add(argument(value));
            Map<String, Object> trace = object("class", declaring.getName(), "descriptor",
                    ChunkLifecycleObservation.signature(void.class, parameters), "arguments", args);
            constructionTrace.add(trace);
            long low = System.currentTimeMillis();
            try {
                Object result = constructor.newInstance(arguments);
                trace.put("result", reference(result)); trace.put("epoch_ms", Arrays.asList(low, System.currentTimeMillis()));
                return result;
            } catch (InvocationTargetException caught) {
                Throwable cause = caught.getCause();
                trace.put("failure", object("class", cause.getClass().getName(), "message", cause.getMessage()));
                trace.put("epoch_ms", Arrays.asList(low, System.currentTimeMillis()));
                if (cause instanceof Exception exception) throw exception;
                if (cause instanceof Error error) throw error;
                throw caught;
            }
        }

        Object preparationCall(String action, Object receiver, Method method, Object... arguments) throws Exception {
            long low = System.currentTimeMillis();
            Object result = invoke(method, receiver, arguments);
            List<Object> args = new ArrayList<>();
            for (Object value : arguments) args.add(argument(value));
            preparationCalls.add(object("action", action, "receiver", reference(receiver), "class", method.getDeclaringClass().getName(),
                    "method", method.getName(), "descriptor", ChunkLifecycleObservation.signature(method.getReturnType(), method.getParameterTypes()),
                    "arguments", args, "result", method.getReturnType().isPrimitive()
                            ? scalar(method.getReturnType(), result) : reference(result),
                    "epoch_ms", Arrays.asList(low, System.currentTimeMillis())));
            return result;
        }

        Integer ref(Object value) {
            if (value == null) return null;
            Integer result = ids.get(value);
            if (result == null) {
                require(ids.size() < 16384, "bounded actual retained identity namespace");
                result = ids.size() + 1; ids.put(value, result);
            }
            return result;
        }

        Object reference(Object value) { return value == null ? null : object("ref", ref(value), "class", value.getClass().getName()); }

        Object argument(Object value) {
            if (value == null || value instanceof Boolean || value instanceof Integer) return value;
            if (value instanceof String text) return object("utf16", utf16(text));
            if (value instanceof Long number) return object("long", ChunkLifecycleObservation.hex(number));
            return reference(value);
        }

        Map<?, ?> map() throws Exception { return (Map<?, ?>) get(cacheType, cache, "f"); }
        List<?> list() throws Exception { return (List<?>) get(cacheType, cache, "g"); }

        void retain(Object value) {
            require(value != null && (value.getClass() == chunkType || value == fallback), "actual ordinary/fallback Chunk");
            for (Object previous : chunks) if (previous == value) return;
            require(chunks.size() < 128, "bounded exact retained Chunk inventory"); chunks.add(value);
        }

        Object storedBlob(byte[] raw, String known) throws Exception {
            require(raw.length <= 8 * 1024 * 1024, "complete bounded raw transport");
            String hash = ChunkLifecycleObservation.hash(raw);
            byte[] previous = blobBytes.get(hash);
            if (previous == null) {
                require(blobs.size() < 4096 && decodedBlobBytes <= MAX_OUTPUT - raw.length, "complete unique raw-byte dictionary bound");
                decodedBlobBytes += raw.length; blobBytes.put(hash, raw.clone());
                blobs.put(hash, object("bytes", raw.length, "hex", java.util.HexFormat.of().formatHex(raw)));
            } else require(Arrays.equals(previous, raw), "lossless raw-byte dictionary");
            return object("bytes", raw.length, "sha256", hash);
        }

        Object infoFields(Object actualInfo) throws Exception {
            List<Object> result = new ArrayList<>();
            for (char name = 'a'; name <= 'o'; ++name) {
                Field member = field(actualInfo.getClass(), Character.toString(name));
                Object value = member.get(actualInfo);
                result.add(object("field", member.getName(), "descriptor", member.getType().descriptorString(),
                        "value", member.getType().isPrimitive() ? scalar(member.getType(), value)
                                : member.getType() == String.class ? object("object", reference(value), "utf16", utf16((String) value))
                                : reference(value)));
            }
            return object("object", reference(actualInfo), "fields", result);
        }

        @Override public void close() throws Exception {
            if (closed) return;
            Object handler = storage == null ? preparingHandler : storage;
            if (handler != null) {
                invoke(method(handler.getClass(), "e", void.class, 1), handler);
                require(((Map<?, ?>) get(type("kq"), null, "a")).isEmpty(), "actual region-cache close");
            }
            closed = true; loader.close();
        }
    }

    /** One complete reachable instance graph per boundary, with real identities.
     * Primitive fields are values; reflection's temporary boxing is never given
     * a source identity. Reference-valued boxed keys remain actual source owners.
     */
    private static final class Graph {
        final Experiment experiment;
        final Original source;
        final IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>();
        final ArrayDeque<Object> pending = new ArrayDeque<>();
        final List<Object> objects = new ArrayList<>();

        Graph(Experiment experiment) { this.experiment = experiment; source = experiment.source; }

        Object reference(Object value) {
            if (value == null) return null;
            if (seen.put(value, Boolean.TRUE) == null) {
                require(seen.size() <= MAX_OBJECTS, "bounded complete reached object graph");
                pending.add(value);
            }
            return experiment.reference(value);
        }

        Object primitiveArray(Object value, Class<?> component, int length) throws Exception {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            try (DataOutputStream output = new DataOutputStream(bytes)) {
                for (int i = 0; i < length; ++i) {
                    if (component == byte.class) output.writeByte(Array.getByte(value, i));
                    else if (component == boolean.class) output.writeByte(Array.getBoolean(value, i) ? 1 : 0);
                    else if (component == short.class) output.writeShort(Array.getShort(value, i));
                    else if (component == char.class) output.writeChar(Array.getChar(value, i));
                    else if (component == int.class) output.writeInt(Array.getInt(value, i));
                    else if (component == long.class) output.writeLong(Array.getLong(value, i));
                    else if (component == float.class) output.writeInt(Float.floatToRawIntBits(Array.getFloat(value, i)));
                    else if (component == double.class) output.writeLong(Double.doubleToRawLongBits(Array.getDouble(value, i)));
                    else throw new IllegalStateException("unsupported primitive array component");
                }
            }
            // This is a helper encoding buffer, not an original byte[] owner.
            return source.storedBlob(bytes.toByteArray(), null);
        }

        Object describe(Object value) throws Exception {
            Class<?> type = value.getClass();
            Map<String, Object> result = object("object", experiment.reference(value));
            if (value instanceof String text) {
                result.put("utf16", utf16(text));
            } else if (value instanceof Boolean || value instanceof Byte || value instanceof Short
                    || value instanceof Integer || value instanceof Long || value instanceof Float
                    || value instanceof Double || value instanceof Character) {
                Class<?> primitive = value instanceof Boolean ? boolean.class : value instanceof Byte ? byte.class
                        : value instanceof Short ? short.class : value instanceof Integer ? int.class
                        : value instanceof Long ? long.class : value instanceof Float ? float.class
                        : value instanceof Double ? double.class : char.class;
                result.put("value_type", primitive.descriptorString());
                result.put("value", scalar(primitive, value));
            } else if (value instanceof Random random) {
                require(type == Random.class, "actual bootstrap Random owner");
                result.put("seed_owner", reference(experiment.random.seed(random)));
                result.put("state", experiment.random.snapshot(random));
            } else if (value instanceof AtomicLong number) {
                require(type == AtomicLong.class, "actual bootstrap AtomicLong owner");
                result.put("raw_long", ChunkLifecycleObservation.hex(number.get()));
            } else if (value instanceof File file) {
                require(type == File.class, "actual bootstrap File path binding");
                result.put("path", reference(file.getPath()));
                result.put("absolute", file.isAbsolute());
            } else if (value instanceof Class<?> constant) {
                experiment.origin(constant);
                result.put("class_literal", constant.getName());
            } else if (type.isArray()) {
                int length = Array.getLength(value);
                require(length <= MAX_ARRAY, "complete array within explicit element bound");
                Class<?> component = type.getComponentType();
                result.put("component", component.descriptorString());
                result.put("length", length);
                if (component.isPrimitive()) result.put("big_endian_data", primitiveArray(value, component, length));
                else {
                    List<Object> entries = new ArrayList<>();
                    for (int i = 0; i < length; ++i) entries.add(reference(Array.get(value, i)));
                    result.put("entries", entries);
                }
            } else if (type.getClassLoader() == null && value instanceof Map<?, ?> map) {
                int size = map.size();
                require(size <= MAX_COLLECTION, "complete bootstrap Map entry bound");
                List<Object> entries = new ArrayList<>();
                for (Map.Entry<?, ?> entry : map.entrySet())
                    entries.add(object("ordinal", entries.size(), "key", reference(entry.getKey()), "value", reference(entry.getValue())));
                require(map.size() == size && entries.size() == size, "stable complete actual Map iteration");
                result.put("size", size);
                result.put("entries", entries);
            } else if (type.getClassLoader() == null && value instanceof Collection<?> collection) {
                int size = collection.size();
                require(size <= MAX_COLLECTION, "complete bootstrap Collection entry bound");
                List<Object> entries = new ArrayList<>();
                for (Object item : collection) entries.add(reference(item));
                require(collection.size() == size && entries.size() == size, "stable complete actual Collection iteration");
                result.put("size", size);
                result.put("entries", entries);
            } else {
                experiment.origin(type);
                if (value instanceof Enum<?> constant) {
                    result.put("enum_name", reference(constant.name()));
                    result.put("enum_ordinal", constant.ordinal());
                }
                List<Object> fields = new ArrayList<>();
                for (Class<?> owner = type; owner != null && owner.getClassLoader() == source.loader; owner = owner.getSuperclass()) {
                    experiment.origin(owner);
                    List<Field> declared = new ArrayList<>(Arrays.asList(owner.getDeclaredFields()));
                    declared.sort(Comparator.comparing(Field::getName));
                    for (Field member : declared) if (!Modifier.isStatic(member.getModifiers())) {
                        member.setAccessible(true);
                        Object actual = member.get(value);
                        fields.add(object("declaring_class", owner.getName(), "field", member.getName(),
                                "descriptor", member.getType().descriptorString(), "modifiers", member.getModifiers(),
                                "value", member.getType().isPrimitive() ? scalar(member.getType(), actual) : reference(actual)));
                    }
                }
                result.put("fields", fields);
            }
            return result;
        }

        Object snapshot() throws Exception {
            require(source.get(source.worldType, source.world, "v") == source.cache
                    && source.get(source.worldType, source.world, "w") == source.storage
                    && source.get(source.worldType, source.world, "x") == source.info,
                    "retained actual World/cache/handler/Info owners");
            Object world = reference(source.world);
            // Retain old raw/loaded Chunk owners even after genuine unpublication.
            List<Object> retained = new ArrayList<>();
            for (Object chunk : source.chunks) retained.add(reference(chunk));
            Map<String, Object> statics = null;
            Boolean fallingValue = null;
            if (experiment.acquiredSourceChunk) {
                Class<?> biome = source.type("gs");
                statics = object();
                for (char label = 'a'; label <= 'm'; ++label) {
                    Field member = source.field(biome, Character.toString(label));
                    require(member.getType() == biome && member.getModifiers() == 25, "actual initialized biome static");
                    statics.put(Character.toString(label), reference(member.get(null)));
                }
                Field falling = source.field(source.type("ei"), "a");
                require(falling.getType() == boolean.class && falling.getModifiers() == 9, "actual initialized falling static");
                fallingValue = falling.getBoolean(null);
            }
            Field enqueue = source.field(source.worldType, "A");
            Field positiveSky = source.field(source.chunkType, "a");
            require(enqueue.getType() == int.class && enqueue.getModifiers() == 8
                    && positiveSky.getType() == boolean.class && positiveSky.getModifiers() == 9,
                    "actual represented shared static declarations");
            Object shared = object("enqueue_depth", enqueue.getInt(null), "chunk_static_a", positiveSky.getBoolean(null),
                    "falling", fallingValue, "biome_statics", statics,
                    "selected_static_scope", experiment.acquiredSourceChunk
                            ? "read after actual Chunk acquisition initialized Block/Biome owners"
                            : "not read before first observed Chunk acquisition; avoid observer class initialization");
            while (!pending.isEmpty()) objects.add(describe(pending.removeFirst()));
            return object("world", world, "retained_chunks", retained, "shared", shared, "objects", objects);
        }
    }

    private static final class Experiment {
        final Original source;
        final RandomReader random;
        final Path root;
        final Method drop, maintain;
        final List<Object> states = new ArrayList<>(), operations = new ArrayList<>();
        final Map<String, Integer> stateHashes = new LinkedHashMap<>();
        final List<byte[]> stateBytes = new ArrayList<>();
        long retainedStateBytes, retainedFileBytes;
        Object oldFar, reloadedFar;
        boolean acquiredSourceChunk;
        String stage = "initial";

        Experiment(Original source, Path root, RandomReader random) throws Exception {
            this.source = source; this.root = root; this.random = random;
            require(source.world.getClass() == source.type("dp")
                    && source.cache.getClass() == source.type("he"), "actual dedicated World/cache class");
            drop = source.method(source.cacheType, "d", void.class, 1, int.class, int.class);
            maintain = source.method(source.cacheType, "a", boolean.class, 1);
            require(source.get(source.worldType, source.world, "v") == source.cache
                    && source.get(source.type("dp"), source.world, "C") == source.cache,
                    "actual derived C is the actual inherited v");
            Class<?> infoType = source.info.getClass();
            require(source.get(infoType, source.info, "a").equals(23L)
                    && source.get(infoType, source.info, "b").equals(0)
                    && source.get(infoType, source.info, "c").equals(64)
                    && source.get(infoType, source.info, "d").equals(0), "actual seed23 spawn0 constructor state");
        }

        void origin(Class<?> type) {
            require(type.getClassLoader() == source.loader && type.getProtectionDomain().getCodeSource() != null
                    && source.jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                    "exact supplied original class origin: " + type.getName());
            source.origins.put(type.getName(), source.jar.toExternalForm());
        }

        Object reference(Object value) {
            return value == null ? null : object("ref", source.ref(value), "class", value.getClass().getName());
        }

        Object dedicated() throws Exception {
            Class<?> type = source.type("dp");
            List<Object> fields = new ArrayList<>();
            Class<?>[] types = {source.cacheType, boolean.class, boolean.class, source.owner.getClass(), source.type("go")};
            for (int i = 0; i < types.length; ++i) {
                String name = Character.toString((char) ('C' + i));
                Field member = source.field(type, name);
                require(member.getType() == types[i] && member.getModifiers() == (i < 3 ? 1 : 2), "exact dedicated field " + name);
                Object value = member.get(source.world);
                fields.add(object("field", name, "descriptor", types[i].descriptorString(), "modifiers", member.getModifiers(),
                        "value", types[i].isPrimitive() ? scalar(types[i], value) : reference(value)));
            }
            require(source.get(type, source.world, "C") == source.cache
                    && source.get(type, source.world, "F") == source.owner,
                    "retained actual dedicated cache/server owners");
            for (String service : new String[] {"c", "d", "e", "f", "p"})
                require(source.get(source.owner.getClass(), source.owner, service) == null,
                        "server startup service remains genuinely uninitialized: " + service);
            return object("world", reference(source.world), "base_cache", reference(source.get(source.worldType, source.world, "v")),
                    "fields", fields);
        }

        Object info() throws Exception {
            String[] roles = {"seed", "spawn_x", "spawn_y", "spawn_z", "time", "last_played", "size_on_disk", "player",
                    "player_dimension", "level_name", "version", "raining", "rain_time", "thundering", "thunder_time"};
            List<Object> fields = new ArrayList<>();
            for (int i = 0; i < roles.length; ++i) {
                Field member = source.field(source.info.getClass(), Character.toString((char) ('a' + i)));
                Object value = member.get(source.info);
                if (member.getType() == String.class)
                    value = object("object", reference(value), "utf16", utf16((String) value));
                else if (!member.getType().isPrimitive()) value = reference(value);
                else value = scalar(member.getType(), value);
                fields.add(object("field", member.getName(), "role", roles[i], "descriptor", member.getType().descriptorString(),
                        "modifiers", member.getModifiers(), "value", value));
            }
            return object("owner", reference(source.info), "fields", fields);
        }

        Object disk() throws Exception {
            List<Path> paths;
            try (var walk = Files.walk(root)) { paths = new ArrayList<>(walk.limit(257).toList()); }
            require(paths.size() <= 256, "complete recursive disk entry bound");
            paths.sort(Comparator.comparing(path -> root.relativize(path).toString().replace('\\', '/')));
            List<Object> entries = new ArrayList<>();
            int files = 0, directories = 0;
            long total = 0;
            for (Path path : paths) {
                require(path.normalize().startsWith(root) && !Files.isSymbolicLink(path), "ordinary owned inventory path");
                BasicFileAttributes before = Files.readAttributes(path, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
                String name = path.equals(root) ? "." : root.relativize(path).toString().replace('\\', '/');
                if (before.isDirectory()) {
                    entries.add(object("path", name, "kind", "directory")); ++directories; continue;
                }
                require(before.isRegularFile() && before.size() <= 4 * 1024 * 1024
                        && total <= 8L * 1024 * 1024 - before.size()
                        && retainedFileBytes <= 16L * 1024 * 1024 - before.size(), "bounded complete retained disk bytes");
                byte[] raw;
                try (var channel = Files.newByteChannel(path, Set.<java.nio.file.OpenOption>of(StandardOpenOption.READ, LinkOption.NOFOLLOW_LINKS));
                     var input = java.nio.channels.Channels.newInputStream(channel)) { raw = input.readNBytes(4 * 1024 * 1024 + 1); }
                BasicFileAttributes after = Files.readAttributes(path, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
                require(raw.length == before.size() && after.isRegularFile() && before.size() == after.size()
                        && before.lastModifiedTime().equals(after.lastModifiedTime())
                        && java.util.Objects.equals(before.fileKey(), after.fileKey()), "complete stable actual file read");
                entries.add(object("path", name, "kind", "file", "data", source.storedBlob(raw, null)));
                ++files; total += raw.length; retainedFileBytes += raw.length;
            }
            return object("complete", true, "entries", entries, "files", files, "directories", directories, "bytes", total);
        }

        Object snapshot() throws Exception {
            for (Object chunk : source.map().values()) source.retain(chunk);
            for (Object chunk : source.list()) source.retain(chunk);
            source.retain(source.fallback);
            Object value = object("dedicated", dedicated(), "graph", new Graph(this).snapshot(), "info", info(), "disk", disk(),
                    "old_far", reference(oldFar), "reloaded_far", reference(reloadedFar));
            byte[] raw = ChunkLifecycleObservation.encoded(value);
            require(raw.length <= MAX_STATE, "complete per-state encoded byte bound");
            String hash = ChunkLifecycleObservation.hash(raw);
            Integer index = stateHashes.get(hash);
            if (index == null) {
                require(states.size() < 64 && retainedStateBytes <= MAX_OUTPUT - raw.length, "complete retained state byte bound");
                index = states.size(); states.add(value); stateHashes.put(hash, index); stateBytes.add(raw); retainedStateBytes += raw.length;
            } else require(Arrays.equals(stateBytes.get(index), raw), "lossless exact state dictionary");
            return object("state_reference", index);
        }

        Object operation(String action, Object receiver, Method method, Object... arguments) throws Exception {
            stage = action;
            Object before = snapshot();
            List<Object> args = new ArrayList<>();
            for (Object value : arguments) args.add(source.argument(value));
            Map<String, Object> row = object("index", operations.size(), "action", action, "receiver", reference(receiver),
                    "class", method.getDeclaringClass().getName(), "method", method.getName(),
                    "descriptor", ChunkLifecycleObservation.signature(method.getReturnType(), method.getParameterTypes()),
                    "arguments", args, "before", before);
            operations.add(row);
            long low = System.currentTimeMillis();
            Object result = null;
            Throwable failure = null;
            try { result = invoke(method, receiver, arguments); }
            catch (Throwable caught) { failure = caught; }
            long high = System.currentTimeMillis();
            require(high >= low, "ordered actual operation epoch interval");
            if (source.chunkType.isInstance(result)) source.retain(result);
            if (failure == null && method.equals(source.acquire) && result != null) acquiredSourceChunk = true;
            if (action.equals("far_c") && arguments[0].equals(16) && arguments[1].equals(0)) oldFar = result;
            if (action.equals("reload_c_16_0")) reloadedFar = result;
            row.put("epoch_ms", Arrays.asList(low, high));
            row.put("result", result == null || result instanceof Boolean || result instanceof Integer ? result : reference(result));
            row.put("failure", failure == null ? null : object("class", failure.getClass().getName(), "message", failure.getMessage()));
            row.put("after", snapshot());
            if (failure instanceof Exception exception) throw exception;
            if (failure instanceof Error error) throw error;
            if (failure != null) throw new IllegalStateException("uncategorized actual source failure", failure);
            return result;
        }

        void run() throws Exception {
            for (int[] pair : ORIGIN) operation("origin_c", source.cache, source.acquire, pair[0], pair[1]);
            operation("world_time_1234", source.world, source.timeSetter, 1234L);
            for (int i = 0; i < 2; ++i) operation("full_world_save_" + i, source.world, source.worldSave, true, null);
            operation("protected_drop_0_0", source.cache, drop, 0, 0);
            for (int[] pair : FAR) operation("far_c", source.cache, source.acquire, pair[0], pair[1]);
            require(oldFar != null, "actual far target retained before drop");
            operation("drop_16_0", source.cache, drop, 16, 0);
            operation("cache_maintenance", source.cache, maintain);
            operation("reload_c_16_0", source.cache, source.acquire, 16, 0);
            Object repeated = operation("repeated_c_16_0", source.cache, source.acquire, 16, 0);
            require(reloadedFar != null && reloadedFar != oldFar && repeated == reloadedFar,
                    "genuine eviction/reload creates a distinct target; repeated cache hit retains it");
            stage = "complete";
        }
    }

    private static Set<String> originalClasses(Path archive) throws Exception {
        Set<String> result = new HashSet<>();
        try (ZipFile jar = new ZipFile(archive.toFile())) {
            var entries = jar.entries();
            while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName();
                if (name.endsWith(".class")) result.add(name.substring(0, name.length() - 6).replace('/', '.'));
            }
        }
        require(result.size() <= 4096, "bounded original class inventory");
        return result;
    }

    public static void main(String[] arguments) {
        int status = 1;
        PrintStream console = System.out;
        System.setOut(System.err);
        Path output = null;
        Original source = null;
        Experiment experiment = null;
        Map<String, Object> record = object("schema", 2, "scope", "original-only-dedicated-natural-population-unload",
                "profile", PROFILE, "side", "server", "seed_hex", SEED, "experiment", true, "qualified", false,
                "native_comparison", false, "tested_commit", null, "mechanical_capture_only", true,
                "source_context", "outer-capture-report", "passed", false);
        try {
            require(arguments.length == 5 && arguments[1].equals("server") && arguments[2].equals(SEED),
                    "jar server 0000000000000017 fresh_owned_parent separate_output_json");
            require(!Arrays.asList("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")
                    .stream().anyMatch(name -> System.getenv(name) != null && !System.getenv(name).isEmpty()), "uninjected Java environment");
            Path archive = Path.of(arguments[0]).toRealPath();
            Path root = Path.of(arguments[3]).toRealPath();
            Path requested = Path.of(arguments[4]).toAbsolutePath().normalize();
            require(Files.isDirectory(root, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(root)
                    && !Files.exists(root.resolve("world"), LinkOption.NOFOLLOW_LINKS)
                    && Files.isDirectory(requested.getParent()) && !requested.startsWith(root)
                    && !Files.exists(requested, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(requested),
                    "fresh owned parent and separate new output");
            try (var children = Files.list(root)) { require(children.findAny().isEmpty(), "actually empty parent before genuine construction"); }
            output = requested;
            require(Files.isRegularFile(archive) && Files.size(archive) <= 2 * 1024 * 1024
                    && ChunkLifecycleObservation.hash(Files.readAllBytes(archive)).equals(ChunkLifecycleObservation.SERVER), "pinned supplied server archive");
            record.put("jar_sha256", ChunkLifecycleObservation.SERVER);
            Set<String> originals = originalClasses(archive);
            RandomReader random = new RandomReader();
            source = new Original(archive, root);
            source.constructWorld();
            record.put("metadata_preparation", source.preparation);
            record.put("world_is_new", source.get(source.worldType, source.world, "s"));
            record.put("construction_epoch_ms", Arrays.asList(source.constructionLow, source.constructionHigh));
            experiment = new Experiment(source, root, random);
            record.put("threads_before", ChunkLifecycleObservation.threads(source.loader, source.owner, originals));
            record.put("initial", experiment.snapshot());
            experiment.run();
            record.put("final", experiment.snapshot());
            record.put("threads_after_operations", ChunkLifecycleObservation.threads(source.loader, source.owner, originals));
            record.put("class_origins", source.origins);
            record.put("bindings", source.bindings);
            record.put("field_bindings", source.fields);
            record.put("construction_trace", source.constructionTrace);
            record.put("source_helpers", List.of("ChunkLifecycleObservation", "ChunkObservation", "CacheCallbacksObservation", "NaturalPopulationObservation"));
            record.put("limits", object("output_bytes", MAX_OUTPUT, "state_bytes", MAX_STATE, "objects_per_state", MAX_OBJECTS,
                    "array_elements", MAX_ARRAY, "collection_entries", MAX_COLLECTION, "disk_entries", 256,
                    "file_bytes", 4 * 1024 * 1024, "snapshot_file_bytes", 8 * 1024 * 1024, "retained_file_bytes", 16 * 1024 * 1024));
            record.put("projection_scope", "All reached original instance fields, complete primitive/object arrays and bootstrap Collection iterator contents; source Class statics and JDK private collection capacities are not a whole-process heap projection. File binds its actual path; Random uses unchanged copied-state observation.");
            record.put("snapshot_compatibility", "Existing Natural Observation binds exact dj/jb; lifecycle snapshot requires empty lighting/finite Boat-Sign scope. Neither snapshot body is called or given substituted fields.");
            record.put("storage_preparation", "Actual original ct constructor/spawn API and handler metadata save create seed23/spawn0 input first; a new handler/new MinecraftServer/dp reads it. No supplied saved Chunk, post-construction spawn edit, pending insertion, cache edit or policy substitution. Explicit c(0,0) starts natural origin generation; constructor cache/queue state is observed without size assertions.");
            record.put("failure", null);
            require(ChunkLifecycleObservation.hash(Files.readAllBytes(archive)).equals(ChunkLifecycleObservation.SERVER), "supplied archive unchanged");
            source.close();
            record.put("region_close_completed", true);
            record.put("disk_after_close", experiment.disk());
            record.put("threads_after_close", ChunkLifecycleObservation.threads(source.loader, source.owner, originals));
            record.put("passed", true);
            status = 0;
        } catch (Throwable failure) {
            record.put("failure", object("stage", experiment == null ? "construction-or-observer-binding" : experiment.stage,
                    "class", failure.getClass().getName(), "message", failure.getMessage()));
            System.err.println("FAILURE: original dedicated population prototype");
            failure.printStackTrace(System.err);
        } finally {
            if (source != null) {
                if (experiment != null) {
                    record.put("operations", experiment.operations);
                    record.put("states", experiment.states);
                    record.put("state_encoding", "state-reference-v1; each graph contains actual integer identities and complete reached fields");
                }
                record.put("blobs", source.blobs);
                record.put("blob_encoding", "SHA256-indexed lossless bytes; primitive arrays use big-endian raw bits");
                record.putIfAbsent("construction_trace", source.constructionTrace);
                record.putIfAbsent("metadata_preparation", source.preparation);
                record.putIfAbsent("metadata_preparation_calls", source.preparationCalls);
                record.putIfAbsent("class_origins", source.origins);
                record.putIfAbsent("bindings", source.bindings);
                record.putIfAbsent("field_bindings", source.fields);
                try { source.close(); }
                catch (Throwable closeFailure) {
                    record.put("close_failure", object("class", closeFailure.getClass().getName(), "message", closeFailure.getMessage()));
                    closeFailure.printStackTrace(System.err); status = 1; record.put("passed", false);
                }
            }
            if (output != null) {
                try {
                    byte[] raw = ChunkLifecycleObservation.encoded(record);
                    require(raw.length <= MAX_OUTPUT, "complete output bound; oversized capture is a failed experiment");
                    Files.write(output, raw, StandardOpenOption.CREATE_NEW);
                } catch (Throwable writeFailure) {
                    writeFailure.printStackTrace(System.err); status = 1;
                }
            }
            System.setOut(console);
        }
        System.exit(status);
    }
}
