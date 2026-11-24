import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;

/** Finite histories through the unchanged original CU body. */
public final class MouseFilterObservation {
    static final int[][] INITIAL = {
        {0, 0, 0}, {0xc0000000, 0x40400000, 0xbf000000}, {1, 0x80000001, 0x80000000},
        {0x7f7fffff, 0xff7fffff, 0x3f800000}, {0x7fa12345, 0, 0}, {0, 0, 0xffc54321},
        {0x7f800000, 0x7f800000, 0x80000000}, {0, 0xff800000, 0x7fc23456}};
    static final int[][] STEPS = {
        {0x3f800000, 0x3d4ccccd}, {0x3f800000, 0x3d4ccccd}, {0xc0000000, 0x3d4ccccd},
        {0, 0x3d4ccccd}, {0x40a00000, 0x3f000000}, {0xc1200000, 0x3f800000},
        {0x7f7fffff, 0}, {1, 0x3d4ccccd}, {0x80000000, 0x40000000},
        {0xbf800000, 0xbf800000}, {0x7fc12345, 0x3f000000}, {0x7f800000, 0}};
    static int network, nativeLinks;

    static void require(boolean value) {
        if (!value) throw new AssertionError("original filter invariant");
    }

    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1);
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
                throw new SecurityException("observer denies writes");
            }
        });
        require(Path.of(cu.class.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar));
        Field[] fields = {cu.class.getDeclaredField("a"), cu.class.getDeclaredField("b"),
                          cu.class.getDeclaredField("c")};
        for (Field field : fields) field.setAccessible(true);
        for (int mode = 0; mode < INITIAL.length; ++mode) {
            cu filter = new cu();
            for (int n = 0; n < 3; ++n) fields[n].setFloat(filter, Float.intBitsToFloat(INITIAL[mode][n]));
            for (int step = 0; step < STEPS.length; ++step) {
                float out = filter.a(Float.intBitsToFloat(STEPS[step][0]),
                                     Float.intBitsToFloat(STEPS[step][1]));
                System.out.printf("FILTER %d %d %08x %08x %08x %08x%n", mode, step,
                                  Float.floatToRawIntBits(out),
                                  Float.floatToRawIntBits(fields[0].getFloat(filter)),
                                  Float.floatToRawIntBits(fields[1].getFloat(filter)),
                                  Float.floatToRawIntBits(fields[2].getFloat(filter)));
            }
        }
        require(network == 0 && nativeLinks == 0);
        System.out.println("Original mouse filter: 96 rows; network=0; native=0");
    }
}
