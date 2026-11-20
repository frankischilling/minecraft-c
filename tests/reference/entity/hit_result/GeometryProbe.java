import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/** Authored inputs; invokes the unchanged original geometry through reflection. */
public final class GeometryProbe {
    private static Class<?> vec, box;
    private static Method direct, pooled, reset, boxDirect, distance, squared, squaredTo;
    private static Method[] planes;
    private static Method inside, intercept;
    private static Field[] xyz;
    private static Field cursor, hitFace, hitPoint;
    private static long state = 0x73acb16050a1b9d3L;

    private static double next() {
        state ^= state << 13;
        state ^= state >>> 7;
        state ^= state << 17;
        return ((state >>> 1) % 129 - 64) / 8.0;
    }

    private static String bits(double value) {
        return String.format("%016x", Double.doubleToRawLongBits(value));
    }

    private static String values(double[] values) {
        StringBuilder text = new StringBuilder();
        for (double value : values) {
            if (text.length() != 0) text.append(',');
            text.append(bits(value));
        }
        return text.toString();
    }

    private static String point(Object value) throws Exception {
        if (value == null) return "-";
        return bits(xyz[0].getDouble(value)) + "," + bits(xyz[1].getDouble(value))
            + "," + bits(xyz[2].getDouble(value));
    }

    private static Object[] operands(int mode, double[] start, double[] end) throws Exception {
        reset.invoke(null);
        Method factory = mode == 1 ? pooled : direct;
        Object first = factory.invoke(null, 0.0, 0.0, 0.0);
        Object second = mode == 2 ? first : factory.invoke(null, 0.0, 0.0, 0.0);
        for (int axis = 0; axis < 3; ++axis) xyz[axis].setDouble(first, start[axis]);
        if (mode != 2)
            for (int axis = 0; axis < 3; ++axis) xyz[axis].setDouble(second, end[axis]);
        if (mode == 1) reset.invoke(null); // Deliberate SAME pooled input reuse.
        return new Object[] {first, second};
    }

    private static void row(int mode, double[] start, double[] end,
                            double[] bounds, double[] coordinates) throws Exception {
        Object[] input = operands(mode, start, end);
        String d = bits((double) distance.invoke(input[0], input[1]));
        String s = bits((double) squared.invoke(input[0], input[1]));
        double[] actualEnd = mode == 2 ? start : end;
        String c = bits((double) squaredTo.invoke(input[0], actualEnd[0], actualEnd[1], actualEnd[2]));
        Object actualBox = boxDirect.invoke(null, bounds[0], bounds[1], bounds[2],
                                                   bounds[3], bounds[4], bounds[5]);
        boolean contained = (boolean) inside.invoke(actualBox, input[0]);
        String[] results = new String[3];
        int[] cursors = new int[4];
        for (int axis = 0; axis < 3; ++axis) {
            input = operands(mode, start, end);
            results[axis] = point(planes[axis].invoke(input[0], input[1], coordinates[axis]));
            cursors[axis] = cursor.getInt(null);
        }
        input = operands(mode, start, end);
        Object hit = intercept.invoke(actualBox, input[0], input[1]);
        String result = hit == null ? "-" : hitFace.getInt(hit) + "," + point(hitPoint.get(hit));
        cursors[3] = cursor.getInt(null);
        System.out.println(mode + " " + values(start) + " " + values(end) + " "
            + values(bounds) + " " + values(coordinates) + " " + d + " " + s + " " + c
            + " " + (contained ? 1 : 0) + " " + results[0] + " " + results[1] + " "
            + results[2] + " " + result + " " + cursors[0] + "," + cursors[1] + ","
            + cursors[2] + "," + cursors[3]);
    }

    public static void main(String[] args) throws Exception {
        boolean server = args.length == 1 && args[0].equals("server");
        vec = Class.forName(server ? "ba" : "bt");
        box = Class.forName(server ? "cz" : "eq");
        Class<?> hit = Class.forName(server ? "nh" : "vf");
        direct = vec.getMethod("a", double.class, double.class, double.class);
        pooled = vec.getMethod("b", double.class, double.class, double.class);
        reset = vec.getMethod(server ? "a" : "b");
        boxDirect = box.getMethod("a", double.class, double.class, double.class,
                                      double.class, double.class, double.class);
        distance = vec.getMethod(server ? "a" : "c", vec);
        squared = vec.getMethod(server ? "b" : "d", vec);
        squaredTo = vec.getMethod("d", double.class, double.class, double.class);
        planes = new Method[] {vec.getMethod("a", vec, double.class),
                               vec.getMethod("b", vec, double.class),
                               vec.getMethod("c", vec, double.class)};
        xyz = new Field[] {vec.getField("a"), vec.getField("b"), vec.getField("c")};
        cursor = vec.getDeclaredField("e");
        cursor.setAccessible(true);
        hitFace = hit.getField("e");
        hitPoint = hit.getField("f");
        inside = box.getMethod("a", vec);
        intercept = box.getMethod("a", vec, vec);
        List<double[]> starts = new ArrayList<>(), ends = new ArrayList<>();
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign : new int[] {-1, 1}) {
                for (double cross : new double[] {-1, 0, 1}) {
                    double[] a = {cross, cross, cross}, b = {cross, cross, cross};
                    a[axis] = sign * 2.0;
                    b[axis] = -sign * 2.0;
                    starts.add(a); ends.add(b);
                    starts.add(new double[] {0, 0, 0}); ends.add(a);
                }
            }
        }
        for (int index = 0; index < 256; ++index) {
            starts.add(new double[] {next(), next(), next()});
            ends.add(new double[] {next(), next(), next()});
        }
        double threshold = Math.sqrt((double) 1.0e-7f);
        for (double delta : new double[] {Math.nextDown(threshold), threshold,
                                          Math.nextUp(threshold), -threshold}) {
            for (int axis = 0; axis < 3; ++axis) {
                double[] a = {0, 0, 0}, b = {0, 0, 0};
                b[axis] = delta;
                starts.add(a); ends.add(b);
            }
        }
        for (double scale : new double[] {0.0, -0.0, 1.0e-100, 1.0e100, 1.0 / 3.0}) {
            starts.add(new double[] {-scale, scale, -0.0});
            ends.add(new double[] {scale, -scale, 0.0});
            starts.add(new double[] {scale, scale, scale});
            ends.add(new double[] {scale, scale, scale});
        }
        for (int index = 0; index < starts.size(); ++index)
            for (int mode = 0; mode < 3; ++mode)
                row(mode, starts.get(index), ends.get(index),
                    new double[] {-1, -1, -1, 1, 1, 1}, new double[] {0, 0, 0});
    }
}
