import java.lang.reflect.Field;
import java.nio.Buffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.StringJoiner;

/** Read-only observations around original calls; all receiver setup uses source methods. */
public final class NwOffsetObservation {
    static int profile = -1, step = -1, call = -1;
    static int network, nativeLinks, writes, execs;
    static nw receiver;

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(String name) throws Exception {
        Field f = nw.class.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static String bits(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    static String translations() throws Exception {
        return bits(field("s").getDouble(receiver)) + "," + bits(field("t").getDouble(receiver)) + "," + bits(field("u").getDouble(receiver));
    }
    static String otherState() throws Exception {
        Field[] fields = nw.class.getDeclaredFields();
        Arrays.sort(fields, Comparator.comparing(Field::getName));
        StringJoiner result = new StringJoiner(",");
        for (Field f : fields) {
            if (java.lang.reflect.Modifier.isStatic(f.getModifiers()) || "stu".contains(f.getName())) continue;
            f.setAccessible(true);
            Class<?> type = f.getType();
            Object value = f.get(receiver);
            if (type == double.class) result.add(f.getName() + "=" + bits((double)value));
            else if (type.isPrimitive()) result.add(f.getName() + "=" + value);
            else if (value instanceof Buffer) {
                Buffer b = (Buffer)value;
                result.add(f.getName() + "=" + b.position() + "/" + b.limit() + "/" + b.capacity());
            } else if (value instanceof int[]) {
                int[] words = (int[])value;
                result.add(f.getName() + "=" + words.length + "/" + Arrays.hashCode(words));
            } else result.add(f.getName() + "=" + (value == null ? "null" : value.getClass().getName()));
        }
        return result.toString();
    }
    static double d(String value) { return Double.longBitsToDouble(Long.parseUnsignedLong(value, 16)); }
    static float f(String value) { return Float.intBitsToFloat(Integer.parseUnsignedInt(value, 16)); }
    static void denial(Runnable action) {
        try { action.run(); throw new AssertionError("guard did not deny"); }
        catch (SecurityException expected) { }
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 3, "jar, library JAR and authored profiles required");
        Path jar = Path.of(args[0]).toRealPath(), library = Path.of(args[1]).toRealPath();
        List<String> inputs = Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) { }
            public void checkConnect(String host, int port) { ++network; throw new SecurityException("connect denied"); }
            public void checkListen(int port) { ++network; throw new SecurityException("listen denied"); }
            public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native denied"); }
            public void checkWrite(String name) { ++writes; throw new SecurityException("write denied"); }
            public void checkExec(String name) { ++execs; throw new SecurityException("exec denied"); }
        });
        for (Class<?> type : new Class<?>[]{nw.class, ge.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "original source origin");
        require(Path.of(org.lwjgl.BufferUtils.class.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(library), "pinned BufferUtils origin");
        receiver = nw.a;
        require(receiver != null && field("b").getBoolean(null) && !field("c").getBoolean(null) && !field("x").getBoolean(receiver), "actual CPU constructor defaults");
        String initial = otherState();
        System.out.println("NW_INITIAL " + translations() + " " + initial);
        int rows = 0;
        for (String line : inputs) {
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] columns = line.split("\\t");
            require(columns.length == 7, "profile schema");
            profile = Integer.parseInt(columns[0]);
            boolean building = columns[2].equals("1");
            receiver.b(d(columns[3]), d(columns[4]), d(columns[5]));
            if (building) receiver.b();
            require(field("w").getBoolean(receiver) == building, "source begin state");
            Object backing = field("g").get(receiver), bytes = field("d").get(receiver), ints = field("e").get(receiver), floats = field("f").get(receiver);
            String[] calls = columns[6].split(";");
            for (step = 0; step < calls.length; ++step) {
                String[] delta = calls[step].split(","); require(delta.length == 3, "delta schema");
                float dx = f(delta[0]), dy = f(delta[1]), dz = f(delta[2]);
                String before = translations(), other = otherState();
                ++call;
                receiver.c(dx, dy, dz);
                String after = translations();
                require(other.equals(otherState()), "unrelated source state changed");
                require(backing == field("g").get(receiver) && bytes == field("d").get(receiver) && ints == field("e").get(receiver) && floats == field("f").get(receiver), "canonical buffer identities changed");
                System.out.println("NW_OFFSET\t" + rows++ + "\t" + profile + "\t" + columns[1] + "\t" + columns[2] + "\t" + step + "\t" + before + "\t" + calls[step] + "\t" + after + "\t" + other);
            }
            // A genuine zero-vertex source draw closes a begun batch without GL work.
            if (building) receiver.a();
            require(!field("w").getBoolean(receiver), "source batch closure");
        }
        profile = step = -1;
        require(network == 0 && nativeLinks == 0 && writes == 0 && execs == 0, "source external attempts");
        System.out.println("NW_GUARD source network=0 native=0 writes=0 execs=0 rows=" + rows);
        denial(() -> System.getSecurityManager().checkConnect("127.0.0.1", 25565));
        denial(() -> System.getSecurityManager().checkListen(25565));
        denial(() -> System.getSecurityManager().checkLink("nw-offset-denial-control"));
        denial(() -> System.getSecurityManager().checkWrite("nw-offset-denial-control"));
        denial(() -> System.getSecurityManager().checkExec("nw-offset-denial-control"));
        require(network == 2 && nativeLinks == 1 && writes == 1 && execs == 1, "five real denial controls");
        System.out.println("NW_GUARD controls network=2 native=1 writes=1 execs=1");
        System.out.println("NW_COMPLETE rows=" + rows + " profiles=" + inputs.size() + " canonical=1 raw-allocations=0 reflection-writes=0");
    }
}
