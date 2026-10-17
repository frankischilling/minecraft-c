import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** V2: actual protected calls with owned fixture patterns and inferred shared draws; no worlds. */
public final class CaveGeometryObservation {
    private static final int BLOCKS = 32768;
    private static final int MAX_CASES = 48;
    private static final long MAX_OUTPUT = 32L * 1024L * 1024L;

    private static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }

    private static String hex(byte[] bytes) {
        char[] result = new char[bytes.length * 2];
        char[] digits = "0123456789abcdef".toCharArray();
        for (int i = 0; i < bytes.length; i++) {
            result[i * 2] = digits[(bytes[i] & 255) >>> 4];
            result[i * 2 + 1] = digits[bytes[i] & 15];
        }
        return new String(result);
    }

    private static String hash(Path path) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path)));
    }

    private static List<String> continuation(Random original) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream stream = new ObjectOutputStream(bytes)) {
            stream.writeObject(original);
        }
        Random copy;
        try (ObjectInputStream stream = new ObjectInputStream(new ByteArrayInputStream(bytes.toByteArray()))) {
            copy = (Random) stream.readObject();
        }
        List<String> result = new ArrayList<>(4);
        for (int i = 0; i < 4; i++) {
            result.add(String.format(Locale.ROOT, "%016x", copy.nextLong()));
        }
        return result;
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number || value instanceof Boolean) return value.toString();
        if (value instanceof String) return "\"" + ((String) value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        StringBuilder result = new StringBuilder(value instanceof Map ? "{" : "[");
        if (value instanceof Map) {
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                if (result.length() > 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
        } else {
            for (Object entry : (List<?>) value) {
                if (result.length() > 1) result.append(',');
                result.append(json(entry));
            }
        }
        return result.append(value instanceof Map ? '}' : ']').toString();
    }

    private static Map<String, Object> blocks(byte[] values) {
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("length", values.length);
        result.put("values", hex(values));
        return result;
    }

    private static long integer(String text) {
        if (!text.matches("-?(?:0|[1-9][0-9]*)")) throw new IllegalArgumentException("canonical signed integer required");
        return Long.parseLong(text);
    }

    private static double binary64(String text) {
        if (!text.matches("[0-9a-f]{16}")) throw new IllegalArgumentException("binary64 raw bits required");
        double value = Double.longBitsToDouble(Long.parseUnsignedLong(text, 16));
        if (!Double.isFinite(value)) throw new IllegalArgumentException("finite geometry required");
        return value;
    }

    private static float binary32(String text) {
        if (!text.matches("[0-9a-f]{8}")) throw new IllegalArgumentException("binary32 raw bits required");
        float value = Float.intBitsToFloat(Integer.parseUnsignedInt(text, 16));
        if (!Float.isFinite(value)) throw new IllegalArgumentException("finite geometry required");
        return value;
    }

    private static int patternInteger(String text, int maximum) {
        long value = integer(text);
        if (value < 0 || value > maximum) throw new IllegalArgumentException("bounded pattern integer required");
        return (int) value;
    }

    private static int[] pattern(String text) {
        String[] parts = text.split(":", -1);
        if (text.equals("stone")) return new int[] {0};
        if (parts.length == 2 && parts[0].equals("solid")) {
            int id = patternInteger(parts[1], 255);
            boolean allowed = false;
            for (int candidate : new int[] {0, 1, 2, 3, 7, 12, 13, 24, 79, 128, 255}) {
                if (id == candidate) allowed = true;
            }
            if (!allowed) throw new IllegalArgumentException("reviewed solid ID required");
            return new int[] {1, id};
        }
        if (parts.length == 5 && parts[0].equals("water")) {
            int id = patternInteger(parts[1], 9);
            if (id != 8 && id != 9) throw new IllegalArgumentException("water ID8/9 required");
            return new int[] {2, id, patternInteger(parts[2], 15), patternInteger(parts[3], 127), patternInteger(parts[4], 15)};
        }
        if (parts.length == 3 && parts[0].equals("water-plane")) {
            int id = patternInteger(parts[1], 9);
            if (id != 8 && id != 9) throw new IllegalArgumentException("water ID8/9 required");
            return new int[] {3, id, patternInteger(parts[2], 127)};
        }
        if (parts.length == 2 && parts[0].equals("strata")) {
            return new int[] {4, patternInteger(parts[1], 127)};
        }
        throw new IllegalArgumentException("reviewed owned-buffer pattern required");
    }

    private static final class Request {
        final String[] fields;
        final long seed;
        final double x, y, z;
        final float width, yaw, pitch;
        final int start, end;
        final int[] patternValues;

        Request(String line) {
            if (line.length() > 256) throw new IllegalArgumentException("geometry request line limit");
            fields = line.split("\t", -1);
            boolean room = fields[0].equals("room");
            if (!(room && fields.length == 8) && !(fields[0].equals("path") && fields.length == 13)) {
                throw new IllegalArgumentException("room/path geometry request required");
            }
            seed = integer(fields[1]);
            if (integer(fields[2]) != 0 || integer(fields[3]) != 0) throw new IllegalArgumentException("protected target must be origin");
            x = binary64(fields[4]); y = binary64(fields[5]); z = binary64(fields[6]);
            if (x < 0 || x > 15 || y < 0 || y > 127 || z < 0 || z > 15) {
                throw new IllegalArgumentException("ordinary finite protected center required");
            }
            patternValues = pattern(fields[7]);
            if (room) {
                width = 0; yaw = 0; pitch = 0; start = 0; end = 0;
            } else {
                width = binary32(fields[8]); yaw = binary32(fields[9]); pitch = binary32(fields[10]);
                long first = integer(fields[11]), last = integer(fields[12]);
                if (width < 0 || width > 3 || yaw < 0 || yaw > 0x1.921fb6p+2f ||
                        pitch < -0.125f || pitch > 0.125f || last < 85 || last > 112 || first < 0 || first >= last) {
                    throw new IllegalArgumentException("bounded protected path required");
                }
                start = (int) first; end = (int) last;
            }
        }
    }

    private static List<Request> requests(Path path) throws Exception {
        if (Files.size(path) > 16384) throw new IllegalArgumentException("geometry request byte limit");
        List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
        if (lines.isEmpty() || lines.size() > MAX_CASES) throw new IllegalArgumentException("geometry case count limit");
        List<Request> result = new ArrayList<>();
        for (String line : lines) result.add(new Request(line));
        return result;
    }

    private static byte[] ownedInput(Request request) {
        byte[] values = new byte[BLOCKS + 3];
        Arrays.fill(values, 0, BLOCKS, (byte) 1);
        int[] p = request.patternValues;
        if (p[0] == 1) {
            Arrays.fill(values, 0, BLOCKS, (byte) p[1]);
        } else if (p[0] == 2) {
            values[(p[2] * 16 + p[4]) * 128 + p[3]] = (byte) p[1];
        } else if (p[0] == 3) {
            for (int x = 0; x < 16; x++) for (int z = 0; z < 16; z++) {
                values[(x * 16 + z) * 128 + p[2]] = (byte) p[1];
            }
        } else if (p[0] == 4) {
            for (int x = 0; x < 16; x++) for (int z = 0; z < 16; z++) for (int y = 0; y < 128; y++) {
                values[(x * 16 + z) * 128 + y] = (byte) (y > p[1] ? 0 : y == p[1] ? 2 : 3);
            }
        }
        values[BLOCKS] = (byte) 0x8a; values[BLOCKS + 1] = (byte) 0x8b; values[BLOCKS + 2] = (byte) 0x8c;
        return values;
    }

    private static Map<String, Object> sharedConsumption(Request request, List<String> observed) throws Exception {
        boolean isRoom = request.fields[0].equals("room");
        int matches = 0, selectedLongs = -1;
        // Bytecode permits one width float + one path seed for rooms, or
        // one root seed versus root + two child seeds for these bounded paths.
        // The trial is the actual JDK Random, not a copied LCG or geometry oracle.
        for (int longs : isRoom ? new int[] {1} : new int[] {1, 3}) {
            Random trial = new Random(request.seed);
            if (isRoom) trial.nextFloat();
            for (int i = 0; i < longs; i++) trial.nextLong();
            if (continuation(trial).equals(observed)) { matches++; selectedLongs = longs; }
        }
        if (matches != 1) throw new AssertionError("shared Random continuation has no unique allowed consumption match");
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("basis", "continuation_match"); result.put("next_float_calls", isRoom ? 1 : 0);
        result.put("next_long_calls", selectedLongs); result.put("fork_inferred", !isRoom && selectedLongs == 3);
        result.put("candidates_matched", matches);
        return result;
    }

    private static final class Model {
        final Class<?> caveClass, caveBase, registry;
        final Field randomField, rangeField, tableField, blockId;
        final Method room, path;
        final float[] table;
        final int[] initialBits;
        final Object[] registered = new Object[6];
        final String[] names = {"u", "v", "w", "B", "C", "D"};
        final int[] ids = {1, 2, 3, 8, 9, 10};

        Model(ClassLoader loader, boolean client) throws Exception {
            caveClass = Class.forName(client ? "sq" : "lt", true, loader);
            caveBase = Class.forName(client ? "fv" : "dy", true, loader);
            registry = Class.forName(client ? "uu" : "na", true, loader);
            blockId = field(registry, "bn");
            for (int i = 0; i < names.length; i++) registered[i] = field(registry, names[i]).get(null);
            randomField = field(caveBase, "b"); rangeField = field(caveBase, "a");
            room = caveClass.getDeclaredMethod("a", int.class, int.class, byte[].class, double.class, double.class, double.class);
            path = caveClass.getDeclaredMethod("a", int.class, int.class, byte[].class, double.class, double.class, double.class,
                    float.class, float.class, float.class, int.class, int.class, double.class);
            room.setAccessible(true); path.setAccessible(true);
            tableField = field(Class.forName(client ? "in" : "fq", true, loader), "a");
            table = (float[]) tableField.get(null);
            if (table.length != 65536) throw new AssertionError("actual trig table shape");
            initialBits = new int[table.length];
            for (int i = 0; i < table.length; i++) initialBits[i] = Float.floatToRawIntBits(table[i]);
            controls();
        }

        String controls() throws Exception {
            for (int i = 0; i < names.length; i++) {
                if (field(registry, names[i]).get(null) != registered[i] || blockId.getInt(registered[i]) != ids[i]) {
                    throw new AssertionError("registered block identity/ID changed");
                }
            }
            if (tableField.get(null) != table) throw new AssertionError("math table identity changed");
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            for (int i = 0; i < table.length; i++) {
                int value = Float.floatToRawIntBits(table[i]);
                if (value != initialBits[i]) throw new AssertionError("math table bits changed");
                digest.update((byte) (value >>> 24)); digest.update((byte) (value >>> 16));
                digest.update((byte) (value >>> 8)); digest.update((byte) value);
            }
            return hex(digest.digest());
        }

        Map<String, Object> observe(Request request, int index) throws Exception {
            Object cave = caveClass.getDeclaredConstructor().newInstance();
            Random random = (Random) randomField.get(cave);
            if (rangeField.getInt(cave) != 8) throw new AssertionError("actual cave range");
            random.setSeed(request.seed);
            List<String> beforeRandom = continuation(random);
            byte[] values = ownedInput(request);
            byte[] before = values.clone();
            String digest = controls();
            try {
                if (request.fields[0].equals("room")) {
                    room.invoke(cave, 0, 0, values, request.x, request.y, request.z);
                } else {
                    path.invoke(cave, 0, 0, values, request.x, request.y, request.z, request.width,
                            request.yaw, request.pitch, request.start, request.end, 1.0d);
                }
            } catch (InvocationTargetException failure) {
                Throwable cause = failure.getCause();
                if (cause instanceof Exception) throw (Exception) cause;
                throw (Error) cause;
            }
            if (randomField.get(cave) != random || rangeField.getInt(cave) != 8 || !digest.equals(controls())) {
                throw new AssertionError("retained cave/math state changed");
            }
            if (!Arrays.equals(Arrays.copyOfRange(before, BLOCKS, before.length),
                    Arrays.copyOfRange(values, BLOCKS, values.length))) throw new AssertionError("geometry suffix changed");
            int changed = 0;
            for (int i = 0; i < BLOCKS; i++) if (before[i] != values[i]) changed++;
            Map<String, Object> result = new LinkedHashMap<>();
            result.put("schema", 2); result.put("case", index); result.put("request", Arrays.asList(request.fields));
            result.put("input_blocks", blocks(before)); result.put("before_continuation", beforeRandom);
            List<String> afterRandom = continuation(random);
            result.put("output_blocks", blocks(values)); result.put("after_continuation", afterRandom);
            result.put("shared_random_consumption", sharedConsumption(request, afterRandom));
            result.put("changed_prefix_bytes", changed); result.put("math_table_sha256_f32be", digest);
            result.put("suffix_unchanged", true); result.put("retained_random", true); result.put("range", 8);
            System.err.println("CONTROL_GEOMETRY table_exact registry_exact range_exact random_identity suffix_exact shared_rng_continuation_match");
            return result;
        }
    }

    private static int run(String[] args) throws Exception {
        if (args.length != 3 || !(args[0].equals("client") || args[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: CaveGeometryObservation side jar requests");
        }
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (!hash(jar).equals(expected)) throw new IllegalArgumentException("original JAR SHA-256 differs");
        List<Request> requests = requests(Path.of(args[2]).toRealPath());
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader())) {
            Model model; PrintStream protocol = System.out;
            try {
                System.setOut(System.err);
                model = new Model(loader, client);
            } catch (Throwable failure) {
                System.err.println("UNAVAILABLE: protected cave class setup failed");
                failure.printStackTrace(System.err);
                return 77;
            } finally {
                System.setOut(protocol);
            }
            System.err.println("GEOMETRY_READY");
            long output = 0;
            for (int i = 0; i < requests.size(); i++) {
                System.err.println("GEOMETRY_BEGIN case=" + (i + 1));
                String line = "OK\t" + json(model.observe(requests.get(i), i + 1));
                long bytes = line.getBytes(StandardCharsets.UTF_8).length + System.lineSeparator().getBytes(StandardCharsets.UTF_8).length;
                if (bytes > 512 * 1024 || output + bytes > MAX_OUTPUT) throw new IllegalStateException("geometry output limit");
                output += bytes;
                System.out.println(line);
                System.err.println("GEOMETRY_DONE case=" + (i + 1));
            }
            return 0;
        } finally {
            if (!hash(jar).equals(expected)) throw new AssertionError("original JAR changed");
        }
    }

    public static void main(String[] args) throws Exception {
        int status = run(args);
        if (status != 0) System.exit(status);
    }
}
