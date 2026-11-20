import java.lang.reflect.Field;
import java.security.Permission;
import java.util.Locale;

/** Unchanged BK metadata colors and inherited UU.g on a real Leaf singleton. */
public final class LeafInventoryObservation {
    static int network, nativeLinks;
    static String bounds(uu block) {
        return String.format(Locale.ROOT, "%016x %016x %016x %016x %016x %016x",
            Double.doubleToRawLongBits(block.bs), Double.doubleToRawLongBits(block.bt),
            Double.doubleToRawLongBits(block.bu), Double.doubleToRawLongBits(block.bv),
            Double.doubleToRawLongBits(block.bw), Double.doubleToRawLongBits(block.bx));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listen");}
            public void checkLink(String library) {++nativeLinks;throw new SecurityException("native");}
        });
        uu leaf=uu.L;
        String cube=bounds(leaf);
        for(int metadata=0;metadata<16;++metadata) {
            int color=leaf.b(metadata);
            leaf.g();
            if(!bounds(leaf).equals(cube)) throw new AssertionError("inherited shape mutated bounds");
            System.out.println("LEAF_INVENTORY_COLOR "+metadata+" "+
                String.format(Locale.ROOT,"%08x",color)+" "+bounds(leaf));
        }
        leaf.a(.125f,.25f,.375f,.75f,.875f,1f);
        String authored=bounds(leaf);
        leaf.g();
        if(!bounds(leaf).equals(authored)) throw new AssertionError("shape rewrote authored bounds");
        System.out.println("LEAF_INVENTORY_SHAPE "+bounds(leaf));
        Field array=jh.class.getDeclaredField("a"); array.setAccessible(true);
        if(((int[])array.get(null)).length!=65536 || network!=0 || nativeLinks!=0)
            throw new AssertionError("actual class initializer or guard");
        System.out.println("LEAF_INVENTORY_GUARD 0 0 65536");
    }
}
