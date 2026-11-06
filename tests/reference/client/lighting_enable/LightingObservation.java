import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.FloatBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.PixelFormat;

/** Observe unchanged U.b state and real GL parameters, without drawing geometry. */
public final class LightingObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String floats(FloatBuffer values, int count) {
        FloatBuffer view = values.duplicate(); view.clear();
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < count; ++i) {
            if (i != 0) text.append(',');
            text.append(String.format("%08x", Float.floatToRawIntBits(view.get(i))));
        }
        return text.toString();
    }
    static String query(int parameter, int count) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(parameter, values); return floats(values, count);
    }
    static String light(int index, int parameter) {
        FloatBuffer values = BufferUtils.createFloatBuffer(16);
        GL11.glGetLight(GL11.GL_LIGHT0 + index, parameter, values); return floats(values, 4);
    }
    static void matrix(int mode, float[] values) {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(16); buffer.put(values).flip();
        GL11.glMatrixMode(mode); GL11.glLoadMatrix(buffer);
    }
    static float[] identity() {
        return new float[]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    }
    static Throwable failedInvoke(Method method) throws Exception {
        try { method.invoke(null); }
        catch (InvocationTargetException error) {
            Throwable cause = error.getCause(); boolean reached = false, context = false;
            for (StackTraceElement frame : cause.getStackTrace()) {
                reached |= frame.getClassName().equals("u") && frame.getMethodName().equals("b");
                context |= frame.getClassName().equals("org.lwjgl.opengl.GLContext") &&
                           frame.getMethodName().equals("getCapabilities");
            }
            require(reached && context && cause.getClass() == RuntimeException.class &&
                    "No OpenGL context found in the current thread.".equals(cause.getMessage()),
                    "actual context-absence failure reached unchanged U.b body"); return cause;
        }
        throw new AssertionError("U.b must fail without an actual current context");
    }
    static void coldFailure(String name, Method enable) throws Exception {
        require(!Display.isCreated(), "no Display created in cold failure profile");
        Throwable first = failedInvoke(enable);
        FloatBuffer firstBuffer = (FloatBuffer)field(u.class, "a").get(null);
        require(firstBuffer != null && firstBuffer.isDirect(), "U initialized its real direct buffer");
        Throwable second = failedInvoke(enable);
        FloatBuffer buffer = (FloatBuffer)field(u.class, "a").get(null);
        require(firstBuffer == buffer && buffer.position() == 0 && buffer.limit() == 16 &&
                buffer.capacity() == 16, "same untouched U buffer survives both first-child failures");
        System.out.println("LIGHT_FAIL " + name + " 1 " + buffer.position() + " " +
                           buffer.limit() + " " + buffer.capacity() + " " + floats(buffer, 16));
        System.out.println("LIGHT_EXCEPTION " + name + " " + first.getClass().getName() + " " +
                           second.getClass().getName());
    }
    static String vector(Object value) throws Exception {
        return String.format("%016x %016x %016x",
            Double.doubleToRawLongBits(field(bt.class, "a").getDouble(value)),
            Double.doubleToRawLongBits(field(bt.class, "b").getDouble(value)),
            Double.doubleToRawLongBits(field(bt.class, "c").getDouble(value)));
    }
    @SuppressWarnings({"removal", "unchecked"})
    public static void main(String[] args) throws Exception {
        require(args.length == 4, "JAR/native/input/profile arguments");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        String[] input = Files.readAllLines(Path.of(args[2])).get(Integer.parseInt(args[3])).split("\\t", -1);
        String name = input[0]; boolean device = input[1].equals("1"), seeded = input[2].equals("1");
        boolean transformed = input[3].equals("1"), replace = input[4].equals("1");
        int calls = Integer.parseInt(input[5]);
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }
        });
        Class<?> math = Class.forName("in", false, LightingObservation.class.getClassLoader());
        for (Class<?> type : new Class<?>[]{u.class, ge.class, bt.class, math})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "original game archive authority");
        Method enable = u.class.getDeclaredMethod("b");
        if (!device) {
            coldFailure(name, enable);
        } else {
            Display.setDisplayMode(new DisplayMode(64, 64)); Display.setTitle("Original standard lighting state");
            try {
                Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
                System.out.println("LIGHT_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                    GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
                for (int channel : new int[]{GL11.GL_RED_BITS, GL11.GL_GREEN_BITS, GL11.GL_BLUE_BITS, GL11.GL_ALPHA_BITS})
                    require(GL11.glGetInteger(channel) == 8, "declared RGBA8 profile");
                System.out.println("LIGHT_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                                   GL11.glGetInteger(GL11.GL_STENCIL_BITS));
                bt.a(); // Declared pool preparation; U and MathHelper are not eagerly initialized.
                if (seeded) { bt.b(7, 8, 9); bt.b(-3, 5, -7); }
                FloatBuffer beforeBuffer = null;
                if (seeded) {
                    beforeBuffer = (FloatBuffer)field(u.class, "a").get(null);
                    FloatBuffer view = beforeBuffer.duplicate(); view.clear();
                    for (int i = 0; i < 16; ++i) view.put(i, Float.intBitsToFloat(0x3f000000 + i * 0x10000));
                    beforeBuffer.limit(11); beforeBuffer.position(5);
                }
                matrix(GL11.GL_MODELVIEW, transformed ?
                       new float[]{2,0,0,0, 0,3,0,0, 0,0,0.5f,0, 7,-9,11,1} : identity());
                matrix(GL11.GL_PROJECTION, transformed ?
                       new float[]{3,0,0,0, 0,5,0,0, 0,0,7,0, 0,0,0,1} : identity());
                GL11.glMatrixMode(transformed ? GL11.GL_PROJECTION : GL11.GL_MODELVIEW);
                for (int call = 0; call < calls; ++call) {
                    List<?> pool = (List<?>)field(bt.class, "d").get(null);
                    int cursor = field(bt.class, "e").getInt(null), size = pool.size();
                    List<Object> prefix = new ArrayList<>(pool);
                    List<String> prefixValues = new ArrayList<>();
                    for (Object value : prefix) prefixValues.add(vector(value));
                    String modelBefore = query(GL11.GL_MODELVIEW_MATRIX, 16);
                    String projectionBefore = query(GL11.GL_PROJECTION_MATRIX, 16);
                    enable.invoke(null);
                    FloatBuffer buffer = (FloatBuffer)field(u.class, "a").get(null);
                    require(buffer.position() == 0 && buffer.limit() == 4 && buffer.capacity() == 16,
                            "same U buffer clear/put/flip result");
                    FloatBuffer values = buffer.duplicate(); values.clear();
                    for (int i = 4; i < 16; ++i)
                        require(Float.floatToRawIntBits(values.get(i)) == (seeded ? 0x3f000000 + i * 0x10000 : 0),
                                "source fills retain the actual suffix");
                    List<?> afterPool = (List<?>)field(bt.class, "d").get(null);
                    int afterCursor = field(bt.class, "e").getInt(null);
                    boolean prefixSame = pool == afterPool && afterCursor == cursor + 4 && afterPool.size() == size + 4;
                    for (int i = 0; i < prefix.size(); ++i)
                        prefixSame &= prefix.get(i) == afterPool.get(i) && prefixValues.get(i).equals(vector(afterPool.get(i)));
                    require(prefixSame, "four actual pooled vectors and unchanged earlier objects");
                    if (replace) {
                        matrix(GL11.GL_MODELVIEW, identity()); GL11.glMatrixMode(GL11.GL_PROJECTION);
                    }
                    String tag = name + " " + call;
                    System.out.println("LIGHT_STATE " + tag + " " + (GL11.glIsEnabled(GL11.GL_LIGHTING) ? 1 : 0) + " " +
                        (GL11.glIsEnabled(GL11.GL_LIGHT0) ? 1 : 0) + " " + (GL11.glIsEnabled(GL11.GL_LIGHT1) ? 1 : 0) + " " +
                        (GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0) + " " +
                        GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE) + " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER) + " " +
                        GL11.glGetInteger(GL11.GL_SHADE_MODEL) + " " + GL11.glGetInteger(GL11.GL_MATRIX_MODE));
                    System.out.println("LIGHT_BUFFER " + tag + " " + (beforeBuffer == null ? -1 : beforeBuffer == buffer ? 1 : 0) + " " +
                        buffer.position() + " " + buffer.limit() + " " + buffer.capacity() + " " + floats(buffer, 16));
                    System.out.println("LIGHT_POOL " + tag + " " + cursor + " " + afterCursor + " " + size + " " +
                        afterPool.size() + " " + (pool == afterPool ? 1 : 0) + " " + (prefixSame ? 1 : 0));
                    for (int i = 0; i < 4; ++i)
                        System.out.println("LIGHT_VECTOR " + tag + " " + i + " " + vector(afterPool.get(cursor + i)));
                    for (int index = 0; index < 2; ++index)
                        for (int parameter : new int[]{GL11.GL_POSITION, GL11.GL_DIFFUSE, GL11.GL_AMBIENT, GL11.GL_SPECULAR})
                            System.out.println("LIGHT_PARAMETER " + tag + " " + index + " " + parameter + " " + light(index, parameter));
                    System.out.println("LIGHT_MODEL " + tag + " " + query(GL11.GL_LIGHT_MODEL_AMBIENT, 4));
                    System.out.println("LIGHT_MATRICES " + tag + " " + modelBefore + " " + query(GL11.GL_MODELVIEW_MATRIX, 16) + " " +
                        projectionBefore + " " + query(GL11.GL_PROJECTION_MATRIX, 16));
                    require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual GL parameter inspection completed");
                    beforeBuffer = buffer;
                }
            } finally { Display.destroy(); }
        }
        require(network == 0, "no network/listener attempts");
        System.out.println("Original lighting: " + name + "; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
