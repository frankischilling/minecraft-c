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
import java.util.Map;

/** Finite untouched-original World query observation. Compile alongside the
 * unchanged project-owned MutationObservation and SolidSupportObservation.
 * Prepared instance inputs are recorded separately from source API calls. */
public final class WorldQueriesObservation {
    private static final String MUTATION_SHA =
            "812fdda343e5282d649ba6b9f125f9fc4f3bb918528e71eddeb267e4b034b4e9";
    private static final String SUPPORT_SHA =
            "04f3ce515f7d493a9d23b69e6a0791e241caeeeec8b1bfbbb4bc39f2d397be94";
    private static final int MAX_CALLS = 80, MAX_SNAPSHOTS = 160, MAX_OPERATIONS = 96;
    private static final int MAX_REFERENCES = 65536, MAX_COLLECTION = 4096;
    private static final long MAX_CONTEXT_BYTES = 96L * 1024 * 1024;
    private static final int MAX_ARRAY_BYTES = 1024 * 1024;
    private static boolean measuring;
    private static final IdentityHashMap<Object, Integer> IDS = new IdentityHashMap<>();
    private static final List<Object> RETAINED = new ArrayList<>();
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
    private static Object describeField(Field member) {
        return object("owner", member.getDeclaringClass().getName(), "name", member.getName(),
                "descriptor", descriptor(member.getType()), "modifiers", member.getModifiers());
    }
    private static Method sourceMethod(Class<?> owner, String name, Class<?> result, Class<?>... arguments)
            throws Exception {
        Method member = method(owner, name, arguments);
        require(member.getReturnType() == result && member.getModifiers() == 1, "original public query descriptor");
        return member;
    }

    private static final class World {
        final String name;
        final Object adapter, model, world, cache;
        final Class<?> modelType, worldType, chunkType;
        final Method get, context, bytes, localRef, acquire, top, channel, combined, visible;
        final Field helperMeasuring;
        final boolean oldMeasuring;
        World(String name, ClassLoader original, boolean client, Path root, Class<?> lightType) throws Exception {
            this.name = name;
            Class<?> adapterType = Class.forName("SolidSupportObservation$Adapter", true,
                    WorldQueriesObservation.class.getClassLoader());
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
            require(world != null && world.getClass() == worldType && worldType.getClassLoader() == original,
                    "genuine base World/loader");
            get = method(adapterType, "get", String.class, Object.class);
            context = method(adapterType, "context"); bytes = method(adapterType, "bytes", Object.class);
            localRef = method(adapterType, "ref", Object.class);
            helperMeasuring = (Field) field(adapterType, "helperMeasuring").get(adapter);
            oldMeasuring = helperMeasuring.getBoolean(null);
            acquire = sourceMethod(worldType, "c", chunkType, int.class, int.class);
            top = sourceMethod(worldType, "e", int.class, int.class, int.class);
            channel = sourceMethod(worldType, "a", int.class, lightType, int.class, int.class, int.class);
            combined = sourceMethod(worldType, client ? "m" : "k", int.class, int.class, int.class, int.class);
            visible = sourceMethod(worldType, client ? "l" : "j", boolean.class, int.class, int.class, int.class);
            ref(world); ref(cache);
        }
        Object get(String role, Object receiver) throws Exception { return invoke(get, adapter, role, receiver); }
        int local(Object value) throws Exception { return (int) invoke(localRef, adapter, value); }
        Object bridge() throws Exception {
            List<?> values = (List<?>) field(modelType, "retainedReferences").get(model);
            require(values.size() <= MAX_REFERENCES, "complete adapter identity cap");
            List<Object> rows = new ArrayList<>();
            for (int i = 0; i < values.size(); ++i)
                rows.add(object("local", i + 1, "global", ref(values.get(i)), "class", values.get(i).getClass().getName()));
            return object("namespace", name, "local_count", rows.size(), "identities", rows);
        }
        Object bindings() {
            List<Object> entries = new ArrayList<>();
            for (Method member : new Method[] {acquire, top, channel, combined, visible}) entries.add(describe(member));
            return object("namespace", name, "world", ref(world), "world_class", ref(worldType),
                    "chunk_class", ref(chunkType), "methods", entries);
        }
    }

    private static final class ArrayImage {
        final int id;
        final Object owner;
        final byte[] copy;
        final String digest;
        ArrayImage(int id, byte[] owner) throws Exception {
            this.id = id; this.owner = owner; this.copy = owner.clone(); this.digest = hash(copy);
        }
        Object reference() { return object("image", id, "owner", ref(owner), "bytes", copy.length, "sha256", digest); }
        Object describe() { return object("image", id, "owner", ref(owner), "bytes", copy.length,
                "sha256", digest, "hex", hex(copy)); }
    }
    private interface Preparation { void apply(List<Object> writes) throws Exception; }

    private static final class Experiment {
        final ClassLoader original;
        final boolean client;
        final Class<?> worldType, chunkType, blankType, lightType, nibbleType;
        final Constructor<?> normalConstructor, blankConstructor;
        final Method normalChannel, normalCombined, normalVisible;
        final Field chunkA, chunkWorld, chunkBlocks, chunkSky, chunkBlockLight, chunkHeights, nibbleBytes;
        final Field lightA, lightB, lightC, lightValues;
        final Object unsafe;
        final Method requiresInitialization;
        final List<World> worlds = new ArrayList<>();
        final List<Object> extras = new ArrayList<>(), operations = new ArrayList<>();
        final List<ArrayImage> images = new ArrayList<>();
        final IdentityHashMap<Object, List<ArrayImage>> versions = new IdentityHashMap<>();
        int snapshots, calls;
        long contextBytes, uniqueArrayBytes;
        String normalStatus = "NOT_MEASURED", blankWorldStatus = "NOT_INDUCED_NO_PROVIDER_FORGERY";

        Experiment(ClassLoader original, boolean client) throws Exception {
            this.original = original; this.client = client;
            worldType = Class.forName(client ? "fd" : "dj", false, original);
            chunkType = Class.forName(client ? "lm" : "hi", false, original);
            blankType = Class.forName(client ? "li" : "hg", false, original);
            lightType = Class.forName(client ? "eb" : "co", false, original);
            nibbleType = Class.forName(client ? "wi" : "ob", false, original);
            normalConstructor = chunkType.getDeclaredConstructor(worldType, byte[].class, int.class, int.class);
            blankConstructor = blankType.getDeclaredConstructor(worldType, int.class, int.class);
            require(normalConstructor.getModifiers() == 1 && blankConstructor.getModifiers() == 1,
                    "actual Chunk constructor descriptors");
            normalConstructor.setAccessible(true); blankConstructor.setAccessible(true);
            normalChannel = sourceMethod(chunkType, "a", int.class, lightType, int.class, int.class, int.class);
            normalCombined = sourceMethod(chunkType, "c", int.class, int.class, int.class, int.class, int.class);
            normalVisible = sourceMethod(chunkType, "c", boolean.class, int.class, int.class, int.class);
            chunkA = field(chunkType, "a"); chunkWorld = field(chunkType, "d");
            chunkBlocks = field(chunkType, "b"); chunkSky = field(chunkType, "f");
            chunkBlockLight = field(chunkType, "g"); chunkHeights = field(chunkType, "h");
            nibbleBytes = field(nibbleType, "a");
            require(chunkA.getModifiers() == 9 && chunkA.getType() == boolean.class,
                    "real Chunk declaring-class shared flag");
            require(chunkWorld.getType() == worldType && chunkBlocks.getType() == byte[].class &&
                    chunkSky.getType() == nibbleType && chunkBlockLight.getType() == nibbleType &&
                    chunkHeights.getType() == byte[].class && nibbleBytes.getType() == byte[].class,
                    "actual query input field descriptors");
            lightA = field(lightType, "a"); lightB = field(lightType, "b"); lightC = field(lightType, "c");
            lightValues = field(lightType, "d");
            require(lightA.getType() == lightType && lightB.getType() == lightType &&
                    lightC.getType() == int.class && Modifier.isStatic(lightA.getModifiers()) &&
                    Modifier.isStatic(lightB.getModifiers()) && Modifier.isFinal(lightC.getModifiers()),
                    "actual Light identity/default field descriptors");
            Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
            unsafe = field(unsafeType, "theUnsafe").get(null);
            requiresInitialization = method(unsafeType, "shouldBeInitialized", Class.class);
            require(requiresInitialization.getReturnType() == boolean.class, "read-only class-initialization probe");
        }
        Object lightState() throws Exception {
            boolean needs = (boolean) invoke(requiresInitialization, unsafe, lightType);
            Object statics = null;
            if (!needs) {
                Object sky = lightA.get(null), block = lightB.get(null), all = lightValues.get(null);
                require(all instanceof Object[] && ((Object[]) all).length <= 16, "actual complete Light values cap");
                List<Object> entries = new ArrayList<>();
                for (Object entry : (Object[]) all) {
                    require(entry == null || lightType.isInstance(entry), "actual nullable Light value");
                    entries.add(object("ordinal", entries.size(), "value", ref(entry)));
                }
                statics = object("sky", light(sky), "block", light(block), "values_owner", ref(all), "values", entries);
            }
            return object("class", ref(lightType), "name", lightType.getName(),
                    "requires_initialization", needs, "statics", statics,
                    "probe", "sun.misc.Unsafe.shouldBeInitialized(Class); read-only; no ensureInitialized");
        }
        Object light(Object value) throws Exception {
            if (value == null) return object("owner", 0);
            require(value.getClass() == lightType && value instanceof Enum<?>, "actual Light enum identity");
            Enum<?> item = (Enum<?>) value;
            return object("owner", ref(value), "class", value.getClass().getName(), "name", item.name(),
                    "ordinal", item.ordinal(), "default", lightC.getInt(value), "fields", fields(lightType, value));
        }
        Object array(Object owner) throws Exception {
            if (owner == null) return object("image", 0, "owner", 0, "bytes", null, "sha256", null);
            require(owner instanceof byte[] && ((byte[]) owner).length <= MAX_ARRAY_BYTES,
                    "actual complete array byte cap");
            byte[] current = (byte[]) owner;
            List<ArrayImage> list = versions.get(owner);
            if (list == null) { list = new ArrayList<>(); versions.put(owner, list); }
            for (ArrayImage old : list) if (Arrays.equals(old.copy, current)) return old.reference();
            require(images.size() < 4096 && current.length <= 64L * 1024 * 1024 - uniqueArrayBytes,
                    "retained distinct full array-image cap");
            ArrayImage image = new ArrayImage(images.size() + 1, current);
            images.add(image); list.add(image); uniqueArrayBytes += current.length;
            return image.reference();
        }
        Object chunk(Object owner) throws Exception {
            require(owner != null && chunkType.isInstance(owner), "actual Chunk image receiver");
            List<Object> arrays = new ArrayList<>();
            arrays.add(object("role", "b", "value", array(chunkBlocks.get(owner))));
            for (String role : new String[] {"e", "f", "g"}) {
                Object wrapper = field(chunkType, role).get(owner);
                require(wrapper == null || wrapper.getClass() == nibbleType, "actual nullable Nibble wrapper");
                arrays.add(object("role", role, "wrapper", ref(wrapper),
                        "wrapper_fields", wrapper == null ? null : fields(nibbleType, wrapper),
                        "value", array(wrapper == null ? null : nibbleBytes.get(wrapper))));
            }
            arrays.add(object("role", "h", "value", array(chunkHeights.get(owner))));
            return object("owner", ref(owner), "class", owner.getClass().getName(),
                    "world", ref(chunkWorld.get(owner)), "fields", fields(chunkType, owner), "arrays", arrays);
        }
        Object registryInputs(World world) throws Exception {
            Object map = world.get("block.m", null);
            require(map instanceof Object[] && ((Object[]) map).length == 256, "actual current registry slots");
            Class<?> blockType = (Class<?>) field(world.modelType, "blockType").get(world.model);
            Field id = field(blockType, "bn"), materialField = field(blockType, "bA");
            List<Object> rows = new ArrayList<>();
            for (int index : new int[] {1, 8, 37, 255}) {
                Object block = ((Object[]) map)[index], material = block == null ? null : materialField.get(block);
                List<Object> declarations = new ArrayList<>();
                if (material != null) {
                    for (Class<?> declaring = material.getClass(); declaring != Object.class &&
                            declaring.getClassLoader() == original; declaring = declaring.getSuperclass())
                        declarations.add(object("class", declaring.getName(), "fields", fields(declaring, material)));
                }
                rows.add(object("index", index, "block", ref(block),
                        "class", block == null ? null : block.getClass().getName(),
                        "bn", block == null ? null : id.getInt(block), "material", ref(material),
                        "material_class", material == null ? null : material.getClass().getName(),
                        "material_declarations", declarations,
                        "predicate_c", material == null ? null : describe(material.getClass().getMethod("c")),
                        "predicate_d", material == null ? null : describe(material.getClass().getMethod("d"))));
            }
            return object("map", ref(map), "entries", rows, "policy", "actual read-only slot/material identities; no helper predicate invocation");
        }
        Object snapshot() throws Exception {
            require(snapshots < MAX_SNAPSHOTS, "full snapshot cap"); ++snapshots;
            Object light = lightState();
            boolean flag = chunkA.getBoolean(null);
            List<Object> states = new ArrayList<>();
            for (World world : worlds) {
                Object context = invoke(world.context, world.adapter);
                byte[] bytes = (byte[]) invoke(world.bytes, world.adapter, context);
                require(bytes.length <= MAX_CONTEXT_BYTES - contextBytes, "all raw Context bytes cap");
                contextBytes += bytes.length;
                Object map = world.get("cache.e", world.cache), insertion = world.get("cache.f", world.cache);
                require(map instanceof Map<?, ?> && ((Map<?, ?>) map).size() <= MAX_COLLECTION &&
                        insertion instanceof List<?> && ((List<?>) insertion).size() <= MAX_COLLECTION,
                        "complete actual cache map/insertion cap");
                List<Object> candidates = new ArrayList<>(), chunks = new ArrayList<>();
                candidates.add(world.get("cache.b", world.cache));
                candidates.addAll(((Map<?, ?>) map).values()); candidates.addAll((List<?>) insertion);
                for (Object extra : extras) if (chunkWorld.get(extra) == world.world) candidates.add(extra);
                IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>();
                for (Object candidate : candidates)
                    if (candidate != null && seen.put(candidate, Boolean.TRUE) == null) chunks.add(chunk(candidate));
                states.add(object("namespace", world.name, "world", ref(world.world),
                        "world_class", ref(world.worldType), "chunk_class", ref(world.chunkType),
                        "chunk_class_a", flag, "fields", fields(world.worldType, world.world),
                        "local_world", world.local(world.world), "raw_context", raw(bytes),
                        "raw_context_bytes", bytes.length, "raw_context_sha256", hash(bytes),
                        "registry_inputs", registryInputs(world), "chunks", chunks));
            }
            List<Object> detached = new ArrayList<>();
            for (Object extra : extras) if (chunkWorld.get(extra) == null) detached.add(chunk(extra));
            return object("ordinal", snapshots - 1, "original_loader", ref(original),
                    "chunk_class", ref(chunkType), "chunk_class_name", chunkType.getName(),
                    "chunk_class_a", flag, "light", light, "worlds", states, "detached_chunks", detached);
        }
        Object call(String label, String scope, Executable member, Object receiver,
                    Object input, Object... arguments) throws Exception {
            require(calls < MAX_CALLS && operations.size() < MAX_OPERATIONS, "finite operation/call cap");
            int callIndex = calls++, operationIndex = operations.size();
            System.err.println("CALL_BEGIN " + callIndex + " " + label);
            Object before = snapshot(), result = null, outcome;
            measuring = true;
            try {
                result = member instanceof Constructor<?> ? ((Constructor<?>) member).newInstance(arguments)
                        : ((Method) member).invoke(receiver, arguments);
                if (result != null && chunkType.isInstance(result)) retain(extras, result, 32);
                Class<?> type = member instanceof Constructor<?> ? member.getDeclaringClass() : ((Method) member).getReturnType();
                outcome = member instanceof Constructor<?> ? object("kind", "CONSTRUCTED", "type", descriptor(type),
                        "value", value(result, type)) : object("kind", "RETURN", "type", descriptor(type),
                        "value", type == void.class ? null : type == int.class || type == boolean.class
                                ? result : value(result, type));
            } catch (InvocationTargetException wrapped) {
                Throwable actual = wrapped.getCause(); require(actual != null, "actual source Throwable");
                System.err.println("SOURCE_THROWABLE " + actual.getClass().getName()); actual.printStackTrace(System.err);
                outcome = object("kind", "THROW", "class", actual.getClass().getName());
            }
            byte[] outcomeBytes = encode(outcome);
            System.err.println("SOURCE_OUTCOME " + callIndex + " " + new String(outcomeBytes, 0,
                    outcomeBytes.length - 1, StandardCharsets.US_ASCII));
            Object after = snapshot();
            operations.add(object("index", operationIndex, "kind", "SOURCE_CALL", "call_index", callIndex,
                    "label", label, "scope", scope, "entry", describe(member),
                    "virtual_target", member instanceof Method && receiver != null
                            ? describe(receiver.getClass().getMethod(member.getName(), member.getParameterTypes())) : null,
                    "receiver", ref(receiver),
                    "input", input, "before", before, "outcome", outcome, "after", after));
            System.err.println("CALL_DONE " + callIndex);
            return result;
        }
        void prepare(String label, String scope, Preparation action) throws Exception {
            require(operations.size() < MAX_OPERATIONS, "preparation operation cap");
            int index = operations.size(); Object before = snapshot(); List<Object> writes = new ArrayList<>();
            action.apply(writes);
            require(writes.size() <= 4096, "ordered prepared-input write cap");
            Object after = snapshot();
            operations.add(object("index", index, "kind", "PREPARED_INPUT", "label", label,
                    "scope", scope, "policy", "EXPLICIT_INSTANCE_INPUT_PREPARATION_NOT_NATURAL_TERRAIN",
                    "before", before, "writes", writes, "after", after));
        }
        void byteWrite(List<Object> writes, byte[] owner, int index, int replacement, String role) {
            require(index >= 0 && index < owner.length && replacement >= 0 && replacement <= 255,
                    "native preparation byte input");
            int old = owner[index] & 255;
            owner[index] = (byte) replacement;
            writes.add(object("ordinal", writes.size(), "kind", "BYTE_ARRAY_STORE", "role", role,
                    "owner", ref(owner), "index", index, "old", old, "new", replacement));
        }
        void nibbleWrite(List<Object> writes, Object wrapper, int x, int y, int z, int replacement, String role)
                throws Exception {
            require(wrapper != null && wrapper.getClass() == nibbleType && replacement >= 0 && replacement <= 15,
                    "actual prepared Nibble input");
            byte[] data = (byte[]) nibbleBytes.get(wrapper); int cell = (x << 11) | (z << 7) | y;
            int index = cell >> 1, shift = (cell & 1) * 4;
            require(data != null && index >= 0 && index < data.length, "prepared Nibble backing/index");
            int byteValue = data[index] & 255;
            byteWrite(writes, data, index, (byteValue & ~(15 << shift)) | (replacement << shift), role);
        }
        void referenceWrite(List<Object> writes, Field member, Object receiver, Object replacement) throws Exception {
            require(!Modifier.isStatic(member.getModifiers()) && !Modifier.isFinal(member.getModifiers()),
                    "prepared instance field only; no class reset/final write");
            Object old = member.get(receiver); member.set(receiver, replacement);
            writes.add(object("ordinal", writes.size(), "kind", "INSTANCE_REFERENCE_STORE", "field", describeField(member),
                    "receiver", ref(receiver), "old", ref(old), "new", ref(replacement)));
        }
        void setup(String name, Path root) throws Exception {
            require(operations.size() < MAX_OPERATIONS, "genuine setup operation cap");
            Object before = snapshot(); int index = operations.size();
            System.err.println("SETUP_BEGIN " + name);
            World world = new World(name, original, client, root, lightType);
            require(world.worldType == worldType && world.chunkType == chunkType, "same actual source class context");
            worlds.add(world);
            Object after = snapshot();
            operations.add(object("index", index, "kind", "GENUINE_WORLD_SETUP", "label", "setup-" + name,
                    "root", root.toString(), "scope", "unchanged sealed Model constructor/setup; no prior main/construct/observe",
                    "before", before, "after", after, "world", ref(world.world)));
            System.err.println("SETUP_DONE " + name);
        }
        boolean footprint(World world, Object chunk) throws Exception {
            if (chunk == null || chunk.getClass() != chunkType || chunkWorld.get(chunk) != world.world) return false;
            if (field(chunkType, "j").getInt(chunk) != 0 || field(chunkType, "k").getInt(chunk) != 0) return false;
            Object map = world.get("cache.e", world.cache);
            Method key = (Method) field(world.modelType, "keyMethod").get(world.model);
            Method presence = (Method) field(world.modelType, "presenceMethod").get(world.model);
            require(map instanceof Map<?, ?>, "actual footprint map");
            int packed = (int) invoke(key, null, 0, 0);
            return (boolean) invoke(presence, world.cache, 0, 0) && ((Map<?, ?>) map).get(packed) == chunk;
        }
        void column(Object chunk, int... pairs) throws Exception {
            require((pairs.length & 1) == 0, "prepared Y/block pairs");
            prepare("prepared-column", "explicit current normal Chunk.b column x8,z8; all Y input stores recorded", writes -> {
                byte[] blocks = (byte[]) chunkBlocks.get(chunk);
                require(blocks != null && blocks.length == 32768, "actual supplied complete block array");
                for (int y = 0; y < 128; ++y) byteWrite(writes, blocks, (8 << 11) | (8 << 7) | y, 0, "b");
                for (int i = 0; i < pairs.length; i += 2) {
                    require(pairs[i] >= 0 && pairs[i] < 128, "prepared column Y");
                    byteWrite(writes, blocks, (8 << 11) | (8 << 7) | pairs[i], pairs[i + 1], "b");
                }
            });
        }
        Object bindings() {
            List<Object> methods = new ArrayList<>(), fieldRows = new ArrayList<>(), worldRows = new ArrayList<>();
            for (Executable member : new Executable[] {normalConstructor, blankConstructor,
                    normalChannel, normalCombined, normalVisible}) methods.add(describe(member));
            for (Field member : new Field[] {chunkA, chunkWorld, chunkBlocks, chunkSky, chunkBlockLight,
                    chunkHeights, nibbleBytes, lightA, lightB, lightC, lightValues}) fieldRows.add(describeField(member));
            for (World world : worlds) worldRows.add(world.bindings());
            return object("direct_members", methods, "fields", fieldRows, "worlds", worldRows,
                    "class_state_probe", describe(requiresInitialization),
                    "adapter_read_scope", "actual sealed complete Context; its documented presence/blank readers remain read-only",
                    "array_scope", "all distinct fallback/map/insertion/explicit Chunk identities; exact immutable full-byte images");
        }
        byte[] run(Path rootA, Path rootB) throws Exception {
            Object initial = snapshot();
            require(Boolean.FALSE.equals(((Map<?, ?>) initial).get("chunk_class_a")), "fresh source Chunk.a default false");
            require(Boolean.TRUE.equals(((Map<?, ?>) ((Map<?, ?>) initial).get("light")).get("requires_initialization")),
                    "fresh Light remains uninitialized before direct prelude");
            byte[] zeroBlocks = new byte[32768];
            Object normal = call("cold-normal-construct", "actual unpublished normal Chunk constructor; nullable World argument",
                    normalConstructor, null, object("world", 0, "blocks", array(zeroBlocks), "x", 10000, "z", -10000),
                    null, zeroBlocks, 10000, -10000);
            Object blank = call("cold-blank-construct", "actual unpublished Blank constructor; nullable World argument",
                    blankConstructor, null, object("world", 0, "x", 10001, "z", -10001), null, 10001, -10001);
            require(normal != null && normal.getClass() == chunkType && blank != null && blank.getClass() == blankType,
                    "genuine direct prelude substrates");
            call("cold-blank-null-channel", "actual Blank override; no normal arrays or Light argument active use",
                    normalChannel, blank, object("light", 0, "x", 8, "y", -500, "z", 8), null, 8, -500, 8);
            call("cold-blank-combined", "actual Blank combined override", normalCombined, blank,
                    object("x", 8, "y", 64, "z", 8, "subtraction", Integer.MIN_VALUE), 8, 64, 8, Integer.MIN_VALUE);
            call("cold-blank-visible", "actual Blank sky-visible override", normalVisible, blank,
                    object("x", 8, "y", 255, "z", 8), 8, 255, 8);
            call("cold-normal-zero-combined", "zero constructor Nibbles; no Light active use or positive sky",
                    normalCombined, normal, object("x", 8, "y", 64, "z", 8, "subtraction", 0), 8, 64, 8, 0);
            call("cold-normal-wrapped-subtraction", "direct combined argument; source wrapped subtraction, separate from World literal-zero wrapper",
                    normalCombined, normal, object("x", 8, "y", 64, "z", 8, "subtraction", Integer.MIN_VALUE),
                    8, 64, 8, Integer.MIN_VALUE);
            call("cold-normal-visible", "normal unsigned height read does not use Light", normalVisible, normal,
                    object("x", 8, "y", 0, "z", 8), 8, 0, 8);
            require((boolean) invoke(requiresInitialization, unsafe, lightType), "zero/blank prelude retained cold Light");
            call("cold-normal-null-channel", "actual normal channel getstatic Sky then Block; nullable argument",
                    normalChannel, normal, object("light", 0, "x", 8, "y", 64, "z", 8), null, 8, 64, 8);
            require(!(boolean) invoke(requiresInitialization, unsafe, lightType), "actual normal NULL channel initialized Light");
            Object sky = lightA.get(null), block = lightB.get(null);
            require(sky != null && block != null && sky != block, "actual initialized distinct Light identities");
            call("warm-direct-sky", "actual normal Sky identity", normalChannel, normal,
                    object("light", ref(sky), "x", 8, "y", 64, "z", 8), sky, 8, 64, 8);
            call("warm-direct-block", "actual normal Block identity", normalChannel, normal,
                    object("light", ref(block), "x", 8, "y", 64, "z", 8), block, 8, 64, 8);
            call("warm-direct-blank-sky", "actual Blank Sky override", normalChannel, blank,
                    object("light", ref(sky), "x", 8, "y", 64, "z", 8), sky, 8, 64, 8);
            setup("A", rootA); setup("B", rootB);
            World a = worlds.get(0), b = worlds.get(1);
            require(a.world != b.world && a.cache != b.cache && a.chunkType == b.chunkType,
                    "distinct genuine Worlds sharing one actual Chunk class");
            for (World world : worlds) world.helperMeasuring.setBoolean(null, true);
            call("A-missing-null-channel", "actual inside-border nonloading absence; no load is forged", a.channel, a.world,
                    object("light", 0, "x", 160008, "y", 64, "z", -159992), null, 160008, 64, -159992);
            call("A-missing-negative-combined", "actual negative-Y return before acquisition", a.combined, a.world,
                    object("x", 160008, "y", -1, "z", -159992), 160008, -1, -159992);
            call("A-outside-null-channel", "actual nullable Light.c outside branch after Y clamp", a.channel, a.world,
                    object("light", 0, "x", 32000000, "y", -500, "z", 8), null, 32000000, -500, 8);
            call("A-outside-sky-channel", "actual Sky.c outside branch after high-Y clamp", a.channel, a.world,
                    object("light", ref(sky), "x", 32000000, "y", 500, "z", 8), sky, 32000000, 500, 8);
            call("A-outside-block-channel", "actual Block.c outside branch", a.channel, a.world,
                    object("light", ref(block), "x", 8, "y", 64, "z", 32000001), block, 8, 64, 32000001);
            call("A-min-x-null-channel", "actual inclusive negative-X border; NULL does not select outside field", a.channel, a.world,
                    object("light", 0, "x", -32000000, "y", 64, "z", 8), null, -32000000, 64, 8);
            call("A-max-z-null-channel", "actual inclusive positive-Z border and low-Y clamp", a.channel, a.world,
                    object("light", 0, "x", 8, "y", -500, "z", 32000000), null, 8, -500, 32000000);
            call("A-below-min-x-null-channel", "actual exclusive point below negative-X domain", a.channel, a.world,
                    object("light", 0, "x", -32000001, "y", 64, "z", 8), null, -32000001, 64, 8);
            call("A-below-min-z-null-channel", "actual exclusive point below negative-Z domain", a.channel, a.world,
                    object("light", 0, "x", 8, "y", 64, "z", -32000001), null, 8, 64, -32000001);
            Object chunkA = call("A-acquire-zero-zero", "actual original World getter; all effects retained", a.acquire, a.world,
                    object("x", 0, "z", 0), 0, 0);
            Object chunkB = call("B-acquire-zero-zero", "second actual original World getter", b.acquire, b.world,
                    object("x", 0, "z", 0), 0, 0);
            if (footprint(a, chunkA) && footprint(b, chunkB)) {
                normalStatus = "ACTUAL_NORMAL_WORLD_CASES_RECORDED";
                Object originalBlockLight = chunkBlockLight.get(chunkA);
                prepare("A-prepared-lights-height-null-block", "explicit Nibble cells, unsigned height255 and NULL block-light wrapper input", writes -> {
                    Object s = chunkSky.get(chunkA), l = chunkBlockLight.get(chunkA);
                    nibbleWrite(writes, s, 8, 0, 8, 6, "f.a"); nibbleWrite(writes, l, 8, 0, 8, 4, "g.a");
                    nibbleWrite(writes, s, 8, 64, 8, 9, "f.a"); nibbleWrite(writes, l, 8, 64, 8, 11, "g.a");
                    nibbleWrite(writes, s, 8, 127, 8, 12, "f.a"); nibbleWrite(writes, l, 8, 127, 8, 5, "g.a");
                    byteWrite(writes, (byte[]) chunkHeights.get(chunkA), (8 << 4) | 8, 255, "h");
                    referenceWrite(writes, chunkBlockLight, chunkA, null);
                });
                call("A-positive-sky-late-null-prefix", "positive actual sky precedes NULL block-light invoke; shared Chunk.a is never reset",
                        a.combined, a.world, object("x", 8, "y", 64, "z", 8), 8, 64, 8);
                prepare("A-input-wrapper-restore", "restore only the prepared g input identity; retain every actual source effect", writes ->
                        referenceWrite(writes, chunkBlockLight, chunkA, originalBlockLight));
                prepare("B-prepared-zero-light", "independent World B actual Nibble input cells", writes -> {
                    nibbleWrite(writes, chunkSky.get(chunkB), 8, 64, 8, 0, "f.a");
                    nibbleWrite(writes, chunkBlockLight.get(chunkB), 8, 64, 8, 0, "g.a");
                });
                call("B-zero-combined-after-A-prefix", "same original Chunk class retains its flag across Worlds and zero sky",
                        b.combined, b.world, object("x", 8, "y", 64, "z", 8), 8, 64, 8);
                call("A-normal-null-channel", "actual normal NULL channel, already initialized Light", a.channel, a.world,
                        object("light", 0, "x", 8, "y", 64, "z", 8), null, 8, 64, 8);
                call("A-low-clamped-sky", "actual World y<0 clamp to0", a.channel, a.world,
                        object("light", ref(sky), "x", 8, "y", -500, "z", 8), sky, 8, -500, 8);
                call("A-high-clamped-sky", "actual World high-Y clamp to127", a.channel, a.world,
                        object("light", ref(sky), "x", 8, "y", 500, "z", 8), sky, 8, 500, 8);
                call("A-high-clamped-block", "actual Block identity at clamped127", a.channel, a.world,
                        object("light", ref(block), "x", 8, "y", 500, "z", 8), block, 8, 500, 8);
                call("A-high-clamped-combined", "World combined passes literal subtraction0", a.combined, a.world,
                        object("x", 8, "y", 500, "z", 8), 8, 500, 8);
                for (int y : new int[] {-1, 0, 254, 255, 4096})
                    call("A-visible-height255-y" + y, "actual acquisition then unsigned prepared height comparison; no Y clamp",
                            a.visible, a.world, object("x", 8, "y", y, "z", 8), 8, y, 8);
                Method key = (Method) field(a.modelType, "keyMethod").get(a.model);
                int aliasX = 32505864;
                int zeroKey = (int) invoke(key, null, 0, 0), aliasKey = (int) invoke(key, null, aliasX >> 4, 0);
                require(zeroKey == aliasKey && ((Map<?, ?>) a.get("cache.e", a.cache)).get(aliasKey) == chunkA,
                        "actual original packed-key alias of same existing normal Chunk");
                Object alias = object("x", aliasX, "z", 8, "actual_key_method", describe(key),
                        "actual_zero_key", zeroKey, "actual_alias_key", aliasKey, "selected_chunk", ref(chunkA));
                call("A-outside-combined-packed-alias", "no X border; actual existing packed-key alias, no forged cache entry",
                        a.combined, a.world, object("alias", alias, "y", 500), aliasX, 500, 8);
                call("A-outside-visible-packed-alias", "no X border; acquire same alias before unsigned height compare",
                        a.visible, a.world, object("alias", alias, "y", 255), aliasX, 255, 8);
                column(chunkA, 0, 1);
                call("A-top-y0-only", "explicit stone only at y0; source scan excludes y0", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
                column(chunkA, 0, 1, 127, 37);
                call("A-top-plant-only-above-y0", "explicit current registry block37 plant above excluded y0", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
                column(chunkA, 0, 1, 64, 8, 127, 37);
                call("A-top-water-under-plant", "explicit current registry water8/plant37 input; real material predicates", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
                column(chunkA, 127, 1);
                call("A-top-solid127", "explicit solid at source first scanned Y", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
                call("A-outside-top-packed-alias", "no X border; single actual captured aliased Chunk", a.top, a.world,
                        object("alias", alias), aliasX, 8);
                column(chunkA, 127, 255);
                call("A-top-null-block255", "explicit invalid raw-array block255; actual null registered block receiver", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
                column(chunkA);
                call("A-top-all-air", "explicit all-air selected column", a.top, a.world,
                        object("x", 8, "z", 8), 8, 8);
            } else normalStatus = "SKIPPED_NO_GENUINE_NORMAL_FOOTPRINT";
            Object end = snapshot();
            Object bindingDescription = bindings();
            List<Object> bridges = new ArrayList<>(), identities = new ArrayList<>(), arrayRows = new ArrayList<>();
            for (World world : worlds) bridges.add(world.bridge());
            for (ArrayImage image : images) arrayRows.add(image.describe());
            for (int i = 0; i < RETAINED.size(); ++i)
                identities.add(object("id", i + 1, "class", RETAINED.get(i).getClass().getName()));
            return encode(object("schema", "WORLD_QUERIES_OBSERVATION_V1", "side", client ? "client" : "server",
                    "scope", "finite untouched-original World and separate direct normal/blank APIs; explicit prepared inputs; no native expected replay",
                    "adapter_sources", object("mutation", MUTATION_SHA, "solid_support", SUPPORT_SHA),
                    "bindings", bindingDescription, "initial", initial, "operations", operations,
                    "operation_count", operations.size(), "call_count", calls, "final", end,
                    "snapshot_count", snapshots, "raw_context_bytes", contextBytes,
                    "array_images", arrayRows, "array_image_count", images.size(), "unique_array_bytes", uniqueArrayBytes,
                    "local_identity_bridges", bridges, "identity_count", identities.size(), "identities", identities,
                    "normal_status", normalStatus, "world_blank_status", blankWorldStatus,
                    "cold_scope", "separate unpublished Chunk APIs precede adapter-induced Light active use; same original loader",
                    "class_static_writes", "NONE_BY_HELPER; actual combined-light store retained; no Chunk.a reset",
                    "source_input_policy", "only explicit recorded instance-array and nonfinal instance-reference preparation",
                    "complete", true));
        }
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out; System.setOut(System.err);
        int status = 1; Experiment experiment = null;
        try {
            require(args.length == 4 && (args[1].equals("client") || args[1].equals("server")),
                    "arguments: jar client|server absent_root_A absent_root_B");
            Path jar = Path.of(args[0]).toRealPath(); require(Files.isRegularFile(jar), "original JAR regular file");
            ClassLoader helpers = WorldQueriesObservation.class.getClassLoader();
            encoder = method(Class.forName("SolidSupportObservation", true, helpers), "encode", Object.class);
            rawJson = Class.forName("SolidSupportObservation$RawJson", true, helpers).getDeclaredConstructor(byte[].class);
            rawJson.setAccessible(true);
            try (URLClassLoader original = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
                experiment = new Experiment(original, args[1].equals("client"));
                byte[] transcript = experiment.run(Path.of(args[2]).toAbsolutePath().normalize(),
                        Path.of(args[3]).toAbsolutePath().normalize());
                protocol.write(transcript, 0, transcript.length); protocol.flush();
                require(!protocol.checkError(), "strict JSON stdout write failure");
                System.err.println("MEASUREMENT_DONE"); status = 0;
            }
        } catch (Throwable failure) {
            status = measuring ? 1 : 77;
            System.err.println(status == 77 ? "UNAVAILABLE: original query prerequisites"
                    : "FAILURE: original query measurement");
            failure.printStackTrace(System.err);
        } finally {
            if (experiment != null && !experiment.worlds.isEmpty()) {
                World first = experiment.worlds.get(0);
                try { first.helperMeasuring.setBoolean(null, first.oldMeasuring); }
                catch (Throwable failure) { failure.printStackTrace(System.err); status = 1; }
            }
            System.setOut(protocol);
        }
        System.exit(status);
    }
}
