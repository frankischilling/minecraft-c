import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.client.Minecraft;
import org.lwjgl.input.Mouse;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;

/** Genuine Display/Mouse transport; prepared original PX focus-prefix receivers. */
public final class FocusObservation {
    static final int[] SENSITIVITY = {0, 0x3f000000, 0x3f800000};
    static final int[][] DELTAS = {{24, -13}, {-70, 41}, {0, 0}, {200, -200}, {-500, 500}, {1, -1}};
    static int network;
    static final List<String> nativeLinks = new ArrayList<>();

    static void require(boolean value) {
        if (!value) throw new AssertionError("original focus-prefix invariant");
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
                     .invoke(field(unsafe, "theUnsafe").get(null), type);
    }

    static int bits(Object value, Class<?> type, String name) throws Exception {
        return Float.floatToRawIntBits(field(type, name).getFloat(value));
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 2);
        Path jar = Path.of(arguments[0]).toRealPath();
        Path natives = Path.of(arguments[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("observer denies listeners");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
            @Override public void checkLink(String library) {
                require(nativeLinks.size() < 16);
                nativeLinks.add(library);
                boolean jdk = List.of("jawt", "awt", "awt_headless").contains(library);
                boolean lwjgl = Path.of(library).isAbsolute() &&
                    Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll"));
                if (!jdk && !lwjgl) throw new SecurityException("unexpected native link " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{px.class, kq.class, dc.class, vy.class, cu.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar));
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original focus-prefix observation");
        try {
            Display.create();
            require(Display.isCreated() && Mouse.isCreated());
            for (int sensitivity = 0; sensitivity < 3; ++sensitivity) {
                for (int invert = 0; invert < 2; ++invert) {
                    for (int smooth = 0; smooth < 2; ++smooth) {
                        int mode = (sensitivity * 2 + invert) * 2 + smooth;
                        px renderer = (px)raw(px.class);
                        Minecraft app = (Minecraft)raw(kq.class);
                        kv settings = (kv)raw(kv.class);
                        dc player = (dc)raw(dc.class);
                        vy mouse = (vy)raw(vy.class);
                        cu horizontal = new cu(), vertical = new cu();
                        field(px.class, "j").set(renderer, app);
                        field(px.class, "n").set(renderer, horizontal);
                        field(px.class, "o").set(renderer, vertical);
                        settings.c = Float.intBitsToFloat(SENSITIVITY[sensitivity]);
                        settings.d = invert != 0;
                        settings.E = smooth != 0;
                        app.z = settings;
                        app.h = player;
                        app.C = mouse;
                        app.N = true;
                        app.w = true;
                        player.aS = 17.25f;
                        player.aT = 87.5f;
                        player.aU = -9.0f;
                        player.aV = 2.5f;
                        for (int step = 0; step < DELTAS.length; ++step) {
                            field(px.class, "H").setLong(renderer, Long.MAX_VALUE);
                            field(Mouse.class, "dx").setInt(null, DELTAS[step][0]);
                            field(Mouse.class, "dy").setInt(null, DELTAS[step][1]);
                            renderer.b(0.375f);
                            System.out.printf("FOCUS %d %d %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %d %d %d %d%n",
                                mode, step, Float.floatToRawIntBits(player.aS),
                                Float.floatToRawIntBits(player.aT), Float.floatToRawIntBits(player.aU),
                                Float.floatToRawIntBits(player.aV), bits(horizontal, cu.class, "a"),
                                bits(horizontal, cu.class, "b"), bits(horizontal, cu.class, "c"),
                                bits(vertical, cu.class, "a"), bits(vertical, cu.class, "b"),
                                bits(vertical, cu.class, "c"), mouse.a, mouse.b,
                                field(Mouse.class, "dx").getInt(null), field(Mouse.class, "dy").getInt(null));
                        }
                    }
                }
            }
            require(network == 0);
            System.out.println("Original focus prefix: 72 rows; network=0; display=1; mouse=1");
            for (String link : nativeLinks) System.out.println("NATIVE_LINK " + link);
        } finally {
            Display.destroy();
        }
    }
}
