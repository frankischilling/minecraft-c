import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;

/** Calls unchanged original brightness on an explicitly prepared World. */
public final class BrightnessObservation {
    static final int[] VALUES = {0, 0x80000000, 0x3f800001, 0xbf800001, 1,
                                 0x7f800000, 0xff800000, 0x7fc12345, 0xffc54321, 0x7fa12345};
    static int network, nativeLinks;

    static void require(boolean value) {
        if (!value) throw new AssertionError("original brightness invariant");
    }

    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        Field field = unsafe.getDeclaredField("theUnsafe");
        field.setAccessible(true);
        return unsafe.getMethod("allocateInstance", Class.class).invoke(field.get(null), type);
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1);
        Path jar = Path.of(arguments[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network;
                throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network;
                throw new SecurityException("observer denies listeners");
            }
            @Override public void checkLink(String library) {
                ++nativeLinks;
                throw new SecurityException("observer denies native links");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
        });
        require(Path.of(fd.class.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar));
        require(Path.of(xa.class.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar));
        fd world = (fd)raw(fd.class); // No source World constructor qualification.
        xa dimension = new rh();
        Field dimensionField = fd.class.getDeclaredField("t");
        dimensionField.setAccessible(true);
        dimensionField.set(world, dimension);
        float[] captured = dimension.f;
        require(captured != null && captured.length == 16);
        for (int index = 0; index < VALUES.length; ++index) {
            captured[15] = Float.intBitsToFloat(VALUES[index]);
            int returned = Float.floatToRawIntBits(world.c(32000000, Integer.MIN_VALUE, 0));
            require(world.t == dimension && dimension.f == captured &&
                    Float.floatToRawIntBits(captured[15]) == VALUES[index]);
            System.out.printf("BRIGHT %d %08x%n", index, returned);
        }
        dimensionField.set(world, null);
        try {
            world.c(32000000, Integer.MIN_VALUE, 0);
            throw new AssertionError("missing null Dimension exception");
        } catch (NullPointerException failure) {
            System.out.println("BRIGHT_NULL dimension " + failure.getClass().getName());
        }
        dimensionField.set(world, dimension);
        dimension.f = null;
        try {
            world.c(32000000, Integer.MIN_VALUE, 0);
            throw new AssertionError("missing null array exception");
        } catch (NullPointerException failure) {
            System.out.println("BRIGHT_NULL table " + failure.getClass().getName());
        }
        require(world.t == dimension && dimension.f == null && network == 0 && nativeLinks == 0);
        System.out.println("Original brightness: 12 rows; network=0; native=0");
    }
}
