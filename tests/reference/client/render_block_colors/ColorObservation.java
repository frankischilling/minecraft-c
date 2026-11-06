import java.lang.reflect.Field;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Locale;
import net.minecraft.client.Minecraft;

/** Unchanged static AO and Block color virtuals on genuine saved World/EW. */
public final class ColorObservation {
    static int network, nativeLinks, aoRows, colorRows;
    static Object previousA, previousB, previousD;
    static xv manager;
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Object unsafe() throws Exception {
        return field(Class.forName("sun.misc.Unsafe"), "theUnsafe").get(null);
    }
    static Object raw(Class<?> type) throws Exception {
        Object u = unsafe();
        return u.getClass().getMethod("allocateInstance", Class.class).invoke(u, type);
    }
    static Object staticRaw(Class<?> type, String name) throws Exception {
        Object u = unsafe();
        Field f = field(type, name);
        Object base = u.getClass().getMethod("staticFieldBase", Field.class).invoke(u, f);
        long offset = (Long)u.getClass().getMethod("staticFieldOffset", Field.class).invoke(u, f);
        return u.getClass().getMethod("getObject", Object.class, long.class).invoke(u, base, offset);
    }
    static void ao(String name) {
        try {
            boolean result = Minecraft.v();
            System.out.println("AO " + name + " OK " + (result ? 1 : 0) + " 23");
        } catch (NullPointerException e) {
            System.out.println("AO " + name + " NULL -1 12");
        }
        ++aoRows;
    }
    static String values(double[] a) {
        if (a == null) return "-";
        if (a.length == 0) return "E";
        StringBuilder text = new StringBuilder();
        for (double value : a) {
            if (text.length() != 0) text.append(',');
            text.append(String.format("%016x", Double.doubleToRawLongBits(value)));
        }
        return text.toString();
    }
    static void color(String name, uu block, xp access) throws Exception {
        String result = "OK";
        int value = 0x12345678;
        try { value = block.b(access, 8, 64, 8); }
        catch (NullPointerException e) { result = "NULL"; }
        catch (ArrayIndexOutOfBoundsException e) { result = "INDEX"; }
        Object a = manager.a, b = manager.b, d = field(xv.class, "d").get(manager);
        System.out.println("COLOR " + name + " " + result + " " + String.format("%08x", value) +
            " " + (staticRaw(ia.class, "a") == null ? 0 : 1) +
            " " + (staticRaw(jh.class, "a") == null ? 0 : 1) +
            " " + values(manager.a) + " " + values(manager.b) +
            " " + (a == previousA ? 1 : 0) + " " + (b == previousB ? 1 : 0) +
            " " + (d == previousD ? 1 : 0));
        previousA = a; previousB = b; previousD = d;
        ++colorRows;
    }
    static void metadata(lm chunk, int value) { chunk.e.a(8, 64, 8, value); }
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 2, "JAR and authored World");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(arguments[0]).toRealPath();
        Path directory = Path.of(arguments[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h, int p) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int p) { ++network; throw new SecurityException(); }
            @Override public void checkLink(String lib) { ++nativeLinks; throw new SecurityException(); }
            @Override public void checkWrite(String file) {
                require(Path.of(file).toAbsolutePath().normalize().startsWith(directory),
                        "private World writes only");
            }
        });
        for (Class<?> type : new Class<?>[]{Minecraft.class, kq.class, kv.class, ew.class,
                 fd.class, lm.class, uu.class, wp.class, bk.class, ru.class, sm.class, ia.class,
                 jh.class, xv.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "original archive authority");
        ao("COLD_NULL_APP");
        Minecraft app = (Minecraft)raw(kq.class), other = (Minecraft)raw(kq.class);
        field(Minecraft.class, "a").set(null, app);
        ao("NULL_SETTINGS");
        kv settings = (kv)raw(kv.class), otherSettings = (kv)raw(kv.class);
        app.z = settings; other.z = otherSettings;
        settings.k = false; ao("OFF");
        settings.k = true; ao("ON");
        otherSettings.k = false;
        field(Minecraft.class, "a").set(null, other); ao("OTHER_PUBLISHED_OFF");
        field(Minecraft.class, "a").set(null, app); ao("FIRST_REPUBLISHED_ON");
        ArrayList<URL> urls = new ArrayList<>();
        for (String path : System.getProperty("java.class.path").split(java.io.File.pathSeparator))
            if (path.endsWith(".jar")) urls.add(Path.of(path).toUri().toURL());
        try (URLClassLoader isolated = new URLClassLoader(urls.toArray(URL[]::new),
                                                          ClassLoader.getPlatformClassLoader())) {
            Class<?> mc = Class.forName("net.minecraft.client.Minecraft", false, isolated);
            require(mc != Minecraft.class, "independent genuine class namespace");
            require(Boolean.FALSE.equals(mc.getMethod("v").invoke(null)), "isolated null App");
            System.out.println("AO INDEPENDENT_NULL_APP OK 0 23"); ++aoRows;
        }
        fd world = new fd(new mx(directory.getParent().toFile(),
                                 directory.getFileName().toString(), false), "", 23L);
        ew cache = new ew(world, 0, -1, 0, 31, 129, 31);
        lm chunk = world.c(0, 0);
        manager = cache.a();
        manager.a(0, 0, 1, 1); // Explicit fixture priming, outside Block methods.
        manager.a = new double[]{9.0, 8.0, 7.0};
        manager.b = new double[]{-1.0, -2.0, -3.0};
        field(xv.class, "c").set(manager, new double[]{6.0, 5.0, 4.0});
        previousA = manager.a; previousB = manager.b; previousD = field(xv.class, "d").get(manager);
        require(staticRaw(ia.class, "a") == null && staticRaw(jh.class, "a") == null,
                "both colorizers initially cold");
        uu dirt = uu.m[3], redstone = uu.m[55], fluid = uu.m[8], grass = uu.m[2];
        uu leaves = uu.m[18], tall = uu.m[31];
        color("BASE_NULL_XP", dirt, null);
        color("REDSTONE_NULL_XP", redstone, null);
        color("FLUID_NULL_XP", fluid, null);
        metadata(chunk, 1); color("LEAF_PINE", leaves, cache);
        metadata(chunk, 2); color("LEAF_BIRCH", leaves, cache);
        metadata(chunk, 3); color("LEAF_BOTH", leaves, cache);
        metadata(chunk, 0); color("TALL_DEAD", tall, cache);
        color("GRASS_COLD", grass, cache);
        color("LEAF_COLD", leaves, cache);
        metadata(chunk, 1); color("TALL_COLD", tall, cache);
        int[] grassMap = new int[65536], foliageMap = new int[65536];
        for (int i = 0; i < 65536; ++i) { grassMap[i] = 0xa5000000 | i; foliageMap[i] = 0x5a000000 | i; }
        ia.a(grassMap); jh.a(foliageMap);
        color("GRASS_ARGB", grass, cache);
        metadata(chunk, 0); color("LEAF_ARGB", leaves, cache);
        metadata(chunk, 1); color("TALL_ARGB_1", tall, cache);
        metadata(chunk, 2); color("TALL_ARGB_2", tall, cache);
        color("GRASS_REUSE", grass, cache);
        color("GRASS_NULL_XP", grass, null);
        color("LEAF_NULL_XP", leaves, null);
        field(ew.class, "d").set(cache, null);
        color("GRASS_NULL_WORLD", grass, cache);
        metadata(chunk, 1); color("LEAF_PINE_NULL_WORLD", leaves, cache);
        field(ew.class, "d").set(cache, world);
        xa dimension = world.t;
        field(fd.class, "t").set(world, null); color("GRASS_NULL_DIM", grass, cache);
        field(fd.class, "t").set(world, new rh()); color("GRASS_NULL_MANAGER", grass, cache);
        field(fd.class, "t").set(world, dimension);
        ia.a((int[])null); color("GRASS_NULL_COLOR_ARRAY", grass, cache);
        jh.a(new int[0]); metadata(chunk, 0); color("LEAF_SHORT_COLOR_ARRAY", leaves, cache);
        jh.a((int[])null); metadata(chunk, 1); color("LEAF_PINE_NULL_COLOR_ARRAY", leaves, cache);
        color("TALL_NULL_COLOR_ARRAY", tall, cache);
        require(network == 0 && nativeLinks == 0, "no network/native effects");
        System.out.println("Original colors: " + aoRows + " AO; " + colorRows +
                           " Block; network=0; native=0");
    }
}
