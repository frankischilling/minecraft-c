import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;
import java.util.Locale;

/** Observe unchanged NW normal packing and its real expanded vertex array. */
public final class NormalObservation {
    static int network, links;
    static Field field(String name) throws Exception {
        Field value = nw.class.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static void state(String tag, nw value) throws Exception {
        System.out.printf("NW_STATE %s %d %d %08x %d %d %d %d %d %d%n", tag,
            field("w").getBoolean(value) ? 1 : 0, field("n").getBoolean(value) ? 1 : 0,
            field("v").getInt(value), field("h").getInt(value), field("o").getInt(value),
            field("p").getInt(value), field("l").getBoolean(value) ? 1 : 0,
            field("m").getBoolean(value) ? 1 : 0, field("q").getBoolean(value) ? 1 : 0);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "archive and authored input arguments");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath();
        List<String> inputs = Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) { ++links; throw new SecurityException(); }
        });
        for (Class<?> type : new Class<?>[]{nw.class, ge.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "unchanged original archive authority");
        nw value = nw.a;
        state("cold", value);
        value.b(1.0f, 0.0f, 0.0f); state("outside0", value);
        value.b(-1.0f, 0.5f, -0.5f); state("outside1", value);
        PrintStream original = System.out;
        boolean nullFailed = false;
        try {
            System.setOut(null);
            try { value.b(0.0f, 0.0f, 0.0f); }
            catch (NullPointerException expected) {
                for (StackTraceElement frame : expected.getStackTrace())
                    nullFailed |= frame.getClassName().equals("nw") && frame.getMethodName().equals("b");
            }
        } finally { System.setOut(original); }
        require(nullFailed, "null System.out fails inside unchanged NW.b");
        state("null_out", value);
        value.b(); state("begin", value);
        for (String row : inputs) {
            String[] cells = row.split("\\t", -1);
            require(cells.length == 4, "authored float-bit input width");
            float x = Float.intBitsToFloat(Integer.parseUnsignedInt(cells[1], 16));
            float y = Float.intBitsToFloat(Integer.parseUnsignedInt(cells[2], 16));
            float z = Float.intBitsToFloat(Integer.parseUnsignedInt(cells[3], 16));
            value.b(x, y, z); state(cells[0], value);
        }
        boolean beginFailed = false;
        try { value.b(); }
        catch (IllegalStateException expected) {
            require(expected.getMessage().equals("Already tesselating!"), "actual begin failure");
            beginFailed = true;
        }
        require(beginFailed, "repeated begin must fail");
        state("begin_failure", value);
        int[] array = (int[])field("g").get(value);
        array[3 * 8 + 6] = 0x11223344; array[4 * 8 + 6] = 0x55667788;
        float[][] normal = {{0,1,0}, {1,0,0}, {0,0,-1}, {-1,0,0}};
        double[][] position = {{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}};
        for (int i = 0; i < 4; ++i) {
            value.b(normal[i][0], normal[i][1], normal[i][2]);
            value.a(position[i][0], position[i][1], position[i][2]);
        }
        require(array == field("g").get(value), "same actual source array");
        state("quad", value);
        for (int i = 0; i < 6; ++i) {
            System.out.printf("NW_VERTEX quad %d ", i);
            for (int word = 0; word < 8; ++word)
                System.out.printf("%s%08x", word == 0 ? "" : ",", array[i * 8 + word]);
            System.out.println();
        }
        require(network == 0 && links == 0, "no network, listener or native-library attempts");
        System.out.println("Original NW normals: complete; network=0; links=0");
    }
}
