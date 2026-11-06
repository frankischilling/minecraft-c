import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Prepared genuine receivers; unchanged original equip and current-Stack bodies. */
public final class EquipObservation {
    static final int[] INPUTS = {0x00000000, 0x80000000, 0x3dcccccd, 0x3f000000,
        0x3f19999a, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000,
        0x7fc12345, 0xffc54321, 0x00000001};
    static final int[][] CASES = {{0,0,0,-1}, {1,0,0,-1}, {1,1,0,0}, {2,1,0,8},
        {4,1,0,0}, {3,1,0,0}, {1,1,1,0}, {0,1,-1,0}, {0,0,9,-1}, {2,1,0,0}};
    static int network, natives;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(String name) throws Exception {
        Field value = ra.class.getDeclaredField(name);
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
        dc player = (dc)prepared(dc.class);
        ix inventory = new ix(null);
        player.c = inventory;
        app.h = player;
        iz[] stacks = {null, new iz(1,1,3), new iz(1,7,3), new iz(1,1,4), new iz(2,1,3)};
        for (Class<?> type : new Class<?>[]{ra.class, ix.class, iz.class, dc.class, kq.class,
                net.minecraft.client.Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign class " + type);
        for (int mode = 0; mode < CASES.length; ++mode) {
            for (int n = 0; n < INPUTS.length; ++n) {
                ra renderer = (ra)prepared(ra.class);
                field("a").set(renderer, app);
                field("b").set(renderer, stacks[CASES[mode][1]]);
                field("c").setFloat(renderer, Float.intBitsToFloat(INPUTS[n]));
                field("d").setFloat(renderer, Float.intBitsToFloat(0x7fc23456));
                field("g").setInt(renderer, CASES[mode][3]);
                for (int slot = 0; slot < inventory.a.length; ++slot) inventory.a[slot] = null;
                inventory.c = CASES[mode][2];
                if (inventory.c >= 0 && inventory.c < 9)
                    inventory.a[inventory.c] = stacks[CASES[mode][0]];
                renderer.a();
                int id = -1;
                for (int candidate = 0; candidate < stacks.length; ++candidate)
                    if (field("b").get(renderer) == stacks[candidate]) id = candidate;
                require(id >= 0 && field("a").get(renderer) == app &&
                    field("e").get(renderer) == null && field("f").get(renderer) == null,
                    "unexpected other RA fields");
                require(Float.floatToRawIntBits(field("d").getFloat(renderer)) == INPUTS[n],
                    "previous equip fraction bits changed");
                System.out.printf("EQUIP %d %d %08x %08x %d %d%n", mode, n,
                    Float.floatToRawIntBits(field("d").getFloat(renderer)),
                    Float.floatToRawIntBits(field("c").getFloat(renderer)),
                    field("g").getInt(renderer), id);
            }
        }
        require(network == 0 && natives == 0, "external work attempted");
        System.out.println("Original held-item tick: 120 rows; network=0; native=0");
    }
}
