import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Locale;

/** Authored inputs; all Player, coordinate and placement bodies are unchanged archive classes. */
public final class PlayerCoordinatesObservation {
    static Path directory;
    static int network, nativeLinks, rejectedWrites;

    static void need(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    @SuppressWarnings("removal")
    static final class Guard extends SecurityManager {
        public void checkPermission(Permission permission) {}
        public void checkConnect(String host, int port) { ++network; throw new SecurityException("network"); }
        public void checkListen(int port) { ++network; throw new SecurityException("listener"); }
        public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native link"); }
        public void checkWrite(String name) {
            if (!Path.of(name).toAbsolutePath().normalize().startsWith(directory)) {
                ++rejectedWrites;
                throw new SecurityException("write outside work directory");
            }
        }
        public void checkDelete(String name) { checkWrite(name); }
    }

    static void placement(dc player, int index) {
        eq box = player.aW;
        double[] row = {player.aM, player.aN, player.aO, player.aJ, player.aK, player.aL,
                        player.bl, player.bm, player.bn, box.a, box.b, box.c, box.d, box.e, box.f};
        System.out.print("COORDINATE_POSITION " + index);
        for (double value : row)
            System.out.printf(Locale.ROOT, " %016x", Double.doubleToRawLongBits(value));
        System.out.println();
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length == 2, "archive work-directory");
        Path archive = Path.of(args[0]).toRealPath();
        directory = Path.of(args[1]).toAbsolutePath().normalize();
        Files.createDirectories(directory);
        System.setSecurityManager(new Guard());
        for (Class<?> type : new Class<?>[]{dc.class, gs.class, ls.class, sn.class, br.class, eq.class})
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath()
                 .equals(archive), "unchanged archive class origin");
        gr session = new gr("CoordinatePlayer", "-");
        fd world = new fd(new wy(), "PlayerCoordinatesReference", xa.a(0), 5143L);
        world.a(new br(0, 64, 0));
        dc player = new dc(null, world, session, 0);
        Field saved = gs.class.getDeclaredField("b");
        saved.setAccessible(true);
        br input = new br(7, 8, 9);
        saved.set(player, input); // Declared source-field identity input.
        need(player.Q() == input && player.Q() == input, "Q same identity");
        player.a(input);
        br copy = player.Q();
        need(copy != input && copy.a == 7 && copy.b == 8 && copy.c == 9, "distinct BR copy");
        player.a(copy);
        need(player.Q() != copy && copy.a == 7 && copy.b == 8 && copy.c == 9,
             "copy from own field retains source local");
        input.a = 0; input.b = 0; input.c = 0;
        need(player.Q().a == 7 && player.Q().b == 8 && player.Q().c == 9, "copy values independent");
        player.a((br)null);
        need(player.Q() == null, "nullable saved spawn");
        System.out.println("COORDINATE_METHODS same=1 distinct=1 replaced-local=1 null=1");
        player.bg = .6f; player.bh = 1.8f; player.bf = 1.62f; player.bo = .125f;
        player.aU = -11; player.aV = -13;
        int[][] vectors = {{4, 64, -7}, {16777217, 16777217, -16777217},
                           {Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE}};
        for (int i = 0; i < vectors.length; ++i) {
            int[] vector = vectors[i];
            player.c((double)((float)vector[0] + .5f), (double)((float)vector[1] + .1f),
                     (double)((float)vector[2] + .5f), 37, -17);
            need(player.aU == -11 && player.aV == -13, "previous angles unchanged");
            placement(player, i);
        }
        Field bounds = sn.class.getDeclaredField("aW");
        bounds.setAccessible(true);
        bounds.set(player, null); // Explicit authored null permanent-bounds operand.
        boolean threw = false;
        try { player.c(1, 2, 3, 4, 5); }
        catch (NullPointerException expected) { threw = true; }
        need(threw && player.aM == 1 && player.aJ == 1 && player.bl == 1 &&
             player.aN == 2 + (double)player.bf && player.aK == player.aN && player.bm == player.aN &&
             player.aO == 3 && player.aL == 3 && player.bn == 3 && player.aS == 4 && player.aT == 5,
             "real SN.e null bounds after complete SN.c stores");
        System.out.println("COORDINATE_POSITION_NULL source=NPE complete-pose=1");
        need(network == 0 && nativeLinks == 0 && rejectedWrites == 0, "external effect guards");
        System.out.println("Original Player coordinates: 5 rows; network=0; native=0; rejected_writes=0");
    }
}
