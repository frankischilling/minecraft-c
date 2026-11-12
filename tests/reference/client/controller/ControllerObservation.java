import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

/** Selected complete original controller bodies on genuine constructed owners. */
public final class ControllerObservation {
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static String state(Object object, boolean skipYaw) throws Exception {
        StringBuilder result = new StringBuilder();
        for (Class<?> type = object.getClass(); type != Object.class; type = type.getSuperclass()) {
            for (Field f : type.getDeclaredFields()) {
                if (Modifier.isStatic(f.getModifiers()) ||
                    (skipYaw && type == sn.class && f.getName().equals("aS"))) continue;
                f.setAccessible(true);
                Object value = f.get(object);
                result.append(type.getName()).append(':').append(f.getName()).append('=');
                if (f.getType() == float.class)
                    result.append(Integer.toHexString(Float.floatToRawIntBits((Float)value)));
                else if (f.getType() == double.class)
                    result.append(Long.toHexString(Double.doubleToRawLongBits((Double)value)));
                else if (f.getType().isPrimitive()) result.append(value);
                else result.append(value == null ? 0 : System.identityHashCode(value));
                result.append(';');
            }
        }
        return result.toString();
    }

    static void constructor(kq app, int scenario) throws Exception {
        os controller = new os(scenario == 0 ? app : null);
        require(controller.getClass() == os.class, "actual os required");
        System.out.printf("CTRLCTOR %d %d %d %d %d %d %08x %08x %08x %d%n", scenario,
            field(ob.class, "a").get(controller) == (scenario == 0 ? app : null) ? 1 : 0,
            controller.b ? 1 : 0,
            field(os.class, "c").getInt(controller), field(os.class, "d").getInt(controller),
            field(os.class, "e").getInt(controller),
            Float.floatToRawIntBits(field(os.class, "f").getFloat(controller)),
            Float.floatToRawIntBits(field(os.class, "g").getFloat(controller)),
            Float.floatToRawIntBits(field(os.class, "h").getFloat(controller)),
            field(os.class, "i").getInt(controller));
        String before = state(controller, false);
        controller.a((fd)null);
        controller.b((gs)null);
        require(before.equals(state(controller, false)), "empty inherited hooks changed fields");
    }

    public static void main(String[] arguments) throws Exception {
        Locale.setDefault(Locale.ROOT);
        require(arguments.length == 2, "expected private directory and original JAR");
        Path directory = Path.of(arguments[0]);
        Files.createDirectories(directory);
        kq app = new kq(null, null, null, 854, 480, false, null);
        try {
            app.k = new gr("Controller", "-");
            fd world = new fd(new mx(directory.toFile(), "Controller", false),
                              "Controller", xa.a(0), 23L);
            lm chunk = world.c(0, 0);
            require(world.getClass() == fd.class && chunk.getClass() == lm.class,
                    "actual World and Chunk required");
            Path jar = Path.of(arguments[1]).toRealPath();
            for (Class<?> type : new Class<?>[]{kq.class, net.minecraft.client.Minecraft.class,
                    os.class, ob.class, dc.class, gs.class, ls.class, sn.class, gr.class,
                    fd.class, lm.class, eq.class, bt.class, uu.class, in.class, mx.class,
                    xa.class, ix.class, aa.class, ei.class}) {
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                            .toRealPath().equals(jar), "foreign original class " + type);
                System.out.println("ORIGIN " + type.getName());
            }
            constructor(app, 0);
            constructor(app, 1);
            Object provider = field(fd.class, "t").get(world);
            gr session = app.k;
            for (int scenario = 0; scenario <= 8; ++scenario) {
                boolean nullApp = scenario == 3 || scenario == 8;
                boolean nullWorld = scenario == 3 || scenario == 4 || scenario == 7;
                boolean nullProvider = scenario == 5 || scenario == 8;
                boolean nullSession = scenario == 6 || scenario == 7;
                field(fd.class, "t").set(world, nullProvider ? null : provider);
                app.k = nullSession ? null : session;
                int y = scenario == 1 ? Integer.MAX_VALUE : scenario == 2 ? Integer.MIN_VALUE : 64;
                ei info = (ei)field(fd.class, "x").get(world);
                require(info != null, "actual WorldInfo required");
                info.a(-7, y, 11);
                br spawn = world.u();
                require(spawn.a == -7 && spawn.b == y && spawn.c == 11, "spawn setter differs");
                os controller = new os(nullApp ? null : app);
                int before = field(sn.class, "a").getInt(null);
                dc player = null;
                boolean complete = false;
                String failureClass = "-", failureMethod = "-";
                try {
                    player = (dc)controller.b(nullWorld ? null : world);
                    complete = true;
                } catch (NullPointerException failure) {
                    StackTraceElement top = failure.getStackTrace()[0];
                    failureClass = top.getClassName();
                    failureMethod = top.getMethodName();
                }
                int consumed = field(sn.class, "a").getInt(null) - before;
                require(complete == (scenario < 3), "unexpected completion");
                System.out.printf("CTRLNEW %d %d %s %s %d %d %d %d %d %016x %016x %016x %08x %08x%n",
                    scenario, complete ? 1 : 0, failureClass, failureMethod, consumed,
                    player == null ? 0 : 1,
                    player != null && player.b == app ? 1 : 0,
                    player != null && player.aI == world ? 1 : 0,
                    player == null ? 0 : player.m,
                    player == null ? 0L : Double.doubleToRawLongBits(player.aM),
                    player == null ? 0L : Double.doubleToRawLongBits(player.aN),
                    player == null ? 0L : Double.doubleToRawLongBits(player.aO),
                    player == null ? 0 : Float.floatToRawIntBits(player.aS),
                    player == null ? 0 : Float.floatToRawIntBits(player.aU));
                if (player != null) {
                    require(player.getClass() == dc.class && player.a == null,
                            "canonical returned Player/input required");
                    int[] values = {0x42140000, 0x80000000, 0x7fc12345};
                    for (int k = 0; k < values.length; ++k) {
                        player.aS = Float.intBitsToFloat(values[k]);
                        player.aU = -11.0f;
                        String unchanged = state(player, true);
                        controller.a((gs)player);
                        require(unchanged.equals(state(player, true)), "yaw hook changed other fields");
                        String beforeEmpty = state(player, false);
                        controller.a(world);
                        controller.b((gs)player);
                        require(beforeEmpty.equals(state(player, false)), "empty hook changed Player");
                        System.out.printf("CTRLYAW %d %d %08x %08x%n", scenario, k,
                            Float.floatToRawIntBits(player.aS), Float.floatToRawIntBits(player.aU));
                    }
                }
                try {
                    controller.a((gs)null);
                    throw new AssertionError("null yaw receiver completed");
                } catch (NullPointerException failure) {
                    require(failure.getStackTrace()[0].getClassName().equals("os") &&
                            failure.getStackTrace()[0].getMethodName().equals("a"),
                            "unexpected null yaw failure");
                }
                field(fd.class, "t").set(world, provider);
                app.k = session;
            }
            os reload = new os(app);
            app.k = new gr(null, "-");
            dc replaced = (dc)reload.b(world);
            require(replaced.l == null && replaced.b == app, "captured a stale App Session");
            app.k = session;
            dc restored = (dc)reload.b(world);
            require(restored.l == session.b && restored.b == app, "failed to reload actual App.k");
            System.out.println("Original single-player controller: 20 states");
        } finally {
            app.u.a();
        }
    }
}
