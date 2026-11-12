import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Original controller body with prepared actual App/GUI/RenderGlobal operands. */
public final class InterpolationObservation {
    static final int[][] CASES = {
        {0x00000000,0x3f800000,0x7fc54321}, {0x80000000,0x40000000,0xff800000},
        {0xbf800000,0x7fc12345,0x7fc54321}, {0xff800000,0x7fc12345,0x7fc54321},
        {0x00000001,0x00000000,0x3f800000}, {0x3f800000,0x00000000,0x00000000},
        {0x3f800000,0x00000000,0x3f000000}, {0x3f800000,0x00000000,0x3f800000},
        {0x3f800000,0x40000000,0xbf800000}, {0x3f800000,0x40000000,0x40000000},
        {0x7f800000,0x3f800000,0x3f000000}, {0x7fc12345,0x40000000,0x3f000000},
        {0xffc12345,0x40000000,0x3f000000}, {0x7fa12345,0x40000000,0x3f000000},
        {0x3f800000,0x7fc12345,0x3f000000}, {0x3f800000,0x40000000,0x7fc12345},
        {0x7f800000,0x7f800000,0x00000000}, {0x00000001,0x80000001,0x3f800000}};
    static int network, natives;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }
    static Object prepared(Class<?> type) throws Exception {
        Constructor<?> constructor = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type, Object.class.getDeclaredConstructor());
        constructor.setAccessible(true);
        return constructor.newInstance();
    }
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1, "expected pinned original JAR");
        Locale.setDefault(Locale.ROOT);
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
                ++natives; throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies file writes");
            }
        });
        kq app = (kq)prepared(kq.class);
        uq gui = (uq)prepared(uq.class);
        n global = (n)prepared(n.class);
        for (Class<?> type : new Class<?>[]{os.class, ob.class, uq.class, n.class, kq.class,
                net.minecraft.client.Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign class " + type);
        for (int mode = 0; mode < 4; ++mode) {
            for (int index = 0; index < CASES.length; ++index) {
                os controller = new os(mode == 0 ? null : app);
                app.v = mode == 1 ? null : gui;
                app.g = mode == 2 ? null : global;
                field(os.class,"f").setFloat(controller,Float.intBitsToFloat(CASES[index][0]));
                field(os.class,"g").setFloat(controller,Float.intBitsToFloat(CASES[index][1]));
                gui.b = Float.intBitsToFloat(0x3f2468ac);
                global.i = Float.intBitsToFloat(0xbf2468ac);
                boolean returned = false;
                try {
                    controller.a(Float.intBitsToFloat(CASES[index][2]));
                    returned = true;
                } catch (NullPointerException failure) {
                    StackTraceElement top = failure.getStackTrace()[0];
                    require(mode < 3 && top.getClassName().equals("os") &&
                        top.getMethodName().equals("a"), "unexpected null failure");
                }
                require(returned == (mode == 3), "source completion differs");
                require(Float.floatToRawIntBits(field(os.class,"f").getFloat(controller)) ==
                    CASES[index][0] &&
                    Float.floatToRawIntBits(field(os.class,"g").getFloat(controller)) ==
                    CASES[index][1], "controller source fields changed");
                System.out.printf("INTERPOLATE %d %d %d %08x %08x%n", mode, index,
                    returned ? 1 : 0, Float.floatToRawIntBits(gui.b), Float.floatToRawIntBits(global.i));
            }
        }
        require(network == 0 && natives == 0, "external work attempted");
        System.out.println("Original controller interpolation: 72 rows; network=0; native=0");
    }
}
