import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import java.util.Random;
import sun.reflect.ReflectionFactory;

/** Finite unchanged rain body on explicitly prepared source operands. */
public final class RainObservation {
    static final int[] INPUTS = {0, 0x80000000, 1, 0x3c23d70a, 0x3dcccccd,
        0xbdcccccd, 0x7fc12345, 0x7f800000, 0xff800000, 0x3f800000};
    static int network, nativeLinks;
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
                ++nativeLinks; throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies file writes");
            }
        });
        for (Class<?> type : new Class<?>[]{px.class, kq.class, fd.class, kv.class, dc.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        px renderer = (px)prepared(px.class);
        kq app = (kq)prepared(kq.class);
        fd world = (fd)prepared(fd.class);
        kv settings = (kv)prepared(kv.class);
        dc camera = (dc)prepared(dc.class);
        camera.aM = -1.25; camera.aN = 64.5; camera.aO = 2.75;
        app.f = world; app.z = settings; app.i = camera;
        field(px.class, "j").set(renderer, app);
        field(px.class, "l").setInt(renderer, -31);
        Method rain = px.class.getDeclaredMethod("c"); rain.setAccessible(true);
        for (int fancy = 0; fancy < 2; ++fancy) {
            settings.j = fancy != 0;
            for (int index = 0; index < INPUTS.length; ++index) {
                world.i = 0.0f; world.j = Float.intBitsToFloat(INPUTS[index]);
                Random random = new Random(-7L);
                field(px.class, "J").set(renderer, random);
                field(px.class, "K").setInt(renderer, 17);
                boolean failed = false;
                try { rain.invoke(renderer); }
                catch (InvocationTargetException exception) {
                    require(exception.getCause() instanceof NullPointerException,
                        "unexpected source failure " + exception.getCause());
                    failed = true;
                }
                float strength = world.g(1.0f);
                if (!settings.j) strength /= 2.0f;
                boolean reseeded = strength != 0.0f;
                require(field(px.class, "K").getInt(renderer) == 17, "unexpected K mutation");
                System.out.printf("RAIN %d %d %s %d %016x %d%n", fancy, index,
                    failed ? "FAIL" : "OK", reseeded ? 1 : 0, random.nextLong(), 17);
            }
        }
        require(network == 0 && nativeLinks == 0, "external operation attempted");
        System.out.println("Original rain body: 20 rows; network=0; native=0");
    }
}
