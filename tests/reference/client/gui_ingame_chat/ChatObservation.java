import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Unchanged UQ/SW/NH components, declared prepared App/SJ width inputs; no draw. */
public final class ChatObservation {
    static int network, natives, writes;
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static Object prepared(Class<?> type) throws Exception {
        Constructor<?> constructor = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type, Object.class.getDeclaredConstructor());
        constructor.setAccessible(true); return constructor.newInstance();
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String repeat(int length) {
        char[] value = new char[length]; Arrays.fill(value, 'W'); return new String(value);
    }
    static String hex(String value) {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < value.length(); ++i)
            result.append(String.format(Locale.ROOT, "%04x", (int)value.charAt(i)));
        return result.length() == 0 ? "-" : result.toString();
    }
    @SuppressWarnings("unchecked")
    static List<sw> lines(uq owner) throws Exception {
        return (List<sw>)field(uq.class, "e").get(owner);
    }
    static void row(uq owner, int profile) throws Exception {
        List<sw> list = lines(owner);
        System.out.printf(Locale.ROOT, "CHAT_CASE %d %d %d %d %d %d%n", profile, list.size(),
            ((Object[])field(ArrayList.class, "elementData").get(list)).length,
            field(AbstractList.class, "modCount").getInt(list), field(uq.class, "h").getInt(owner),
            field(uq.class, "j").getInt(owner));
        for (int i = 0; i < list.size(); ++i) {
            sw message = list.get(i);
            System.out.printf(Locale.ROOT, "CHAT_LINE %d %d %d %s %s%n", profile, i, message.b,
                message.a == null ? "null" : "text", message.a == null ? "-" : hex(message.a));
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 1, "pinned original JAR required");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(arguments[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("network denied");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("listener denied");
            }
            @Override public void checkLink(String library) {
                ++natives; throw new SecurityException("native link denied");
            }
            @Override public void checkWrite(String file) {
                ++writes; throw new SecurityException("file write denied");
            }
        });
        kq app = (kq)prepared(kq.class);
        sj font = (sj)prepared(sj.class);
        int[] widths = new int[256]; Arrays.fill(widths, 7); widths[32] = 4;
        field(sj.class, "b").set(font, widths);
        app.q = font;
        uq owner = new uq(app);
        for (Class<?> type : new Class<?>[]{uq.class, sw.class, sj.class, nh.class, fp.class,
                ub.class, kq.class, net.minecraft.client.Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign class " + type);
        require(font.a("W") == 7, "declared prepared Font width");
        for (int profile = 0; profile < 12; ++profile) {
            switch (profile) {
                case 0: owner.a((String)null); break;
                case 1: owner.a(""); break;
                case 2: owner.a("Bed"); break;
                case 3: owner.a(repeat(45)); break;
                case 4: owner.a(repeat(46)); break;
                case 5: owner.a(repeat(92)); break;
                case 6: owner.a("\u00a7c" + repeat(46)); break;
                case 7: owner.a("A\nB"); break;
                case 8: owner.a(new String(new char[]{'A', 0, 'B', '\ud800', 'C'})); break;
                case 9: owner.c("tile.bed.notValid");
                    require(lines(owner).get(0).a == nh.a().a("tile.bed.notValid"), "translated identity");
                    break;
                case 10: String missing = new String("chat.missing.key"); owner.c(missing);
                    require(lines(owner).get(0).a == missing, "fallback original key identity");
                    break;
                default: for (int i = 0; i < 51; ++i)
                    owner.a(String.format(Locale.ROOT, "v%02d", i));
            }
            row(owner, profile); owner.b();
        }
        owner.a("old"); owner.a((String)null);
        lines(owner).get(0).b = Integer.MAX_VALUE; lines(owner).get(1).b = 199;
        field(uq.class, "j").setInt(owner, 2); owner.a();
        row(owner, 12); owner.b(); row(owner, 13);
        require(network == 0 && natives == 0 && writes == 0, "observer side effect");
        System.out.println("CHAT_ORIGINAL vm=1 network=0 natives=0 writes=0 prepared-app=1 prepared-font=1 draw=0");
    }
}
