import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Locale;

/** Only reflection glue; every shape, setter and pool operation is original. */
public final class BlockShapeObservation {
    private static final Class<?>[] SIX_D = {double.class, double.class, double.class,
        double.class, double.class, double.class};
    private static final Class<?>[] SIX_F = {float.class, float.class, float.class,
        float.class, float.class, float.class};
    private final Object[] blocks;
    private final double[][] initial = new double[97][6];
    private final Field[] blockFields = new Field[6], boxFields = new Field[6];
    private final Field poolField, cursorField;
    private final Method getter, contribution, setter, direct, pooled, reset, moving;

    private BlockShapeObservation(ClassLoader loader, boolean client, Path inventory)
            throws Exception {
        Class<?> block = Class.forName(client ? "uu" : "na", true, loader);
        Class<?> world = Class.forName(client ? "fd" : "dj", false, loader);
        Class<?> box = Class.forName(client ? "eq" : "cz", true, loader);
        blocks = (Object[])block.getDeclaredField("m").get(null);
        if (blocks.length != 256 || blocks[0] != null) {
            throw new IllegalStateException("unexpected actual registered array");
        }
        getter = block.getDeclaredMethod("e", world, int.class, int.class, int.class);
        contribution = block.getDeclaredMethod("a", world, int.class, int.class, int.class,
            box, ArrayList.class);
        setter = block.getDeclaredMethod("a", SIX_F);
        direct = box.getDeclaredMethod("a", SIX_D);
        pooled = box.getDeclaredMethod("b", SIX_D);
        reset = box.getDeclaredMethod(client ? "b" : "a");
        poolField = box.getDeclaredField("g");
        cursorField = box.getDeclaredField("h");
        poolField.setAccessible(true);
        cursorField.setAccessible(true);
        moving = Class.forName(client ? "ut" : "mz", false, loader).getDeclaredMethod("a",
            world, int.class, int.class, int.class, int.class, float.class, int.class);
        String[] names = {"bs", "bt", "bu", "bv", "bw", "bx"};
        for (int i = 0; i < 6; ++i) {
            blockFields[i] = block.getDeclaredField(names[i]);
            boxFields[i] = box.getDeclaredField(String.valueOf((char)('a' + i)));
        }
        StringBuilder metadata = new StringBuilder("BLOCK_SHAPE_REGISTRY_V1\n");
        for (int id = 1; id <= 96; ++id) {
            Object object = blocks[id];
            if (object == null || block.getDeclaredField("bn").getInt(object) != id) {
                throw new IllegalStateException("missing actual registered Block " + id);
            }
            for (int i = 0; i < 6; ++i) {
                initial[id][i] = blockFields[i].getDouble(object);
                if (Double.doubleToRawLongBits((double)(float)initial[id][i]) !=
                        Double.doubleToRawLongBits(initial[id][i])) {
                    throw new IllegalStateException("constructor bounds not float-representable");
                }
            }
            Method actualGetter = object.getClass().getMethod("e", world, int.class,
                int.class, int.class);
            Method actualContribution = object.getClass().getMethod("a", world, int.class,
                int.class, int.class, box, ArrayList.class);
            metadata.append(id).append('\t').append(object.getClass().getName()).append('\t')
                .append(actualGetter.getDeclaringClass().getName()).append('\t')
                .append(actualContribution.getDeclaringClass().getName()).append('\t')
                .append(coordinates(object, blockFields)).append('\n');
        }
        for (int id = 97; id < blocks.length; ++id) {
            if (blocks[id] != null) {
                throw new IllegalStateException("unexpected later registered Block " + id);
            }
        }
        Files.writeString(inventory, metadata.toString(), StandardCharsets.US_ASCII,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
    }

    private static double number(String bits) {
        if (!bits.matches("[0-9a-f]{16}")) {
            throw new IllegalArgumentException("expected raw binary64");
        }
        return Double.longBitsToDouble(Long.parseUnsignedLong(bits, 16));
    }

    private static float single(String bits) {
        if (!bits.matches("[0-9a-f]{8}")) {
            throw new IllegalArgumentException("expected raw binary32");
        }
        return Float.intBitsToFloat(Integer.parseUnsignedInt(bits, 16));
    }

    private static String coordinates(Object object, Field[] fields) throws Exception {
        if (object == null) {
            return "-";
        }
        StringBuilder text = new StringBuilder();
        for (Field field : fields) {
            if (text.length() != 0) {
                text.append(',');
            }
            text.append(String.format(Locale.ROOT, "%016x",
                Double.doubleToRawLongBits(field.getDouble(object))));
        }
        return text.toString();
    }

    private Object direct(String token) throws Exception {
        if (token.equals("-")) {
            return null;
        }
        String[] parts = token.split(",", -1);
        if (parts.length != 6) {
            throw new IllegalArgumentException("expected six query fields");
        }
        Object[] values = new Object[6];
        for (int i = 0; i < 6; ++i) {
            values[i] = number(parts[i]);
        }
        return direct.invoke(null, values);
    }

    private void shape(int id, String specification) throws Exception {
        if (specification.equals("keep")) {
            return;
        }
        Object[] values = new Object[6];
        if (specification.equals("ctor")) {
            for (int i = 0; i < 6; ++i) {
                values[i] = (float)initial[id][i];
            }
        } else {
            String[] tokens = specification.split(",", -1);
            if (tokens.length != 6) {
                throw new IllegalArgumentException("expected six raw float bounds");
            }
            for (int i = 0; i < 6; ++i) {
                values[i] = single(tokens[i]);
            }
        }
        setter.invoke(blocks[id], values);
    }

    private List<?> pool() throws Exception {
        return (List<?>)poolField.get(null);
    }

    private String identity(Object object, Object query, Object marker) throws Exception {
        if (object == null) {
            return "-";
        }
        List<?> values = pool();
        for (int i = 0; i < values.size(); ++i) {
            if (values.get(i) == object) {
                return "p" + i;
            }
        }
        if (object == query) {
            return "q";
        }
        if (object == marker) {
            return "m";
        }
        throw new IllegalStateException("unknown actual box origin");
    }

    private String poolValues() throws Exception {
        StringBuilder text = new StringBuilder();
        List<?> values = pool();
        for (int i = 0; i < values.size(); ++i) {
            if (i != 0) {
                text.append(';');
            }
            text.append('p').append(i).append(':').append(coordinates(values.get(i), boxFields));
        }
        return text.length() == 0 ? "_" : text.toString();
    }

    private String row(String line) throws Exception {
        String[] tokens = line.split("\t", -1);
        if (tokens.length != 14) {
            throw new IllegalArgumentException("expected fourteen protocol fields");
        }
        String operation = tokens[1];
        int id = Integer.parseInt(tokens[2]);
        int x = Integer.parseInt(tokens[3]), y = Integer.parseInt(tokens[4]),
            z = Integer.parseInt(tokens[5]);
        int stored = Integer.parseInt(tokens[11]);
        float progress = single(tokens[12]);
        int direction = Integer.parseInt(tokens[13]);
        if (id < 1 || id > 96 || (!operation.equals("get") &&
                !operation.equals("contribute") && !operation.equals("moving")) ||
                (operation.equals("moving") && id != 36)) {
            throw new IllegalArgumentException("unknown registered operation");
        }
        int shapeId = operation.equals("moving") && stored > 0 && stored <= 96 ? stored : id;
        shape(shapeId, tokens[6]);
        Object query = direct(tokens[7]);
        String queryBefore = coordinates(query, boxFields);
        Object marker = null;
        ArrayList<Object> destination = null;
        if (!tokens[8].equals("null")) {
            destination = new ArrayList<>();
            if (tokens[8].equals("marker")) {
                marker = direct.invoke(null, -9.0, -8.0, -7.0, -6.0, -5.0, -4.0);
                destination.add(marker);
            } else if (!tokens[8].equals("empty")) {
                throw new IllegalArgumentException("unknown destination");
            }
        }
        reset.invoke(null);
        Object anchor = null;
        if (tokens[9].equals("used") || tokens[9].equals("alias")) {
            anchor = pooled.invoke(null, 101.0, 102.0, 103.0, 104.0, 105.0, 106.0);
            if (tokens[9].equals("alias")) {
                reset.invoke(null);
            }
        } else if (!tokens[9].equals("reset") && !tokens[9].equals("between")) {
            throw new IllegalArgumentException("unknown pool mode");
        }
        int repeats = Integer.parseInt(tokens[10]);
        if (repeats < 1 || repeats > 2) {
            throw new IllegalArgumentException("observer protocol expects one or two calls");
        }
        int cursorBefore = cursorField.getInt(null), sizeBefore = pool().size();
        String blockBefore = coordinates(blocks[shapeId], blockFields);
        Object result = null, first = null;
        String outcome = "OK";
        for (int call = 0; call < repeats; ++call) {
            if (call != 0 && tokens[9].equals("between")) {
                reset.invoke(null);
            }
            try {
                if (operation.equals("get")) {
                    result = getter.invoke(blocks[id], null, x, y, z);
                } else if (operation.equals("contribute")) {
                    contribution.invoke(blocks[id], null, x, y, z, query, destination);
                } else {
                    result = moving.invoke(blocks[id], null, x, y, z, stored, progress, direction);
                }
                if (call == 0) {
                    first = result;
                }
            } catch (InvocationTargetException error) {
                outcome = error.getCause().getClass().getName();
                StringWriter trace = new StringWriter();
                error.getCause().printStackTrace(new PrintWriter(trace));
                System.err.println("EXCEPTION\t" + tokens[0] + "\t" +
                    HexFormat.of().formatHex(trace.toString().getBytes(StandardCharsets.UTF_8)));
                break;
            }
        }
        StringBuilder listed = new StringBuilder();
        if (destination == null) {
            listed.append('-');
        } else {
            for (Object value : destination) {
                if (listed.length() != 0) {
                    listed.append(';');
                }
                listed.append(identity(value, query, marker)).append(':')
                    .append(coordinates(value, boxFields));
            }
            if (listed.length() == 0) {
                listed.append('_');
            }
        }
        if (!queryBefore.equals(coordinates(query, boxFields))) {
            throw new IllegalStateException("original shape mutated caller query");
        }
        return String.join("\t", tokens[0], operation, tokens[2], outcome,
            String.valueOf(cursorBefore), String.valueOf(cursorField.getInt(null)),
            String.valueOf(sizeBefore), String.valueOf(pool().size()),
            identity(result, query, marker), coordinates(result, boxFields),
            String.valueOf(destination == null ? -1 : destination.size()), listed.toString(),
            blockBefore, coordinates(blocks[shapeId], blockFields),
            identity(anchor, query, marker), coordinates(anchor, boxFields), poolValues(),
            identity(first, query, marker), coordinates(first, boxFields));
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 3 ||
            (!arguments[1].equals("CLIENT") && !arguments[1].equals("SERVER"))) {
            throw new IllegalArgumentException("expected JAR path CLIENT/SERVER fresh-inventory-path");
        }
        URL jar = Path.of(arguments[0]).toAbsolutePath().toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar},
                ClassLoader.getPlatformClassLoader());
             BufferedReader input = new BufferedReader(new InputStreamReader(System.in,
                 StandardCharsets.US_ASCII));
             PrintWriter output = new PrintWriter(System.out, false, StandardCharsets.US_ASCII)) {
            BlockShapeObservation observer = new BlockShapeObservation(loader,
                arguments[1].equals("CLIENT"), Path.of(arguments[2]));
            output.println("BLOCK_SHAPES_V1\t" + arguments[1]);
            String line;
            while ((line = input.readLine()) != null) {
                output.println(observer.row(line));
            }
            output.flush();
            if (output.checkError()) {
                throw new IllegalStateException("stdout write failed");
            }
        }
    }
}
