import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Original CH/DA/KE bodies with a declared prepared App and genuine Session. */
public final class DeathScreenObservation {
    static int network, natives, writes;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
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
                ++writes; throw new SecurityException("observer denies writes");
            }
        });
        kq app = (kq)prepared(kq.class);
        gr session = new gr(null, null);
        Field selected = da.class.getDeclaredField("a");
        selected.setAccessible(true);
        for (Class<?> type : new Class<?>[]{ch.class, da.class, ke.class, gr.class, kq.class,
                uu.class, gm.class, net.minecraft.client.Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign class " + type);
        int[][] sizes = {{427,240},{320,200},{-1,-1},{Integer.MIN_VALUE,Integer.MAX_VALUE}};
        for (int mode = 0; mode < 3; ++mode) {
            for (int size = 0; size < sizes.length; ++size) {
                ch screen = new ch();
                require(screen.e != null && screen.e.size() == 0 && selected.get(screen) == null &&
                    screen.b == null && screen.c == 0 && screen.d == 0 && !screen.f &&
                    screen.g == null && screen.h == null && screen.k == 0.0f,
                    "CH constructor fields differ");
                screen.b = mode == 0 ? null : app;
                screen.c = sizes[size][0];
                screen.d = sizes[size][1];
                app.k = mode == 1 ? null : session;
                boolean returned = false;
                try {
                    screen.b();
                    returned = true;
                } catch (NullPointerException failure) {
                    require(mode == 0 && failure.getStackTrace()[0].getClassName().equals("ch"),
                        "unexpected source failure");
                }
                require(returned == (mode != 0) && screen.e.size() == 2 && !screen.c(),
                    "CH initialization/pause result differs");
                System.out.printf("DEATH %d %d %d %d %d%n", mode, size, returned?1:0,
                    screen.e.size(), screen.c()?1:0);
                for (int index = 0; index < screen.e.size(); ++index) {
                    ke button = (ke)screen.e.get(index);
                    System.out.printf("DEATHBUTTON %d %d %d %d %d %d %d %d %d %d ",
                        mode,size,index,button.f,button.c,button.d,button.a,button.b,
                        button.g?1:0,button.h?1:0);
                    for (int unit = 0; unit < button.e.length(); ++unit)
                        System.out.printf("%04x",(int)button.e.charAt(unit));
                    System.out.println();
                }
                screen.b = null;
                for (int key = 0; key < 3; ++key) {
                    screen.a('x', key);
                    require(screen.b == null && screen.e.size() == 2 && !screen.c(),
                        "CH keyboard changed source fields");
                    System.out.printf("DEATHKEY %d %d %d 1 2 0%n", mode,size,key);
                }
            }
        }
        require(network == 0 && natives == 0 && writes == 0, "external work attempted");
        System.out.println("Original death screen: 72 rows; network=0; native=0; writes=0");
    }
}
