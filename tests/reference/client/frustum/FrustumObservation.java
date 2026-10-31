import java.lang.reflect.Field;
import java.nio.FloatBuffer;
import java.nio.ByteOrder;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.util.glu.GLU;

/** Unchanged real w/dr constructors, singleton, matrix extraction and box math. */
public final class FrustumObservation {
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
    static void zero(dr object) {
        require(object.a.length == 16 && object.b.length == 16 &&
                object.c.length == 16 && object.d.length == 16, "source array dimensions");
        for (int i = 0; i < 16; ++i) {
            require(object.a[i].length == 16, "source plane row dimensions");
            for (int j = 0; j < i; ++j)
                require(object.a[i] != object.a[j], "source distinct plane row identity");
            for (int j = 0; j < 16; ++j)
                require(Float.floatToRawIntBits(object.a[i][j]) == 0, "source zero plane");
            require(object.b[i] == 0 && object.c[i] == 0 && object.d[i] == 0, "source zero matrices");
        }
    }
    static void matrices(int mode) {
        GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
        if (mode == 1) GL11.glTranslatef(0.25f, -0.5f, 0.75f);
        if (mode >= 2) GLU.gluPerspective(90, mode == 2 ? 1 : 2, 1, 17);
        GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
        if (mode == 1) GL11.glTranslatef(1, -2, -4);
        if (mode == 2) { GL11.glTranslatef(3.25f, -0.5f, -4); GL11.glScalef(2, 0.5f, 4); }
        if (mode == 3) { GL11.glTranslatef(-0.5f, 1.25f, -8); GL11.glScalef(1, 2, 0.5f); }
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
            @Override public void checkWrite(String path) {
                throw new SecurityException("observer denies writes");
            }
            @Override public void checkLink(String library) {
                nativeLinks.add(library);
                boolean lwjgl = Path.of(library).isAbsolute() &&
                    Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll"));
                if (!lwjgl) throw new SecurityException("unexpected native link " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{w.class, dr.class, ge.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        zero(new dr());
        w singleton = (w)field(w.class, "e").get(null); /* Genuine cold w.<clinit>. */
        zero(singleton);
        for (String name : new String[]{"f", "g", "h"}) {
            FloatBuffer b = (FloatBuffer)field(w.class, name).get(singleton);
            require(b.isDirect() && b.order() == ByteOrder.nativeOrder() &&
                    b.position() == 0 && b.limit() == 16 && b.capacity() == 16,
                    "source ge FloatBuffer constructor");
        }
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original frustum observation");
        try {
            Display.create();
            double[][] boxes = {{-0.5,-0.5,-0.5,0.5,0.5,0.5},{2,2,2,3,3,3},
                {-1,-1,-1,1,1,1},{1,0,0,1,0.25,0.25},{-100,-100,-100,100,100,100},
                {0,0,0,0,0,0},{2,2,2,-2,-2,-2}};
            for (int mode = 0; mode < 4; ++mode) {
                matrices(mode);
                dr result = w.a();
                require(result == singleton && result == field(w.class, "e").get(null),
                        "source singleton identity");
                System.out.printf("FRUSTUM %d", mode);
                for (String name : new String[]{"f", "g", "h"}) {
                    FloatBuffer b = (FloatBuffer)field(w.class, name).get(singleton);
                    System.out.printf(" %d %d", b.position(), b.limit());
                }
                for (float[] array : new float[][]{result.b,result.c,result.d})
                    for (float value : array) System.out.printf(" %08x", Float.floatToRawIntBits(value));
                for (int plane = 0; plane < 6; ++plane)
                    for (int component = 0; component < 4; ++component)
                        System.out.printf(" %08x", Float.floatToRawIntBits(result.a[plane][component]));
                int mask = 0;
                for (int i = 0; i < boxes.length; ++i) {
                    double[] b = boxes[i];
                    if (result.a(b[0],b[1],b[2],b[3],b[4],b[5])) mask |= 1 << i;
                }
                require(!result.a(Double.NaN,-1,-1,Double.NaN,1,1), "source NaN box");
                require(GL11.glGetError() == GL11.GL_NO_ERROR, "source GL state");
                System.out.printf(" %d%n", mask);
            }
            require(network == 0, "external operation attempted");
            System.out.println("Original frustum: 4 rows; network=0; display=1; constructors=1");
            for (String link : nativeLinks) System.out.println("NATIVE_LINK " + link);
        } finally { Display.destroy(); }
    }
}
