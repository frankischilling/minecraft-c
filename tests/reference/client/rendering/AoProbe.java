import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public final class AoProbe {
    static int scenario;

    static float light(int x, int y, int z) {
        return (float)((x + 1) * 9 + (y + 1) * 3 + (z + 1) + 1) / 32.0f;
    }

    static boolean flag(int x, int y, int z) {
        return scenario == 1 || scenario == 2 && ((x + 2 * y + 4 * z) & 2) != 0;
    }

    public static void main(String[] args) throws Exception {
        Class<?> uu = Class.forName("uu"), xp = Class.forName("xp"), cv = Class.forName("cv"),
                 nw = Class.forName("nw");
        Object block = uu.getField("u").get(null), tess = nw.getField("a").get(null);
        Object access =
            Proxy.newProxyInstance(xp.getClassLoader(), new Class<?>[] {xp}, (p, m, a) -> {
                if (m.getName().equals("a") && a != null && a.length == 4)
                    return light((int)a[0], (int)a[1] - 64, (int)a[2]);
                if (m.getName().equals("a") && a != null && a.length == 3)
                    return flag((int)a[0], (int)a[1] - 64, (int)a[2]) ? 0 : 1;
                if (m.getName().equals("e"))
                    return 0;
                if (m.getName().equals("g"))
                    return false;
                throw new IllegalStateException(m.toString());
            });
        Object renderer = cv.getConstructor(xp).newInstance(access);
        Field active = nw.getDeclaredField("w"), data = nw.getDeclaredField("g");
        active.setAccessible(true);
        data.setAccessible(true);
        Method ao = cv.getMethod("a", uu, int.class, int.class, int.class, float.class, float.class,
                                 float.class);
        for (scenario = 0; scenario < 3; ++scenario) {
            active.setBoolean(tess, false);
            nw.getMethod("b").invoke(tess);
            ao.invoke(renderer, block, 0, 64, 0, .3f, .6f, .9f);
            int[] words = (int[])data.get(tess);
            for (int side = 0; side < 6; ++side)
                for (int i = 0; i < 6; ++i) {
                    int color = words[(side * 6 + i) * 8 + 5];
                    color = (color & 0xff00ff00) | ((color & 255) << 16) | ((color >>> 16) & 255);
                    System.out.printf("%d %d %d %08x%n", scenario, side, i, color);
                }
        }
    }
}
