import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.IntBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import org.lwjgl.BufferUtils;

/** Original private query-poll method: closed CPU prefixes only. */
public final class PollObservation {
    static int network, nativeLinks;
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name); result.setAccessible(true); return result;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> u = Class.forName("sun.misc.Unsafe");
        return u.getMethod("allocateInstance", Class.class)
                .invoke(field(u, "theUnsafe").get(null), type);
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2, "JAR and declared TSV inputs");
        Path jar = Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkLink(String library) { ++nativeLinks; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
        });
        for (Class<?> type : new Class<?>[]{n.class, dk.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar), "original archive authority");
        Method method = n.class.getDeclaredMethod("a", int.class, int.class);
        method.setAccessible(true);
        int count = 0;
        for (String line : Files.readAllLines(Path.of(args[1]))) {
            String[] input = line.split("\t", -1);
            n renderer = (n)raw(n.class);
            dk[] actors = new dk[3];
            for (int i = 0; i < actors.length; ++i) {
                actors[i] = (dk)raw(dk.class);
                actors[i].x = i != 0; actors[i].y = i == 1;
            }
            dk[] sorted = null;
            if (!input[4].equals("N")) {
                String[] ids = input[4].isEmpty() ? new String[0] : input[4].split(",");
                sorted = new dk[ids.length];
                for (int i = 0; i < ids.length; ++i) {
                    int id = Integer.parseInt(ids[i]); sorted[i] = id == 0 ? null : actors[id - 1];
                }
            }
            field(n.class, "n").set(renderer, sorted);
            IntBuffer buffer = null;
            if (input[3].equals("1")) {
                buffer = BufferUtils.createIntBuffer(2); buffer.position(1); buffer.limit(1);
            }
            field(n.class, "c").set(renderer, buffer);
            String result = "OK";
            try { method.invoke(renderer, Integer.parseInt(input[1]), Integer.parseInt(input[2])); }
            catch (InvocationTargetException wrapper) {
                Throwable e = wrapper.getCause();
                if (e instanceof NullPointerException) result = "NULL";
                else if (e instanceof ArrayIndexOutOfBoundsException) result = "INDEX";
                else throw wrapper;
            }
            StringBuilder flags = new StringBuilder();
            for (dk d : actors) {
                if (flags.length() != 0) flags.append(',');
                flags.append(d.x ? 1 : 0).append(d.y ? 1 : 0);
            }
            System.out.println("POLL " + input[0] + " " + result + " " + flags + " " +
                (buffer == null ? -1 : buffer.position()) + " " + (buffer == null ? -1 : buffer.limit()));
            ++count;
        }
        require(count == 14 && network == 0 && nativeLinks == 0, "complete guarded CPU cohort");
        System.out.println("Original poll: 14 closed CPU rows; network=0; native=0");
    }
}
