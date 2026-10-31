import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Prepared App operand; genuine OS/YO construction and unchanged OS bodies. */
public final class ControllerTickObservation {
    static int networkAttempts, nativeAttempts;
    static final int[] INPUTS = {
        0x00000000, 0x80000000, 0x3f800001, 0xbf800001, 0x00000001,
        0x7f800000, 0xff800000, 0x7fc12345, 0xffc54321, 0x7fa12345
    };

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }

    static int bits(os controller, String name) throws Exception {
        return Float.floatToRawIntBits(field(os.class, name).getFloat(controller));
    }

    static String unchanged(os controller, boolean reset) throws Exception {
        StringBuilder value = new StringBuilder();
        for (Class<?> type = os.class; type != Object.class; type = type.getSuperclass()) {
            for (Field f : type.getDeclaredFields()) {
                if (Modifier.isStatic(f.getModifiers()) ||
                    (type == os.class && (reset ? f.getName().equals("f") || f.getName().equals("i")
                                               : f.getName().equals("g")))) continue;
                f.setAccessible(true);
                Object part = f.get(controller);
                value.append(type.getName()).append(':').append(f.getName()).append('=');
                if (f.getType() == float.class)
                    value.append(Integer.toHexString(Float.floatToRawIntBits((Float)part)));
                else if (f.getType().isPrimitive()) value.append(part);
                else value.append(part == null ? 0 : System.identityHashCode(part));
                value.append(';');
            }
        }
        return value.toString();
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1, "expected original JAR");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(arguments[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++networkAttempts;
                throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++networkAttempts;
                throw new SecurityException("observer denies listeners");
            }
            @Override public void checkLink(String library) {
                ++nativeAttempts;
                throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies file writes");
            }
        });
        Constructor<?> prepared = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(kq.class, Object.class.getDeclaredConstructor());
        prepared.setAccessible(true);
        kq app = (kq)prepared.newInstance();
        yo manager = new yo();
        require(!field(yo.class, "g").getBoolean(null), "actual uninitialized YO namespace");
        for (Class<?> type : new Class<?>[]{os.class, ob.class, yo.class, hr.class, kq.class,
                                            net.minecraft.client.Minecraft.class}) {
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "foreign original class " + type);
        }
        for (int mode = 0; mode < 3; ++mode) {
            for (int n = 0; n < INPUTS.length; ++n) {
                app.B = mode == 1 ? null : manager;
                os controller = new os(mode == 0 ? null : app);
                controller.b = true;
                field(os.class, "c").setInt(controller, Integer.MIN_VALUE);
                field(os.class, "d").setInt(controller, 37);
                field(os.class, "e").setInt(controller, Integer.MAX_VALUE);
                field(os.class, "i").setInt(controller, Integer.MIN_VALUE + 7);
                field(os.class, "f").setFloat(controller, Float.intBitsToFloat(INPUTS[n]));
                field(os.class, "g").setFloat(controller, Float.intBitsToFloat(0x7fc23456));
                field(os.class, "h").setFloat(controller, Float.intBitsToFloat(0xffa34567));
                String before = unchanged(controller, false);
                boolean returned = false;
                try {
                    controller.c();
                    returned = true;
                } catch (NullPointerException failure) {
                    StackTraceElement top = failure.getStackTrace()[0];
                    require(mode < 2 && top.getClassName().equals("os") &&
                            top.getMethodName().equals("c"), "actual null operand failure");
                }
                require(returned == (mode == 2), "source return differs");
                require(before.equals(unchanged(controller, false)), "tick changed other fields");
                require(bits(controller, "g") == INPUTS[n], "g does not preserve f bits");
                System.out.printf("CTICK %d %d %08x %08x %08x %d%n", mode, n,
                    bits(controller, "f"), bits(controller, "g"), bits(controller, "h"),
                    field(os.class, "i").getInt(controller));
                before = unchanged(controller, true);
                controller.a();
                require(before.equals(unchanged(controller, true)), "reset changed other fields");
                require(bits(controller, "f") == 0 && field(os.class, "i").getInt(controller) == 0,
                        "actual reset fields differ");
                System.out.printf("CRESET %d %d %08x %08x %08x %d%n", mode, n,
                    bits(controller, "f"), bits(controller, "g"), bits(controller, "h"),
                    field(os.class, "i").getInt(controller));
            }
        }
        require(networkAttempts == 0 && nativeAttempts == 0, "unexpected external work");
        System.out.println("Original controller tick/reset: 60 rows; network=0; native=0");
    }
}
