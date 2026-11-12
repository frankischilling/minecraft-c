import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Random;
import sun.reflect.ReflectionFactory;

/** Unchanged original c(F) dry returns and reached null-camera failures. */
public final class PrecipitationObservation {
    static final int[] VALUES = {0, 0x80000000, 1, 0x3c23d70a, 0x3dcccccd,
        0xbdcccccd, 0x7fc12345, 0x7f800000, 0xff800000, 0x3f800000};
    static Object prepared(Class<?> type) throws Exception {
        Constructor<?> constructor = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type, Object.class.getDeclaredConstructor());
        constructor.setAccessible(true);
        return constructor.newInstance();
    }
    static Field field(String name) throws Exception {
        Field field = px.class.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar = Path.of(args[0]).toRealPath();
        for (Class<?> type : new Class<?>[]{px.class, kq.class, fd.class})
            if (!Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar)) throw new AssertionError("foreign original class");
        px renderer = (px)prepared(px.class);
        kq app = (kq)prepared(kq.class);
        fd world = (fd)prepared(fd.class);
        field("j").set(renderer, app);
        app.f = world;
        Method method = px.class.getDeclaredMethod("c", float.class);
        method.setAccessible(true);
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                throw new AssertionError("unexpected network access");
            }
            @Override public void checkListen(int port) {
                throw new AssertionError("unexpected listener");
            }
            @Override public void checkLink(String library) {
                throw new AssertionError("unexpected native access");
            }
            @Override public void checkWrite(String file) {
                throw new AssertionError("unexpected file write");
            }
        });
        for (int partial = 0; partial < VALUES.length; ++partial)
            for (int strength = 0; strength < VALUES.length; ++strength) {
                world.i = 0.0f;
                world.j = Float.intBitsToFloat(VALUES[strength]);
                Random random = new Random(-7);
                field("J").set(renderer, random);
                String outcome = "OK";
                try { method.invoke(renderer, Float.intBitsToFloat(VALUES[partial])); }
                catch (InvocationTargetException exception) {
                    if (!(exception.getCause() instanceof NullPointerException)) throw exception;
                    outcome = "NULL";
                }
                System.out.printf("PRECIP_PREFIX %d %d %s %016x%n", partial, strength,
                                  outcome, random.nextLong());
            }
        System.out.println("Original precipitation: 100 prefix rows");
    }
}
