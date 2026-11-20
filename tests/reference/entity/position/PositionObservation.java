import java.io.BufferedReader;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

/** Original inherited position arithmetic on an actual constructed LocalPlayer. */
public final class PositionObservation {
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static double doubleBits(String value) {
        return Double.longBitsToDouble(Long.parseUnsignedLong(value, 16));
    }

    static float floatBits(String value) {
        return Float.intBitsToFloat(Integer.parseUnsignedInt(value, 16));
    }

    public static void main(String[] arguments) throws Exception {
        Locale.setDefault(Locale.ROOT);
        require(arguments.length == 3, "expected private World directory, original JAR and vectors");
        Path directory = Path.of(arguments[0]);
        Files.createDirectories(directory);
        kq app = new kq(null, null, null, 854, 480, false, null);
        try {
            app.k = new gr("PositionPlayer", "-");
            fd world = new fd(new mx(directory.toFile(), "Position", false), "Position", xa.a(0), 23L);
            lm chunk = world.c(0, 0);
            dc player = new dc(app, world, app.k, 0);
            require(player.getClass() == dc.class && world.getClass() == fd.class && chunk.getClass() == lm.class,
                    "actual constructed owners required");
            Path jar = Path.of(arguments[1]).toRealPath();
            for (Class<?> type : new Class<?>[]{kq.class, net.minecraft.client.Minecraft.class,
                    dc.class, gs.class, ls.class, sn.class, gr.class, fd.class, lm.class, eq.class,
                    uu.class, xa.class}) {
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                            .toRealPath().equals(jar), "foreign actual class " + type);
                System.out.println("ORIGIN " + type.getName());
            }
            int count = 0;
            try (BufferedReader reader = Files.newBufferedReader(Path.of(arguments[2]))) {
                for (String line; (line = reader.readLine()) != null;) {
                    String[] cells = line.split(" ");
                    require(cells.length == 8 && Integer.parseInt(cells[0]) == count, "vector inventory");
                    double x = doubleBits(cells[1]), y = doubleBits(cells[2]), z = doubleBits(cells[3]);
                    player.b(floatBits(cells[4]), floatBits(cells[5]));
                    player.bf = floatBits(cells[6]);
                    player.bo = floatBits(cells[7]);
                    player.e(x, y, z);
                    eq box = player.aW;
                    require(box != null, "actual permanent bounds");
                    System.out.print("POSITION " + count);
                    double[] values = {player.aM, player.aN, player.aO,
                                       box.a, box.b, box.c, box.d, box.e, box.f};
                    for (double value : values) System.out.printf(" %016x", Double.doubleToRawLongBits(value));
                    System.out.println();
                    ++count;
                }
            }
            require(count == 36, "expected complete finite vector inventory");
            System.out.println("Original Entity position: 36 states");
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
