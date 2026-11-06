import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
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

/** Unchanged outer hand call with declared no-child and null-child operands. */
public final class HandObservation {
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
                require(links.size() < 16, "native link bound");
                links.add(library);
                if (!Path.of(library).isAbsolute() || !Path.of(library).normalize()
                    .equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{px.class, kq.class, dc.class, kv.class,
                                            gs.class, ls.class, in.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original hand observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR " + GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER " + GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION " + GL11.glGetString(GL11.GL_VERSION));
            Minecraft app = (Minecraft)raw(kq.class);
            kv settings = (kv)raw(kv.class);
            dc camera = (dc)raw(dc.class);
            px renderer = (px)raw(px.class);
            app.z = settings; app.i = camera;
            field(px.class, "j").set(renderer, app);
            camera.Y = 20; camera.aa = 7; camera.ab = 10; camera.ac = 60; camera.ad = 0;
            camera.bi = 1.125f; camera.bj = 1.5f;
            camera.h = .1f; camera.i = .175f; camera.af = 2; camera.ag = -1;
            Method hand = px.class.getDeclaredMethod("b", float.class, int.class);
            hand.setAccessible(true);
            FloatBuffer model = BufferUtils.createFloatBuffer(16);
            FloatBuffer projection = BufferUtils.createFloatBuffer(16);
            int index = 0;
            for (int which = 0; which < 4; ++which)
                for (int partial = 0; partial < 2; ++partial)
                    for (int bob = 0; bob < 2; ++bob)
                        for (int anaglyph = 0; anaglyph < 2; ++anaglyph)
                            for (int eye = 0; eye < 2; ++eye) {
                                settings.A = which == 0;
                                settings.z = which == 2;
                                settings.f = bob != 0; settings.g = anaglyph != 0;
                                field(gs.class, "u").setBoolean(camera, which == 1);
                                GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
                                GL11.glScalef(1.25f, .75f, 1.5f);
                                GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
                                GL11.glTranslatef(5, 6, 7); GL11.glScalef(2, 3, 4);
                                GL11.glColor4f(.125f, .25f, .5f, .75f);
                                String outcome = "OK";
                                try { hand.invoke(renderer, partial == 0 ? .375f : 1.0f, eye); }
                                catch (InvocationTargetException exception) {
                                    require(exception.getCause() instanceof NullPointerException,
                                            "unexpected original source failure");
                                    outcome = "NULL";
                                    System.out.print("HAND_EXCEPTION " + index);
                                    for (StackTraceElement frame : exception.getCause().getStackTrace())
                                        System.out.print(" " + frame.getClassName() + "." +
                                                         frame.getMethodName() + ":" + frame.getLineNumber());
                                    System.out.println();
                                }
                                require(outcome.equals(which < 2 ? "OK" : "NULL"),
                                        "outer source outcome differs");
                                GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX, model);
                                GL11.glGetFloat(GL11.GL_PROJECTION_MATRIX, projection);
                                int depth = GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH);
                                int mode = GL11.glGetInteger(GL11.GL_MATRIX_MODE);
                                System.out.printf("HAND_OUTER %d %d %d %d %d %d %s %d %d", index,
                                                  which, partial, bob, anaglyph, eye, outcome, depth, mode);
                                for (int i = 0; i < 16; ++i)
                                    System.out.printf(" %08x", Float.floatToRawIntBits(model.get(i)));
                                for (int i = 0; i < 16; ++i)
                                    System.out.printf(" %08x", Float.floatToRawIntBits(projection.get(i)));
                                System.out.println();
                                require(GL11.glGetError() == GL11.GL_NO_ERROR, "original hand GL error");
                                require(depth == (which == 3 ? 2 : 1), "source outer stack prefix differs");
                                // The source failure remains recorded above. This later fixture
                                // preparation supplies the next independent call's initial stack.
                                while (GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH) > 1)
                                    GL11.glPopMatrix();
                                ++index;
                            }
            require(index == 64 && network == 0, "profile count or network attempt");
            System.out.println("Original first-person hand: 64 outer rows; network=0; display=1");
            for (String library : links) System.out.println("NATIVE_LINK " + library);
        } finally { Display.destroy(); }
    }
}
