import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Unchanged original h(F), real World color methods and native clear color. */
public final class FogObservation {
    /* Only block/metadata transport is authored. No color, celestial, biome,
       weather, immersion, MathHelper or PX method is overridden. */
    public static final class FogWorld extends fd {
        int block;
        FogWorld() { super((wt)null, "unreached", 0L); }
        @Override public int a(int x, int y, int z) { return block; }
        @Override public int e(int x, int y, int z) { return 0; }
    }
    static int network;
    static final List<String> nativeLinks = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
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
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        Locale.setDefault(Locale.ROOT);
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
                require(nativeLinks.size() < 16, "native link bound");
                nativeLinks.add(library);
                boolean jdk = List.of("jawt", "awt", "awt_headless").contains(library);
                boolean lwjgl = Path.of(library).isAbsolute() &&
                    Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll"));
                if (!jdk && !lwjgl) throw new SecurityException("unexpected native link " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{px.class, kq.class, fd.class, dc.class,
                                           kv.class, ei.class, rh.class, xv.class, kd.class, bt.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original renderer fog observation");
        try {
            Display.create();
            px renderer = (px)raw(px.class);
            Minecraft app = (Minecraft)raw(kq.class);
            FogWorld world = (FogWorld)raw(FogWorld.class);
            dc camera = (dc)raw(dc.class);
            kv settings = (kv)raw(kv.class);
            ei info = new ei(23L, "Pick");
            xa dimension = new rh();
            field(fd.class, "x").set(world, info);
            field(fd.class, "t").set(world, dimension);
            dimension.a(world); /* Genuine manager/noise construction, no Chunk generation. */
            field(fd.class, "H").setLong(world, 0xffffffL);
            field(fd.class, "i").setFloat(world, 0.2f);
            field(fd.class, "j").setFloat(world, 0.8f);
            field(fd.class, "k").setFloat(world, 0.1f);
            field(fd.class, "l").setFloat(world, 0.7f);
            camera.aI = world;
            camera.aM = 8.5; camera.aN = 64.5; camera.aO = 8.5;
            app.f = world; app.i = camera; app.z = settings;
            field(px.class, "j").set(renderer, app);
            field(px.class, "L").setFloat(renderer, 0.25f);
            field(px.class, "M").setFloat(renderer, 0.875f);
            Method fog = px.class.getDeclaredMethod("h", float.class);
            fog.setAccessible(true);
            FloatBuffer actual = BufferUtils.createFloatBuffer(16);
            for (long time : new long[]{0, 6000, 18000})
                for (int distance = 0; distance < 4; ++distance)
                    for (int anaglyph = 0; anaglyph < 2; ++anaglyph)
                        for (int branch = 0; branch < 4; ++branch) {
                            info.a(time);
                            settings.e = distance;
                            settings.g = anaglyph != 0;
                            world.block = branch == 1 ? 9 : branch == 2 ? 11 : 0;
                            field(px.class, "D").setBoolean(renderer, branch == 3);
                            bt.b(); /* Authored between-call pool reset, never inside h. */
                            fog.invoke(renderer, 0.375f);
                            int red = Float.floatToRawIntBits(renderer.g);
                            int green = Float.floatToRawIntBits(renderer.h);
                            int blue = Float.floatToRawIntBits(renderer.i);
                            GL11.glGetFloat(GL11.GL_COLOR_CLEAR_VALUE, actual);
                            require(Float.floatToRawIntBits(actual.get(0)) == red &&
                                    Float.floatToRawIntBits(actual.get(1)) == green &&
                                    Float.floatToRawIntBits(actual.get(2)) == blue &&
                                    actual.get(3) == 0.0f, "real GL clear-color state differs");
                            require(GL11.glGetError() == GL11.GL_NO_ERROR, "original GL error");
                            System.out.printf("FOG %d %d %d %d %08x %08x %08x%n",
                                time, distance, anaglyph, branch, red, green, blue);
                        }
            require(network == 0, "external work attempted");
            System.out.println("Original full fog: 96 rows; network=0; display=1; clearcolor=1");
            for (String link : nativeLinks) System.out.println("NATIVE_LINK " + link);
        } finally {
            Display.destroy();
        }
    }
}
