import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
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
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/** Untouched-original, finite direct World/Block/Material API experiment.
 * Compile alongside the project-owned MutationObservation.java.
 * Only that helper's genuine setup and read-only Context/identity adapter are
 * reused. No original class, World/cache publication or result is fabricated. */
public final class SolidSupportObservation {
    private static final String ADAPTER_SHA256 =
            "812fdda343e5282d649ba6b9f125f9fc4f3bb918528e71eddeb267e4b034b4e9";
    private static final int MAX_OPERATIONS = 80;
    private static final int MAX_CONTEXTS = 192;
    private static final int MAX_COLLECTION = 4096;
    private static final int MAX_CONTEXT_BYTES = 32 * 1024 * 1024;
    private static final long MAX_ALL_CONTEXT_BYTES = 96L * 1024 * 1024;
    private static final int MAX_OUTPUT_BYTES = 128 * 1024 * 1024;
    private static boolean measuring;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static Object invoke(Method method, Object receiver, Object... arguments)
            throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException wrapped) {
            Throwable actual = wrapped.getCause();
            if (actual instanceof Exception) throw (Exception) actual;
            if (actual instanceof Error) throw (Error) actual;
            throw wrapped;
        }
    }

    private static Object construct(Constructor<?> constructor, Object... arguments)
            throws Exception {
        try { return constructor.newInstance(arguments); }
        catch (InvocationTargetException wrapped) {
            Throwable actual = wrapped.getCause();
            if (actual instanceof Exception) throw (Exception) actual;
            if (actual instanceof Error) throw (Error) actual;
            throw wrapped;
        }
    }

    private static Method ownMethod(Class<?> type, String name, Class<?>... arguments)
            throws Exception {
        Method method = type.getDeclaredMethod(name, arguments);
        method.setAccessible(true);
        return method;
    }

    private static Field ownField(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }

    private static Field sourceField(Class<?> type, String name, Class<?> fieldType, int modifiers)
            throws Exception {
        Field field = ownField(type, name);
        require(field.getType() == fieldType && field.getModifiers() == modifiers,
                "original field descriptor/modifiers: " + type.getName() + "." + name);
        return field;
    }

    private static Method sourceMethod(Class<?> type, String name, Class<?> result,
                                       Class<?>... arguments) throws Exception {
        Method method = type.getDeclaredMethod(name, arguments);
        require(method.getReturnType() == result && method.getModifiers() == 1,
                "original public method descriptor: " + type.getName() + "." + name);
        method.setAccessible(true);
        return method;
    }

    private static Method virtualMethod(Object receiver, String name) throws Exception {
        Method method = receiver.getClass().getMethod(name);
        require(method.getReturnType() == boolean.class && method.getModifiers() == 1,
                "actual public virtual Boolean predicate");
        method.setAccessible(true);
        return method;
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

    private static String bits(long value) { return String.format(Locale.ROOT, "%016x", value); }
    private static String floatBits(float value) {
        return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value));
    }
    private static String doubleBits(double value) { return bits(Double.doubleToRawLongBits(value)); }

    private static String sha256(byte[] bytes) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder text = new StringBuilder(64);
        for (byte value : digest) text.append(String.format(Locale.ROOT, "%02x", value & 255));
        return text.toString();
    }

    private static final class RawJson {
        final byte[] bytes;
        RawJson(byte[] bytes) { this.bytes = bytes; }
    }

    private static final class LimitedBytes extends ByteArrayOutputStream {
        private void room(int extra) {
            require(extra >= 0 && extra <= MAX_OUTPUT_BYTES - count, "transcript byte cap");
        }
        @Override public synchronized void write(int value) { room(1); super.write(value); }
        @Override public synchronized void write(byte[] bytes, int offset, int length) {
            room(length); super.write(bytes, offset, length);
        }
    }

    private static void ascii(LimitedBytes out, String value) {
        byte[] bytes = value.getBytes(StandardCharsets.US_ASCII);
        out.write(bytes, 0, bytes.length);
    }

    private static void json(LimitedBytes out, Object value) {
        if (value instanceof RawJson) {
            byte[] bytes = ((RawJson) value).bytes;
            out.write(bytes, 0, bytes.length);
        } else if (value == null) ascii(out, "null");
        else if (value instanceof String) {
            out.write('"');
            for (char unit : ((String) value).toCharArray()) {
                if (unit == '"' || unit == '\\') { out.write('\\'); out.write(unit); }
                else if (unit < 32 || unit > 126)
                    ascii(out, String.format(Locale.ROOT, "\\u%04x", (int) unit));
                else out.write(unit);
            }
            out.write('"');
        } else if (value instanceof Boolean || value instanceof Integer || value instanceof Long)
            ascii(out, value.toString());
        else if (value instanceof List<?>) {
            out.write('[');
            boolean comma = false;
            for (Object item : (List<?>) value) {
                if (comma) out.write(',');
                json(out, item); comma = true;
            }
            out.write(']');
        } else if (value instanceof Map<?, ?>) {
            out.write('{');
            boolean comma = false;
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                require(entry.getKey() instanceof String, "JSON string key");
                if (comma) out.write(',');
                json(out, entry.getKey()); out.write(':'); json(out, entry.getValue()); comma = true;
            }
            out.write('}');
        } else throw new IllegalStateException("unsupported JSON value: " + value.getClass().getName());
    }

    private static byte[] encode(Object value) {
        LimitedBytes out = new LimitedBytes();
        json(out, value); out.write('\n');
        return out.toByteArray();
    }

    private static String descriptor(Class<?> type) {
        if (type == void.class) return "V";
        if (type == boolean.class) return "Z";
        if (type == byte.class) return "B";
        if (type == char.class) return "C";
        if (type == short.class) return "S";
        if (type == int.class) return "I";
        if (type == long.class) return "J";
        if (type == float.class) return "F";
        if (type == double.class) return "D";
        if (type.isArray()) return type.getName().replace('.', '/');
        return "L" + type.getName().replace('.', '/') + ";";
    }

    private static Object describe(Method method) {
        StringBuilder signature = new StringBuilder("(");
        for (Class<?> type : method.getParameterTypes()) signature.append(descriptor(type));
        signature.append(')').append(descriptor(method.getReturnType()));
        return object("owner", method.getDeclaringClass().getName(), "name", method.getName(),
                "descriptor", signature.toString(), "modifiers", method.getModifiers());
    }

    private static Object describe(Field field) {
        return object("owner", field.getDeclaringClass().getName(), "name", field.getName(),
                "descriptor", descriptor(field.getType()), "modifiers", field.getModifiers());
    }

    private static Object describe(Constructor<?> constructor) {
        StringBuilder signature = new StringBuilder("(");
        for (Class<?> type : constructor.getParameterTypes()) signature.append(descriptor(type));
        signature.append(")V");
        return object("owner", constructor.getDeclaringClass().getName(), "name", "<init>",
                "descriptor", signature.toString(), "modifiers", constructor.getModifiers());
    }

    private static final class Adapter {
        final Object model, world, cache, baseline;
        final Class<?> modelType, contextType, worldType, chunkType, blockType;
        final Constructor<?> contextConstructor;
        final Method get, ref, table, same, sameCache, sameQueue, sameObservers;
        final Method randomSame, registrySame;
        final Field contextEncoded, contextRandom, contextRegistry, helperMeasuring;
        final boolean oldHelperMeasuring;
        long contextBytes;
        int contexts;

        Adapter(ClassLoader original, boolean client, Path root) throws Exception {
            ClassLoader helpers = SolidSupportObservation.class.getClassLoader();
            Class<?> outer = Class.forName("MutationObservation", true, helpers);
            modelType = Class.forName("MutationObservation$Model", true, helpers);
            contextType = Class.forName("MutationObservation$Context", true, helpers);
            Constructor<?> constructor = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
            constructor.setAccessible(true);
            model = construct(constructor, original, client);
            invoke(ownMethod(modelType, "setup", Path.class), model, root);
            get = ownMethod(modelType, "get", String.class, Object.class);
            ref = ownMethod(modelType, "ref", Object.class);
            table = ownMethod(modelType, "identityTable");
            contextConstructor = contextType.getDeclaredConstructor(modelType);
            contextConstructor.setAccessible(true);
            same = ownMethod(contextType, "same", contextType);
            sameCache = ownMethod(contextType, "sameCache", contextType);
            sameQueue = ownMethod(contextType, "sameQueue", contextType);
            sameObservers = ownMethod(contextType, "sameObservers", contextType);
            contextEncoded = ownField(contextType, "encoded");
            contextRandom = ownField(contextType, "random");
            contextRegistry = ownField(contextType, "registry");
            randomSame = ownMethod(contextRandom.getType(), "same", contextRandom.getType());
            registrySame = ownMethod(contextRegistry.getType(), "same", contextRegistry.getType());
            helperMeasuring = ownField(outer, "measuring");
            oldHelperMeasuring = helperMeasuring.getBoolean(null);
            world = ownField(modelType, "world").get(model);
            cache = ownField(modelType, "cache").get(model);
            baseline = ownField(modelType, "baseline").get(model);
            worldType = (Class<?>) ownField(modelType, "worldType").get(model);
            chunkType = (Class<?>) ownField(modelType, "chunkType").get(model);
            blockType = (Class<?>) ownField(modelType, "blockType").get(model);
            require(world != null && world.getClass() == worldType, "genuine base World receiver");
        }

        Object get(String role, Object receiver) throws Exception { return invoke(get, model, role, receiver); }
        int ref(Object value) throws Exception { return (int) invoke(ref, model, value); }
        Object context() throws Exception { return construct(contextConstructor, model); }
        Object identities() throws Exception { return invoke(table, model); }
        boolean compare(Method method, Object before, Object after) throws Exception {
            return (boolean) invoke(method, before, after);
        }
        byte[] bytes(Object context) throws Exception {
            byte[] bytes = (byte[]) contextEncoded.get(context);
            require(bytes != null && bytes.length > 0 && bytes.length <= MAX_CONTEXT_BYTES,
                    "individual complete raw Context cap");
            require(contexts < MAX_CONTEXTS && bytes.length <= MAX_ALL_CONTEXT_BYTES - contextBytes,
                    "retained complete Context count/bytes cap");
            ++contexts; contextBytes += bytes.length;
            return bytes;
        }

        Object bindings() throws Exception {
            Object bound = ownField(modelType, "fields").get(model);
            require(bound instanceof Map<?, ?>, "sealed Model bindings");
            List<Object> fields = new ArrayList<>();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) bound).entrySet()) {
                require(entry.getKey() instanceof String && entry.getValue() instanceof Field,
                        "sealed Model field binding");
                fields.add(object("role", entry.getKey(), "field", describe((Field) entry.getValue())));
            }
            Class<?> storageType = (Class<?>) ownField(modelType, "storageType").get(model);
            Class<?> dimensionType = (Class<?>) ownField(modelType, "providerType").get(model);
            Object storage = ownField(modelType, "storage").get(model);
            Object dimension = ownField(modelType, "provider").get(model);
            boolean client = ownField(modelType, "client").getBoolean(model);
            Constructor<?> worldConstructor = client
                    ? worldType.getDeclaredConstructor(storageType, String.class, dimensionType, long.class)
                    : worldType.getDeclaredConstructor(storageType, String.class, long.class, dimensionType);
            Constructor<?> storageConstructor = client ? storage.getClass().getDeclaredConstructor()
                    : storage.getClass().getDeclaredConstructor(java.io.File.class, String.class, boolean.class);
            return object("source", "tests/reference/world/MutationObservation.java",
                    "source_sha256", ADAPTER_SHA256, "model_class", modelType.getName(),
                    "context_class", contextType.getName(), "actual_field_bindings", fields,
                    "used", Arrays.asList("Model.<init>(ClassLoader,boolean)", "Model.setup(Path)",
                            "Model.get(String,Object)", "Model.ref(Object)", "Model.identityTable()",
                            "Context.<init>(Model)", "Context.encoded", "Context.same(Context)",
                            "Context.sameCache(Context)", "Context.sameQueue(Context)",
                            "Context.sameObservers(Context)", "RandomState.same(RandomState)",
                            "RegistryState.same(RegistryState)"),
                    "helper_only_budget_flag", "MutationObservation.measuring",
                    "actual_world_constructor", describe(worldConstructor),
                    "actual_storage_constructor", describe(storageConstructor),
                    "actual_dimension_constructor", describe(dimension.getClass().getDeclaredConstructor()),
                    "no_prior_main_construct_or_observe", true);
        }
    }

    private static final class Model {
        final Adapter adapter;
        final boolean client;
        final Class<?> materialType, slabType;
        final Method acquire, blockGet, solid, sound, particle;
        final Field immediate, suppress, remote, info, ordered, membership, infoTime;
        final Field blockMaterial, blockId, slabDouble, materialF;
        final List<Object> operations = new ArrayList<>();
        final List<Subject> subjects = new ArrayList<>();

        Model(ClassLoader original, boolean client, Path root) throws Exception {
            this.client = client;
            adapter = new Adapter(original, client, root);
            Class<?> world = adapter.worldType;
            Class<?> infoType = Class.forName(client ? "ei" : "ct", false, original);
            materialType = Class.forName(client ? "ln" : "hj", false, original);
            slabType = Class.forName(client ? "ys" : "pw", false, original);
            acquire = sourceMethod(world, "c", adapter.chunkType, int.class, int.class);
            blockGet = sourceMethod(world, "a", int.class, int.class, int.class, int.class);
            solid = sourceMethod(world, client ? "h" : "e", boolean.class,
                    int.class, int.class, int.class);
            sound = sourceMethod(world, "a", void.class, double.class, double.class, double.class,
                    String.class, float.class, float.class);
            particle = sourceMethod(world, "a", void.class, String.class, double.class,
                    double.class, double.class, double.class, double.class, double.class);
            immediate = sourceField(world, "a", boolean.class, 1);
            suppress = sourceField(world, "o", boolean.class, 1);
            remote = sourceField(world, "B", boolean.class, 1);
            info = sourceField(world, "x", infoType, 4);
            infoTime = sourceField(infoType, "e", long.class, 2);
            ordered = sourceField(world, "E", TreeSet.class, 2);
            membership = sourceField(world, "F", Set.class, 2);
            blockMaterial = sourceField(adapter.blockType, "bA", materialType, 17);
            blockId = sourceField(adapter.blockType, "bn", int.class, 17);
            slabDouble = sourceField(slabType, "b", boolean.class, 2);
            materialF = sourceField(materialType, "F", boolean.class, 2);
            for (String[] entry : new String[][] {{"stone", "u"}, {"double-slab", "ak"},
                    {"single-slab", "al"}, {"glass", "N"}, {"leaves", "L"},
                    {"flowing-water", "B"}, {"flowing-lava", "D"}}) {
                Field field = adapter.blockType.getDeclaredField(entry[1]);
                require(adapter.blockType.isAssignableFrom(field.getType()) && field.getModifiers() == 25,
                        "actual registered Block static descriptor");
                field.setAccessible(true);
                Object block = field.get(null);
                require(block != null && adapter.blockType.isInstance(block), "actual registered Block");
                int id = blockId.getInt(block);
                Object[] map = (Object[]) adapter.get("block.m", null);
                require(id >= 0 && id < 256 && map[id] == block, "actual named Block/array identity");
                subjects.add(new Subject(entry[0], field, block, id));
            }
        }

        Object queue(Object owner) throws Exception {
            require(owner instanceof Set<?>, "actual scheduled Set");
            Set<?> set = (Set<?>) owner;
            require(set.size() <= MAX_COLLECTION, "full scheduled collection cap");
            List<Object> entries = new ArrayList<>();
            for (Object record : set) {
                require(record != null && record.getClass().getName().equals(client ? "qy" : "ki"),
                        "actual scheduled record class");
                Class<?> type = record.getClass();
                entries.add(object("ordinal", entries.size(), "owner", adapter.ref(record),
                        "class", type.getName(), "x", sourceField(type, "a", int.class, 1).getInt(record),
                        "y", sourceField(type, "b", int.class, 1).getInt(record),
                        "z", sourceField(type, "c", int.class, 1).getInt(record),
                        "block", sourceField(type, "d", int.class, 1).getInt(record),
                        "due_bits", bits(sourceField(type, "e", long.class, 1).getLong(record)),
                        "sequence_bits", bits(sourceField(type, "g", long.class, 2).getLong(record))));
            }
            return object("owner", adapter.ref(owner), "size", set.size(), "entries", entries);
        }

        Object bindings() {
            List<Object> methods = new ArrayList<>(), fields = new ArrayList<>();
            for (Method method : new Method[] {acquire, blockGet, solid, sound, particle})
                methods.add(describe(method));
            for (Field field : new Field[] {immediate, suppress, remote, info, infoTime, ordered,
                    membership, blockMaterial, blockId, slabDouble, materialF}) fields.add(describe(field));
            return object("methods", methods, "fields", fields,
                    "scheduled_static_counter", "NEVER_READ");
        }

        Operation observe(String label, String scope, Method method, Object receiver,
                          Object input, Subject subject, Object... arguments) throws Exception {
            require(operations.size() < MAX_OPERATIONS, "source operation count cap");
            int index = operations.size();
            System.err.println("CALL_BEGIN " + index + " " + label);
            Snapshot before = new Snapshot(this);
            Object subjectBefore = subject == null ? null : subject.state(this);
            if (method == sound || method == particle) {
                Object current = adapter.get("world.u", adapter.world);
                require(current instanceof List<?> && ((List<?>) current).isEmpty(),
                        "genuine empty-u presentation domain at each actual call");
            }
            Outcome outcome = Outcome.capture(adapter, method, receiver, arguments);
            byte[] outcomeJson = encode(outcome.description);
            System.err.println("SOURCE_OUTCOME " + index + " " +
                    new String(outcomeJson, 0, outcomeJson.length - 1, StandardCharsets.US_ASCII));
            Object subjectAfter = subject == null ? null : subject.state(this);
            Snapshot after = new Snapshot(this);
            operations.add(object("index", index, "label", label, "scope", scope,
                    "method", describe(method), "receiver", adapter.ref(receiver), "input", input,
                    "subject_before", subjectBefore, "before", before.description,
                    "outcome", outcome.description, "after", after.description,
                    "subject_after", subjectAfter, "receipts", before.receipts(this, after)));
            System.err.println("CALL_DONE " + index + " " + outcome.tag);
            return new Operation(index, outcome);
        }
    }

    private static final class Subject {
        final String label;
        final Field field;
        final Object block;
        final int id;
        Subject(String label, Field field, Object block, int id) {
            this.label = label; this.field = field; this.block = block; this.id = id;
        }
        Object state(Model model) throws Exception {
            Object material = model.blockMaterial.get(block);
            return object("label", label, "named_field", describe(field), "named_current", model.adapter.ref(field.get(null)),
                    "block", model.adapter.ref(block), "class", block.getClass().getName(),
                    "id", model.blockId.getInt(block), "material", model.adapter.ref(material),
                    "material_class", material == null ? null : material.getClass().getName(),
                    "material_F", material == null ? null : model.materialF.getBoolean(material),
                    "slab_b", model.slabType.isInstance(block) ? model.slabDouble.getBoolean(block) : null);
        }
    }

    private static final class Snapshot {
        final Object context, info, ordered, membership;
        final boolean immediate, suppress, remote;
        final long time;
        final Object description, queues;
        Snapshot(Model model) throws Exception {
            Adapter adapter = model.adapter;
            context = adapter.context();
            byte[] bytes = adapter.bytes(context);
            info = model.info.get(adapter.world);
            require(info != null, "actual WorldInfo reference");
            time = model.infoTime.getLong(info);
            immediate = model.immediate.getBoolean(adapter.world);
            suppress = model.suppress.getBoolean(adapter.world);
            remote = model.remote.getBoolean(adapter.world);
            ordered = model.ordered.get(adapter.world);
            membership = model.membership.get(adapter.world);
            queues = object("E", model.queue(ordered), "F", model.queue(membership));
            description = object("context", new RawJson(bytes), "context_bytes", bytes.length,
                    "context_sha256", sha256(bytes), "info", adapter.ref(info), "time_bits", bits(time),
                    "flags", object("a", immediate, "o", suppress, "B", remote), "scheduled", queues);
        }
        Object receipts(Model model, Snapshot after) throws Exception {
            Adapter a = model.adapter;
            return object("context_same", a.compare(a.same, context, after.context),
                    "cache_same", a.compare(a.sameCache, context, after.context),
                    "lighting_same", a.compare(a.sameQueue, context, after.context),
                    "observers_same", a.compare(a.sameObservers, context, after.context),
                    "random_same", a.compare(a.randomSame, a.contextRandom.get(context), a.contextRandom.get(after.context)),
                    "registry_same", a.compare(a.registrySame, a.contextRegistry.get(context), a.contextRegistry.get(after.context)),
                    "info_same", info == after.info, "time_same", time == after.time,
                    "flags_same", immediate == after.immediate && suppress == after.suppress && remote == after.remote,
                    "scheduled_same", Arrays.equals(encode(queues), encode(after.queues)),
                    "E_owner_same", ordered == after.ordered, "F_owner_same", membership == after.membership);
        }
    }

    private static final class Outcome {
        final String tag;
        final Object value, description;
        Outcome(String tag, Object value, Object description) {
            this.tag = tag; this.value = value; this.description = description;
        }
        static Outcome capture(Adapter adapter, Method method, Object receiver, Object[] arguments)
                throws Exception {
            Object result;
            try { result = method.invoke(receiver, arguments); }
            catch (InvocationTargetException wrapped) {
                Throwable actual = wrapped.getCause();
                require(actual != null, "actual source Throwable");
                System.err.println("SOURCE_THROWABLE " + actual.getClass().getName());
                actual.printStackTrace(System.err);
                return new Outcome("THROW", null, object("kind", "THROW", "class", actual.getClass().getName()));
            }
            Class<?> type = method.getReturnType();
            if (type == void.class)
                return new Outcome("VOID", null, object("kind", "RETURN", "type", "V"));
            if (type == boolean.class) {
                require(result instanceof Boolean, "actual Boolean result");
                return new Outcome("BOOLEAN", result, object("kind", "RETURN", "type", "Z", "value", result));
            }
            if (type == int.class) {
                require(result instanceof Integer, "actual int result");
                return new Outcome("INT", result, object("kind", "RETURN", "type", "I", "value", result));
            }
            require(!type.isPrimitive(), "supported observation result type");
            adapter.ref(result);
            return new Outcome("REFERENCE", result, object("kind", "RETURN", "type", descriptor(type),
                    "owner", adapter.ref(result), "class", result == null ? null : result.getClass().getName()));
        }
    }

    private static final class Operation {
        final int index;
        final Outcome outcome;
        Operation(int index, Outcome outcome) { this.index = index; this.outcome = outcome; }
    }

    private static Object text(Adapter adapter, String value) throws Exception {
        List<Object> units = new ArrayList<>();
        if (value != null) for (int i = 0; i < value.length(); ++i) units.add((int) value.charAt(i));
        return object("owner", adapter.ref(value), "utf16", value == null ? null : units);
    }

    private static Object footprint(Model model, Object chunk) throws Exception {
        Adapter a = model.adapter;
        if (chunk == null || !a.chunkType.isInstance(chunk))
            return object("usable", false, "reason", "NO_ACTUAL_CHUNK", "owner", a.ref(chunk));
        Object raw = a.get("chunk.b", chunk), boundWorld = a.get("chunk.d", chunk);
        int x = (int) a.get("chunk.j", chunk), z = (int) a.get("chunk.k", chunk);
        Object mapOwner = a.get("cache.e", a.cache);
        require(mapOwner instanceof Map<?, ?>, "actual cache map");
        Method presence = (Method) ownField(a.modelType, "presenceMethod").get(a.model);
        Method key = (Method) ownField(a.modelType, "keyMethod").get(a.model);
        boolean present = (boolean) invoke(presence, a.cache, 0, 0);
        int actualKey = (int) invoke(key, null, 0, 0);
        Map<?, ?> map = (Map<?, ?>) mapOwner;
        Object selected = map.get(actualKey);
        boolean usable = chunk.getClass() == a.chunkType && boundWorld == a.world && x == 0 && z == 0 &&
                raw instanceof byte[] && ((byte[]) raw).length == 32768 && present && selected == chunk;
        return object("usable", usable, "reason", usable ? "ACTUAL_RETURNED_PRESENT_CHUNK" : "NO_ACTUAL_FOOTPRINT",
                "owner", a.ref(chunk), "class", chunk.getClass().getName(), "world", a.ref(boundWorld),
                "x", x, "z", z, "blocks", a.ref(raw),
                "block_length", raw instanceof byte[] ? ((byte[]) raw).length : null,
                "present", present, "key", actualKey, "cache_value", a.ref(selected));
    }

    private static Object worldCase(Model model, Object chunk, String label, int id,
                                    Subject subject) throws Exception {
        Adapter a = model.adapter;
        byte[] blocks = (byte[]) a.get("chunk.b", chunk);
        int index = (8 << 11) | (8 << 7) | 64;
        byte original = blocks[index], prepared = (byte) id;
        Snapshot controlBefore = new Snapshot(model);
        Operation getter = null, solid = null;
        String restoration = "NOT_STARTED";
        try {
            blocks[index] = prepared; /* Explicit sole source-input substrate control. */
            getter = model.observe(label + "-actual-id", "World prepared substrate validation", model.blockGet,
                    a.world, object("x", 8, "y", 64, "z", 8), subject, 8, 64, 8);
            solid = model.observe(label + "-world-solid", "actual World solid-support predicate", model.solid,
                    a.world, object("x", 8, "y", 64, "z", 8), subject, 8, 64, 8);
        } finally {
            if (a.get("chunk.b", chunk) != blocks) restoration = "PRESERVED_SOURCE_REBOUND_ARRAY";
            else if (blocks[index] != prepared) restoration = "PRESERVED_SOURCE_CHANGED_BYTE";
            else { blocks[index] = original; restoration = "RESTORED_ONLY_HELPER_BYTE"; }
        }
        Snapshot restored = new Snapshot(model);
        return object("label", label, "chunk", a.ref(chunk), "blocks", a.ref(blocks), "byte_index", index,
                "original_byte", original & 255, "prepared_byte", prepared & 255, "requested_id", id,
                "control_before", controlBefore.description, "getter_operation", getter.index,
                "solid_operation", solid.index, "query_matches_input",
                getter.outcome.value instanceof Integer && ((Integer) getter.outcome.value).intValue() == id,
                "restoration", restoration, "after_restoration", restored.description,
                "restoration_receipts", controlBefore.receipts(model, restored));
    }

    private static byte[] observe(Model model) throws Exception {
        Adapter a = model.adapter;
        Snapshot setup = new Snapshot(model);
        require(a.compare(a.same, a.baseline, setup.context), "setup Context reader stability");
        List<Object> subjects = new ArrayList<>(), worldCases = new ArrayList<>(), presentation = new ArrayList<>();
        for (Subject subject : model.subjects) subjects.add(subject.state(model));
        measuring = true;
        a.helperMeasuring.setBoolean(null, true); /* Only our helper's cap categorization. */
        Operation acquisition = model.observe("actual-world-acquisition", "genuine original warmup, effects retained",
                model.acquire, a.world, object("chunk_x", 0, "chunk_z", 0), null, 0, 0);
        Object chunk = acquisition.outcome.value;
        Object actualFootprint = footprint(model, chunk);
        require(actualFootprint instanceof Map<?, ?>, "footprint description");
        boolean usable = Boolean.TRUE.equals(((Map<?, ?>) actualFootprint).get("usable"));

        for (Subject subject : model.subjects) {
            Object material = model.blockMaterial.get(subject.block);
            model.observe(subject.label + "-block-support", "direct Block predicate, narrower than World",
                    virtualMethod(subject.block, model.client ? "d" : "b"), subject.block,
                    object("block", a.ref(subject.block)), subject);
            model.observe(subject.label + "-block-opaque", "direct Block predicate, narrower than World",
                    virtualMethod(subject.block, model.client ? "c" : "a"), subject.block,
                    object("block", a.ref(subject.block)), subject);
            if (material != null) {
                model.observe(subject.label + "-material-c", "direct Material predicate, narrower than World",
                        virtualMethod(material, "c"), material, object("material", a.ref(material)), subject);
                model.observe(subject.label + "-material-h", "direct Material predicate, narrower than World",
                        virtualMethod(material, "h"), material, object("material", a.ref(material)), subject);
            }
        }
        if (usable) {
            worldCases.add(worldCase(model, chunk, "air", 0, null));
            for (Subject subject : model.subjects)
                worldCases.add(worldCase(model, chunk, subject.label, subject.id, subject));
            worldCases.add(worldCase(model, chunk, "null-registry255", 255, null));
        }
        for (int[] point : new int[][] {{8, -1, 8}, {8, 128, 8}, {-32000001, 64, 8},
                {32000000, 64, 8}, {8, 64, 32000001}})
            model.observe("world-border-" + point[0] + "," + point[1] + "," + point[2],
                    "actual World out-of-domain predicate", model.solid, a.world,
                    object("x", point[0], "y", point[1], "z", point[2]), null, point[0], point[1], point[2]);

        Object observers = a.get("world.u", a.world);
        require(observers instanceof List<?>, "actual observer list");
        if (((List<?>) observers).isEmpty()) {
            for (String name : new String[] {"random.fizz", null}) {
                Operation operation = model.observe(name == null ? "sound-null" : "sound-named",
                        "actual empty-u World sound", model.sound, a.world,
                        object("name", text(a, name), "x_bits", doubleBits(8.5), "y_bits", doubleBits(64.5),
                                "z_bits", doubleBits(-0.0), "volume_bits", floatBits(0.5f),
                                "pitch_bits", floatBits(2.6f)), null, 8.5, 64.5, -0.0, name, 0.5f, 2.6f);
                presentation.add(operation.index);
            }
            for (String name : new String[] {"largesmoke", null}) {
                Operation operation = model.observe(name == null ? "particle-null" : "particle-named",
                        "actual empty-u World particle", model.particle, a.world,
                        object("name", text(a, name), "x_bits", doubleBits(8.5), "y_bits", doubleBits(65.2),
                                "z_bits", doubleBits(-0.0), "vx_bits", doubleBits(0.0),
                                "vy_bits", doubleBits(-0.0), "vz_bits", doubleBits(0.0)),
                        null, name, 8.5, 65.2, -0.0, 0.0, -0.0, 0.0);
                presentation.add(operation.index);
            }
        }
        Snapshot end = new Snapshot(model);
        List<?> identities = (List<?>) a.identities();
        return encode(object("schema", "SOLID_SUPPORT_OBSERVATION_V1", "side", model.client ? "client" : "server",
                "scope", "finite actual base World and narrower Block/Material APIs; no native comparison",
                "adapter", a.bindings(), "bindings", model.bindings(), "setup", setup.description,
                "subjects", subjects, "acquisition_operation", acquisition.index, "footprint", actualFootprint,
                "world_substrate_status", usable ? "ACTUAL_WORLD_CASES_RECORDED" : "SKIPPED_NO_ACTUAL_FOOTPRINT",
                "world_cases", worldCases, "presentation_status", presentation.isEmpty() ? "SKIPPED_NONEMPTY_U" : "ACTUAL_EMPTY_U_CALLS_RECORDED",
                "presentation_operations", presentation, "operations", model.operations, "calls", model.operations.size(),
                "final", end.description, "context_count", a.contexts, "context_bytes", a.contextBytes,
                "identity_count", identities.size(), "identities", identities, "complete", true));
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out;
        System.setOut(System.err);
        int status = 1;
        Adapter adapter = null;
        try {
            require(args.length == 3, "arguments: jar client|server new_owned_world_root");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            Path jar = Path.of(args[0]).toRealPath();
            require(Files.isRegularFile(jar), "original JAR regular file");
            Path root = Path.of(args[2]).toAbsolutePath().normalize();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                    ClassLoader.getPlatformClassLoader())) {
                System.err.println("SETUP_BEGIN");
                Model model = new Model(loader, args[1].equals("client"), root);
                adapter = model.adapter;
                System.err.println("SETUP_DONE");
                byte[] transcript = observe(model);
                protocol.write(transcript, 0, transcript.length);
                protocol.flush();
                require(!protocol.checkError(), "JSON protocol output failure");
                System.err.println("MEASUREMENT_DONE");
                status = 0;
            }
        } catch (Throwable failure) {
            status = measuring ? 1 : 77;
            System.err.println(status == 77 ? "UNAVAILABLE: genuine original support setup"
                    : "FAILURE: original support measurement");
            failure.printStackTrace(System.err);
        } finally {
            if (adapter != null) {
                try { adapter.helperMeasuring.setBoolean(null, adapter.oldHelperMeasuring); }
                catch (Throwable failure) { failure.printStackTrace(System.err); status = 1; }
            }
            System.setOut(protocol);
        }
        System.exit(status);
    }
}
