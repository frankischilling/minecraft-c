import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Files;
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
import org.lwjgl.opengl.PixelFormat;

/** Whole original N method, genuine GL and explicit prepared graph inputs. */
public final class SubmitObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static Field field(Class<?> type, String name) throws Exception {
        Field f = type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> u = Class.forName("sun.misc.Unsafe");
        return u.getMethod("allocateInstance", Class.class).invoke(field(u, "theUnsafe").get(null), type);
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String ids(List<?> values, dk[] actors) {
        StringBuilder out = new StringBuilder();
        for (Object value : values) {
            int found = 0;
            for (int i = 0; i < actors.length; ++i) if (value == actors[i]) found = i + 1;
            require(found != 0, "actual DK identity");
            if (out.length() != 0) out.append(','); out.append(found);
        }
        return out.length() == 0 ? "-" : out.toString();
    }
    static void list(int id, int localX, int pass) {
        GL11.glNewList(id, GL11.GL_COMPILE);
        nw tessellator = nw.a;
        tessellator.b(0.0, 0.0, 0.0);
        tessellator.b();
        tessellator.a(pass == 0 ? 255 : 0, pass == 0 ? 0 : 255, 0);
        tessellator.a(localX - 0.5, -0.5, 0.0);
        tessellator.a(localX + 0.5, -0.5, 0.0);
        tessellator.a(localX + 0.5, 0.5, 0.0);
        tessellator.a(localX - 0.5, 0.5, 0.0);
        tessellator.a();
        GL11.glEndList();
    }
    static long bufferHash(IntBuffer buffer) {
        IntBuffer view = buffer.duplicate(); view.clear();
        long hash = 0xcbf29ce484222325L;
        for (int i = 0; i < view.capacity(); ++i)
            hash = (hash ^ (view.get(i) & 0xffffffffL)) * 1099511628211L;
        return hash;
    }
    static String matrix() {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, values);
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < 16; ++i) {
            if (i != 0) text.append(',');
            text.append(String.format("%08x", Float.floatToRawIntBits(values.get(i))));
        }
        return text.toString();
    }
    @SuppressWarnings({"removal", "unchecked"})
    public static void main(String[] args) throws Exception {
        require(args.length == 4, "JAR/native/input/profile arguments");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        String[] input = Files.readAllLines(Path.of(args[2])).get(Integer.parseInt(args[3])).split("\t", -1);
        String name = input[0]; int pass = Integer.parseInt(input[1]);
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }
        });
        for (Class<?> type : new Class<?>[]{n.class, dk.class, tg.class, u.class, ge.class,
                                           Minecraft.class, kq.class, kv.class, dc.class, nw.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "original game archive authority");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original N terrain submission");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("SUBMIT_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            for (int channel : new int[]{GL11.GL_RED_BITS, GL11.GL_GREEN_BITS, GL11.GL_BLUE_BITS, GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8, "declared common RGBA8 device profile");
            System.out.println("SUBMIT_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                               GL11.glGetInteger(GL11.GL_STENCIL_BITS));
            Class.forName("in", true, SubmitObservation.class.getClassLoader());
            Minecraft app = (Minecraft)raw(kq.class);
            boolean queryProfile = input[7].equals("1");
            boolean capability = queryProfile && new cx().a();
            if (queryProfile) {
                require(capability, "original supported-context occlusion cohort");
                System.out.println("SUBMIT_CAPABILITY " + name + " 1");
            }
            kv settings = (kv)raw(kv.class); settings.e = 3; settings.h = queryProfile; settings.g = false; app.z = settings;
            dc camera = (dc)raw(dc.class), view = (dc)raw(dc.class);
            camera.bl = 19; camera.bm = 29; camera.bn = 39;
            camera.aM = 20; camera.aN = 30; camera.aO = 40;
            view.aM = 0.5; view.aN = 0.25; view.aO = -0.5;
            app.i = input[6].equals("1") ? null : view;
            n renderer = (n)raw(n.class);
            field(n.class, "t").set(renderer, app); field(n.class, "H").setInt(renderer, 3);
            field(n.class, "f").setDouble(renderer, 20); field(n.class, "g").setDouble(renderer, 30);
            field(n.class, "h").setDouble(renderer, 40); field(n.class, "R").setInt(renderer, -1);
            field(n.class, "w").setBoolean(renderer, queryProfile ? capability : input[3].equals("1"));
            String[] counters = {"M", "N", "O", "P", "Q"};
            for (int i = 0; i < counters.length; ++i) field(n.class, counters[i]).setInt(renderer, 11 + i);
            ArrayList<dk> dirty = new ArrayList<>(), selected = new ArrayList<>();
            ArrayList<Object> tiles = new ArrayList<>();
            field(n.class, "m").set(renderer, dirty); field(n.class, "S").set(renderer, selected);
            field(n.class, "c").set(renderer, ge.d(64));
            tg[] batches = {new tg(), new tg(), new tg(), new tg()};
            field(n.class, "T").set(renderer, batches);
            int[] xs = {0, 16, 1024}; dk[] actors = new dk[3];
            for (int i = 0; i < actors.length; ++i) {
                int base = GL11.glGenLists(3);
                actors[i] = new dk(null, tiles, xs[i], 0, 0, 16, base);
                list(base, xs[i] & 1023, 0); list(base + 1, xs[i] & 1023, 1);
                actors[i].p[0] = false; actors[i].p[1] = i == 2;
                actors[i].o = !(i == 1 && input[4].equals("1"));
                actors[i].x = !(i == 0 && input[3].equals("1"));
                actors[i].u = i != 1;
                field(dk.class, "E").setBoolean(actors[i], true);
                if (i == 2 && input[5].equals("1")) actors[i].p[0] = actors[i].p[1] = true;
            }
            dirty.add(actors[0]); field(n.class, "o").set(renderer, actors);
            String[] ids = input[2].isEmpty() ? new String[0] : input[2].split(",");
            dk[] sorted = new dk[ids.length];
            for (int i = 0; i < ids.length; ++i) sorted[i] = actors[Integer.parseInt(ids[i]) - 1];
            field(n.class, "n").set(renderer, sorted);
            GL11.glViewport(0, 0, 64, 64);
            GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
            GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
            for (int feature : new int[]{GL11.GL_TEXTURE_2D, GL11.GL_LIGHTING, GL11.GL_COLOR_MATERIAL,
                    GL11.GL_FOG, GL11.GL_BLEND, GL11.GL_ALPHA_TEST, GL11.GL_DEPTH_TEST, GL11.GL_CULL_FACE})
                GL11.glDisable(feature);
            GL11.glClearColor(0, 0, 0, 0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);
            String result = "OK"; int count = -1;
            try { count = renderer.a(camera, pass, 0.5); }
            catch (NullPointerException e) { result = "NULL"; }
            System.out.print("SUBMIT " + name + " " + result + " " + count + " " + field(n.class, "R").getInt(renderer));
            for (String counter : counters) System.out.print(" " + field(n.class, counter).getInt(renderer));
            ByteBuffer pixel = BufferUtils.createByteBuffer(16);
            GL11.glReadBuffer(GL11.GL_BACK);
            GL11.glReadPixels(32, 32, 1, 1, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, pixel);
            System.out.print(" " + ids(dirty, actors) + " " + ids(selected, actors) + " " + matrix());
            for (int i = 0; i < 4; ++i) System.out.print(" " + (pixel.get(i) & 255));
            System.out.println();
            for (int i = 0; i < batches.length; ++i) {
                tg b = batches[i]; IntBuffer buffer = (IntBuffer)field(tg.class, "g").get(b);
                System.out.print("SUBMIT_TG " + name + " " + i + " " + (field(tg.class, "h").getBoolean(b) ? 1 : 0) +
                    " " + (field(tg.class, "i").getBoolean(b) ? 1 : 0));
                for (String key : new String[]{"a", "b", "c"}) System.out.print(" " + field(tg.class, key).getInt(b));
                for (String key : new String[]{"d", "e", "f"})
                    System.out.printf(" %08x", Float.floatToRawIntBits(field(tg.class, key).getFloat(b)));
                System.out.printf(" %d %d %d %016x%n", buffer.position(), buffer.limit(), buffer.capacity(), bufferHash(buffer));
            }
            require(GL11.glGetError() == GL11.GL_NO_ERROR && network == 0, "actual GL completion/no network");
            System.out.println("Original submit: " + name + "; actual GL; network=0");
            for (String link : links) System.out.println("NATIVE_LINK " + link);
        } finally { Display.destroy(); }
    }
}
