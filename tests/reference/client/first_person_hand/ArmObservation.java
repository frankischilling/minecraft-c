import java.lang.reflect.Field;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Genuine DS/FH/PS construction and unchanged arm/list/face rendering. */
public final class ArmObservation {
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
    static void word(MessageDigest digest, int value) {
        digest.update((byte)(value >>> 24)); digest.update((byte)(value >>> 16));
        digest.update((byte)(value >>> 8)); digest.update((byte)value);
    }
    static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder();
        for (byte value : bytes) text.append(String.format(Locale.ROOT, "%02x", value & 255));
        return text.toString();
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
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
                require(links.size() < 16, "native link bound"); links.add(library);
                if (!Path.of(library).isAbsolute() || !Path.of(library).normalize()
                    .equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{ds.class, fh.class, ps.class, tz.class, ib.class,
                                            bt.class, nw.class, ge.class, in.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original arm observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR " + GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER " + GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION " + GL11.glGetString(GL11.GL_VERSION));
            ds renderer = new ds();
            fh model = (fh)field(ds.class, "a").get(renderer);
            ps arm = model.d;
            require(!field(ps.class, "n").getBoolean(arm) && field(ps.class, "o").getInt(arm) == 0,
                    "real uncompiled arm");
            tz[] faces = (tz[])field(ps.class, "k").get(arm);
            require(faces.length == 6, "actual six-face arm");
            int[] actual = (int[])field(nw.class, "g").get(nw.a);
            IntBuffer integers = (IntBuffer)field(nw.class, "e").get(nw.a);
            FloatBuffer matrix = BufferUtils.createFloatBuffer(16);
            int previousList = 0;
            for (int repeat = 0; repeat < 2; ++repeat) {
                Arrays.fill(actual, 0); integers.position(0); bt.b();
                GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
                GL11.glDisable(GL11.GL_LIGHTING); GL11.glDisable(GL11.GL_FOG);
                GL11.glDisable(GL11.GL_BLEND); GL11.glDisable(GL11.GL_CULL_FACE);
                GL11.glColor4f(.125f, .25f, .5f, .75f);
                renderer.b();
                int list = field(ps.class, "o").getInt(arm);
                GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, matrix);
                System.out.printf("ARM_PART %d %d %d %d %d %d", repeat,
                    field(ps.class, "n").getBoolean(arm) ? 1 : 0, GL11.glIsList(list) ? 1 : 0,
                    list == previousList ? 1 : 0, GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),
                    field(bt.class, "e").getInt(null));
                for (int i = 0; i < 16; ++i)
                    System.out.printf(" %08x", Float.floatToRawIntBits(matrix.get(i)));
                for (float value : new float[]{model.m, arm.a, arm.b, arm.c, arm.d, arm.e, arm.f})
                    System.out.printf(" %08x", Float.floatToRawIntBits(value));
                System.out.println();
                require(list > 0 && (repeat == 0 || list == previousList), "actual lazy-list identity");
                require(GL11.glGetError() == GL11.GL_NO_ERROR, "original arm GL error");
                previousList = list;
            }
            Arrays.fill(actual, 0); integers.position(0); bt.b();
            for (int face = 0; face < faces.length; ++face) {
                faces[face].a(nw.a, .0625f);
                int vertices = integers.position() / 8;
                require(integers.position() == 48 && vertices == 6, "actual converted face size");
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                StringBuilder words = new StringBuilder();
                for (int vertex = 0; vertex < vertices; ++vertex)
                    for (int selected = 0; selected < 7; ++selected) {
                        int value = actual[vertex * 8 + selected]; word(digest, value);
                        words.append(String.format(Locale.ROOT, "%08x", value));
                    }
                System.out.printf("ARM_FACE %d %d %d %s%n", face, vertices,
                                  field(bt.class, "e").getInt(null), hex(digest.digest()));
                System.out.println("ARM_WORDS " + face + " " + words);
                require(GL11.glGetError() == GL11.GL_NO_ERROR, "original face GL error");
            }
            for (int flags = 0; flags < 16; ++flags) {
                model.j = (flags & 1) != 0; model.k = (flags & 2) != 0;
                model.l = (flags & 4) != 0; model.n = (flags & 8) != 0;
                bt.b(); renderer.b();
                System.out.printf("ARM_POSE %d %d", flags, field(bt.class, "e").getInt(null));
                for (ps part : new ps[]{model.a, model.b, model.c, model.d, model.e,
                                        model.f, model.g, model.h, model.i})
                    for (float value : new float[]{part.a, part.b, part.c, part.d, part.e, part.f})
                        System.out.printf(" %08x", Float.floatToRawIntBits(value));
                System.out.println();
                require(GL11.glGetError() == GL11.GL_NO_ERROR, "original pose GL error");
            }
            require(network == 0, "network attempt");
            System.out.println("Original arm: 2 part rows; 6 face rows; 16 pose rows; network=0; display=1");
            for (String library : links) System.out.println("NATIVE_LINK " + library);
        } finally { Display.destroy(); }
    }
}
