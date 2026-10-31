import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
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
import java.util.concurrent.atomic.AtomicLong;

/** Observe actual World and Chunk Entity publication with complete retained state.
 * Uses the unchanged SolidSupportObservation and MutationObservation setup,
 * read-only Context/identity adapter and strict encoder. Original classes are
 * loaded from the supplied archive without bytecode changes. */
public final class WorldEntityPublicationObservation {
    private static final String MUTATION_SHA =
            "812fdda343e5282d649ba6b9f125f9fc4f3bb918528e71eddeb267e4b034b4e9";
    private static final String SUPPORT_SHA =
            "04f3ce515f7d493a9d23b69e6a0791e241caeeeec8b1bfbbb4bc39f2d397be94";
    private static final int MAX_CALLS = 48, MAX_SNAPSHOTS = 128, MAX_ENTITIES = 32;
    private static final int MAX_REFERENCES = 65536, MAX_COLLECTION = 4096;
    private static final long MAX_CONTEXT_BYTES = 96L * 1024 * 1024;
    private static final int MAX_ARRAY_BYTES = 1024 * 1024;
    private static boolean measuring;
    private static final IdentityHashMap<Object, Integer> IDS = new IdentityHashMap<>();
    private static final List<Object> RETAINED = new ArrayList<>();
    private static final List<Object> ENTITIES = new ArrayList<>(), STACKS = new ArrayList<>();
    private static Method encoder;
    private static Constructor<?> rawJson;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private static int ref(Object value) {
        if (value == null) return 0;
        Integer old = IDS.get(value);
        if (old != null) return old;
        require(RETAINED.size() < MAX_REFERENCES, "global identity cap");
        int id = RETAINED.size() + 1;
        IDS.put(value, id); RETAINED.add(value);
        return id;
    }
    private static void retain(List<Object> owners, Object value, int cap) {
        if (value == null) return;
        for (Object old : owners) if (old == value) return;
        require(owners.size() < cap, "retained subject cap");
        owners.add(value); ref(value);
    }
    private static Map<String, Object> object(Object... pairs) {
        require((pairs.length & 1) == 0, "JSON key/value pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) {
            String key = (String) pairs[i];
            require(!result.containsKey(key), "duplicate JSON key");
            result.put(key, pairs[i + 1]);
        }
        return result;
    }
    private static String hex(long value, int width) {
        String bits = Long.toHexString(value);
        if (bits.length() > width) bits = bits.substring(bits.length() - width);
        return "0".repeat(width - bits.length()) + bits;
    }
    private static String hex(byte[] bytes) {
        char[] text = new char[bytes.length * 2], digits = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; ++i) {
            text[2 * i] = digits[(bytes[i] & 255) >>> 4];
            text[2 * i + 1] = digits[bytes[i] & 15];
        }
        return new String(text);
    }
    private static String hash(byte[] bytes) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    private static Method method(Class<?> type, String name, Class<?>... args) throws Exception {
        Method member = type.getDeclaredMethod(name, args); member.setAccessible(true); return member;
    }
    private static Field field(Class<?> type, String name) throws Exception {
        Field member = type.getDeclaredField(name); member.setAccessible(true); return member;
    }
    private static Object invoke(Method member, Object receiver, Object... args) throws Exception {
        try { return member.invoke(receiver, args); }
        catch (InvocationTargetException wrapped) { throwActual(wrapped); return null; }
    }
    private static Object create(Constructor<?> member, Object... args) throws Exception {
        try { return member.newInstance(args); }
        catch (InvocationTargetException wrapped) { throwActual(wrapped); return null; }
    }
    private static void throwActual(InvocationTargetException wrapped) throws Exception {
        Throwable actual = wrapped.getCause();
        if (actual instanceof Exception) throw (Exception) actual;
        if (actual instanceof Error) throw (Error) actual;
        throw wrapped;
    }
    private static byte[] encode(Object value) throws Exception { return (byte[]) invoke(encoder, null, value); }
    private static Object raw(byte[] bytes) throws Exception { return create(rawJson, (Object) bytes); }
    private static String descriptor(Class<?> type) {
        if (type == void.class) return "V";
        if (type == boolean.class) return "Z";
        if (type == byte.class) return "B";
        if (type == short.class) return "S";
        if (type == char.class) return "C";
        if (type == int.class) return "I";
        if (type == long.class) return "J";
        if (type == float.class) return "F";
        if (type == double.class) return "D";
        return type.isArray() ? type.getName().replace('.', '/') : "L" + type.getName().replace('.', '/') + ";";
    }
    private static Object describe(Executable member) {
        StringBuilder signature = new StringBuilder("(");
        for (Class<?> type : member.getParameterTypes()) signature.append(descriptor(type));
        signature.append(')').append(member instanceof Method ? descriptor(((Method) member).getReturnType()) : "V");
        return object("owner", member.getDeclaringClass().getName(),
                "name", member instanceof Method ? member.getName() : "<init>",
                "descriptor", signature.toString(), "modifiers", member.getModifiers());
    }
    private static Object value(Object value, Class<?> declared) {
        if (declared == boolean.class) return value;
        if (declared == float.class) return hex(Float.floatToRawIntBits((Float) value), 8);
        if (declared == double.class) return hex(Double.doubleToRawLongBits((Double) value), 16);
        if (declared == long.class) return hex((Long) value, 16);
        if (declared == int.class) return hex((Integer) value, 8);
        if (declared == short.class) return hex((Short) value, 4);
        if (declared == byte.class) return hex((Byte) value, 2);
        if (declared == char.class) return hex((Character) value, 4);
        List<Object> units = null;
        if (value instanceof String) {
            units = new ArrayList<>();
            String text = (String) value;
            for (int i = 0; i < text.length(); ++i) units.add((int) text.charAt(i));
        }
        return object("owner", ref(value), "class", value == null ? null : value.getClass().getName(), "utf16", units);
    }
    private static Object fields(Class<?> declared, Object receiver) throws Exception {
        List<Object> rows = new ArrayList<>();
        for (Field member : declared.getDeclaredFields()) {
            if (Modifier.isStatic(member.getModifiers())) continue;
            member.setAccessible(true);
            rows.add(object("ordinal", rows.size(), "owner", declared.getName(), "name", member.getName(),
                    "descriptor", descriptor(member.getType()), "modifiers", member.getModifiers(),
                    "value", value(member.get(receiver), member.getType())));
        }
        return rows;
    }
    private static Object random(Object owner) throws Exception {
        require(owner != null && owner.getClass() == Random.class, "actual ordinary Random");
        Object seed = field(Random.class, "seed").get(owner);
        require(seed != null && seed.getClass() == AtomicLong.class, "actual AtomicLong seed");
        return object("owner", ref(owner), "seed_owner", ref(seed), "seed_bits", hex(((AtomicLong) seed).get(), 16),
                "cached_valid", field(Random.class, "haveNextNextGaussian").getBoolean(owner),
                "cached_bits", hex(Double.doubleToRawLongBits(field(Random.class, "nextNextGaussian").getDouble(owner)), 16));
    }

    /* Each immutable adapter constructs its own real World using the SAME
     * supplied original loader. Its local ID namespace remains untouched. */
    private static final class World {
        final String name;
        final Object adapter, model, world, cache;
        final Class<?> adapterType, modelType, worldType, chunkType;
        final Method get, context, bytes, localRef;
        final Field helperMeasuring;
        final boolean oldMeasuring;
        final Method acquisition, publication;
        final Field loaded, players;

        World(String name, ClassLoader original, boolean client, Path root) throws Exception {
            this.name = name;
            ClassLoader helpers = WorldEntityPublicationObservation.class.getClassLoader();
            adapterType = Class.forName("SolidSupportObservation$Adapter", true, helpers);
            Constructor<?> constructor = adapterType.getDeclaredConstructor(ClassLoader.class, boolean.class, Path.class);
            constructor.setAccessible(true);
            adapter = create(constructor, original, client, root);
            model = field(adapterType, "model").get(adapter);
            modelType = (Class<?>) field(adapterType, "modelType").get(adapter);
            require(field(modelType, "loader").get(model) == original, "unchanged original loader binding");
            world = field(adapterType, "world").get(adapter);
            cache = field(adapterType, "cache").get(adapter);
            worldType = (Class<?>) field(adapterType, "worldType").get(adapter);
            chunkType = (Class<?>) field(adapterType, "chunkType").get(adapter);
            require(world.getClass() == worldType && worldType.getClassLoader() == original, "genuine base World/loader");
            get = method(adapterType, "get", String.class, Object.class);
            context = method(adapterType, "context");
            bytes = method(adapterType, "bytes", Object.class);
            localRef = method(adapterType, "ref", Object.class);
            helperMeasuring = (Field) field(adapterType, "helperMeasuring").get(adapter);
            oldMeasuring = helperMeasuring.getBoolean(null);
            acquisition = method(worldType, "c", int.class, int.class);
            publication = method(worldType, "b", Class.forName(client ? "sn" : "lq", false, original));
            require(acquisition.getReturnType() == chunkType && acquisition.getModifiers() == 1 &&
                    publication.getReturnType() == boolean.class && publication.getModifiers() == 1, "source World entry descriptors");
            loaded = field(worldType, "b"); players = field(worldType, "d");
            require(loaded.getType() == List.class && players.getType() == List.class, "actual World membership fields");
            ref(world); ref(cache);
        }
        Object get(String role, Object receiver) throws Exception { return invoke(get, adapter, role, receiver); }
        int local(Object value) throws Exception { return (int) invoke(localRef, adapter, value); }
        Object bridge() throws Exception {
            List<?> retained = (List<?>) field(modelType, "retainedReferences").get(model);
            List<Object> rows = new ArrayList<>();
            for (int i = 0; i < retained.size(); ++i)
                rows.add(object("local", i + 1, "global", ref(retained.get(i)), "class", retained.get(i).getClass().getName()));
            return object("namespace", name, "local_count", rows.size(), "identities", rows);
        }
    }

    private static final class Experiment {
        final boolean client;
        final List<World> worlds;
        final Class<?> entityType, itemType, stackType, boxType, watcherType;
        final Constructor<?> itemConstructor, stackConstructor;
        final Field entityWorld, entityRandom, entityBounds, entityWatcher, itemStack, counter;
        final Field uniquifier;
        boolean entityClassObserved, mathObserved;
        int snapshots;
        long contextBytes;
        final List<Object> calls = new ArrayList<>();
        final List<Object> extraChunks = new ArrayList<>();

        Experiment(ClassLoader original, boolean client, List<World> worlds) throws Exception {
            this.client = client; this.worlds = worlds;
            entityType = Class.forName(client ? "sn" : "lq", false, original);
            itemType = Class.forName(client ? "hl" : "ez", false, original);
            stackType = Class.forName(client ? "iz" : "fy", false, original);
            boxType = Class.forName(client ? "eq" : "cz", false, original);
            watcherType = Class.forName(client ? "ud" : "mo", false, original);
            itemConstructor = itemType.getDeclaredConstructor(worlds.get(0).worldType,
                    double.class, double.class, double.class, stackType);
            stackConstructor = stackType.getDeclaredConstructor(int.class, int.class, int.class);
            require(itemConstructor.getModifiers() == 1 && stackConstructor.getModifiers() == 1, "actual constructors");
            itemConstructor.setAccessible(true); stackConstructor.setAccessible(true);
            entityWorld = field(entityType, client ? "aI" : "aL");
            entityRandom = field(entityType, client ? "bs" : "bv");
            entityBounds = field(entityType, client ? "aW" : "aZ");
            entityWatcher = field(entityType, client ? "bD" : "bE");
            itemStack = field(itemType, "a"); counter = field(entityType, "a");
            uniquifier = field(Random.class, "seedUniquifier");
            require(Modifier.isStatic(uniquifier.getModifiers()) && uniquifier.getType() == AtomicLong.class,
                    "actual class-shared Random uniquifier");
        }
        Object entitiesIn(Object listOwner) throws Exception {
            if (listOwner == null) return object("owner", 0, "entries", null);
            require(listOwner instanceof List<?>, "actual Entity list");
            List<?> list = (List<?>) listOwner;
            require(list.size() <= MAX_COLLECTION, "complete Entity list cap");
            List<Object> entries = new ArrayList<>();
            for (Object entry : list) {
                require(entry == null || entityType.isInstance(entry), "actual nullable Entity entry");
                retain(ENTITIES, entry, MAX_ENTITIES);
                entries.add(object("ordinal", entries.size(), "owner", ref(entry)));
            }
            return object("owner", ref(listOwner), "count", list.size(), "entries", entries);
        }
        Object array(Object owner) {
            if (owner == null) return object("owner", 0, "bytes", null, "hex", null);
            require(owner instanceof byte[] && ((byte[]) owner).length <= MAX_ARRAY_BYTES, "complete raw byte array cap");
            byte[] bytes = ((byte[]) owner).clone();
            return object("owner", ref(owner), "bytes", bytes.length, "hex", hex(bytes));
        }
        Object chunk(World world, Object chunk) throws Exception {
            require(chunk != null && world.chunkType.isInstance(chunk), "genuine Chunk image");
            Object slices = world.get("chunk.m", chunk);
            require(slices instanceof Object[] && ((Object[]) slices).length <= 64, "actual slice array cap");
            List<Object> lists = new ArrayList<>();
            for (Object list : (Object[]) slices) lists.add(entitiesIn(list));
            List<Object> arrays = new ArrayList<>();
            arrays.add(object("role", "b", "value", array(world.get("chunk.b", chunk))));
            for (String role : new String[] {"e", "f", "g"}) {
                Object wrapper = world.get("chunk." + role, chunk);
                arrays.add(object("role", role, "wrapper", ref(wrapper), "value",
                        array(wrapper == null ? null : world.get("nibble.a", wrapper))));
            }
            arrays.add(object("role", "h", "value", array(world.get("chunk.h", chunk))));
            return object("owner", ref(chunk), "class", chunk.getClass().getName(),
                    "fields", fields(world.chunkType, chunk), "arrays", arrays,
                    "slice_array", ref(slices), "slice_count", ((Object[]) slices).length,
                    "slices", lists, "q", world.get("chunk.q", chunk));
        }
        Object entity(Object entity) throws Exception {
            List<Object> declarations = new ArrayList<>();
            declarations.add(object("class", entityType.getName(), "fields", fields(entityType, entity)));
            if (itemType.isInstance(entity)) declarations.add(object("class", itemType.getName(), "fields", fields(itemType, entity)));
            require(entity.getClass() == itemType, "finite exact ItemEntity subject domain");
            Object box = entityBounds.get(entity), watcher = entityWatcher.get(entity), stack = itemStack.get(entity);
            retain(STACKS, stack, MAX_ENTITIES);
            require(box != null && box.getClass() == boxType && watcher != null && watcher.getClass() == watcherType,
                    "actual constructed Entity children");
            Object mapOwner = field(watcherType, "b").get(watcher);
            require(mapOwner instanceof Map<?, ?> && ((Map<?, ?>) mapOwner).size() <= MAX_COLLECTION, "actual watcher map");
            List<Object> records = new ArrayList<>();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) mapOwner).entrySet()) {
                Object record = entry.getValue();
                require(record != null, "actual watcher record");
                Object boxed = field(record.getClass(), "c").get(record);
                records.add(object("ordinal", records.size(), "key", value(entry.getKey(), Object.class),
                        "key_integer", entry.getKey() instanceof Integer ? (Integer) entry.getKey() : null,
                        "owner", ref(record), "fields", fields(record.getClass(), record),
                        "boxed_byte", boxed instanceof Byte ? hex(((Byte) boxed).byteValue(), 2) : null));
            }
            return object("owner", ref(entity), "world", ref(entityWorld.get(entity)), "declarations", declarations,
                    "bounds", object("owner", ref(box), "fields", fields(boxType, box)),
                    "private_random", random(entityRandom.get(entity)),
                    "watcher", object("owner", ref(watcher), "fields", fields(watcherType, watcher),
                            "map", ref(mapOwner), "records", records), "stack", ref(stack));
        }
        Object profile() throws Exception {
            Object u = uniquifier.get(null);
            require(u != null && u.getClass() == AtomicLong.class, "actual uniquifier AtomicLong");
            Object math = "UNOBSERVED_BEFORE_GENUINE_ITEM_CONSTRUCTOR";
            if (mathObserved) {
                Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
                math = random(field(holder, "randomNumberGenerator").get(null));
            }
            return object("uniquifier_owner", ref(u), "uniquifier_bits", hex(((AtomicLong) u).get(), 16),
                    "entity_counter", entityClassObserved ? hex(counter.getInt(null), 8) : "UNOBSERVED",
                    "math", math, "clock_samples", "UNOBSERVED");
        }
        Object snapshot() throws Exception {
            require(snapshots < MAX_SNAPSHOTS, "full snapshot count cap"); ++snapshots;
            List<Object> worldStates = new ArrayList<>();
            for (World world : worlds) {
                Object context = invoke(world.context, world.adapter);
                byte[] bytes = (byte[]) invoke(world.bytes, world.adapter, context);
                require(bytes.length <= MAX_CONTEXT_BYTES - contextBytes, "all raw Context byte cap");
                contextBytes += bytes.length;
                List<Object> chunks = new ArrayList<>();
                IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>();
                List<Object> candidates = new ArrayList<>();
                candidates.add(world.get("cache.b", world.cache));
                Object map = world.get("cache.e", world.cache);
                require(map instanceof Map<?, ?> && ((Map<?, ?>) map).size() <= MAX_COLLECTION, "actual cache map cap");
                candidates.addAll(((Map<?, ?>) map).values());
                Object insertions = world.get("cache.f", world.cache);
                require(insertions instanceof List<?> && ((List<?>) insertions).size() <= MAX_COLLECTION,
                        "actual complete cache insertion List");
                candidates.addAll((List<?>) insertions);
                for (Object extra : extraChunks)
                    if (world.get("chunk.d", extra) == world.world) candidates.add(extra);
                for (Object candidate : candidates) {
                    if (candidate != null && seen.put(candidate, Boolean.TRUE) == null) chunks.add(chunk(world, candidate));
                }
                worldStates.add(object("namespace", world.name, "world", ref(world.world),
                        "fields", fields(world.worldType, world.world),
                        "local_world", world.local(world.world), "raw_context", raw(bytes),
                        "raw_context_bytes", bytes.length, "raw_context_sha256", hash(bytes),
                        "world_b", entitiesIn(world.loaded.get(world.world)),
                        "world_d", entitiesIn(world.players.get(world.world)), "chunks", chunks));
            }
            List<Object> entities = new ArrayList<>(), stacks = new ArrayList<>();
            for (int i = 0; i < ENTITIES.size(); ++i) entities.add(entity(ENTITIES.get(i)));
            for (Object stack : STACKS) stacks.add(object("owner", ref(stack), "class", stack.getClass().getName(),
                    "fields", fields(stackType, stack)));
            return object("worlds", worldStates, "entities", entities, "stacks", stacks, "profile", profile());
        }
        Result observe(String label, String scope, Executable member, Object receiver,
                       Object input, Object... arguments) throws Exception {
            require(calls.size() < MAX_CALLS, "source call count cap");
            int index = calls.size();
            System.err.println("CALL_BEGIN " + index + " " + label);
            Object before = snapshot();
            Object result = null, outcome;
            try {
                result = member instanceof Constructor<?> ? ((Constructor<?>) member).newInstance(arguments)
                        : ((Method) member).invoke(receiver, arguments);
                if (member == itemConstructor) {
                    require(result != null && result.getClass() == itemType, "actual ItemEntity constructor result");
                    retain(ENTITIES, result, MAX_ENTITIES); entityClassObserved = true; mathObserved = true;
                } else if (member == stackConstructor) retain(STACKS, result, MAX_ENTITIES);
                else if (result != null && worlds.get(0).chunkType.isInstance(result))
                    retain(extraChunks, result, 16);
                Class<?> returns = member instanceof Constructor<?> ? member.getDeclaringClass() : ((Method) member).getReturnType();
                outcome = member instanceof Constructor<?> ? object("kind", "CONSTRUCTED",
                        "type", descriptor(returns), "value", value(result, returns))
                        : returns == void.class ? object("kind", "RETURN", "type", "V")
                        : returns == boolean.class ? object("kind", "RETURN", "type", "Z", "value", result)
                        : object("kind", "RETURN", "type", descriptor(returns), "value", value(result, returns));
            } catch (InvocationTargetException wrapped) {
                Throwable actual = wrapped.getCause(); require(actual != null, "actual source Throwable");
                System.err.println("SOURCE_THROWABLE " + actual.getClass().getName()); actual.printStackTrace(System.err);
                outcome = object("kind", "THROW", "class", actual.getClass().getName());
            }
            byte[] outcomeBytes = encode(outcome);
            System.err.println("SOURCE_OUTCOME " + index + " " + new String(outcomeBytes, 0, outcomeBytes.length - 1,
                    StandardCharsets.US_ASCII));
            Object after = snapshot();
            calls.add(object("index", index, "label", label, "scope", scope, "entry", describe(member),
                    "receiver", ref(receiver), "input", input, "before", before, "outcome", outcome, "after", after,
                    "state_same", Arrays.equals(encode(before), encode(after))));
            System.err.println("CALL_DONE " + index);
            return new Result(result, index);
        }
        Object footprint(World world, Object chunk) throws Exception {
            if (chunk == null || chunk.getClass() != world.chunkType)
                return object("usable", false, "owner", ref(chunk), "reason", "NO_ACTUAL_NORMAL_CHUNK");
            Object mapOwner = world.get("cache.e", world.cache);
            Method presence = (Method) field(world.modelType, "presenceMethod").get(world.model);
            Method key = (Method) field(world.modelType, "keyMethod").get(world.model);
            boolean present = (boolean) invoke(presence, world.cache, 0, 0);
            int k = (int) invoke(key, null, 0, 0);
            Object selected = ((Map<?, ?>) mapOwner).get(k);
            boolean usable = world.get("chunk.d", chunk) == world.world && (int) world.get("chunk.j", chunk) == 0 &&
                    (int) world.get("chunk.k", chunk) == 0 && present && selected == chunk;
            return object("usable", usable, "owner", ref(chunk), "world", ref(world.world),
                    "key", k, "present", present, "cache_value", ref(selected),
                    "reason", usable ? "ACTUAL_RETURNED_PRESENT_NORMAL_CHUNK" : "NO_ACTUAL_FOOTPRINT");
        }
        Object makeEntity(String label, World world, double x, double y, double z, Object stack) throws Exception {
            return observe(label, "genuine full ItemEntity constructor", itemConstructor, null,
                    object("world", ref(world.world), "x_bits", hex(Double.doubleToRawLongBits(x), 16),
                            "y_bits", hex(Double.doubleToRawLongBits(y), 16), "z_bits", hex(Double.doubleToRawLongBits(z), 16),
                            "stack", ref(stack)), world.world, x, y, z, stack).value;
        }
        void publish(String label, World world, Object entity) throws Exception {
            observe(label, "actual base World API", world.publication, world.world,
                    object("entity", ref(entity)), entity);
        }
        void add(String label, Object chunk, Object entity, String scope) throws Exception {
            Method add = chunk.getClass().getMethod("a", entityType);
            require(add.getReturnType() == void.class && add.getModifiers() == 1, "actual Chunk virtual add");
            add.setAccessible(true);
            observe(label, scope, add, chunk, object("entity", ref(entity)), entity);
        }
        byte[] run() throws Exception {
            Object initial = snapshot();
            for (World world : worlds) world.helperMeasuring.setBoolean(null, true);
            measuring = true;
            World a = worlds.get(0);
            Object stack = observe("numeric-stack", "genuine numeric Stack constructor", stackConstructor, null,
                    object("id", 1, "count", 1, "damage", 0), 1, 1, 0).value;
            require(stack != null && stack.getClass() == stackType, "genuine typed Stack substrate unavailable");
            Object absent = makeEntity("absent-item", a, 160008.25, 64.0, -159991.75, null);
            require(absent != null, "genuine absent ItemEntity substrate unavailable");
            publish("world-absent", a, absent);
            publish("world-null", a, null);
            Object normal = observe("acquire-A", "genuine original acquisition; effects retained", a.acquisition,
                    a.world, object("x", 0, "z", 0), 0, 0).value;
            Object footprintA = footprint(a, normal);
            boolean usableA = Boolean.TRUE.equals(((Map<?, ?>) footprintA).get("usable"));
            Object loaded = null;
            if (usableA) {
                add("normal-null-prefix", normal, null, "direct normal Chunk API; no World append");
                loaded = makeEntity("loaded-item", a, 8.25, 64.5, 8.5, stack);
                require(loaded != null, "genuine loaded ItemEntity substrate unavailable");
                publish("world-normal", a, loaded); publish("world-normal-duplicate", a, loaded);
                Object low = makeEntity("low-item", a, 8.25, -32.25, 8.5, null);
                Object high = makeEntity("high-item", a, 8.25, 4096.5, 8.5, stack);
                require(low != null && high != null, "genuine clamp ItemEntity substrate unavailable");
                publish("world-low", a, low); publish("world-high", a, high);
                add("direct-normal-duplicate", normal, loaded, "direct normal Chunk API; World.b unchanged");
                add("direct-low-duplicate", normal, low, "direct normal Chunk Y clamp");
                add("direct-high-duplicate", normal, high, "direct normal Chunk Y clamp");
            }
            Object blank = a.get("cache.b", a.cache);
            require(blank != null && blank.getClass().getName().equals(client ? "li" : "hg"),
                    "genuine provider-owned blank fallback");
            add("blank-item", blank, absent, "direct actual Blank override; not a World blank publication route");
            add("blank-null", blank, null, "direct actual Blank override; nullable source argument");
            Object footprintB = null;
            boolean crossCalled = false;
            if (worlds.size() == 2) {
                World b = worlds.get(1);
                Object other = observe("acquire-B", "second genuine World acquisition; effects retained", b.acquisition,
                        b.world, object("x", 0, "z", 0), 0, 0).value;
                footprintB = footprint(b, other);
                if (loaded != null && Boolean.TRUE.equals(((Map<?, ?>) footprintB).get("usable"))) {
                    publish("cross-world", b, loaded); publish("cross-world-duplicate", b, loaded);
                    crossCalled = true;
                }
            }
            Object end = snapshot();
            List<Object> bridges = new ArrayList<>();
            for (World world : worlds) bridges.add(world.bridge());
            List<Object> identities = new ArrayList<>();
            for (int i = 0; i < RETAINED.size(); ++i)
                identities.add(object("id", i + 1, "class", RETAINED.get(i).getClass().getName()));
            return encode(object("schema", "WORLD_ENTITY_PUBLICATION_OBSERVATION_V1", "side", client ? "client" : "server",
                    "scope", "finite base World and separate normal/blank Chunk APIs; no native comparison",
                    "adapter_sources", object("mutation", MUTATION_SHA, "solid_support", SUPPORT_SHA),
                    "same_original_loader", true, "cross_world_requested", worlds.size() == 2,
                    "cross_world_status", worlds.size() == 1 ? "NOT_REQUESTED"
                            : crossCalled ? "ACTUAL_CROSS_WORLD_CALLS_RECORDED" : "SKIPPED_NO_GENUINE_NORMAL_SUBSTRATE",
                    "initial", initial, "calls", calls, "call_count", calls.size(), "final", end,
                    "footprint_A", footprintA, "footprint_B", footprintB,
                    "normal_status", usableA ? "ACTUAL_NORMAL_CASES_RECORDED" : "SKIPPED_NO_ACTUAL_FOOTPRINT",
                    "local_identity_bridges", bridges, "identity_count", identities.size(), "identities", identities,
                    "snapshot_count", snapshots, "raw_context_bytes", contextBytes,
                    "source_field_restorations", "NONE_REQUIRED_CONSTRUCTOR_INPUTS_ONLY",
                    "profile_policy", "ACTUAL_ENDPOINTS_NO_CLOCK_SAMPLES_OR_INFERRED_DRAWS", "complete", true));
        }
    }
    private static final class Result {
        final Object value; final int index;
        Result(Object value, int index) { this.value = value; this.index = index; }
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out; System.setOut(System.err);
        int status = 1; List<World> worlds = new ArrayList<>();
        try {
            require((args.length == 3 || args.length == 4) && (args[1].equals("client") || args[1].equals("server")),
                    "arguments: jar client|server absent_root_A [absent_root_B]");
            Path jar = Path.of(args[0]).toRealPath(); require(Files.isRegularFile(jar), "original JAR regular file");
            ClassLoader helpers = WorldEntityPublicationObservation.class.getClassLoader();
            encoder = method(Class.forName("SolidSupportObservation", true, helpers), "encode", Object.class);
            rawJson = Class.forName("SolidSupportObservation$RawJson", true, helpers).getDeclaredConstructor(byte[].class);
            rawJson.setAccessible(true);
            try (URLClassLoader original = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
                boolean client = args[1].equals("client");
                System.err.println("SETUP_BEGIN A");
                worlds.add(new World("A", original, client, Path.of(args[2]).toAbsolutePath().normalize()));
                System.err.println("SETUP_DONE A");
                if (args.length == 4) {
                    System.err.println("SETUP_BEGIN B");
                    worlds.add(new World("B", original, client, Path.of(args[3]).toAbsolutePath().normalize()));
                    require(worlds.get(0).world != worlds.get(1).world && worlds.get(0).worldType == worlds.get(1).worldType,
                            "distinct genuine Worlds in same actual loader");
                    System.err.println("SETUP_DONE B");
                }
                Experiment experiment = new Experiment(original, client, worlds);
                byte[] transcript = experiment.run();
                protocol.write(transcript, 0, transcript.length); protocol.flush();
                require(!protocol.checkError(), "strict JSON stdout write failure");
                System.err.println("MEASUREMENT_DONE"); status = 0;
            }
        } catch (Throwable failure) {
            status = measuring ? 1 : 77;
            System.err.println(status == 77 ? "UNAVAILABLE: genuine original entity-publication setup"
                    : "FAILURE: original entity-publication measurement");
            failure.printStackTrace(System.err);
        } finally {
            for (World world : worlds) {
                try { world.helperMeasuring.setBoolean(null, world.oldMeasuring); }
                catch (Throwable failure) { failure.printStackTrace(System.err); status = 1; }
            }
            System.setOut(protocol);
        }
        System.exit(status);
    }
}
