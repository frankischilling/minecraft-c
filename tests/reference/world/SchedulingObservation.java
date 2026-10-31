import java.io.ByteArrayOutputStream;
import java.io.File;
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

/** Original-only direct World scheduling experiment. Compile alongside the
 * sealed project-owned MutationObservation.java. Its private Model/Context are
 * accessed through the explicit adapter below; its main/construct/observe
 * methods are never invoked. No original class bytes or queues are replaced. */
public final class SchedulingObservation {
    private static final long WORLD_SEED = 23L;
    private static final int MAX_SCHEDULED = 4096;
    private static final int MAX_CONTEXT_BYTES = 32 * 1024 * 1024;
    private static final long MAX_TOTAL_CONTEXT_BYTES = 96L * 1024 * 1024;
    private static final int MAX_OUTPUT_BYTES = 128 * 1024 * 1024;
    private static final int MAX_SELECTION_ATTEMPTS = 512;
    private static final int MAX_CALLS = 19;
    private static final String ADAPTER_SOURCE_SHA256 =
            "812fdda343e5282d649ba6b9f125f9fc4f3bb918528e71eddeb267e4b034b4e9";
    private static boolean measuring;

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static void cap(boolean value, String message) throws Exception {
        if (!value) throw new Exception((measuring ? "measurement" : "setup") + " cap: " + message);
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException wrapped) {
            Throwable actual = wrapped.getCause();
            if (actual instanceof Exception) throw (Exception) actual;
            if (actual instanceof Error) throw (Error) actual;
            throw wrapped;
        }
    }

    private static Object construct(Constructor<?> constructor, Object... arguments) throws Exception {
        try { return constructor.newInstance(arguments); }
        catch (InvocationTargetException wrapped) {
            Throwable actual = wrapped.getCause();
            if (actual instanceof Exception) throw (Exception) actual;
            if (actual instanceof Error) throw (Error) actual;
            throw wrapped;
        }
    }

    private static Map<String, Object> object(Object... pairs) {
        require((pairs.length & 1) == 0, "JSON key/value pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2)
            require(result.put((String) pairs[i], pairs[i + 1]) == null, "duplicate JSON key");
        return result;
    }

    private static String bits(long value) { return String.format(Locale.ROOT, "%016x", value); }

    private static String sha256(byte[] value) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(value);
        StringBuilder result = new StringBuilder(64);
        for (byte unit : digest) result.append(String.format(Locale.ROOT, "%02x", unit & 255));
        return result.toString();
    }

    /* RawJson is ONLY a complete byte[] emitted by the sealed Context's own
     * jsonBytes serializer. Copy it verbatim as a nested JSON value: no parse,
     * normalization, field removal, context compression or string re-encoding. */
    private static final class RawJson {
        final byte[] bytes;
        RawJson(byte[] bytes) { this.bytes = bytes; }
    }

    private static final class LimitedBytes extends ByteArrayOutputStream {
        private void room(int extra) {
            require(extra >= 0 && extra <= MAX_OUTPUT_BYTES - count, "JSON transcript cap");
        }
        @Override public synchronized void write(int value) { room(1); super.write(value); }
        @Override public synchronized void write(byte[] value, int offset, int length) {
            room(length);
            super.write(value, offset, length);
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
                json(out, item);
                comma = true;
            }
            out.write(']');
        } else if (value instanceof Map<?, ?>) {
            out.write('{');
            boolean comma = false;
            for (Map.Entry<?, ?> item : ((Map<?, ?>) value).entrySet()) {
                require(item.getKey() instanceof String, "JSON string key");
                if (comma) out.write(',');
                json(out, item.getKey()); out.write(':'); json(out, item.getValue());
                comma = true;
            }
            out.write('}');
        } else throw new IllegalStateException("unsupported JSON value");
    }

    private static byte[] encode(Object value) {
        LimitedBytes out = new LimitedBytes();
        json(out, value);
        out.write('\n');
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
        require(!type.isPrimitive(), "used primitive descriptor");
        return "L" + type.getName().replace('.', '/') + ";";
    }

    private static Object describe(Field field) {
        return object("owner", field.getDeclaringClass().getName(), "name", field.getName(),
                "descriptor", descriptor(field.getType()), "modifiers", field.getModifiers());
    }

    private static Object describe(Method method) {
        StringBuilder signature = new StringBuilder("(");
        for (Class<?> type : method.getParameterTypes()) signature.append(descriptor(type));
        signature.append(')').append(descriptor(method.getReturnType()));
        return object("owner", method.getDeclaringClass().getName(), "name", method.getName(),
                "descriptor", signature.toString(), "modifiers", method.getModifiers());
    }

    private static Object describe(Constructor<?> constructor) {
        StringBuilder signature = new StringBuilder("(");
        for (Class<?> type : constructor.getParameterTypes()) signature.append(descriptor(type));
        signature.append(")V");
        return object("owner", constructor.getDeclaringClass().getName(), "name", "<init>",
                "descriptor", signature.toString(), "modifiers", constructor.getModifiers());
    }

    /** Reflection here targets OUR sealed helper, not an alternate World model.
     * Original object identity admission is delegated to the SAME Model.ref
     * table as every nested Context. All retained references remain strong. */
    private static final class Adapter {
        final Class<?> modelType, contextType;
        final boolean client;
        final Object model;
        final Constructor<?> contextConstructor;
        final Method get, ref, table, same, sameCache, sameQueue, sameObservers;
        final Method randomSame, registrySame;
        final Field contextEncoded, contextRandom, contextRegistry, helperMeasuring;
        final Object world, cache, stone;
        final Object baseline;
        final Class<?> worldType, chunkType;
        final int stoneId;
        long contextBytes;

        private static Method method(Class<?> owner, String name, Class<?>... arguments)
                throws Exception {
            Method method = owner.getDeclaredMethod(name, arguments);
            method.setAccessible(true);
            return method;
        }

        private static Field field(Class<?> owner, String name) throws Exception {
            Field field = owner.getDeclaredField(name);
            field.setAccessible(true);
            return field;
        }

        Adapter(ClassLoader original, boolean client, Path root) throws Exception {
            this.client = client;
            ClassLoader helpers = SchedulingObservation.class.getClassLoader();
            Class<?> outer = Class.forName("MutationObservation", true, helpers);
            modelType = Class.forName("MutationObservation$Model", true, helpers);
            contextType = Class.forName("MutationObservation$Context", true, helpers);
            Constructor<?> constructor = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
            constructor.setAccessible(true);
            model = construct(constructor, original, client);
            invoke(method(modelType, "setup", Path.class), model, root);
            get = method(modelType, "get", String.class, Object.class);
            ref = method(modelType, "ref", Object.class);
            table = method(modelType, "identityTable");
            contextConstructor = contextType.getDeclaredConstructor(modelType);
            contextConstructor.setAccessible(true);
            same = method(contextType, "same", contextType);
            sameCache = method(contextType, "sameCache", contextType);
            sameQueue = method(contextType, "sameQueue", contextType);
            sameObservers = method(contextType, "sameObservers", contextType);
            contextEncoded = field(contextType, "encoded");
            contextRandom = field(contextType, "random");
            contextRegistry = field(contextType, "registry");
            randomSame = method(contextRandom.getType(), "same", contextRandom.getType());
            registrySame = method(contextRegistry.getType(), "same", contextRegistry.getType());
            helperMeasuring = field(outer, "measuring");
            world = field(modelType, "world").get(model);
            cache = field(modelType, "cache").get(model);
            stone = field(modelType, "stone").get(model);
            baseline = field(modelType, "baseline").get(model);
            worldType = (Class<?>) field(modelType, "worldType").get(model);
            chunkType = (Class<?>) field(modelType, "chunkType").get(model);
            stoneId = field(modelType, "stoneId").getInt(model);
            require(world != null && world.getClass() == worldType, "actual base World identity");
        }

        Object get(String role, Object receiver) throws Exception { return invoke(get, model, role, receiver); }
        int ref(Object value) throws Exception { return (int) invoke(ref, model, value); }
        Object table() throws Exception { return invoke(table, model); }
        Object context() throws Exception { return construct(contextConstructor, model); }

        byte[] bytes(Object context) throws Exception {
            byte[] bytes = (byte[]) contextEncoded.get(context);
            cap(bytes != null && bytes.length > 0 && bytes.length <= MAX_CONTEXT_BYTES,
                    "individual full Context");
            cap(bytes.length <= MAX_TOTAL_CONTEXT_BYTES - contextBytes, "all retained Context bytes");
            contextBytes += bytes.length;
            return bytes;
        }

        boolean compare(Method method, Object before, Object after) throws Exception {
            return (boolean) invoke(method, before, after);
        }

        Object receipts(Snapshot before, Snapshot after) throws Exception {
            return object("context_same", compare(same, before.context, after.context),
                    "cache_same", compare(sameCache, before.context, after.context),
                    "lighting_same", compare(sameQueue, before.context, after.context),
                    "observers_same", compare(sameObservers, before.context, after.context),
                    "random_same", compare(randomSame, contextRandom.get(before.context),
                            contextRandom.get(after.context)),
                    "registry_same", compare(registrySame, contextRegistry.get(before.context),
                            contextRegistry.get(after.context)),
                    "flags_same", before.mode.same(after.mode), "time_same", before.time == after.time,
                    "info_owner_same", before.info == after.info,
                    "E_owner_same", before.ordered == after.ordered,
                    "F_owner_same", before.membership == after.membership);
        }

        Object describe() throws Exception {
            Object bound = field(modelType, "fields").get(model);
            require(bound instanceof Map<?, ?>, "sealed Model field binding table");
            List<Object> fields = new ArrayList<>(), methods = new ArrayList<>();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) bound).entrySet()) {
                require(entry.getKey() instanceof String && entry.getValue() instanceof Field,
                        "sealed Model actual field binding");
                fields.add(object("role", entry.getKey(), "field", SchedulingObservation.describe((Field) entry.getValue())));
            }
            for (String role : new String[] {"seedMethod", "emptyMethod", "presenceMethod", "keyMethod",
                    "setter4", "setter5", "stoneRemove", "stoneAdd"}) {
                Object actual = field(modelType, role).get(model);
                require(actual instanceof Method, "sealed Model actual method binding");
                methods.add(object("role", role, "method", SchedulingObservation.describe((Method) actual)));
            }
            Class<?> storageType = (Class<?>) field(modelType, "storageType").get(model);
            Class<?> providerType = (Class<?>) field(modelType, "providerType").get(model);
            Constructor<?> worldConstructor = client
                    ? worldType.getDeclaredConstructor(storageType, String.class, providerType, long.class)
                    : worldType.getDeclaredConstructor(storageType, String.class, long.class, providerType);
            Object actualStorage = field(modelType, "storage").get(model);
            Object actualProvider = field(modelType, "provider").get(model);
            Constructor<?> storageConstructor = client
                    ? actualStorage.getClass().getDeclaredConstructor()
                    : actualStorage.getClass().getDeclaredConstructor(File.class, String.class, boolean.class);
            return object("source", "tests/reference/world/MutationObservation.java",
                    "source_sha256", ADAPTER_SOURCE_SHA256,
                    "model_class", modelType.getName(), "context_class", contextType.getName(),
                    "used", Arrays.asList("Model.<init>(ClassLoader,boolean)", "Model.setup(Path)",
                            "Model.get(String,Object)", "Model.ref(Object)", "Model.identityTable()",
                            "Context.<init>(Model)", "Context.encoded", "Context.same(Context)",
                            "Context.sameCache(Context)", "Context.sameQueue(Context)",
                            "Context.sameObservers(Context)", "RandomState.same(RandomState)",
                            "RegistryState.same(RegistryState)"),
                    "helper_only_budget_flag", "MutationObservation.measuring",
                    "actual_field_bindings", fields, "actual_method_bindings", methods,
                    "actual_world_constructor", SchedulingObservation.describe(worldConstructor),
                    "actual_storage_constructor", SchedulingObservation.describe(storageConstructor),
                    "actual_dimension_constructor", SchedulingObservation.describe(actualProvider.getClass().getDeclaredConstructor()),
                    "no_mutation_observe_or_construct_call", true);
        }
    }

    private static final class Mode {
        final boolean immediate, suppress, remote;
        Mode(Model model) throws Exception {
            immediate = model.immediate.getBoolean(model.adapter.world);
            suppress = model.suppress.getBoolean(model.adapter.world);
            remote = model.remote.getBoolean(model.adapter.world);
        }
        boolean same(Mode other) {
            return immediate == other.immediate && suppress == other.suppress && remote == other.remote;
        }
        Object describe() { return object("a", immediate, "o", suppress, "B", remote); }
    }

    private static final class Model {
        final Adapter adapter;
        final boolean client;
        final Class<?> recordType, infoType;
        final Field immediate, suppress, remote, ordered, membership, info, sequence;
        final Field[] recordFields;
        final Method schedule, time, region, hash, stoneTick;
        boolean sequenceObserved;

        private static Field field(Class<?> owner, String name, Class<?> type, int modifiers)
                throws Exception {
            Field field = owner.getDeclaredField(name);
            require(field.getType() == type && field.getModifiers() == modifiers,
                    "actual source field descriptor/modifiers: " + owner.getName() + "." + name);
            field.setAccessible(true);
            return field;
        }

        private static Method method(Class<?> owner, String name, Class<?> result, Class<?>... args)
                throws Exception {
            Method method = owner.getDeclaredMethod(name, args);
            require(method.getReturnType() == result && method.getModifiers() == 1,
                    "actual source public method: " + owner.getName() + "." + name);
            method.setAccessible(true);
            return method;
        }

        Model(ClassLoader original, boolean client, Path root) throws Exception {
            this.client = client;
            adapter = new Adapter(original, client, root);
            Class<?> world = adapter.worldType;
            /* Loading/reflecting declarations does not initialize this class.
             * Never call sequence.getLong until an actual schedule returns. */
            recordType = Class.forName(client ? "qy" : "ki", false, original);
            infoType = Class.forName(client ? "ei" : "ct", false, original);
            immediate = field(world, "a", boolean.class, 1);
            suppress = field(world, "o", boolean.class, 1);
            remote = field(world, "B", boolean.class, 1);
            ordered = field(world, "E", TreeSet.class, 2);
            membership = field(world, "F", Set.class, 2);
            info = field(world, "x", infoType, 4);
            sequence = field(recordType, "f", long.class, 10);
            recordFields = new Field[] {field(recordType, "a", int.class, 1),
                    field(recordType, "b", int.class, 1), field(recordType, "c", int.class, 1),
                    field(recordType, "d", int.class, 1), field(recordType, "e", long.class, 1),
                    field(recordType, "g", long.class, 2)};
            schedule = method(world, "c", void.class,
                    int.class, int.class, int.class, int.class, int.class);
            time = method(infoType, "f", long.class);
            region = method(world, "a", boolean.class,
                    int.class, int.class, int.class, int.class, int.class, int.class);
            hash = method(recordType, "hashCode", int.class);
            stoneTick = adapter.stone.getClass().getMethod("a", world,
                    int.class, int.class, int.class, java.util.Random.class);
            require(stoneTick.getReturnType() == void.class && stoneTick.getModifiers() == 1,
                    "actual registered stone tick descriptor");
        }

        Object records(Object[] entries) throws Exception {
            List<Object> result = new ArrayList<>();
            for (int i = 0; i < entries.length; ++i) {
                Object entry = entries[i];
                require(entry != null && entry.getClass() == recordType, "actual exact scheduled record");
                result.add(object("ordinal", i, "owner", adapter.ref(entry),
                        "class", entry.getClass().getName(),
                        "x", recordFields[0].getInt(entry), "y", recordFields[1].getInt(entry),
                        "z", recordFields[2].getInt(entry), "block", recordFields[3].getInt(entry),
                        "due_bits", bits(recordFields[4].getLong(entry)),
                        "sequence_bits", bits(recordFields[5].getLong(entry)),
                        "actual_hash", invoke(hash, entry)));
            }
            return result;
        }

        Object bindings() {
            List<Object> fields = new ArrayList<>();
            for (Field field : new Field[] {immediate, suppress, remote, ordered, membership, info, sequence})
                fields.add(describe(field));
            for (Field field : recordFields) fields.add(describe(field));
            return object("world", adapter.worldType.getName(), "record", recordType.getName(),
                    "metadata", infoType.getName(), "schedule", describe(schedule),
                    "time", describe(time), "region", describe(region), "record_hash", describe(hash),
                    "registered_stone_tick", describe(stoneTick),
                    "fields", fields);
        }
    }

    private static final class Snapshot {
        final Object context, info, ordered, membership;
        final Mode mode;
        final long time;
        final byte[] contextBytes;
        final Object description;

        Snapshot(Model model) throws Exception {
            Adapter adapter = model.adapter;
            mode = new Mode(model);
            info = model.info.get(adapter.world);
            require(info != null && info.getClass() == model.infoType, "actual World metadata owner");
            time = (long) invoke(model.time, info);
            ordered = model.ordered.get(adapter.world);
            membership = model.membership.get(adapter.world);
            require(ordered instanceof TreeSet<?> && membership instanceof Set<?>,
                    "actual scheduled collection types");
            int eSize = ((Set<?>) ordered).size(), fSize = ((Set<?>) membership).size();
            cap(eSize <= MAX_SCHEDULED && fSize <= MAX_SCHEDULED, "complete E/F observation");
            Object[] e = ((Set<?>) ordered).toArray(), f = ((Set<?>) membership).toArray();
            require(e.length == eSize && f.length == fSize, "stable full E/F snapshots");
            Object eRows = model.records(e), fRows = model.records(f);
            Object counter = model.sequenceObserved
                    ? object("state", "OBSERVED", "bits", bits(model.sequence.getLong(null)))
                    : object("state", "UNOBSERVED", "bits", null);
            context = adapter.context();
            contextBytes = adapter.bytes(context);
            description = object("world", adapter.ref(adapter.world), "flags", mode.describe(),
                    "metadata", adapter.ref(info), "time_bits", bits(time), "counter", counter,
                    "E", object("owner", adapter.ref(ordered), "size", eSize, "entries", eRows),
                    "F", object("owner", adapter.ref(membership), "size", fSize, "entries", fRows),
                    "context_bytes", contextBytes.length, "context_sha256", sha256(contextBytes),
                    "context", new RawJson(contextBytes));
        }
    }

    private static final class Call {
        final String name;
        final int x, y, z, block, delay;
        final boolean immediate;
        final Boolean suppress, remote;
        Call(String name, int x, int y, int z, int block, int delay, boolean immediate) {
            this(name, x, y, z, block, delay, immediate, null, null);
        }
        Call(String name, int x, int y, int z, int block, int delay, boolean immediate,
             Boolean suppress, Boolean remote) {
            this.name = name; this.x = x; this.y = y; this.z = z;
            this.block = block; this.delay = delay; this.immediate = immediate;
            this.suppress = suppress; this.remote = remote;
        }
        Object input() { return object("x", x, "y", y, "z", z, "block", block, "delay", delay); }
    }

    private static Object observeCall(Model model, Call call, int ordinal) throws Exception {
        Mode saved = new Mode(model);
        boolean wroteA = false, wroteO = false, wroteB = false;
        Map<String, Object> observed;
        try {
            model.immediate.setBoolean(model.adapter.world, call.immediate); wroteA = true;
            if (call.suppress != null) {
                model.suppress.setBoolean(model.adapter.world, call.suppress); wroteO = true;
            }
            if (call.remote != null) {
                model.remote.setBoolean(model.adapter.world, call.remote); wroteB = true;
            }
            Snapshot before = new Snapshot(model);
            Object outcome;
            System.err.println("SCHEDULE_BEGIN " + ordinal + " " + call.name);
            try {
                Object returned = model.schedule.invoke(model.adapter.world,
                        call.x, call.y, call.z, call.block, call.delay);
                require(returned == null, "actual void scheduling return");
                /* A normal return proves the real source new/constructor ran.
                 * Until then, avoid the reflective static read entirely. */
                model.sequenceObserved = true;
                outcome = object("kind", "RETURN", "ordinary", "VOID");
            } catch (InvocationTargetException wrapped) {
                Throwable actual = wrapped.getCause();
                require(actual != null, "actual source Throwable");
                String message = actual.getMessage();
                cap(message == null || message.length() <= 65536, "Throwable message");
                outcome = object("kind", "THROW", "class", actual.getClass().getName(),
                        "owner", model.adapter.ref(actual), "message", message);
                System.err.println("SOURCE_THROWABLE " + actual.getClass().getName());
                actual.printStackTrace(System.err);
            }
            Snapshot after = new Snapshot(model);
            observed = object("ordinal", ordinal, "name", call.name, "input", call.input(),
                    "preparation", object("saved", saved.describe(),
                            "requested", object("a", call.immediate, "o", call.suppress, "B", call.remote)),
                    "before", before.description, "outcome", outcome, "after", after.description,
                    "receipts", model.adapter.receipts(before, after));
            System.err.println("SCHEDULE_DONE " + ordinal);
        } finally {
            /* Restore only helper-written fields, AFTER recording the complete
             * source return/exception prefix. Never touch queues/time/entropy. */
            try {
                if (wroteB) model.remote.setBoolean(model.adapter.world, saved.remote);
            } finally {
                try {
                    if (wroteO) model.suppress.setBoolean(model.adapter.world, saved.suppress);
                } finally {
                    if (wroteA) model.immediate.setBoolean(model.adapter.world, saved.immediate);
                }
            }
        }
        Mode restored = new Mode(model);
        boolean helperRestored = restored.immediate == saved.immediate &&
                (call.suppress == null || restored.suppress == saved.suppress) &&
                (call.remote == null || restored.remote == saved.remote);
        require(helperRestored, "helper-written flags restored");
        observed.put("restoration", object("flags", restored.describe(), "helper_fields_restored", helperRestored));
        return observed;
    }

    private static List<Call> calls() {
        List<Call> calls = new ArrayList<>();
        calls.add(new Call("absent-y-first", 0, -9, 0, 1, 7, false));
        calls.add(new Call("absent-y-upper", 0, 136, 0, 1, 7, false));
        calls.add(new Call("far-absent", 160000, 64, -160000, 1, 5, false));
        calls.add(new Call("wrapped-x-positive", Integer.MAX_VALUE, 64, 0, 1, 5, false));
        calls.add(new Call("wrapped-x-duplicate-delay", Integer.MAX_VALUE, 64, 0, 1, Integer.MAX_VALUE, false));
        calls.add(new Call("wrapped-x-full-id", Integer.MAX_VALUE, 64, 0, 257, -7, false));
        calls.add(new Call("wrapped-x-zero-id", Integer.MAX_VALUE, 64, 0, 0, 99, false));
        calls.add(new Call("wrapped-x-negative-id", Integer.MAX_VALUE, 64, 0, -1, Integer.MIN_VALUE, false));
        calls.add(new Call("wrapped-z-positive", 0, 64, Integer.MAX_VALUE, 1, -3, false));
        calls.add(new Call("wrapped-z-full-coordinate-key", 128, 64, Integer.MAX_VALUE, 1, -3, false));
        calls.add(new Call("wrapped-z-full-id256", 0, 64, Integer.MAX_VALUE, 256, Integer.MIN_VALUE, false));
        calls.add(new Call("deferred-o-B-control", Integer.MAX_VALUE, 65, 0, 2, -11, false, true, true));
        calls.add(new Call("immediate-absent-y", 0, -9, 0, 1, Integer.MAX_VALUE, true));
        calls.add(new Call("immediate-far-absent", 160000, 64, -160000, 1, Integer.MIN_VALUE, true));
        calls.add(new Call("immediate-outdomain-mismatch", Integer.MAX_VALUE, 64, 0, 1, -2, true));
        calls.add(new Call("immediate-outdomain-air", Integer.MAX_VALUE, 64, 0, 0, Integer.MAX_VALUE, true));
        calls.add(new Call("immediate-outdomain-negative", 0, 64, Integer.MAX_VALUE, -1, 7, true));
        calls.add(new Call("immediate-outdomain-full-id", 0, 64, Integer.MAX_VALUE, 257, 7, true));
        return calls;
    }

    private static final class Selection {
        final List<Object> attempts = new ArrayList<>();
        Call selected;

        Selection(Model model) throws Exception {
            Object actual = model.adapter.get("cache.e", model.adapter.cache);
            require(actual instanceof Map<?, ?>, "actual setup cache map");
            for (Object chunk : ((Map<?, ?>) actual).values()) {
                cap(attempts.size() < MAX_SELECTION_ATTEMPTS, "optional selection attempts");
                require(chunk != null && model.adapter.chunkType.isInstance(chunk), "actual cached Chunk");
                byte[] blocks = (byte[]) model.adapter.get("chunk.b", chunk);
                if (blocks == null || blocks.length != 32768) continue;
                int localY = -1;
                for (int y = 0; y < 128; ++y) {
                    if ((blocks[(8 * 16 + 8) * 128 + y] & 255) == model.adapter.stoneId) {
                        localY = y; break;
                    }
                }
                if (localY < 0) continue;
                int chunkX = (int) model.adapter.get("chunk.j", chunk);
                int chunkZ = (int) model.adapter.get("chunk.k", chunk);
                long wideX = (long) chunkX * 16L + 8L, wideZ = (long) chunkZ * 16L + 8L;
                if (wideX < -32000000L || wideX >= 32000000L ||
                        wideZ < -32000000L || wideZ > 32000000L) continue;
                int x = (int) wideX, z = (int) wideZ;
                boolean present = (boolean) invoke(model.region, model.adapter.world,
                        x - 8, localY - 8, z - 8, x + 8, localY + 8, z + 8);
                attempts.add(object("chunk", model.adapter.ref(chunk), "x", x, "y", localY, "z", z,
                        "observed_center_byte", blocks[(8 * 16 + 8) * 128 + localY] & 255,
                        "region_input", Arrays.asList(x - 8, localY - 8, z - 8, x + 8, localY + 8, z + 8),
                        "actual_region_present", present));
                if (present) {
                    selected = new Call("immediate-present-stone", x, localY, z,
                            model.adapter.stoneId, Integer.MIN_VALUE, true);
                    break;
                }
            }
        }

        Object describe() {
            return object("attempts", attempts, "status", selected == null ? "SKIPPED_NO_ACTUAL_FOOTPRINT" : "SELECTED",
                    "selected", selected == null ? null : selected.input(),
                    "no_cache_publication_or_block_write", true);
        }
    }

    private static byte[] observe(Model model) throws Exception {
        Object adapterBindings = model.adapter.describe(), sourceBindings = model.bindings();
        Method seed = (Method) Adapter.field(model.adapter.modelType, "seedMethod").get(model.adapter.model);
        long initialSeed = (long) invoke(seed, model.adapter.world);
        List<Call> plan = calls();
        Selection selection = new Selection(model);
        if (selection.selected != null) plan.add(selection.selected);
        require(plan.size() >= 18 && plan.size() <= MAX_CALLS, "bounded direct schedule");
        Snapshot initial = new Snapshot(model);
        cap((long) initial.contextBytes.length * (2L * plan.size() + 3L) <= MAX_TOTAL_CONTEXT_BYTES,
                "full per-operation Context retention preflight");
        cap((long) encode(initial.description).length * (2L * plan.size() + 2L) <=
                MAX_OUTPUT_BYTES - 8L * 1024 * 1024, "full snapshot baseline output preflight");
        byte[] setupContext = model.adapter.bytes(model.adapter.baseline);
        boolean selectionPreservedContext = model.adapter.compare(model.adapter.same,
                model.adapter.baseline, initial.context);
        Mode initialFlags = new Mode(model);
        System.err.println("SETUP_DONE");
        measuring = true;
        model.adapter.helperMeasuring.setBoolean(null, true);
        List<Object> observations = new ArrayList<>();
        for (Call call : plan) observations.add(observeCall(model, call, observations.size()));
        Snapshot last = new Snapshot(model);
        Object identities = model.adapter.table();
        require(identities instanceof List<?>, "complete actual identity table");
        long finalSeed = (long) invoke(seed, model.adapter.world);
        return encode(object("schema", "SCHEDULING_OBSERVATION_V1", "side", model.client ? "client" : "server",
                "world_seed_input_bits", bits(WORLD_SEED), "initial_world_seed_bits", bits(initialSeed),
                "final_world_seed_bits", bits(finalSeed),
                "adapter", adapterBindings, "bindings", sourceBindings,
                "counter_policy", "UNOBSERVED until first actual source schedule normal return; no static read before it",
                "setup_schedule_calls", "Original constructor internals are uninstrumented; every helper-issued c(IIIII)V call follows",
                "setup_context_bytes", setupContext.length, "setup_context_sha256", sha256(setupContext),
                "setup_context", new RawJson(setupContext),
                "optional_selection_preserved_context", selectionPreservedContext,
                "optional_present_tick", selection.describe(), "initial", initial.description,
                "operations", observations, "final", last.description,
                "final_flags_equal_initial", initialFlags.same(last.mode),
                "identity_table", identities, "references", ((List<?>) identities).size(),
                "calls", observations.size(),
                "context_bytes_retained", model.adapter.contextBytes,
                "scope", "direct base World API; no dedicated, population, whole tick or native parity claim",
                "complete", true));
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out;
        System.setOut(System.err);
        int status = 1;
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
                byte[] transcript = observe(model);
                protocol.write(transcript, 0, transcript.length);
                protocol.flush();
                require(!protocol.checkError(), "JSON protocol output failure");
                System.err.println("MEASUREMENT_DONE " + (model.sequenceObserved ? "COUNTER_OBSERVED" : "COUNTER_UNOBSERVED"));
                status = 0;
            }
        } catch (Throwable failure) {
            status = measuring ? 1 : 77;
            System.err.println(status == 77 ? "UNAVAILABLE: genuine original scheduling setup"
                    : "FAILURE: original scheduling measurement");
            failure.printStackTrace(System.err);
        } finally {
            System.setOut(protocol);
        }
        System.exit(status);
    }
}
