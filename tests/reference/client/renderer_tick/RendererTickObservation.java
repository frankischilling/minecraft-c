import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import net.minecraft.client.Minecraft;

/** Prepared genuine receivers, unchanged PX/World/RA/rain bodies. */
public final class RendererTickObservation {
    static final int[] DISTANCE = {0, 1, 2, 3, -1, Integer.MAX_VALUE, Integer.MIN_VALUE};
    static final int[] M = {0, 0x80000000, 0x3e4ccccd, 0xbf800000, 0x41200000};
    static int network, nativeLinks;

    static void require(boolean value) {
        if (!value) throw new AssertionError("original renderer tick invariant");
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }

    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        Field field = field(unsafe, "theUnsafe");
        return unsafe.getMethod("allocateInstance", Class.class).invoke(field.get(null), type);
    }

    static void put(Object receiver, Class<?> type, String name, int bits) throws Exception {
        field(type, name).setFloat(receiver, Float.intBitsToFloat(bits));
    }

    static int bits(Object receiver, Class<?> type, String name) throws Exception {
        return Float.floatToRawIntBits(field(type, name).getFloat(receiver));
    }

    static void run(int mode, int index, int distance, int m) throws Exception {
        px renderer = (px)raw(px.class);
        Minecraft app = (Minecraft)raw(kq.class);
        fd world = (fd)raw(fd.class);
        dc player = (dc)raw(dc.class);
        ra held = (ra)raw(ra.class);
        kv settings = (kv)raw(kv.class);
        xa dimension = new rh();
        dimension.f[15] = Float.intBitsToFloat(0x3e99999a);
        field(fd.class, "t").set(world, dimension);
        player.aM = 32000000.0;
        player.aN = 64.0;
        player.aO = -2.0;
        player.c = new ix(null);
        settings.e = distance;
        settings.j = true;
        app.f = world;
        app.h = player;
        app.i = index % 2 == 0 ? null : player;
        app.z = settings;
        field(px.class, "j").set(renderer, app);
        renderer.c = held;
        field(ra.class, "a").set(held, app);
        field(ra.class, "g").setInt(held, -1);
        put(held, ra.class, "c", 0x3f000000);
        put(held, ra.class, "d", 0x7fc23456);
        put(renderer, px.class, "M", m);
        put(renderer, px.class, "L", 0x7fc23456);
        put(renderer, px.class, "t", 0x7fa12345);
        put(renderer, px.class, "u", 0x3f800000);
        put(renderer, px.class, "v", 0x80000000);
        put(renderer, px.class, "x", 1);
        put(renderer, px.class, "z", 0xff800000);
        put(renderer, px.class, "B", 0x7fc12345);
        field(px.class, "l").setInt(renderer, index % 2 == 0 ? Integer.MAX_VALUE : -1);
        if (mode == 1) field(px.class, "j").set(renderer, null);
        if (mode == 2) app.f = null;
        if (mode == 3) { app.i = null; app.h = null; }
        if (mode == 4) app.z = null;
        if (mode == 5) renderer.c = null;
        if (mode == 6) field(ra.class, "a").set(held, null);
        if (mode == 7) { app.i = player; app.h = null; }
        if (mode == 8) player.c = null;
        String status = "OK";
        try {
            renderer.a();
        } catch (NullPointerException failure) {
            status = "NULL";
        }
        require(status.equals(mode == 0 ? "OK" : "NULL"));
        System.out.printf("RTICK %d %d %s %08x %08x %08x %08x %08x %08x %08x %d %d %08x %08x %d%n",
                          mode, index, status, bits(renderer, px.class, "L"),
                          bits(renderer, px.class, "M"), bits(renderer, px.class, "u"),
                          bits(renderer, px.class, "w"), bits(renderer, px.class, "y"),
                          bits(renderer, px.class, "A"), bits(renderer, px.class, "C"),
                          field(px.class, "l").getInt(renderer), app.i == player ? 1 : 0,
                          bits(held, ra.class, "c"), bits(held, ra.class, "d"),
                          field(ra.class, "g").getInt(held));
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1);
        Path jar = Path.of(arguments[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("observer denies listeners");
            }
            @Override public void checkLink(String library) {
                ++nativeLinks; throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
        });
        for (Class<?> type : new Class<?>[]{px.class, fd.class, ra.class, kq.class, dc.class,
                                           rh.class, ix.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar));
        for (int d = 0; d < DISTANCE.length; ++d)
            for (int n = 0; n < M.length; ++n)
                run(0, d * M.length + n, DISTANCE[d], M[n]);
        for (int mode = 1; mode <= 8; ++mode) run(mode, 0, 2, 0x3e4ccccd);
        require(network == 0 && nativeLinks == 0);
        System.out.println("Original renderer tick: 43 rows; network=0; native=0");
    }
}
