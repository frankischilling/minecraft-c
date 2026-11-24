import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public final class FamilyProbe {
    static int scenario, id;
    static final int X = 13, Y = 64, Z = -17;

    static int voxel(int x, int y, int z) {
        x -= X;
        y -= Y;
        z -= Z;
        if (x == 0 && y == 0 && z == 0)
            return id;
        if (scenario == 1 && y == 0 && ((x == -1 && z == 0) || (x == 0 && z == -1)))
            return 8;
        if (scenario == 2 && x == 1 && z == 0 && (y == 0 || y == 1))
            return 1;
        if (scenario == 3 && y == 0 && x >= -1 && x <= 1 && z >= -1 && z <= 1)
            return 8;
        return 0;
    }

    static int meta(int x, int y, int z) {
        if (x == X && y == Y && z == Z) {
            if (scenario == 1)
                return 4;
            if (scenario == 2)
                return 8;
            if (scenario == 6)
                return 1;
            if (scenario == 7)
                return 5;
            if (scenario == 10)
                return 1;
            if (scenario >= 12)
                return 17 - scenario;
        }
        if (scenario == 1 && y == Y) {
            if (x == X - 1 && z == Z)
                return 1;
            if (x == X && z == Z - 1)
                return 2;
        }
        return 0;
    }

    public static void main(String[] args) throws Exception {
        Class<?> uu = Class.forName("uu"), xp = Class.forName("xp"), cv = Class.forName("cv"),
                 nw = Class.forName("nw"), ln = Class.forName("ln");
        if ((boolean)Class.forName("net.minecraft.client.Minecraft").getMethod("v").invoke(null))
            throw new IllegalStateException(
                "Family fixtures require the no-Minecraft flat selector");
        Object[] blocks = (Object[])uu.getField("m").get(null);
        Object air = ln.getField("a").get(null), tess = nw.getField("a").get(null);
        Method opaque = uu.getMethod("c");
        Field material = uu.getField("bA");
        Object access =
            Proxy.newProxyInstance(xp.getClassLoader(), new Class<?>[] {xp}, (p, m, a) -> {
                String name = m.getName();
                int n = a == null ? 0 : a.length;
                if (name.equals("a") && n == 4 || name.equals("c") && n == 3)
                    return .75f;
                if (name.equals("a") && n == 3)
                    return voxel((int)a[0], (int)a[1], (int)a[2]);
                if (name.equals("e") && n == 3)
                    return meta((int)a[0], (int)a[1], (int)a[2]);
                int v = n >= 3 ? voxel((int)a[0], (int)a[1], (int)a[2]) : 0;
                if (name.equals("f"))
                    return v == 0 ? air : material.get(blocks[v]);
                if (name.equals("g"))
                    return v != 0 && (boolean)opaque.invoke(blocks[v]);
                throw new IllegalStateException(m.toString());
            });
        Object renderer = cv.getConstructor(xp).newInstance(access);
        Field active = nw.getDeclaredField("w"), data = nw.getDeclaredField("g"),
              count = nw.getDeclaredField("h");
        active.setAccessible(true);
        data.setAccessible(true);
        count.setAccessible(true);
        Method render = cv.getMethod("b", uu, int.class, int.class, int.class);
        int[] ids = {8, 8, 8, 8, 37, 31, 6, 59, 81, 53, 53, 85, 65, 65, 65, 65};
        for (scenario = 0; scenario < ids.length; ++scenario) {
            id = ids[scenario];
            active.setBoolean(tess, false);
            nw.getMethod("b", double.class, double.class, double.class)
                .invoke(tess, 0.0, -64.0, 32.0);
            nw.getMethod("b").invoke(tess);
            render.invoke(renderer, blocks[id], X, Y, Z);
            int[] words = (int[])data.get(tess);
            int vertices = count.getInt(tess);
            System.out.printf("case %d %d%n", scenario, vertices);
            for (int i = 0; i < vertices; ++i) {
                int color = words[i * 8 + 5];
                color = (color & 0xff00ff00) | ((color & 255) << 16) | ((color >>> 16) & 255);
                System.out.printf("%d %d %08x %08x %08x %08x %08x %08x%n", scenario, i,
                                  words[i * 8], words[i * 8 + 1], words[i * 8 + 2],
                                  words[i * 8 + 3], words[i * 8 + 4], color);
            }
        }
    }
}
