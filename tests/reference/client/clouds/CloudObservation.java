import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Unchanged original cloud calls and actual final Tessellator batch storage. */
public final class CloudObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
            .invoke(field(unsafe, "theUnsafe").get(null), type);
    }
    static void set(Object owner, Class<?> type, String name, Object value) throws Exception {
        field(type, name).set(owner, value);
    }
    static int flag(int capability) { return GL11.glIsEnabled(capability) ? 1 : 0; }
    static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder();
        for (byte value : bytes) text.append(String.format(Locale.ROOT, "%02x", value & 255));
        return text.toString();
    }
    static void word(MessageDigest digest, int value) {
        digest.update((byte)(value >>> 24));
        digest.update((byte)(value >>> 16));
        digest.update((byte)(value >>> 8));
        digest.update((byte)value);
    }
    @SuppressWarnings({"removal", "unchecked"})
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "arguments");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("network denied");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("listeners denied");
            }
            @Override public void checkWrite(String path) {
                throw new SecurityException("file writes denied");
            }
            @Override public void checkLink(String library) {
                require(links.size() < 16, "native link bound");
                links.add(library);
                boolean lwjgl = Path.of(library).isAbsolute() && Path.of(library).normalize()
                    .equals(natives.resolve("lwjgl64.dll"));
                if (!lwjgl) throw new SecurityException("unexpected native " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{n.class, kq.class, fd.class, dc.class, kv.class,
                ei.class, xa.class, nw.class, ji.class, ik.class, cx.class, bt.class, px.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original cloud observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR " + GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER " + GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION " + GL11.glGetString(GL11.GL_VERSION));
            Minecraft app = (Minecraft)raw(kq.class);
            kv settings = (kv)raw(kv.class);
            dc camera = (dc)raw(dc.class);
            app.i = camera; app.z = settings;
            set(app, Minecraft.class, "S", new cx());
            ji engine = (ji)raw(ji.class);
            HashMap<String, Integer> cache = new HashMap<>();
            set(engine, ji.class, "b", cache);
            set(engine, ji.class, "l", raw(ik.class));
            int texture = GL11.glGenTextures();
            ByteBuffer image = BufferUtils.createByteBuffer(32 * 32 * 4);
            for (int i = 0; i < 32 * 32 * 4; ++i) image.put((byte)255);
            image.flip();
            GL11.glBindTexture(GL11.GL_TEXTURE_2D, texture);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MIN_FILTER, GL11.GL_NEAREST);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MAG_FILTER, GL11.GL_NEAREST);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D, 0, GL11.GL_RGBA, 32, 32, 0, GL11.GL_RGBA,
                             GL11.GL_UNSIGNED_BYTE, image);
            cache.put("/environment/clouds.png", texture);
            n renderer = new n(app, engine);
            set(renderer, n.class, "x", 17);
            int[] actual = (int[])field(nw.class, "g").get(nw.a);
            IntBuffer integers = (IntBuffer)field(nw.class, "e").get(nw.a);
            FloatBuffer color = BufferUtils.createFloatBuffer(16);
            FloatBuffer matrix = BufferUtils.createFloatBuffer(16);
            ByteBuffer mask = BufferUtils.createByteBuffer(16);
            int[][] tuples = {{0, 0}, {1, 0}, {1, 1}};
            double[] heights = {64.5, 108.5, 120.5};
            int index = 0;
            for (int sample = 0; sample < 2; ++sample)
                for (int dim = 0; dim < 3; ++dim) {
                    fd world = (fd)raw(fd.class);
                    ei info = new ei(23L, "Clouds");
                    xa dimension = xa.a(dim == 0 ? 0 : dim == 1 ? -1 : 1);
                    set(world, fd.class, "x", info);
                    set(world, fd.class, "t", dimension);
                    dimension.a(world);
                    app.f = world; set(renderer, n.class, "k", world);
                    info.a(6000L);
                    set(world, fd.class, "i", 0f); set(world, fd.class, "j", 0f);
                    set(world, fd.class, "k", 0f); set(world, fd.class, "l", 0f);
                    set(world, fd.class, "H", 0xffffffL);
                    for (int fancy = 0; fancy < 2; ++fancy)
                        for (int height = 0; height < heights.length; ++height) {
                            if (sample == 1 && height != 1) continue;
                            for (int[] tuple : tuples) {
                                settings.j = fancy != 0; settings.g = tuple[0] != 0;
                                field(px.class, "b").setInt(null, tuple[1]);
                                camera.aM = sample == 0 ? 8.5 : -2049.25;
                                camera.aN = heights[height];
                                camera.aO = sample == 0 ? 8.5 : 4097.75;
                                camera.aJ = camera.aM - 1.0; camera.aL = camera.aO - 2.0;
                                camera.bl = camera.aM - 1.0; camera.bm = camera.aN - 1.0;
                                camera.bn = camera.aO - 2.0;
                                Arrays.fill(actual, 0); integers.position(0);
                                GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
                                GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
                                GL11.glColor4f(.125f, .25f, .5f, .75f);
                                GL11.glDisable(GL11.GL_CULL_FACE); GL11.glDisable(GL11.GL_BLEND);
                                GL11.glEnable(GL11.GL_TEXTURE_2D); GL11.glEnable(GL11.GL_ALPHA_TEST);
                                GL11.glDisable(GL11.GL_LIGHTING); GL11.glDisable(GL11.GL_FOG);
                                GL11.glDepthMask(true); GL11.glColorMask(true, true, true, true);
                                GL11.glShadeModel(GL11.GL_SMOOTH);
                                bt.b(); renderer.b(.375f);
                                GL11.glGetFloat(GL11.GL_CURRENT_COLOR, color);
                                GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, matrix);
                                GL11.glGetBoolean(GL11.GL_COLOR_WRITEMASK, mask);
                                int words = integers.position();
                                System.out.printf("CLOUD_STATE %d %d %d %d %d %d %d", index, dim,
                                                  fancy, height, tuple[0], tuple[1], sample);
                                for (int i = 0; i < 4; ++i)
                                    System.out.printf(" %08x", Float.floatToRawIntBits(color.get(i)));
                                System.out.printf(" %d %d %d %d %d %d %d", flag(GL11.GL_CULL_FACE),
                                    flag(GL11.GL_BLEND), flag(GL11.GL_TEXTURE_2D), flag(GL11.GL_ALPHA_TEST),
                                    flag(GL11.GL_FOG), GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK) ? 1 : 0,
                                    GL11.glGetInteger(GL11.GL_SHADE_MODEL));
                                for (int i = 0; i < 4; ++i) System.out.print(" " + mask.get(i));
                                System.out.print(" " + field(bt.class, "e").getInt(null));
                                for (int i = 0; i < 16; ++i)
                                    System.out.printf(" %08x", Float.floatToRawIntBits(matrix.get(i)));
                                System.out.println(" " + words);
                                int width = fancy == 0 ? 6 : 7;
                                require(words % 8 == 0 && words >= 0 && words <= actual.length,
                                        "actual final uploaded word span");
                                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                                StringBuilder rawWords = new StringBuilder();
                                for (int v = 0; v < words / 8; ++v)
                                    for (int w = 0; w < width; ++w) {
                                        int value = actual[v * 8 + w];
                                        word(digest, value);
                                        rawWords.append(String.format(Locale.ROOT, "%08x", value));
                                    }
                                System.out.printf("CLOUD_GEOMETRY %d %d %d %s%n", index, width,
                                                  words / 8, hex(digest.digest()));
                                System.out.println("CLOUD_WORDS " + index + " " + rawWords);
                                require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual cloud GL error");
                                require(cache.size() == 1 && cache.get("/environment/clouds.png") == texture,
                                        "actual Engine cache stable");
                                ++index;
                            }
                        }
                }
            require(index == 72 && network == 0, "profile count or network attempt");
            System.out.println("Original clouds: 72 rows; network=0; display=1; unchanged-methods=2");
            for (String library : links) System.out.println("NATIVE_LINK " + library);
        } finally { Display.destroy(); }
    }
}
