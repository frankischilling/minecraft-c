import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.Locale;

/** Reflection only: all geometry/pool operations execute the original jar. */
public final class BoundsObservation {
    private static final Class<?>[] SIX = {
        double.class, double.class, double.class,
        double.class, double.class, double.class
    };
    private static final Class<?>[] THREE = {double.class, double.class, double.class};
    private final boolean client;
    private final Class<?> bounds;
    private final Method get, direct, reset, clear, sweep, expand, shift, contract, copy;
    private final Method translate, assign, clipX, clipY, clipZ, intersects;
    private final Field[] fields = new Field[6];

    private BoundsObservation(Class<?> type, boolean clientSide) throws Exception {
        client = clientSide;
        bounds = type;
        get = type.getDeclaredMethod("b", SIX);
        direct = type.getDeclaredMethod("a", SIX);
        reset = type.getDeclaredMethod(client ? "b" : "a");
        clear = client ? type.getDeclaredMethod("a") : null;
        sweep = type.getDeclaredMethod("a", THREE);
        expand = type.getDeclaredMethod("b", THREE);
        shift = type.getDeclaredMethod("c", THREE);
        contract = type.getDeclaredMethod("e", THREE);
        copy = type.getDeclaredMethod(client ? "d" : "b");
        translate = type.getDeclaredMethod("d", THREE);
        assign = type.getDeclaredMethod("b", bounds);
        clipX = type.getDeclaredMethod("a", bounds, double.class);
        clipY = type.getDeclaredMethod("b", bounds, double.class);
        clipZ = type.getDeclaredMethod("c", bounds, double.class);
        intersects = type.getDeclaredMethod("a", bounds);
        for (int i = 0; i < fields.length; ++i) {
            fields[i] = type.getDeclaredField(String.valueOf((char)('a' + i)));
        }
    }

    private static double number(String value) {
        if (!value.matches("[0-9a-f]{16}")) {
            throw new IllegalArgumentException("expected lowercase raw binary64 bits");
        }
        return Double.longBitsToDouble(Long.parseUnsignedLong(value, 16));
    }

    private static Object[] six(double[] values, int start) {
        return new Object[] {values[start], values[start + 1], values[start + 2],
                             values[start + 3], values[start + 4], values[start + 5]};
    }

    private void appendBox(StringBuilder output, Object box) throws Exception {
        for (Field field : fields) {
            output.append('\t').append(String.format(Locale.ROOT, "%016x",
                Double.doubleToRawLongBits(field.getDouble(box))));
        }
    }

    private String row(String line) throws Exception {
        String[] tokens = line.split("\\s+");
        if (tokens.length != 18 || (!tokens[2].equals("0") && !tokens[2].equals("1"))) {
            throw new IllegalArgumentException("expected 18 fields and mode 0/1");
        }
        boolean alias = tokens[2].equals("1");
        String operation = tokens[1];
        double[] values = new double[15];
        for (int i = 0; i < values.length; ++i) {
            values[i] = number(tokens[i + 3]);
        }
        reset.invoke(null);
        Object initial = get.invoke(null, six(values, 0));
        // Direct factory supplies an actual original box for the second input.
        // The C interface also admits caller-owned coordinate values.
        Object other = direct.invoke(null, six(values, 6));
        Object result = initial;
        long scalar = 0;
        switch (operation) {
            case "get":
                break;
            case "reset":
                reset.invoke(null);
                result = get.invoke(null, six(values, 6));
                break;
            case "clear":
                if (!client) {
                    throw new IllegalArgumentException("server has no clear operation");
                }
                clear.invoke(null);
                result = get.invoke(null, six(values, 6));
                break;
            case "sweep":
            case "expand":
            case "shift":
            case "contract":
            case "copy":
                if (alias) {
                    reset.invoke(null);
                }
                Method transform = operation.equals("sweep") ? sweep
                    : operation.equals("expand") ? expand
                    : operation.equals("shift") ? shift
                    : operation.equals("contract") ? contract : copy;
                result = operation.equals("copy") ? transform.invoke(initial)
                    : transform.invoke(initial, values[12], values[13], values[14]);
                break;
            case "translate":
                result = translate.invoke(initial, values[12], values[13], values[14]);
                break;
            case "assign":
                assign.invoke(initial, alias ? initial : other);
                break;
            case "clip_x":
                scalar = Double.doubleToRawLongBits((Double)clipX.invoke(initial,
                    alias ? initial : other, values[12]));
                break;
            case "clip_y":
                scalar = Double.doubleToRawLongBits((Double)clipY.invoke(initial,
                    alias ? initial : other, values[13]));
                break;
            case "clip_z":
                scalar = Double.doubleToRawLongBits((Double)clipZ.invoke(initial,
                    alias ? initial : other, values[14]));
                break;
            case "intersects":
                scalar = (Boolean)intersects.invoke(initial, alias ? initial : other) ? 1 : 0;
                break;
            default:
                throw new IllegalArgumentException("unknown operation " + operation);
        }
        StringBuilder output = new StringBuilder(tokens[0]).append('\t').append(operation);
        appendBox(output, result);
        appendBox(output, initial);
        return output.append('\t').append(String.format(Locale.ROOT, "%016x", scalar))
            .append('\t').append(result == initial ? "1" : "0").toString();
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 ||
            (!arguments[1].equals("CLIENT") && !arguments[1].equals("SERVER"))) {
            throw new IllegalArgumentException("expected jar path and CLIENT/SERVER");
        }
        boolean client = arguments[1].equals("CLIENT");
        URL jar = Path.of(arguments[0]).toAbsolutePath().toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar},
                ClassLoader.getPlatformClassLoader());
             BufferedReader input = new BufferedReader(new InputStreamReader(System.in,
                 StandardCharsets.US_ASCII));
             PrintWriter output = new PrintWriter(System.out, false, StandardCharsets.US_ASCII)) {
            BoundsObservation observer = new BoundsObservation(
                Class.forName(client ? "eq" : "cz", true, loader), client);
            String line;
            while ((line = input.readLine()) != null) {
                output.print(observer.row(line));
                output.print('\n');
            }
            output.flush();
            if (output.checkError()) {
                throw new IllegalStateException("stdout write failed");
            }
        }
    }
}
