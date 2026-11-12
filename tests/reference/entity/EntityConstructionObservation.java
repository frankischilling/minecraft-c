import java.io.PrintWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

/** Original constructors only; recovered Math inputs are inference, not a method-exit trace. */
public final class EntityConstructionObservation {
    private static final long MULTIPLIER = 0x5deece66dL, MASK = (1L << 48) - 1;
    private static final String BASE =
            "aD:aG:I aE:aH:D aF:aI:Z aG:aJ:R aH:aK:R aI:aL:R "
            + "aJ:aM:D aK:aN:D aL:aO:D aM:aP:D aN:aQ:D aO:aR:D "
            + "aP:aS:D aQ:aT:D aR:aU:D aS:aV:F aT:aW:F aU:aX:F aV:aY:F "
            + "aW:aZ:R aX:ba:Z aY:bb:Z aZ:bc:Z ba:bd:Z bb:be:Z bc:bf:Z bd:bg:Z be:bh:Z "
            + "bf:bi:F bg:bj:F bh:bk:F bi:bl:F bj:bm:F bk:bn:F b:b:I "
            + "bl:bo:D bm:bp:D bn:bq:D bo:br:F bp:bs:F bq:bt:Z br:bu:F bs:bv:R "
            + "bt:bw:I bu:bx:I bv:by:I bw:bz:I bx:bA:Z by:bB:I bz:bC:I c:c:Z "
            + "bA:-:R bB:-:R bC:bD:Z bD:bE:R bE:bF:F d:d:D e:e:D bF:bG:Z "
            + "bG:bH:I bH:bI:I bI:bJ:I bJ:-:I bK:-:I bL:-:I bM:bK:Z";
    private static final String ITEM = "a:a:R e:e:I b:b:I c:c:I f:f:I d:d:F";
    private static final IdentityHashMap<Object, Integer> IDS = new IdentityHashMap<>();
    private static final IdentityHashMap<Object, Boolean> EXCLUSIVE = new IdentityHashMap<>();

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static int identity(Object object) {
        if (object == null) return 0;
        Integer existing = IDS.get(object);
        if (existing != null) return existing;
        int next = IDS.size() + 1;
        IDS.put(object, next); // Strongly retain every observed identity until the run ends.
        return next;
    }

    private static void distinct(Object object) {
        require(object != null && EXCLUSIVE.put(object, Boolean.TRUE) == null,
                "constructor reused an exclusive child identity");
        identity(object);
    }

    private static String hex(long value, int width) {
        if (width < 16) value &= (1L << (width * 4)) - 1;
        String digits = Long.toHexString(value);
        return "0".repeat(width - digits.length()) + digits;
    }

    private static String bits(double value) {
        return hex(Double.doubleToRawLongBits(value), 16);
    }

    private static void row(StringBuilder output, Object... columns) {
        for (int index = 0; index < columns.length; ++index) {
            if (index != 0) output.append('\t');
            output.append(columns[index]);
        }
        output.append('\n');
        require(output.length() <= 131072, "bounded output exceeded");
    }

    private static Object field(Class<?> type, String name, Object receiver) throws Exception {
        Field member = type.getDeclaredField(name);
        require(Modifier.isStatic(member.getModifiers()) == (receiver == null),
                "field staticness: " + name);
        member.setAccessible(true);
        return member.get(receiver);
    }

    private static Object mapped(Class<?> type, String client, String server,
            boolean isClient, Object receiver) throws Exception {
        return field(type, isClient ? client : server, receiver);
    }

    private static void fields(StringBuilder output, int caseId, String scope,
            Class<?> type, Object receiver, String specification, boolean client) throws Exception {
        Set<String> seen = new HashSet<>();
        int present = 0;
        for (String token : specification.split(" ")) {
            String[] parts = token.split(":");
            String selected = parts[client ? 0 : 1], tag = parts[2];
            if (selected.equals("-")) {
                row(output, "FIELD", caseId, scope, parts[0], parts[1], tag, 0, "-");
                continue;
            }
            require(seen.add(selected), "duplicate field specification");
            Field member = type.getDeclaredField(selected);
            require(!Modifier.isStatic(member.getModifiers()), "instance field required");
            member.setAccessible(true);
            Object value = member.get(receiver);
            String encoded;
            if (tag.equals("I")) {
                require(member.getType() == int.class, "int descriptor");
                encoded = hex((Integer)value, 8);
            } else if (tag.equals("Z")) {
                require(member.getType() == boolean.class, "boolean descriptor");
                encoded = (Boolean)value ? "1" : "0";
            } else if (tag.equals("F")) {
                require(member.getType() == float.class, "float descriptor");
                encoded = hex(Float.floatToRawIntBits((Float)value), 8);
            } else if (tag.equals("D")) {
                require(member.getType() == double.class, "double descriptor");
                encoded = bits((Double)value);
            } else {
                require(tag.equals("R") && !member.getType().isPrimitive(), "reference descriptor");
                encoded = Integer.toString(identity(value));
            }
            row(output, "FIELD", caseId, scope, parts[0], parts[1], tag, 1, encoded);
            ++present;
        }
        int declared = 0;
        for (Field member : type.getDeclaredFields())
            if (!Modifier.isStatic(member.getModifiers())) ++declared;
        require(present == declared, "incomplete declared-field coverage");
    }

    // Called only AFTER an original constructor has returned. Never warm this holder early.
    private static Object mathOwner() throws Exception {
        Class<?> holder = Class.forName("java.lang.Math$RandomNumberGeneratorHolder", false, null);
        Object owner = field(holder, "randomNumberGenerator", null);
        require(owner.getClass() == Random.class, "ordinary shared Math Random required");
        return owner;
    }

    private static long[] randomState(Object owner) throws Exception {
        require(owner.getClass() == Random.class, "ordinary private Random required");
        Object seed = field(Random.class, "seed", owner);
        require(seed.getClass() == AtomicLong.class, "AtomicLong seed descriptor");
        long state = ((AtomicLong)seed).get();
        require((state & MASK) == state, "48-bit state required");
        return new long[] {state,
                Double.doubleToRawLongBits((Double)field(Random.class, "nextNextGaussian", owner)),
                (Boolean)field(Random.class, "haveNextNextGaussian", owner) ? 1 : 0};
    }

    private static double[] infer(long postState, int count, long previousPost) {
        long inverse = 1;
        // Newton's error squares each step: six fixed Java-wrapped steps cover 64 bits.
        for (int step = 0; step < 6; ++step) inverse *= 2 - MULTIPLIER * inverse;
        require(((inverse * MULTIPLIER) & MASK) == 1, "LCG inverse identity");
        long[] states = new long[2 * count + 1];
        states[2 * count] = postState;
        for (int index = 2 * count; index > 0; --index)
            states[index - 1] = ((states[index] - 11) * inverse) & MASK;
        if (previousPost >= 0) require(states[0] == previousPost, "inter-case Math continuation");
        double[] draws = new double[count];
        for (int index = 1; index < states.length; ++index)
            require(((states[index - 1] * MULTIPLIER + 11) & MASK) == states[index],
                    "inverse forward check");
        for (int index = 0; index < count; ++index)
            draws[index] = (((states[2 * index + 1] >>> 22) << 27)
                    + (states[2 * index + 2] >>> 21)) * 0x1.0p-53;
        return draws;
    }

    private static void verifyDraws(Class<?> base, Class<?> item, Object entity,
            boolean client, double[] draws) throws Exception {
        double phaseProduct = draws[0] * 3.141592653589793;
        float phase = (float)(phaseProduct * 2.0);
        require(Float.floatToRawIntBits((Float)field(item, "d", entity))
                == Float.floatToRawIntBits(phase), "inferred phase does not match original");
        float yaw = draws.length == 1 ? 0.0f : (float)(draws[1] * 360.0);
        require(Float.floatToRawIntBits((Float)mapped(base, "aS", "aV", client, entity))
                == Float.floatToRawIntBits(yaw), "inferred yaw does not match original");
        for (int axis = 0; axis < 3; ++axis) {
            double expected = 0.0;
            if (draws.length == 4) {
                if (axis == 1) expected = 0.20000000298023224;
                else {
                    double product = draws[axis == 0 ? 2 : 3] * 0.20000000298023224;
                    expected = (double)(float)(product - 0.10000000149011612);
                }
            }
            double actual = (Double)mapped(base, new String[] {"aP", "aQ", "aR"}[axis],
                    new String[] {"aS", "aT", "aU"}[axis], client, entity);
            require(Double.doubleToRawLongBits(actual) == Double.doubleToRawLongBits(expected),
                    "inferred velocity does not match original");
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server")))
            throw new IllegalArgumentException("Usage: EntityConstructionObservation client|server jar");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        require(Files.size(jar) == (client ? 1465375 : 503100), "original JAR size");
        require(HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256")
                .digest(Files.readAllBytes(jar))).equals(expected), "original JAR hash");
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            Class<?> base = Class.forName(client ? "sn" : "lq", false, loader);
            Class<?> item = Class.forName(client ? "hl" : "ez", false, loader);
            Class<?> world = Class.forName(client ? "fd" : "dj", false, loader);
            Class<?> stackClass = Class.forName(client ? "iz" : "fy", false, loader);
            Class<?> boxClass = Class.forName(client ? "eq" : "cz", false, loader);
            Class<?> watcherClass = Class.forName(client ? "ud" : "mo", false, loader);
            Class<?> recordClass = Class.forName(client ? "ma" : "ht", false, loader);
            Constructor<?> shortConstructor = item.getDeclaredConstructor(world);
            Constructor<?> fullConstructor = item.getDeclaredConstructor(world,
                    double.class, double.class, double.class, stackClass);
            Constructor<?> stackConstructor = stackClass.getDeclaredConstructor(int.class, int.class, int.class);
            shortConstructor.setAccessible(true);
            fullConstructor.setAccessible(true);
            stackConstructor.setAccessible(true);
            double[][] points = {{0.0, 0.0, 0.0}, {1.25, 64.0, -2.5},
                    {-0.0, -0.0, 0.0}, {33554432.125, -17.75, -33554432.125},
                    {0x1.0p53, -0x1.0p53, -0x1.0p53}};
            StringBuilder output = new StringBuilder();
            row(output, "ENTITY_CONSTRUCTION_V1", args[0]);
            row(output, "POLICY", "MATH_INPUTS_INFERRED_FROM_POST_STATE", "NO_CLOCK_OR_U_OBSERVATION");
            Object sharedMath = null, firstByte = null, sharedPool = null, sharedTypes = null;
            long previousPost = -1;
            for (int caseId = 0; caseId < points.length; ++caseId) {
                Object supplied = caseId == 1 || caseId == 3
                        ? stackConstructor.newInstance(caseId == 1 ? 1 : -1,
                                caseId == 1 ? 7 : -7, caseId == 1 ? 3 : 32768) : null;
                Object entity = caseId == 0 ? shortConstructor.newInstance(new Object[] {null})
                        : fullConstructor.newInstance(null, points[caseId][0], points[caseId][1],
                                points[caseId][2], supplied);
                Object math = mathOwner(); // First Math/private-JDK read is after construction.
                long[] mathState = randomState(math);
                if (sharedMath == null) sharedMath = math;
                require(sharedMath == math, "Math owner changed");
                double[] draws = infer(mathState[0], caseId == 0 ? 1 : 4, previousPost);
                verifyDraws(base, item, entity, client, draws);
                previousPost = mathState[0];
                Object bounds = mapped(base, "aW", "aZ", client, entity);
                Object random = mapped(base, "bs", "bv", client, entity);
                Object watcher = mapped(base, "bD", "bE", client, entity);
                require(bounds.getClass() == boxClass && watcher.getClass() == watcherClass,
                        "actual child classes");
                Object mapObject = field(watcherClass, "b", watcher);
                require(mapObject instanceof Map, "watcher map descriptor");
                Map<?, ?> map = (Map<?, ?>)mapObject;
                Object record = map.get(Integer.valueOf(0));
                require(map.size() == 1 && record != null && record.getClass() == recordClass,
                        "watcher membership");
                Object byteValue = field(recordClass, "c", record);
                require(byteValue instanceof Byte && (Byte)byteValue == 0, "Byte0 value");
                if (firstByte == null) firstByte = byteValue;
                require(firstByte == byteValue, "shared Byte0 identity");
                require((Integer)field(recordClass, "b", record) == 0
                        && (Integer)field(recordClass, "a", record) == 0
                        && (Boolean)field(recordClass, "d", record)
                        && !(Boolean)field(watcherClass, "c", watcher), "watcher flags/type/index");
                for (Object exclusive : new Object[] {entity, bounds, random, watcher, map, record})
                    distinct(exclusive);
                require(random != math && field(item, "a", entity) == supplied
                        && mapped(base, "aI", "aL", client, entity) == null, "owner/world/stack alias");
                long[] privateState = randomState(random);
                require(privateState[1] == 0 && privateState[2] == 0, "fresh private Gaussian cache");
                row(output, "CASE", caseId, caseId == 0 ? "WORLD" : "FULL",
                        bits(points[caseId][0]), bits(points[caseId][1]), bits(points[caseId][2]), identity(supplied));
                row(output, "IDENTITY", caseId, identity(entity), identity(bounds), identity(watcher),
                        identity(record), identity(random), identity(supplied), identity(math));
                fields(output, caseId, "B", base, entity, BASE, client);
                fields(output, caseId, "I", item, entity, ITEM, client);
                row(output, "BOUNDS", caseId, identity(bounds), bits((Double)field(boxClass, "a", bounds)),
                        bits((Double)field(boxClass, "b", bounds)), bits((Double)field(boxClass, "c", bounds)),
                        bits((Double)field(boxClass, "d", bounds)), bits((Double)field(boxClass, "e", bounds)),
                        bits((Double)field(boxClass, "f", bounds)));
                row(output, "WATCH", caseId, identity(watcher), identity(map), identity(record),
                        hex((Integer)field(recordClass, "b", record), 8),
                        hex((Integer)field(recordClass, "a", record), 8), hex((Byte)byteValue, 2),
                        identity(byteValue), (Boolean)field(recordClass, "d", record) ? 1 : 0,
                        (Boolean)field(watcherClass, "c", watcher) ? 1 : 0);
                row(output, "PRIVATE_RANDOM", caseId, identity(random), hex(privateState[0], 16),
                        hex(privateState[1], 16), privateState[2]);
                row(output, "MATH_POST", caseId, identity(math), hex(mathState[0], 16),
                        hex(mathState[1], 16), mathState[2]);
                for (int index = 0; index < draws.length; ++index)
                    row(output, "INFERRED_DRAW", caseId, index + 1, bits(draws[index]));
                if (supplied == null) row(output, "STACK", caseId, 0, "-", "-", "-", "-");
                else row(output, "STACK", caseId, identity(supplied),
                        hex((Integer)field(stackClass, "c", supplied), 8),
                        hex((Integer)field(stackClass, "a", supplied), 8),
                        hex((Integer)field(stackClass, "d", supplied), 8),
                        hex((Integer)field(stackClass, "b", supplied), 8));
                Object pool = field(boxClass, "g", null), typesObject = field(watcherClass, "a", null);
                require(pool instanceof List && typesObject instanceof Map, "class graph descriptors");
                Map<?, ?> types = (Map<?, ?>)typesObject;
                if (sharedPool == null) { sharedPool = pool; sharedTypes = typesObject; }
                require(pool == sharedPool && typesObject == sharedTypes && ((List<?>)pool).isEmpty()
                        && (Integer)field(boxClass, "h", null) == 0 && types.size() == 7,
                        "retained empty pool/type graph");
                require((Integer)field(base, "a", null) == caseId + 1
                        && (Integer)mapped(base, "aD", "aG", client, entity) == caseId,
                        "class counter/instance ID continuation");
                row(output, "CLASSES", caseId, hex((Integer)field(base, "a", null), 8),
                        identity(pool), hex((Integer)field(boxClass, "h", null), 8),
                        ((List<?>)pool).size(), identity(types), types.size());
                Class<?>[] keys = {Byte.class, Short.class, Integer.class, Float.class, String.class,
                        stackClass, Class.forName(client ? "br" : "ay", false, loader)};
                for (int index = 0; index < keys.length; ++index) {
                    Object value = types.get(keys[index]);
                    require(value instanceof Integer && (Integer)value == index, "type graph entry");
                    row(output, "TYPE", caseId, index, identity(keys[index]), keys[index].getName(), hex((Integer)value, 8));
                }
                require(randomState(math)[0] == mathState[0], "observer changed Math continuation");
            }
            row(output, "END", points.length);
            PrintWriter writer = new PrintWriter(System.out, false, StandardCharsets.US_ASCII);
            writer.print(output);
            writer.flush();
            require(!writer.checkError(), "output failure");
        }
    }
}
