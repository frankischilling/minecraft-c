import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Complete original spawn/reset bodies on genuine constructed client owners. */
public final class SpawnResetObservation {
    static kq app;
    static fd world;
    static lm chunk;
    static dc player;
    static MethodHandle base;

    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static Random random() throws Exception {
        return (Random)field(sn.class, "bs").get(player);
    }

    static long seed(Random random) throws Exception {
        return ((AtomicLong)field(Random.class, "seed").get(random)).get();
    }

    static void prepare(int scenario) throws Exception {
        Arrays.fill(chunk.b, (byte)0);
        Arrays.fill(chunk.e.a, (byte)0);
        player.aI = world;
        player.b(1.0f, 1.0f);
        player.bf = .25f;
        player.bo = .125f;
        player.e(8.5, 8.0, 8.5);
        player.aP = 2;
        player.aQ = 3;
        player.aR = 4;
        player.aJ = -9;
        player.aK = -10;
        player.aL = -11;
        player.bl = -12;
        player.bm = -13;
        player.bn = -14;
        player.aS = 37;
        player.aT = -17;
        player.aU = -11;
        player.aV = -13;
        player.Y = 3;
        player.ad = 9;
        random().setSeed(38);
        world.r.setSeed(38);
        eq.b();
        bt.b();
        if (scenario == 1) {
            for (int x = 6; x <= 12; ++x)
                for (int z = 6; z <= 12; ++z) chunk.b[(x << 11) | (z << 7) | 7] = 1;
        }
        if (scenario == 2)
            for (int y = 0; y <= 12; ++y) chunk.b[(8 << 11) | (8 << 7) | y] = 1;
        if (scenario == 3 || scenario == 9) player.aN = 0.0;
        if (scenario == 4) player.aN = -0.0;
        if (scenario == 5) player.aN = -2.0;
        if (scenario == 6 || scenario == 17)
            player.aN = Double.longBitsToDouble(0x7ff8123456789abcL);
        if (scenario == 7 || scenario == 16) player.aI = null;
        if (scenario == 8 || scenario == 9 || scenario == 16 || scenario == 17)
            field(sn.class, "aW").set(player, null);
        if (scenario == 10) chunk.b[(8 << 11) | (8 << 7) | 7] = 44;
        if (scenario == 11) chunk.b[(8 << 11) | (8 << 7) | 7] = 85;
        if (scenario == 12) chunk.b[(8 << 11) | (8 << 7) | 7] = 26;
        if (scenario == 13) field(fd.class, "K").set(world, null);
        if (scenario == 14) field(fd.class, "v").set(world, null);
        if (scenario == 15) field(fd.class, "R").set(world, null);
        if (scenario == 18) player.aN = Double.longBitsToDouble(1L);
        if (scenario == 19) player.aN = Double.NEGATIVE_INFINITY;
        if (scenario == 20) player.aM = Double.longBitsToDouble(0x7ff8123456789abcL);
    }

    static void row(int body, int scenario, boolean complete, String failureClass,
                    String failureMethod) throws Exception {
        System.out.printf("SPAWN %d %d %d %s %s", body, scenario, complete ? 1 : 0,
                          failureClass, failureMethod);
        eq box = player.aW;
        double[] values = {player.aM, player.aN, player.aO, player.aP, player.aQ, player.aR,
                           player.aJ, player.aK, player.aL, player.bl, player.bm, player.bn,
                           box == null ? 0 : box.a, box == null ? 0 : box.b,
                           box == null ? 0 : box.c, box == null ? 0 : box.d,
                           box == null ? 0 : box.e, box == null ? 0 : box.f};
        for (double value : values) System.out.printf(" %016x", Double.doubleToRawLongBits(value));
        float[] angles = {player.bf, player.bg, player.bh, player.bo, player.aS, player.aT,
                          player.aU, player.aV};
        for (float value : angles) System.out.printf(" %08x", Float.floatToRawIntBits(value));
        System.out.printf(" %d %d %d %d %012x %012x%n", player.Y, player.ad,
                          player.aI == null ? 0 : 1, box == null ? 0 : 1,
                          seed(random()), seed(world.r));
    }

    public static void main(String[] arguments) throws Throwable {
        Locale.setDefault(Locale.ROOT);
        require(arguments.length == 2, "expected private world directory and original JAR");
        Path directory = Path.of(arguments[0]);
        Files.createDirectories(directory);
        app = new kq(null, null, null, 854, 480, false, null);
        try {
            app.k = new gr("SpawnPlayer", "-");
            world = new fd(new mx(directory.toFile(), "Spawn", false), "Spawn", xa.a(0), 23L);
            chunk = world.c(0, 0);
            player = new dc(app, world, app.k, 0);
            require(player.getClass() == dc.class && world.getClass() == fd.class &&
                    chunk.getClass() == lm.class, "actual concrete classes required");
            MethodHandles.Lookup lookup = MethodHandles.privateLookupIn(sn.class, MethodHandles.lookup());
            base = lookup.findSpecial(sn.class, "t_", MethodType.methodType(void.class), sn.class);
            MethodHandleInfo information = lookup.revealDirect(base);
            require(information.getReferenceKind() == MethodHandleInfo.REF_invokeSpecial &&
                    information.getDeclaringClass() == sn.class && information.getName().equals("t_"),
                    "direct original sn body handle required");
            Path jar = Path.of(arguments[1]).toRealPath();
            for (Class<?> type : new Class<?>[]{kq.class, net.minecraft.client.Minecraft.class,
                    dc.class, gs.class, ls.class, sn.class, gr.class, fd.class, lm.class,
                    eq.class, bt.class, uu.class, in.class, mx.class, xa.class, ix.class, aa.class}) {
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                            .toRealPath().equals(jar), "foreign original class " + type);
                System.out.println("ORIGIN " + type.getName());
            }
            eq savedBounds = player.aW;
            Object savedK = field(fd.class, "K").get(world);
            Object savedV = field(fd.class, "v").get(world);
            Object savedR = field(fd.class, "R").get(world);
            for (int body = 0; body < 2; ++body) {
                for (int scenario = 0; scenario <= 20; ++scenario) {
                    String failureClass = "-", failureMethod = "-";
                    boolean complete = false;
                    try {
                        prepare(scenario);
                        try {
                            if (body == 0) player.t_();
                            else base.invokeExact((sn)player);
                            complete = true;
                        } catch (NullPointerException failure) {
                            StackTraceElement top = failure.getStackTrace()[0];
                            failureClass = top.getClassName();
                            failureMethod = top.getMethodName();
                        }
                        boolean expected = scenario != 8 && scenario != 13 && scenario != 14 && scenario != 15;
                        require(complete == expected, "completion differs body " + body + " scenario " + scenario);
                        if (complete) {
                            require(body == 0 ? player.Y == 20 && player.ad == 0
                                               : player.Y == 3 && player.ad == 9,
                                    "wrapper/base health distinction");
                            require(body == 0 ? player.bg == .6f && player.bh == 1.8f && player.bf == 1.62f
                                               : player.bg == 1 && player.bh == 1 && player.bf == .25f,
                                    "wrapper/base size distinction");
                        }
                        row(body, scenario, complete, failureClass, failureMethod);
                    } finally {
                        field(sn.class, "aW").set(player, savedBounds);
                        player.aI = world;
                        field(fd.class, "K").set(world, savedK);
                        field(fd.class, "v").set(world, savedV);
                        field(fd.class, "R").set(world, savedR);
                    }
                }
            }
            System.out.println("Original Player spawn reset: 42 states");
        } finally {
            app.J = false;
            for (Thread thread : Thread.getAllStackTraces().keySet()) {
                if (thread.getName().equals("Timer hack thread")) {
                    thread.interrupt();
                    thread.join(2000);
                    require(!thread.isAlive(), "original Timer thread retirement");
                }
            }
        }
    }
}
