import java.lang.reflect.Field;

public final class FaceProbe {
    public static void main(String[] args) throws Exception {
        Class<?> blockClass = Class.forName("uu"), rendererClass = Class.forName("cv"),
                 tessClass = Class.forName("nw");
        Object block = blockClass.getField("u").get(null),
               renderer = rendererClass.getConstructor().newInstance();
        Object tess = tessClass.getField("a").get(null);
        Field data = tessClass.getDeclaredField("g"), active = tessClass.getDeclaredField("w");
        data.setAccessible(true);
        active.setAccessible(true);
        String[] bounds = {"bs", "bt", "bu", "bv", "bw", "bx"};
        double[][] cases = {{0, 0, 0, 1, 1, 1},
                            {.125, .25, .375, .75, .875, 1},
                            {-.125, -.25, -.375, 1.125, 1.25, 1.375}};
        for (int test = 0; test < cases.length; ++test) {
            for (int i = 0; i < 6; ++i)
                blockClass.getField(bounds[i]).setDouble(block, cases[test][i]);
            for (int side = 0; side < 6; ++side) {
                active.setBoolean(tess, false);
                tessClass.getMethod("b", double.class, double.class, double.class)
                    .invoke(tess, 16.0, -48.0, -32.0);
                tessClass.getMethod("b").invoke(tess);
                tessClass.getMethod("a", int.class, int.class, int.class)
                    .invoke(tess, 31, 127, 233);
                rendererClass
                    .getMethod("" + (char)('a' + side), blockClass, double.class, double.class,
                               double.class, int.class)
                    .invoke(renderer, block, -3.0, 50.0, 39.0, 83);
                int[] words = (int[])data.get(tess);
                for (int i = 0; i < 6; ++i) {
                    int color = words[i * 8 + 5];
                    color = (color & 0xff00ff00) | ((color & 255) << 16) | ((color >>> 16) & 255);
                    System.out.printf("%d %d %d %08x %08x %08x %08x %08x %08x%n", test, side, i,
                                      words[i * 8], words[i * 8 + 1], words[i * 8 + 2],
                                      words[i * 8 + 3], words[i * 8 + 4], color);
                }
            }
        }
    }
}
