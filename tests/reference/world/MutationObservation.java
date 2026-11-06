import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
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
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

/** Direct original Chunk API experiment in genuine constructed Worlds.
 * Measured Chunks are unpublished and their surrounding provider entries are
 * absent. Original setup cache/queue/RNG state is observed, never cleared.
 * This helper records outcomes and arrays; it computes no expected mutation.
 */
public final class MutationObservation {
    private static final int BLOCK_BYTES = 32768;
    private static final int PACKED_BYTES = 16384;
    private static final int HEIGHT_BYTES = 256;
    private static final int CHUNK_X = 10000;
    private static final int CHUNK_Z = -10000;
    private static final long WORLD_SEED = 23L;
    private static final int MAX_COLLECTION = 512;
    private static final int MAX_CACHED_CHUNKS = 1025;
    private static final int MAX_SETUP_REGIONS = 16384;
    private static final int MAX_ARRAY_BYTES = 1024 * 1024;
    private static final long MAX_CACHE_ARRAY_BYTES = 128L * 1024 * 1024;
    private static final int MAX_JSON_BYTES = 32 * 1024 * 1024;
    private static final int MAX_RECORD_BYTES = 64 * 1024 * 1024;
    private static final int MAX_REFERENCES = 65536;
    private static final String[] ARRAY_ROLES = {"b", "e.a", "f.a", "g.a", "h"};
    private static final String[] BASE_CASES = {"air-noop", "stone-raise", "stone-remove",
            "top127-raise", "full257", "full256", "null-old255", "height255-alias",
            "height255-oob", "all128-min127"};
    private static boolean measuring;

    private static final class SetupUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupUnavailable(String message, Throwable cause) { super(message, cause); }
    }

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static void budget(boolean value, String message) throws SetupUnavailable {
        if (!value) {
            if (!measuring) throw new SetupUnavailable(message, null);
            throw new IllegalStateException(message);
        }
    }

    private static String hex(byte[] bytes) {
        char[] text = new char[bytes.length * 2];
        char[] digits = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; ++i) {
            text[i * 2] = digits[(bytes[i] & 255) >>> 4];
            text[i * 2 + 1] = digits[bytes[i] & 15];
        }
        return new String(text);
    }

    private static String bits(long value) {
        return String.format(Locale.ROOT, "%016x", value);
    }

    private static String hash(byte[] bytes) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static Map<String, Object> object(Object... pairs) {
        require((pairs.length & 1) == 0, "JSON key/value pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2)
            require(result.put((String) pairs[i], pairs[i + 1]) == null, "duplicate JSON key");
        return result;
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) {
            StringBuilder result = new StringBuilder("\"");
            for (char unit : ((String) value).toCharArray()) {
                if (unit == '"' || unit == '\\') result.append('\\').append(unit);
                else if (unit < 32 || unit > 126)
                    result.append(String.format(Locale.ROOT, "\\u%04x", (int) unit));
                else result.append(unit);
            }
            return result.append('"').toString();
        }
        if (value instanceof Boolean || value instanceof Integer || value instanceof Long)
            return value.toString();
        if (value instanceof Map<?, ?>) {
            StringBuilder result = new StringBuilder("{");
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                if (result.length() != 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return result.append('}').toString();
        }
        if (value instanceof List<?>) {
            StringBuilder result = new StringBuilder("[");
            for (Object item : (List<?>) value) {
                if (result.length() != 1) result.append(',');
                result.append(json(item));
            }
            return result.append(']').toString();
        }
        throw new IllegalStateException("unsupported JSON type");
    }

    private static byte[] jsonBytes(Object value) throws Exception {
        byte[] bytes = json(value).getBytes(StandardCharsets.UTF_8);
        budget(bytes.length != 0 && bytes.length <= MAX_JSON_BYTES, "JSON observation cap");
        return bytes;
    }

    private static Map<String, Object> fingerprint(byte[] bytes) throws Exception {
        return bytes == null ? object("length", null, "sha256", null)
                : object("length", bytes.length, "sha256", hash(bytes));
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException wrapped) {
            Throwable cause = wrapped.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            if (cause instanceof Error) throw (Error) cause;
            throw wrapped;
        }
    }

    private static final class LimitedBytes extends ByteArrayOutputStream {
        private void room(int extra) {
            require(extra >= 0 && extra <= MAX_RECORD_BYTES - count, "binary transcript cap");
        }
        @Override public synchronized void write(int value) { room(1); super.write(value); }
        @Override public synchronized void write(byte[] bytes, int offset, int length) {
            room(length);
            super.write(bytes, offset, length);
        }
    }

    private static void shortAscii(DataOutputStream out, String value) throws Exception {
        require(value != null && value.length() > 0 && value.length() <= 65535,
                "short ASCII framing length");
        for (int i = 0; i < value.length(); ++i)
            require(value.charAt(i) >= 32 && value.charAt(i) <= 126, "short ASCII framing text");
        byte[] bytes = value.getBytes(StandardCharsets.US_ASCII);
        out.writeShort(bytes.length);
        out.write(bytes);
    }

    private static void sized(DataOutputStream out, byte[] bytes) throws Exception {
        out.writeInt(bytes.length);
        out.write(bytes);
    }

    private static final class RandomState {
        final Object owner, seedOwner;
        final long seed, cachedBits;
        final boolean cachedValid;

        RandomState(Model model) throws Exception {
            owner = model.get("world.r", model.world);
            require(owner != null && owner.getClass() == Random.class, "ordinary World Random");
            seedOwner = model.randomSeed.get(owner);
            require(seedOwner != null && seedOwner.getClass() == AtomicLong.class,
                    "actual Random seed AtomicLong");
            seed = ((AtomicLong) seedOwner).get();
            cachedValid = model.randomValid.getBoolean(owner);
            cachedBits = Double.doubleToRawLongBits(model.randomCached.getDouble(owner));
        }

        boolean same(RandomState other) {
            return owner == other.owner && seedOwner == other.seedOwner && seed == other.seed &&
                    cachedValid == other.cachedValid && cachedBits == other.cachedBits;
        }

        Map<String, Object> describe(Model model) throws Exception {
            return object("owner", model.ref(owner), "seed_owner", model.ref(seedOwner),
                    "seed_bits", bits(seed), "cached_valid", cachedValid,
                    "cached_bits", bits(cachedBits));
        }
    }

    private static final class RegistryState {
        final Object mapOwner, opacityOwner;
        final Object[] blocks;
        final int[] opacity;

        RegistryState(Model model) throws Exception {
            mapOwner = model.blockMap.get(null);
            opacityOwner = model.opacityField.get(null);
            require(mapOwner instanceof Object[] && opacityOwner instanceof int[],
                    "actual registry array types");
            blocks = ((Object[]) mapOwner).clone();
            opacity = ((int[]) opacityOwner).clone();
            budget(blocks.length == 256 && opacity.length == 256, "registry array observation shape");
        }

        boolean same(RegistryState other) {
            if (mapOwner != other.mapOwner || opacityOwner != other.opacityOwner ||
                    !Arrays.equals(opacity, other.opacity) || blocks.length != other.blocks.length)
                return false;
            for (int i = 0; i < blocks.length; ++i)
                if (blocks[i] != other.blocks[i]) return false;
            return true;
        }

        Map<String, Object> describe(Model model) throws Exception {
            List<Object> entries = new ArrayList<>(), opacities = new ArrayList<>();
            for (Object block : blocks) entries.add(model.ref(block));
            for (int value : opacity) opacities.add(value);
            return object("m", model.ref(mapOwner), "m_entries", entries,
                    "q", model.ref(opacityOwner), "q_entries", opacities,
                    "stone", model.ref(model.stone), "stone_id", model.stoneId,
                    "stone_remove_owner", model.stoneRemove.getDeclaringClass().getName(),
                    "stone_add_owner", model.stoneAdd.getDeclaringClass().getName());
        }
    }

    private static final class ChunkState {
        final Object owner;
        final Object[] roles;
        final byte[][] arrays;
        final int x, z, minimum, flags, tileCount;
        final int[] sliceCounts;
        final Object[] tileKeys, tileValues;
        final Object[][] sliceEntries;
        final long time;

        ChunkState(Model model, Object target) throws Exception {
            owner = target;
            require(target != null && model.chunkType.isInstance(target), "actual Chunk receiver");
            Object[] wrappers = {model.get("chunk.e", target), model.get("chunk.f", target),
                    model.get("chunk.g", target)};
            byte[][] raw = new byte[5][];
            raw[0] = (byte[]) model.get("chunk.b", target);
            for (int i = 0; i < 3; ++i)
                raw[i + 1] = wrappers[i] == null ? null : (byte[]) model.get("nibble.a", wrappers[i]);
            raw[4] = (byte[]) model.get("chunk.h", target);
            arrays = new byte[5][];
            for (int i = 0; i < raw.length; ++i) {
                budget(raw[i] == null || raw[i].length <= MAX_ARRAY_BYTES, "Chunk array observation cap");
                arrays[i] = raw[i] == null ? null : raw[i].clone();
            }
            Object tiles = model.get("chunk.l", target);
            Object slices = model.get("chunk.m", target);
            require(tiles instanceof Map<?, ?> && slices instanceof Object[], "actual Chunk children");
            Object[] selected = (Object[]) slices;
            require(selected.length == 8, "actual eight entity slices");
            roles = new Object[19];
            roles[0] = model.get("chunk.d", target);
            roles[1] = raw[0];
            for (int i = 0; i < 3; ++i) {
                roles[2 + i * 2] = wrappers[i];
                roles[3 + i * 2] = raw[i + 1];
            }
            roles[8] = raw[4];
            roles[9] = tiles;
            roles[10] = slices;
            sliceCounts = new int[8];
            sliceEntries = new Object[8][];
            for (int i = 0; i < 8; ++i) {
                require(selected[i] instanceof List<?>, "actual entity slice list");
                roles[11 + i] = selected[i];
                sliceCounts[i] = ((List<?>) selected[i]).size();
                budget(sliceCounts[i] <= MAX_COLLECTION, "entity slice observation cap");
                sliceEntries[i] = ((List<?>) selected[i]).toArray();
                require(sliceEntries[i].length == sliceCounts[i], "stable entity slice snapshot size");
            }
            tileCount = ((Map<?, ?>) tiles).size();
            budget(tileCount <= MAX_COLLECTION, "tile map observation cap");
            tileKeys = new Object[tileCount];
            tileValues = new Object[tileCount];
            int tileIndex = 0;
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) tiles).entrySet()) {
                require(tileIndex < tileCount, "stable tile map snapshot size");
                tileKeys[tileIndex] = entry.getKey();
                tileValues[tileIndex++] = entry.getValue();
            }
            require(tileIndex == tileCount, "complete tile map snapshot size");
            x = (int) model.get("chunk.j", target);
            z = (int) model.get("chunk.k", target);
            minimum = (int) model.get("chunk.i", target);
            int mask = 0;
            String[] names = {"c", "n", "o", "p", "q"};
            for (int i = 0; i < names.length; ++i)
                if ((boolean) model.get("chunk." + names[i], target)) mask |= 1 << i;
            flags = mask;
            time = (long) model.get("chunk.r", target);
        }

        boolean rolesSame(ChunkState other) {
            if (owner != other.owner || roles.length != other.roles.length) return false;
            for (int i = 0; i < roles.length; ++i)
                if (roles[i] != other.roles[i]) return false;
            return true;
        }

        boolean same(ChunkState other) {
            if (!rolesSame(other) || x != other.x || z != other.z || minimum != other.minimum ||
                    flags != other.flags || time != other.time || tileCount != other.tileCount ||
                    !Arrays.equals(sliceCounts, other.sliceCounts)) return false;
            for (int i = 0; i < arrays.length; ++i)
                if (!Arrays.equals(arrays[i], other.arrays[i])) return false;
            for (int i = 0; i < tileKeys.length; ++i)
                if (tileKeys[i] != other.tileKeys[i] || tileValues[i] != other.tileValues[i]) return false;
            for (int i = 0; i < sliceEntries.length; ++i) {
                if (sliceEntries[i].length != other.sliceEntries[i].length) return false;
                for (int j = 0; j < sliceEntries[i].length; ++j)
                    if (sliceEntries[i][j] != other.sliceEntries[i][j]) return false;
            }
            return true;
        }

        long arrayBytes() {
            long count = 0;
            for (byte[] bytes : arrays) if (bytes != null) count += bytes.length;
            return count;
        }

        Map<String, Object> describe(Model model) throws Exception {
            List<Object> refs = new ArrayList<>(), data = new ArrayList<>(), counts = new ArrayList<>();
            List<Object> tiles = new ArrayList<>(), entities = new ArrayList<>();
            for (Object role : roles) refs.add(model.ref(role));
            for (int i = 0; i < arrays.length; ++i)
                data.add(object("role", ARRAY_ROLES[i], "data", fingerprint(arrays[i])));
            for (int count : sliceCounts) counts.add(count);
            for (int i = 0; i < tileKeys.length; ++i)
                tiles.add(object("ordinal", i, "key", model.ref(tileKeys[i]),
                        "value", model.ref(tileValues[i])));
            for (Object[] slice : sliceEntries) {
                List<Object> entries = new ArrayList<>();
                for (Object entity : slice) entries.add(model.ref(entity));
                entities.add(entries);
            }
            return object("owner", model.ref(owner), "roles", refs, "x", x, "z", z,
                    "minimum", minimum, "flags", flags, "time_bits", bits(time),
                    "tile_count", tileCount, "slice_counts", counts, "tile_entries", tiles,
                    "slice_entries", entities, "arrays", data,
                    "blank", invoke(model.emptyMethod, owner));
        }

        void write(Model model, DataOutputStream out) throws Exception {
            out.writeInt(model.ref(owner));
            for (Object role : roles) out.writeInt(model.ref(role));
            out.writeInt(x);
            out.writeInt(z);
            out.writeInt(minimum);
            out.writeByte(flags);
            out.writeLong(time);
            out.writeInt(tileCount);
            for (int count : sliceCounts) out.writeInt(count);
            for (byte[] bytes : arrays) {
                out.writeInt(bytes == null ? -1 : bytes.length);
                if (bytes != null) out.write(bytes);
            }
        }
    }

    private static final class Context {
        final RandomState random;
        final RegistryState registry;
        final List<ChunkState> cached;
        final Object observerOwner;
        final Object[] observers;
        final byte[] encoded, queueEncoded, cacheEncoded;
        final boolean absent, unpublished;

        Context(Model model) throws Exception {
            require(model.get("world.v", model.world) == model.cache, "retained actual World provider");
            Object mapOwner = model.get("cache.e", model.cache);
            Object insertionOwner = model.get("cache.f", model.cache);
            Object pendingOwner = model.get("cache.a", model.cache);
            Object queueOwner = model.get("world.C", model.world);
            observerOwner = model.get("world.u", model.world);
            require(mapOwner instanceof Map<?, ?> && insertionOwner instanceof List<?> &&
                    pendingOwner instanceof Set<?> && queueOwner instanceof List<?> &&
                    observerOwner instanceof List<?>, "actual World collection types");
            Map<?, ?> map = (Map<?, ?>) mapOwner;
            List<?> insertion = (List<?>) insertionOwner;
            Set<?> pending = (Set<?>) pendingOwner;
            List<?> queue = (List<?>) queueOwner;
            List<?> observerList = (List<?>) observerOwner;
            budget(map.size() <= MAX_COLLECTION && insertion.size() <= MAX_COLLECTION &&
                    pending.size() <= MAX_COLLECTION && queue.size() <= MAX_SETUP_REGIONS &&
                    observerList.size() <= MAX_COLLECTION, "complete World collection observation cap");
            observers = observerList.toArray();
            List<Object> observerRefs = new ArrayList<>();
            for (Object observer : observers) observerRefs.add(model.ref(observer));
            random = new RandomState(model);
            registry = new RegistryState(model);

            Object fallback = model.get("cache.b", model.cache);
            List<Object> mapEntries = new ArrayList<>(), inserted = new ArrayList<>();
            List<Object> pendingEntries = new ArrayList<>(), values = new ArrayList<>();
            values.add(fallback);
            boolean targetUnpublished = !model.isExperimental(fallback);
            int ordinal = 0;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                require(entry.getKey() != null && entry.getKey().getClass() == Integer.class,
                        "actual signed Integer cache key");
                mapEntries.add(object("ordinal", ordinal++, "key", entry.getKey(),
                        "key_ref", model.ref(entry.getKey()), "value", model.ref(entry.getValue())));
                if (entry.getValue() != null) values.add(entry.getValue());
                targetUnpublished &= !model.isExperimental(entry.getValue());
            }
            ordinal = 0;
            for (Object value : insertion) {
                inserted.add(object("ordinal", ordinal++, "value", model.ref(value)));
                values.add(value);
                targetUnpublished &= !model.isExperimental(value);
            }
            ordinal = 0;
            for (Object value : pending) {
                require(value != null && value.getClass() == Integer.class,
                        "actual signed Integer pending key");
                pendingEntries.add(object("ordinal", ordinal++, "key", value,
                        "key_ref", model.ref(value)));
            }
            IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>();
            cached = new ArrayList<>();
            List<Object> describedChunks = new ArrayList<>();
            long cacheArrayBytes = 0;
            for (Object value : values) {
                if (seen.put(value, Boolean.TRUE) == null) {
                    budget(cached.size() < MAX_CACHED_CHUNKS, "deduplicated cached Chunk cap");
                    ChunkState state = new ChunkState(model, value);
                    cacheArrayBytes += state.arrayBytes();
                    budget(cacheArrayBytes <= MAX_CACHE_ARRAY_BYTES, "raw cache array copy cap");
                    cached.add(state);
                    describedChunks.add(state.describe(model));
                }
            }
            Map<String, Object> cacheDescription = object("owner", model.ref(model.cache),
                    "world", model.ref(model.get("cache.g", model.cache)),
                    "generator", model.ref(model.get("cache.c", model.cache)),
                    "loader", model.ref(model.get("cache.d", model.cache)),
                    "map", object("owner", model.ref(mapOwner), "entries", mapEntries),
                    "insertion", object("owner", model.ref(insertionOwner), "entries", inserted),
                    "pending", object("owner", model.ref(pendingOwner), "entries", pendingEntries),
                    "fallback", model.ref(fallback), "chunks", describedChunks);
            cacheEncoded = jsonBytes(cacheDescription);

            List<Object> regions = new ArrayList<>();
            ordinal = 0;
            for (Object region : queue) {
                require(region != null && region.getClass() == model.regionType,
                        "actual queued light region class");
                Object kind = model.get("region.a", region);
                require(kind == model.skyKind || kind == model.blockKind,
                        "actual queued light channel");
                List<Object> bounds = new ArrayList<>();
                for (String name : new String[] {"b", "c", "d", "e", "f", "g"})
                    bounds.add(model.get("region." + name, region));
                Enum<?> channel = (Enum<?>) kind;
                regions.add(object("ordinal", ordinal++, "owner", model.ref(region),
                        "channel", model.ref(kind), "channel_name", channel.name(),
                        "channel_ordinal", channel.ordinal(), "channel_default", model.get("kind.c", kind),
                        "bounds", bounds));
            }
            Map<String, Object> queueDescription = object("owner", model.ref(queueOwner),
                    "regions", regions, "enqueue_depth", model.get("world.A", null),
                    "drain_depth", model.get("world.M", model.world),
                    "chunk_static_a", model.get("chunk.a", null));
            queueEncoded = jsonBytes(queueDescription);

            List<Object> presence = new ArrayList<>();
            boolean allAbsent = true;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    int x = CHUNK_X + dx, z = CHUNK_Z + dz;
                    int key = (int) invoke(model.keyMethod, null, x, z);
                    boolean actualPresence = (boolean) invoke(model.presenceMethod, model.cache, x, z);
                    boolean contains = map.containsKey(key);
                    require(actualPresence == contains, "actual provider presence/key consistency");
                    Object selected = map.get(key);
                    allAbsent &= !actualPresence && !contains;
                    presence.add(object("x", x, "z", z, "key", key,
                            "present", actualPresence, "contains", contains,
                            "value", model.ref(selected)));
                }
            }
            absent = allAbsent;
            unpublished = targetUnpublished;
            encoded = jsonBytes(object("schema", 1, "side", model.client ? "client" : "server",
                    "bindings", model.bindings(), "world", model.ref(model.world),
                    "provider", model.ref(model.get("world.t", model.world)),
                    "storage", model.ref(model.get("world.w", model.world)),
                    "provider_world", model.ref(model.get("provider.a", model.provider)),
                    "manager", model.ref(model.get("provider.b", model.provider)),
                    "no_sky", model.get("provider.e", model.provider),
                    "remote", model.get("world.B", model.world), "random", random.describe(model),
                    "observers", object("owner", model.ref(observerOwner), "entries", observerRefs),
                    "cache", cacheDescription, "queue", queueDescription,
                    "registry", registry.describe(model), "presence", presence));
        }

        boolean sameQueue(Context other) { return Arrays.equals(queueEncoded, other.queueEncoded); }

        boolean sameCache(Context other) {
            if (!Arrays.equals(cacheEncoded, other.cacheEncoded) || cached.size() != other.cached.size())
                return false;
            for (int i = 0; i < cached.size(); ++i)
                if (!cached.get(i).same(other.cached.get(i))) return false;
            return true;
        }

        boolean sameObservers(Context other) {
            if (observerOwner != other.observerOwner || observers.length != other.observers.length)
                return false;
            for (int i = 0; i < observers.length; ++i)
                if (observers[i] != other.observers[i]) return false;
            return true;
        }

        boolean same(Context other) {
            return Arrays.equals(encoded, other.encoded) && random.same(other.random) &&
                    registry.same(other.registry) && sameQueue(other) && sameCache(other) &&
                    sameObservers(other);
        }
    }

    private static final class Model {
        final boolean client;
        final ClassLoader loader;
        final Class<?> worldType, providerType, normalType, storageType, chunkType;
        final Class<?> nibbleType, cacheType, kindType, regionType, blockType;
        final Map<String, Field> fields = new LinkedHashMap<>();
        final IdentityHashMap<Object, Integer> references = new IdentityHashMap<>();
        final List<Object> retainedReferences = new ArrayList<>();
        final List<Object> experimental = new ArrayList<>();
        final Method seedMethod, emptyMethod, presenceMethod, keyMethod, setter4, setter5;
        final Constructor<?> chunkConstructor;
        final Field blockMap, opacityField, randomSeed, randomValid, randomCached;
        final Object stone, skyKind, blockKind;
        final Method stoneRemove, stoneAdd;
        final int stoneId;
        Object world, provider, storage, cache;
        Context baseline;

        Class<?> type(String name) throws Exception { return Class.forName(name, true, loader); }

        Field bind(String role, Class<?> owner, String name, Class<?> expected, int modifiers)
                throws Exception {
            Field field = owner.getDeclaredField(name);
            require(field.getType() == expected && field.getModifiers() == modifiers,
                    "field descriptor/modifiers: " + owner.getName() + "." + name);
            field.setAccessible(true);
            fields.put(role, field);
            return field;
        }

        Method method(Class<?> owner, String name, Class<?> result, int modifiers, Class<?>... args)
                throws Exception {
            Method method = owner.getDeclaredMethod(name, args);
            require(method.getReturnType() == result && method.getModifiers() == modifiers,
                    "method descriptor/modifiers: " + owner.getName() + "." + name);
            method.setAccessible(true);
            return method;
        }

        Object get(String role, Object receiver) throws Exception { return fields.get(role).get(receiver); }

        int ref(Object value) throws Exception {
            if (value == null) return 0;
            Integer old = references.get(value);
            if (old != null) return old;
            budget(retainedReferences.size() < MAX_REFERENCES, "actual identity table cap");
            int id = retainedReferences.size() + 1;
            references.put(value, id);
            retainedReferences.add(value);
            return id;
        }

        boolean isExperimental(Object value) {
            for (Object chunk : experimental) if (chunk == value) return true;
            return false;
        }

        Map<String, Object> bindings() {
            return object("world", worldType.getName(), "dimension", providerType.getName(),
                    "normal_dimension", normalType.getName(), "storage", storageType.getName(),
                    "chunk", chunkType.getName(), "nibble", nibbleType.getName(),
                    "cache", cacheType.getName(), "block", blockType.getName(),
                    "setter4", setter4.getName(), "setter5", setter5.getName(),
                    "seed", seedMethod.getName(), "presence", presenceMethod.getName());
        }

        Model(ClassLoader loader, boolean client) throws Exception {
            this.loader = loader;
            this.client = client;
            worldType = type(client ? "fd" : "dj");
            providerType = type(client ? "xa" : "os");
            normalType = type(client ? "rh" : "kp");
            storageType = type(client ? "wt" : "om");
            chunkType = type(client ? "lm" : "hi");
            nibbleType = type(client ? "wi" : "ob");
            cacheType = type(client ? "ok" : "jb");
            kindType = type(client ? "eb" : "co");
            regionType = type(client ? "st" : "lv");
            blockType = type(client ? "uu" : "na");
            Class<?> generator = type(client ? "cl" : "bl");
            Class<?> loaderType = type(client ? "bf" : "an");
            Class<?> manager = type(client ? "xv" : "ph");
            bind("chunk.a", chunkType, "a", boolean.class, 9);
            bind("chunk.b", chunkType, "b", byte[].class, 1);
            bind("chunk.d", chunkType, "d", worldType, 1);
            for (String name : new String[] {"e", "f", "g"})
                bind("chunk." + name, chunkType, name, nibbleType, 1);
            bind("chunk.h", chunkType, "h", byte[].class, 1);
            bind("chunk.i", chunkType, "i", int.class, 1);
            for (String name : new String[] {"j", "k"})
                bind("chunk." + name, chunkType, name, int.class, 17);
            bind("chunk.l", chunkType, "l", Map.class, 1);
            bind("chunk.m", chunkType, "m", List[].class, 1);
            for (String name : new String[] {"c", "n", "o", "p", "q"})
                bind("chunk." + name, chunkType, name, boolean.class, 1);
            bind("chunk.r", chunkType, "r", long.class, 1);
            bind("nibble.a", nibbleType, "a", byte[].class, 17);
            bind("world.t", worldType, "t", providerType, 17);
            bind("world.v", worldType, "v", generator, 4);
            bind("world.w", worldType, "w", storageType, 20);
            bind("world.C", worldType, "C", List.class, 2);
            bind("world.A", worldType, "A", int.class, 8);
            bind("world.M", worldType, "M", int.class, 2);
            bind("world.u", worldType, "u", List.class, 4);
            bind("world.r", worldType, "r", Random.class, 1);
            bind("world.B", worldType, "B", boolean.class, 1);
            bind("provider.a", providerType, "a", worldType, 1);
            bind("provider.b", providerType, "b", manager, 1);
            bind("provider.e", providerType, "e", boolean.class, 1);
            bind("cache.a", cacheType, "a", Set.class, 2);
            bind("cache.b", cacheType, "b", chunkType, 2);
            bind("cache.c", cacheType, "c", generator, 2);
            bind("cache.d", cacheType, "d", loaderType, 2);
            bind("cache.e", cacheType, "e", Map.class, 2);
            bind("cache.f", cacheType, "f", List.class, 2);
            bind("cache.g", cacheType, "g", worldType, 2);
            bind("region.a", regionType, "a", kindType, 17);
            for (String name : new String[] {"b", "c", "d", "e", "f", "g"})
                bind("region." + name, regionType, name, int.class, 1);
            bind("kind.a", kindType, "a", kindType, 0x4019);
            bind("kind.b", kindType, "b", kindType, 0x4019);
            bind("kind.c", kindType, "c", int.class, 17);
            skyKind = get("kind.a", null);
            blockKind = get("kind.b", null);
            require(skyKind != null && blockKind != null && skyKind != blockKind, "actual light identities");
            seedMethod = method(worldType, client ? "s" : "l", long.class, 1);
            emptyMethod = method(chunkType, client ? "h" : "g", boolean.class, 1);
            presenceMethod = method(cacheType, "a", boolean.class, 1, int.class, int.class);
            keyMethod = method(type(client ? "yy" : "qa"), "a", int.class, 9, int.class, int.class);
            setter4 = method(chunkType, "a", boolean.class, 1,
                    int.class, int.class, int.class, int.class);
            setter5 = method(chunkType, "a", boolean.class, 1,
                    int.class, int.class, int.class, int.class, int.class);
            chunkConstructor = chunkType.getDeclaredConstructor(worldType, byte[].class, int.class, int.class);
            require(chunkConstructor.getModifiers() == 1, "actual supplied-byte Chunk constructor");
            blockMap = bind("block.m", blockType, "m", Array.newInstance(blockType, 0).getClass(), 25);
            opacityField = bind("block.q", blockType, "q", int[].class, 25);
            Field stoneField = bind("block.u", blockType, "u", blockType, 25);
            Field idField = bind("block.bn", blockType, "bn", int.class, 17);
            stone = stoneField.get(null);
            require(stone != null, "actual registered stone receiver");
            stoneId = idField.getInt(stone);
            require(stoneId == 1, "selected actual stone ID for finite source substrate");
            Object[] actualBlocks = (Object[]) blockMap.get(null);
            int[] actualOpacity = (int[]) opacityField.get(null);
            require(actualBlocks.length == 256 && actualOpacity.length == 256 && actualBlocks[1] == stone,
                    "actual initialized registry bindings");
            require(actualBlocks[255] == null, "actual null old-ID255 control");
            stoneRemove = stone.getClass().getMethod("b", worldType, int.class, int.class, int.class);
            stoneAdd = stone.getClass().getMethod("c", worldType, int.class, int.class, int.class);
            require(stoneRemove.getReturnType() == void.class && stoneAdd.getReturnType() == void.class,
                    "actual stone hook signatures");
            randomSeed = bind("random.seed", Random.class, "seed", AtomicLong.class, 18);
            randomValid = bind("random.valid", Random.class, "haveNextNextGaussian", boolean.class, 2);
            randomCached = bind("random.cached", Random.class, "nextNextGaussian", double.class, 2);
        }

        void setup(Path root) throws Exception {
            require(root.getParent() != null && Files.isDirectory(root.getParent()) &&
                    !Files.exists(root) && !Files.isSymbolicLink(root),
                    "new owned World root beneath existing parent");
            Files.createDirectory(root);
            try {
                provider = normalType.getDeclaredConstructor().newInstance();
                if (client) {
                    storage = type("wy").getDeclaredConstructor().newInstance();
                    world = worldType.getDeclaredConstructor(storageType, String.class, providerType, long.class)
                            .newInstance(storage, "mutation-reference", provider, WORLD_SEED);
                } else {
                    storage = type("dq").getDeclaredConstructor(File.class, String.class, boolean.class)
                            .newInstance(root.toFile(), "world", false);
                    world = worldType.getDeclaredConstructor(storageType, String.class, long.class, providerType)
                            .newInstance(storage, "mutation-reference", WORLD_SEED, provider);
                }
            } catch (InvocationTargetException | LinkageError failure) {
                throw new SetupUnavailable("genuine original World construction failed", failure);
            }
            require((long) invoke(seedMethod, world) == WORLD_SEED, "actual World seed");
            require(get("world.t", world) == provider && get("world.w", world) == storage,
                    "actual World owned constructor fields");
            cache = get("world.v", world);
            require(cache != null && cache.getClass() == cacheType, "actual normal cache provider");
            require(get("cache.g", cache) == world && get("provider.a", provider) == world,
                    "actual World/cache/dimension bindings");
            require(!(boolean) get("provider.e", provider), "actual normal sky dimension");
            ref(world); ref(provider); ref(storage); ref(cache);
            baseline = new Context(this);
            if (!baseline.absent || !baseline.unpublished || baseline.observers.length != 0)
                throw new SetupUnavailable("required actual absent-neighbor/empty-observer setup unavailable", null);
        }

        Object construct(String name) throws Exception {
            byte[] blocks = new byte[BLOCK_BYTES];
            if (name.equals("stone-remove")) blocks[index(8, 8, 8)] = 1;
            else if (name.equals("null-old255")) blocks[index(8, 8, 8)] = (byte) 255;
            else if (name.equals("height255-alias") || name.equals("height255-oob") ||
                    name.equals("all128-min127")) Arrays.fill(blocks, (byte) 1);
            else if (name.equals("negative-q")) {
                for (int y = 0; y < 8; ++y) blocks[index(8, y, 8)] = 1;
            }
            Object chunk = chunkConstructor.newInstance(world, blocks, CHUNK_X, CHUNK_Z);
            require(chunk.getClass() == chunkType && get("chunk.b", chunk) == blocks &&
                    get("chunk.d", chunk) == world, "actual unpublished measured Chunk");
            experimental.add(chunk);
            ref(chunk);
            byte[] metadata = (byte[]) get("nibble.a", get("chunk.e", chunk));
            byte[] sky = (byte[]) get("nibble.a", get("chunk.f", chunk));
            byte[] blockLight = (byte[]) get("nibble.a", get("chunk.g", chunk));
            byte[] heights = (byte[]) get("chunk.h", chunk);
            require(metadata.length == PACKED_BYTES && sky.length == PACKED_BYTES &&
                    blockLight.length == PACKED_BYTES && heights.length == HEIGHT_BYTES,
                    "actual constructor fixed array shape");
            if (name.equals("stone-remove")) heights[(8 << 4) | 8] = 9;
            else if (name.equals("null-old255")) Arrays.fill(metadata, (byte) 0xa5);
            else if (name.equals("height255-alias")) heights[(8 << 4) | 8] = (byte) 255;
            else if (name.equals("height255-oob")) heights[(15 << 4) | 15] = (byte) 255;
            else if (name.equals("all128-min127")) Arrays.fill(heights, (byte) 128);
            return chunk;
        }

        List<Object> identityTable() {
            List<Object> entries = new ArrayList<>();
            for (int i = 0; i < retainedReferences.size(); ++i)
                entries.add(object("id", i + 1, "class", retainedReferences.get(i).getClass().getName()));
            return entries;
        }
    }

    private static int index(int x, int y, int z) { return (x << 11) | (z << 7) | y; }

    private static final class Outcome {
        final int tag;
        final String throwable;
        Outcome(int tag, String throwable) { this.tag = tag; this.throwable = throwable; }

        static Outcome observe(Model model, Object chunk, int overload, int x, int y, int z,
                               int id, int metadata) throws Exception {
            Object result;
            try {
                result = overload == 4 ? model.setter4.invoke(chunk, x, y, z, id)
                        : model.setter5.invoke(chunk, x, y, z, id, metadata);
            } catch (InvocationTargetException wrapped) {
                Throwable actual = wrapped.getCause();
                require(actual != null, "actual invocation cause");
                System.err.println("SOURCE_THROWABLE " + actual.getClass().getName());
                actual.printStackTrace(System.err);
                return new Outcome(2, actual.getClass().getName());
            }
            require(result instanceof Boolean, "actual ordinary Boolean outcome");
            return new Outcome((boolean) result ? 1 : 0, null);
        }

        void write(DataOutputStream out) throws Exception {
            out.writeByte(tag);
            if (tag == 2) shortAscii(out, throwable);
        }
    }

    private static final class Recorder {
        final Model model;
        final DataOutputStream out;
        int chunks, calls;
        boolean allReceipts = true;

        Recorder(Model model, DataOutputStream out) { this.model = model; this.out = out; }

        void context(Context actual) throws Exception {
            if (Arrays.equals(actual.encoded, model.baseline.encoded)) out.writeInt(0);
            else sized(out, actual.encoded);
        }

        void state(ChunkState state, Context actual) throws Exception {
            state.write(model, out);
            context(actual);
        }

        Object begin(String name, int callCount) throws Exception {
            Object chunk = model.construct(name);
            out.writeByte(0x43);
            out.writeInt(chunks++);
            shortAscii(out, name);
            out.writeInt(callCount);
            return chunk;
        }

        void call(Object chunk, int overload, int x, int y, int z, int id, int metadata)
                throws Exception {
            System.err.println("CALL_BEGIN " + calls + " " + overload);
            Context beforeContext = new Context(model);
            ChunkState before = new ChunkState(model, chunk);
            Outcome outcome = Outcome.observe(model, chunk, overload, x, y, z, id, metadata);
            ChunkState after = new ChunkState(model, chunk);
            Context afterContext = new Context(model);
            boolean[] receipts = {
                beforeContext.random.same(afterContext.random),
                beforeContext.sameQueue(afterContext),
                beforeContext.sameCache(afterContext),
                beforeContext.sameObservers(afterContext),
                beforeContext.absent && afterContext.absent && beforeContext.unpublished &&
                        afterContext.unpublished && beforeContext.observers.length == 0 &&
                        afterContext.observers.length == 0,
                before.rolesSame(after),
                beforeContext.registry.same(afterContext.registry)
            };
            out.writeByte(0x4f);
            out.writeInt(calls++);
            out.writeByte(overload);
            out.writeInt(x); out.writeInt(y); out.writeInt(z); out.writeInt(id); out.writeInt(metadata);
            state(before, beforeContext);
            outcome.write(out);
            state(after, afterContext);
            for (boolean receipt : receipts) { out.writeBoolean(receipt); allReceipts &= receipt; }
            System.err.println("CALL_DONE " + (calls - 1));
        }
    }

    private static byte[] observe(Model model, String scenario) throws Exception {
        LimitedBytes bytes = new LimitedBytes();
        DataOutputStream out = new DataOutputStream(bytes);
        out.write("MUTATION_OBSERVATION_V1\n".getBytes(StandardCharsets.US_ASCII));
        out.writeByte(model.client ? 0 : 1);
        boolean negative = scenario.equals("negative-q");
        out.writeByte(negative ? 1 : 0);
        out.writeLong(WORLD_SEED);
        out.writeInt(CHUNK_X); out.writeInt(CHUNK_Z);
        sized(out, model.baseline.encoded);
        out.writeInt(negative ? 2 : 21);
        Recorder recorder = new Recorder(model, out);
        RegistryState initialRegistry = new RegistryState(model);
        System.err.println("MEASUREMENT_BEGIN");
        measuring = true;
        if (!negative) {
            for (int overload = 4; overload <= 5; ++overload) {
                for (String name : BASE_CASES) {
                    Object chunk = recorder.begin(name, 1);
                    int x = 8, y = 8, z = 8, id = 0;
                    if (name.equals("stone-raise")) id = 1;
                    else if (name.equals("top127-raise")) { y = 127; id = 1; }
                    else if (name.equals("full257")) id = 257;
                    else if (name.equals("full256")) id = 256;
                    else if (name.equals("height255-alias")) y = 254;
                    else if (name.equals("height255-oob")) { x = 15; y = 254; z = 15; }
                    else if (name.equals("all128-min127")) y = 127;
                    recorder.call(chunk, overload, x, y, z, id, 0);
                }
            }
            Object repeated = recorder.begin("meta16-repeat", 3);
            recorder.call(repeated, 5, 8, 8, 8, 0, 16);
            recorder.call(repeated, 5, 8, 8, 8, 0, 16);
            recorder.call(repeated, 5, 8, 8, 8, 0, 0);
        } else {
            int[] actualOpacity = (int[]) model.opacityField.get(null);
            for (int overload = 4; overload <= 5; ++overload) {
                Object chunk = recorder.begin("negative-q", 1);
                int saved = actualOpacity[1];
                try {
                    actualOpacity[1] = -1;
                    recorder.call(chunk, overload, 8, 8, 8, 1, 0);
                } finally {
                    actualOpacity[1] = saved;
                }
            }
        }
        Context finalContext = new Context(model);
        RegistryState finalRegistry = finalContext.registry;
        boolean restored = initialRegistry.same(finalRegistry);
        boolean preserved = recorder.allReceipts && model.baseline.same(finalContext) &&
                finalContext.absent && finalContext.unpublished && finalContext.observers.length == 0;
        require(recorder.chunks == (negative ? 2 : 21) && recorder.calls == (negative ? 2 : 23),
                "complete finite operation schedule");
        // Append the full actual table only after every emitted reference is admitted.
        byte[] table = jsonBytes(model.identityTable());
        sized(out, table);
        out.write("END!".getBytes(StandardCharsets.US_ASCII));
        out.writeInt(recorder.chunks); out.writeInt(recorder.calls);
        out.writeInt(model.retainedReferences.size());
        out.writeBoolean(restored); out.writeBoolean(preserved);
        out.flush();
        System.err.println("MEASUREMENT_DONE " + recorder.chunks + " " + recorder.calls);
        return bytes.toByteArray();
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out;
        System.setOut(System.err);
        int status = 1;
        try {
            require(args.length == 4, "arguments: jar client|server new_owned_world_root baseline|negative-q");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            require(args[3].equals("baseline") || args[3].equals("negative-q"), "scenario");
            Path jar = Path.of(args[0]).toRealPath();
            require(Files.isRegularFile(jar), "original JAR regular file");
            Path root = Path.of(args[2]).toAbsolutePath().normalize();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                    ClassLoader.getPlatformClassLoader())) {
                Model model;
                try {
                    System.err.println("SETUP_BEGIN");
                    model = new Model(loader, args[1].equals("client"));
                    model.setup(root);
                    System.err.println("SETUP_DONE");
                } catch (Throwable unavailable) {
                    System.err.println("UNAVAILABLE: genuine original mutation setup");
                    unavailable.printStackTrace(System.err);
                    System.exit(77);
                    return;
                }
                byte[] encoded = observe(model, args[3]);
                require(encoded.length <= MAX_RECORD_BYTES, "complete binary transcript cap");
                protocol.write(encoded, 0, encoded.length);
                protocol.flush();
                require(!protocol.checkError(), "binary protocol output failure");
                status = 0;
            }
        } catch (Throwable failure) {
            if (!measuring && failure instanceof SetupUnavailable) status = 77;
            System.err.println(status == 77 ? "UNAVAILABLE: original mutation observation cap"
                    : "FAILURE: direct Chunk mutation observation");
            failure.printStackTrace(System.err);
        } finally {
            System.setOut(protocol);
        }
        System.exit(status);
    }
}
