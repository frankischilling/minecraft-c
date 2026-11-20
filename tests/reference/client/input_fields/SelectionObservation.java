import java.nio.file.Path;
import java.security.Permission;

/** Observes the original InventoryPlayer wheel body without a Player or World. */
public final class SelectionObservation {
    static int network, nativeLinks;
    static final int[] SELECTED = {-28, -9, -1, 0, 1, 7, 8, 9, 28};
    static final int[] WHEELS = {Integer.MIN_VALUE, -120, -1, 0, 1, 120, Integer.MAX_VALUE};

    static void require(boolean condition) {
        if (!condition) throw new AssertionError("original selection invariant");
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
        require(Path.of(ix.class.getProtectionDomain().getCodeSource().getLocation().toURI())
                    .toRealPath().equals(jar));
        for (int selected : SELECTED) {
            for (int wheel : WHEELS) {
                ix inventory = new ix(null);
                inventory.c = selected;
                inventory.e = true;
                iz[] main = inventory.a, armor = inventory.b;
                inventory.b(wheel);
                require(inventory.a == main && inventory.b == armor && inventory.d == null &&
                        inventory.e && inventory.i() == null);
                for (iz stack : main) require(stack == null);
                for (iz stack : armor) require(stack == null);
                System.out.printf("SCROLL %d %d %d%n", selected, wheel, inventory.c);
            }
        }
        require(network == 0 && nativeLinks == 0);
        System.out.println("Original selection: 63 rows; network=0; native=0");
    }
}
