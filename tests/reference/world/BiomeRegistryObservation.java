import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Set;

/** Observe an untouched original Biome bootstrap and explicitly declared controls.
 * The protocol goes only to a CREATE_NEW file; original System.out is untouched.
 * No game object is allocated, patched, substituted or forced READY by this helper.
 */
public final class BiomeRegistryObservation {
    private static final int REQUEST_LIMIT = 256 * 1024;
    private static final int OUTPUT_LIMIT = 4 * 1024 * 1024;
    private static final int OP_LIMIT = 4096;
    private static final int SNAPSHOT_LIMIT = 8;
    private static final int REFERENCE_LIMIT = 512;
    private static final String[] CLIENT_TYPES = {
        "yj", "tf", "kd", "rb", "fs", "kd", "g", "fs", "fs", "fs", "kd", "t", "ry"
    };
    private static final String[] SERVER_TYPES = {
        "pp", "ma", "gs", "kk", "dw", "gs", "f", "dw", "dw", "dw", "gs", "m", "le"
    };
    private static final Set<String> CLIENT_ENTITIES = Set.of(
            "cn", "uz", "fr", "gb", "uw", "dl", "wh", "ww", "bx", "xt", "gi", "bp", "ya");
    private static final Set<String> SERVER_ENTITIES = Set.of(
            "bn", "ne", "dv", "ec", "nb", "ce", "oc", "or", "bd", "pf", "eh", "aw", "pi");

    private static final class Request {
        final String[] fields;
        Request(String[] fields) { this.fields = fields; }
    }

    private final boolean client;
    private final Path jar;
    private final URLClassLoader loader;
    private final Class<?> biomeType;
    private Class<?> recordType;
    private final StringBuilder output = new StringBuilder();
    private final IdentityHashMap<Object, Integer> references = new IdentityHashMap<>();
    private final IdentityHashMap<Object, Character> names = new IdentityHashMap<>();
    private final IdentityHashMap<Class<?>, Boolean> checkedOrigins = new IdentityHashMap<>();
    private final Field[] named = new Field[13];
    private Field n, o, p, q, r, s, t, u, v, w, x, recordClass, recordWeight;
    private Method rebuild, floatSelector, doubleAccessor;
    private Object originalTable;
    private int snapshots;

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private BiomeRegistryObservation(boolean client, Path jar, URLClassLoader loader)
            throws Exception {
        this.client = client;
        this.jar = jar;
        this.loader = loader;
        biomeType = Class.forName(client ? "kd" : "gs", false, loader);
        origin(biomeType);
    }

    private void origin(Class<?> type) throws Exception {
        if (checkedOrigins.containsKey(type)) return;
        require(type.getClassLoader() == loader, "original class loader: " + type.getName());
        require(type.getProtectionDomain().getCodeSource() != null,
                "original CodeSource: " + type.getName());
        require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "original CodeSource path: " + type.getName());
        checkedOrigins.put(type, Boolean.TRUE);
    }

    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers)
            throws Exception {
        Field field = owner.getDeclaredField(name);
        require(field.getType() == type && field.getModifiers() == modifiers,
                "exact field descriptor/modifiers: " + owner.getName() + "." + name);
        field.setAccessible(true);
        return field;
    }

    private Method method(Class<?> result, Class<?>... parameters) throws Exception {
        Method method = biomeType.getDeclaredMethod("a", parameters);
        require(method.getReturnType() == result
                        && method.getModifiers() == (Modifier.PUBLIC | Modifier.STATIC),
                "exact Biome method descriptor/modifiers");
        return method;
    }

    private void schema() throws Exception {
        require(biomeType.getSuperclass() == Object.class
                        && biomeType.getDeclaredFields().length == 24,
                "exact Biome field inventory and ancestor");
        for (int i = 0; i < named.length; ++i)
            named[i] = field(biomeType, Character.toString((char)('a' + i)), biomeType,
                    Modifier.PUBLIC | Modifier.STATIC | Modifier.FINAL);
        n = field(biomeType, "n", String.class, Modifier.PUBLIC);
        o = field(biomeType, "o", int.class, Modifier.PUBLIC);
        p = field(biomeType, "p", byte.class, Modifier.PUBLIC);
        q = field(biomeType, "q", byte.class, Modifier.PUBLIC);
        r = field(biomeType, "r", int.class, Modifier.PUBLIC);
        s = field(biomeType, "s", List.class, Modifier.PROTECTED);
        t = field(biomeType, "t", List.class, Modifier.PROTECTED);
        u = field(biomeType, "u", List.class, Modifier.PROTECTED);
        v = field(biomeType, "v", boolean.class, Modifier.PRIVATE);
        w = field(biomeType, "w", boolean.class, Modifier.PRIVATE);
        x = biomeType.getDeclaredField("x");
        require(x.getModifiers() == (Modifier.PRIVATE | Modifier.STATIC)
                        && x.getType().isArray() && x.getType().getComponentType() == biomeType,
                "exact Biome x array descriptor/modifiers");
        x.setAccessible(true);
        recordType = Class.forName(client ? "bj" : "aq", false, loader);
        origin(recordType);
        require(recordType.getSuperclass() == Object.class
                        && recordType.getDeclaredFields().length == 2,
                "exact spawn-record field inventory and ancestor");
        recordClass = field(recordType, "a", Class.class, Modifier.PUBLIC);
        recordWeight = field(recordType, "b", int.class, Modifier.PUBLIC);
        rebuild = method(void.class);
        floatSelector = method(biomeType, float.class, float.class);
        doubleAccessor = method(biomeType, double.class, double.class);
    }

    private void row(Object... values) {
        for (int i = 0; i < values.length; ++i) {
            if (i != 0) output.append('\t');
            String value = String.valueOf(values[i]);
            require(value.indexOf('\t') < 0 && value.indexOf('\r') < 0
                            && value.indexOf('\n') < 0, "literal protocol field");
            for (int j = 0; j < value.length(); ++j)
                require(value.charAt(j) >= 32 && value.charAt(j) < 127,
                        "ASCII protocol field");
            output.append(value);
        }
        output.append('\n');
        require(output.length() <= OUTPUT_LIMIT, "bounded observation file");
    }

    private String reference(Object value) {
        if (value == null) return "NULL";
        Integer id = references.get(value);
        if (id == null) {
            require(references.size() < REFERENCE_LIMIT, "bounded identity graph");
            id = references.size() + 1;
            references.put(value, id);
        }
        return "R:" + id;
    }

    private static String units(String value) {
        require(value.length() <= 15, "source-sized Biome name");
        if (value.isEmpty()) return "-";
        StringBuilder text = new StringBuilder(value.length() * 4);
        final char[] hex = "0123456789abcdef".toCharArray();
        for (int i = 0; i < value.length(); ++i) {
            int unit = value.charAt(i);
            for (int shift = 12; shift >= 0; shift -= 4)
                text.append(hex[(unit >>> shift) & 15]);
        }
        return text.toString();
    }

    private Object named(String name) throws Exception {
        return named[name.charAt(0) - 'a'].get(null);
    }

    private Object[] table() throws Exception {
        Object table = x.get(null);
        require(table != null && table.getClass() == x.getType(), "actual x array type");
        Object[] values = (Object[])table;
        require(values.length == 4096, "source-sized x array");
        return values;
    }

    private void snapshot(String label, boolean controlled) throws Exception {
        require(++snapshots <= SNAPSHOT_LIMIT, "bounded snapshots");
        IdentityHashMap<Object, Boolean> biomes = new IdentityHashMap<>();
        IdentityHashMap<Object, Boolean> strings = new IdentityHashMap<>();
        IdentityHashMap<Object, Boolean> lists = new IdentityHashMap<>();
        IdentityHashMap<Object, Boolean> records = new IdentityHashMap<>();
        IdentityHashMap<Class<?>, Boolean> entities = new IdentityHashMap<>();
        String[] expectedTypes = client ? CLIENT_TYPES : SERVER_TYPES;
        Set<String> expectedEntities = client ? CLIENT_ENTITIES : SERVER_ENTITIES;
        row("SNAPSHOT", label);
        for (int index = 0; index < named.length; ++index) {
            char selector = (char)('a' + index);
            Object biome = named[index].get(null);
            require(biome != null && biomes.put(biome, Boolean.TRUE) == null,
                    "thirteen distinct named Biomes");
            Class<?> actualType = biome.getClass();
            origin(actualType);
            require(actualType.getName().equals(expectedTypes[index])
                            && (actualType == biomeType || (actualType.getSuperclass() == biomeType
                                && actualType.getDeclaredFields().length == 0)),
                    "actual named Biome concrete class/fields");
            Character previous = names.put(biome, selector);
            require(previous == null || previous.charValue() == selector,
                    "stable named Biome identities");
            String owner = reference(biome);
            row("NAMED", label, selector, owner);
            Object rawName = n.get(biome);
            require(rawName instanceof String, "actual named String receiver");
            String name = (String)rawName;
            String nameReference = reference(name);
            row("BIOME", label, owner, actualType.getName(), nameReference,
                    o.getInt(biome), p.getByte(biome), q.getByte(biome), r.getInt(biome),
                    v.getBoolean(biome) ? 1 : 0, w.getBoolean(biome) ? 1 : 0);
            if (strings.put(name, Boolean.TRUE) == null)
                row("STRING", label, nameReference, name.length(), units(name));
            Field[] fields = {s, t, u};
            for (int listIndex = 0; listIndex < fields.length; ++listIndex) {
                Object rawList = fields[listIndex].get(biome);
                require(rawList != null && rawList.getClass() == ArrayList.class
                                && lists.put(rawList, Boolean.TRUE) == null,
                        "distinct actual ArrayList owner");
                List<?> list = (List<?>)rawList;
                require(list.size() <= 5, "source-sized live spawn list");
                String listReference = reference(list);
                row("LIST", label, owner, (char)('s' + listIndex), listReference, list.size());
                for (int ordinal = 0; ordinal < list.size(); ++ordinal) {
                    Object record = list.get(ordinal);
                    require(record != null && record.getClass() == recordType
                                    && records.put(record, Boolean.TRUE) == null,
                            "distinct actual live spawn record");
                    String recordReference = reference(record);
                    Object rawClass = recordClass.get(record);
                    require(rawClass instanceof Class<?>, "actual spawn Class receiver");
                    Class<?> entity = (Class<?>)rawClass;
                    origin(entity);
                    require(expectedEntities.contains(entity.getName()),
                            "actual entity Class inventory");
                    String classReference = reference(entity);
                    row("ELEMENT", label, listReference, ordinal, recordReference);
                    row("RECORD", label, recordReference, classReference, recordWeight.getInt(record));
                    if (entities.put(entity, Boolean.TRUE) == null)
                        row("ENTITY_CLASS", label, classReference, entity.getName());
                }
            }
        }
        require(biomes.size() == 13 && strings.size() == 13 && lists.size() == 39
                        && records.size() == 115 && entities.size() == 13,
                "complete source-sized live graph inventory");
        Object[] table = table();
        if (originalTable == null) originalTable = table;
        require(table == originalTable, "warm/control rebuild retains actual x identity");
        row("TABLE", label, reference(table), table.length);
        for (int index = 0; index < table.length; ++index) {
            Object value = table[index];
            require(value == null ? controlled : biomes.containsKey(value),
                    "actual table aliases named Biomes");
            row("TABLE_SLOT", label, index, reference(value));
        }
        row("END_SNAPSHOT", label, biomes.size(), lists.size(), records.size(), entities.size());
    }

    private static String targetException(InvocationTargetException wrapper) {
        Throwable target = wrapper.getCause();
        require(target != null, "actual invocation target exception");
        return target.getClass().getName();
    }

    private void query(Request request) throws Exception {
        String[] values = request.fields;
        Object result;
        try {
            if (values[0].equals("F")) {
                float first = Float.intBitsToFloat((int)Long.parseUnsignedLong(values[2], 16));
                float second = Float.intBitsToFloat((int)Long.parseUnsignedLong(values[3], 16));
                result = floatSelector.invoke(null, first, second);
            } else {
                double first = Double.longBitsToDouble(Long.parseUnsignedLong(values[2], 16));
                double second = Double.longBitsToDouble(Long.parseUnsignedLong(values[3], 16));
                result = doubleAccessor.invoke(null, first, second);
            }
        } catch (InvocationTargetException actual) {
            row("QUERY", values[1], values[0], values[2], values[3], "THROW", targetException(actual));
            return;
        }
        require(result == null || names.containsKey(result), "actual query returns named alias");
        row("QUERY", values[1], values[0], values[2], values[3], "RETURN", reference(result));
    }

    private void operations(List<Request> requests) throws Exception {
        for (Request request : requests) {
            String[] values = request.fields;
            switch (values[0]) {
                case "F":
                case "D":
                    query(request);
                    break;
                case "SET_BYTE":
                    (values[3].equals("p") ? p : q).setByte(named(values[2]),
                            Byte.parseByte(values[4]));
                    row("CONTROL", values[1], "SET_BYTE", values[2], values[3], values[4]);
                    break;
                case "SET_TABLE":
                    table()[Integer.parseInt(values[2])] = values[3].equals("NULL")
                            ? null : named(values[3]);
                    row("CONTROL", values[1], "SET_TABLE", values[2], values[3]);
                    break;
                case "REBUILD":
                    try {
                        rebuild.invoke(null);
                        row("CONTROL", values[1], "REBUILD", "RETURN");
                    } catch (InvocationTargetException actual) {
                        row("CONTROL", values[1], "REBUILD", "THROW", targetException(actual));
                    }
                    require(table() == originalTable, "actual control retains x identity");
                    break;
                case "SNAPSHOT":
                    snapshot(values[1], true);
                    break;
                default:
                    throw new AssertionError("unparsed operation");
            }
        }
    }

    private void bootstrapFailure(Throwable actual) {
        row("BOOTSTRAP", "THROW", actual.getClass().getName(), reference(actual));
        IdentityHashMap<Throwable, Boolean> seen = new IdentityHashMap<>();
        Throwable next = actual;
        int cause = 0;
        while (next != null && seen.put(next, Boolean.TRUE) == null) {
            require(cause < 16, "bounded actual Throwable cause chain");
            Throwable child = next.getCause();
            row("THROWABLE", cause, reference(next), next.getClass().getName(), reference(child));
            StackTraceElement[] trace = next.getStackTrace();
            require(trace.length <= 256, "bounded actual Throwable stack");
            for (int i = 0; i < trace.length; ++i)
                row("FRAME", reference(next), i, trace[i].getClassName(), trace[i].getMethodName(),
                        trace[i].getLineNumber());
            next = child;
            ++cause;
        }
        row("END", "BOOTSTRAP_THROW", references.size());
    }

    private void observe(List<Request> requests) throws Throwable {
        row("BIOME_REGISTRY_V1", client ? "client" : "server");
        row("POLICY", "UNTOUCHED_ORIGINAL_BOOTSTRAP", "EXPLICIT_MUTATION_CONTROLS",
                "NOT_FULL_WORLD_OR_GAME_STARTUP");
        Class<?> initialized;
        try {
            initialized = Class.forName(biomeType.getName(), true, loader);
        } catch (Throwable actual) {
            try {
                bootstrapFailure(actual);
            } catch (Throwable observationFailure) {
                actual.addSuppressed(observationFailure);
            }
            throw actual; // No reflective Field.get follows an erroneous bootstrap.
        }
        require(initialized == biomeType, "ordinary bootstrap Class identity");
        row("BOOTSTRAP", "RETURN");
        schema();
        snapshot("baseline", false);
        rebuild.invoke(null); // Actual public warm helper; no new or copied table.
        snapshot("warm-rebuild", false);
        operations(requests);
        row("END", snapshots, requests.size(), references.size());
    }

    private static boolean label(String value) {
        return value.matches("[A-Za-z0-9_.-]{1,48}")
                && !value.equals("baseline") && !value.equals("warm-rebuild");
    }

    private static boolean selector(String value) { return value.matches("[a-m]"); }

    private static int decimal(String value, int minimum, int maximum) {
        require(value.matches("0|-?[1-9][0-9]{0,9}"), "canonical signed decimal input");
        int number = Integer.parseInt(value);
        require(number >= minimum && number <= maximum, "bounded declaration value");
        return number;
    }

    private static List<Request> requests(Path path) throws Exception {
        require(Files.size(path) <= REQUEST_LIMIT, "bounded request file");
        byte[] bytes = Files.readAllBytes(path);
        require(bytes.length != 0 && bytes.length <= REQUEST_LIMIT
                        && bytes[bytes.length - 1] == '\n', "literal final request LF");
        for (byte value : bytes)
            require(value == '\n' || value == '\t' || (value >= 32 && value < 127),
                    "ASCII literal request syntax");
        String[] lines = new String(bytes, StandardCharsets.US_ASCII).split("\n", -1);
        require(lines.length >= 3 && lines[0].equals("BIOME_REGISTRY_REQUEST_V1")
                        && lines[lines.length - 2].equals("END")
                        && lines[lines.length - 1].isEmpty(), "complete neutral request framing");
        List<Request> requests = new ArrayList<>();
        Set<String> labels = new HashSet<>();
        int snapshots = 2;
        for (int i = 1; i < lines.length - 2; ++i) {
            String[] fields = lines[i].split("\t", -1);
            require(fields.length >= 2 && label(fields[1]) && labels.add(fields[1]),
                    "unique operation label");
            switch (fields[0]) {
                case "F":
                case "D":
                    int digits = fields[0].equals("F") ? 8 : 16;
                    require(fields.length == 4 && fields[2].matches("[0-9a-f]{" + digits + "}")
                                    && fields[3].matches("[0-9a-f]{" + digits + "}"),
                            "typed raw-bit query declarations");
                    break;
                case "SET_BYTE":
                    require(fields.length == 5 && selector(fields[2])
                                    && (fields[3].equals("p") || fields[3].equals("q")),
                            "public byte control declaration");
                    decimal(fields[4], -128, 127);
                    break;
                case "SET_TABLE":
                    require(fields.length == 4 && (selector(fields[3]) || fields[3].equals("NULL")),
                            "actual array-cell control declaration");
                    decimal(fields[2], 0, 4095);
                    break;
                case "REBUILD":
                    require(fields.length == 2, "public helper declaration");
                    break;
                case "SNAPSHOT":
                    require(fields.length == 2 && ++snapshots <= SNAPSHOT_LIMIT,
                            "bounded graph snapshot declaration");
                    break;
                default:
                    throw new IllegalArgumentException("unknown request operation");
            }
            require(requests.size() < OP_LIMIT, "bounded declared operations");
            requests.add(new Request(fields));
        }
        return requests;
    }

    private static void verifyJar(Path jar, boolean client) throws Exception {
        require(Files.size(jar) == (client ? 1465375L : 503100L), "pinned original JAR size");
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        String actual = HexFormat.of().formatHex(
                MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)));
        require(actual.equals(expected), "pinned original JAR SHA-256");
    }

    public static void main(String[] args) throws Throwable {
        if (args.length != 4 || !(args[0].equals("client") || args[0].equals("server")))
            throw new IllegalArgumentException(
                    "Usage: BiomeRegistryObservation client|server jar requests.tsv output.tsv");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        Path requestPath = Path.of(args[2]).toRealPath();
        Path outputPath = Path.of(args[3]).toAbsolutePath();
        List<Request> requests = requests(requestPath); // Parse every declaration before game use.
        verifyJar(jar, client);
        BiomeRegistryObservation observer = null;
        Throwable failure = null;
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            observer = new BiomeRegistryObservation(client, jar, loader);
            observer.observe(requests);
        } catch (Throwable actual) {
            failure = actual;
        }
        try {
            if (observer != null)
                Files.write(outputPath, observer.output.toString().getBytes(StandardCharsets.US_ASCII),
                        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
        } catch (Throwable outputFailure) {
            if (failure == null) failure = outputFailure;
            else failure.addSuppressed(outputFailure);
        }
        if (failure != null) throw failure;
        verifyJar(jar, client);
    }
}
