import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;
import sun.reflect.ReflectionFactory;

/** Real OZ/DA/KE construction and method bodies; prepared App and World operands. */
public final class PauseObservation {
    static int network, natives;
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(String name) throws Exception {
        Field result = oz.class.getDeclaredField(name);
        result.setAccessible(true);
        return result;
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
        fd world = (fd)prepared(fd.class);
        world.B = true;
        int[][] sizes = {{427,240},{320,200},{-1,-1},{Integer.MIN_VALUE,Integer.MAX_VALUE}};
        for (Class<?> type : new Class<?>[]{oz.class,da.class,ke.class,kq.class,fd.class,
                net.minecraft.client.Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign class " + type);
        for (int mode = 0; mode < 3; ++mode) {
            for (int n = 0; n < sizes.length; ++n) {
                oz pause = new oz();
                require(field("a").getInt(pause) == 0 && field("i").getInt(pause) == 0,
                    "constructor fields differ");
                pause.b = mode == 0 ? null : app;
                pause.c = sizes[n][0];
                pause.d = sizes[n][1];
                app.f = mode == 2 ? world : null;
                field("a").setInt(pause,123);
                field("i").setInt(pause,17+n);
                boolean returned = false;
                try {
                    pause.b();
                    returned = true;
                } catch (NullPointerException failure) {
                    require(mode == 0 && failure.getStackTrace()[0].getClassName().equals("oz"),
                        "unexpected source failure");
                }
                require(returned == (mode != 0) && pause.e.size() == (mode == 0 ? 1 : 5),
                    "initialization prefix differs");
                System.out.printf("PAUSE %d %d %d %d %d %d%n",mode,n,
                    field("a").getInt(pause),field("i").getInt(pause),returned?1:0,pause.e.size());
                for (int index = 0; index < pause.e.size(); ++index) {
                    ke button = (ke)pause.e.get(index);
                    System.out.printf("PAUSEBUTTON %d %d %d %d %d %d %d %d %d %d ",mode,n,index,
                        button.f,button.c,button.d,button.a,button.b,button.g?1:0,button.h?1:0);
                    for (int unit = 0; unit < button.e.length(); ++unit)
                        System.out.printf("%04x",(int)button.e.charAt(unit));
                    System.out.println();
                }
                pause.a();
                System.out.printf("PAUSETICK %d %d %d %d%n",mode,n,
                    field("a").getInt(pause),field("i").getInt(pause));
            }
        }
        require(network == 0 && natives == 0, "external work attempted");
        System.out.println("Original pause screen: 68 rows; network=0; native=0");
    }
}
