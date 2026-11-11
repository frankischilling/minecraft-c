import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Authored inputs and observations around the unchanged original damage call. */
public final class RecursiveWakeObservation {
    static final String[] MODES = {"success", "observer-error", "null-exit"};
    static final String[] FIELDS = {"position", "dimensions", "bounds", "sleeping", "timer",
        "bed", "spawn", "private_coordinate", "world_same", "bounds_same", "health", "idle",
        "world_sleeping", "head", "planes", "random", "statistics", "sounds"};

    static Object cell(int x, int z) {
        lm chunk = PlayerDamageWakeObservation.chunk;
        int index = (x * 16 + z) * 128 + 64;
        return List.of(chunk.b[index] & 255, (chunk.e.a[index >> 1] >> ((index & 1) * 4)) & 15);
    }

    static Object state() throws Exception {
        Map<?, ?> existing = (Map<?, ?>) PlayerDamageWakeObservation.state();
        Map<String, Object> selected = new LinkedHashMap<>();
        for (String key : FIELDS) selected.put(key, existing.get(key));
        selected.put("orphan", cell(7, 8));
        return selected;
    }

    static final class Observer implements pm {
        final String mode;
        final List<Object> calls = new ArrayList<>();
        Observer(String mode) { this.mode = mode; }

        public void a(int x, int y, int z) {
            dc actor = PlayerDamageWakeObservation.actor;
            List<String> frames = new ArrayList<>();
            for (StackTraceElement frame : Thread.currentThread().getStackTrace()) {
                String owner = frame.getClassName();
                if (owner.equals("fd") || owner.equals("ve") || owner.equals("gs"))
                    frames.add(owner + "." + frame.getMethodName());
            }
            calls.add(ConstructorJson.map("xyz", List.of(x, y, z), "head", cell(8, 8),
                "orphan", cell(7, 8), "sleeping", actor.u, "frames", frames));
            if (x == 7 && y == 64 && z == 8) {
                if (mode.equals("observer-error"))
                    throw new IllegalStateException("orphan cell observer interruption");
                if (mode.equals("null-exit")) actor.aI = null;
            }
        }

        public void a(String n, double x, double y, double z, float v, float p) {
            throw new AssertionError("unexpected sound");
        }
        public void a(sn e) { throw new AssertionError("unexpected publication"); }
        public void b(sn e) { throw new AssertionError("unexpected removal"); }
        public void a(String n, double x, double y, double z, double a, double b, double c) {
            throw new AssertionError("unexpected particle");
        }
        public void b(int x, int y, int z, int a, int b, int c) {}
        public void e() { throw new AssertionError("unexpected refresh"); }
        public void a(String n, int x, int y, int z) { throw new AssertionError("unexpected record"); }
        public void a(int x, int y, int z, ow t) { throw new AssertionError("unexpected tile"); }
        public void a(gs p, int a, int b, int c, int d, int e) { throw new AssertionError("unexpected event"); }
    }

    static void run(String mode) throws Exception {
        PlayerDamageWakeObservation.setup(new int[]{0, 0, 20, 0, 0});
        fd world = PlayerDamageWakeObservation.world;
        List<?> observers = (List<?>) PlayerDamageWakeObservation.get(world, fd.class, "u");
        PlayerDamageWakeObservation.need(observers.size() == 1 &&
            observers.get(0) instanceof PlayerDamageWakeObservation.Observer, "actual setup observer");
        world.b((pm) observers.get(0));
        Observer observer = new Observer(mode);
        world.a(observer);
        PlayerDamageWakeObservation.need(observers.size() == 1 && observers.get(0) == observer,
            "one actual callback receiver");
        PlayerDamageWakeObservation.cell(7, 64, 8, 26, 8);
        PlayerDamageWakeObservation.need(cell(7, 7).equals(List.of(0, 0)), "orphan foot absent");

        Object before = state(), result = null;
        String exception = "NONE";
        List<String> frames = new ArrayList<>();
        try {
            result = PlayerDamageWakeObservation.actor.a((sn) null, 0);
        } catch (IllegalStateException | NullPointerException failure) {
            if (failure instanceof IllegalStateException) {
                PlayerDamageWakeObservation.need(mode.equals("observer-error") &&
                    "orphan cell observer interruption".equals(failure.getMessage()), "declared callback failure");
            } else {
                PlayerDamageWakeObservation.need(mode.equals("null-exit"), "declared null exit");
            }
            exception = failure.getClass().getName().replace('.', '/');
            for (StackTraceElement frame : failure.getStackTrace()) {
                if (frame.getClassName().equals(RecursiveWakeObservation.class.getName())) break;
                frames.add(frame.getClassName() + "." + frame.getMethodName());
            }
        }
        System.out.println("RECURSIVE_WAKE " + ConstructorJson.json(ConstructorJson.map(
            "mode", mode, "result", result, "exception", exception, "exception_frames", frames,
            "before", before, "after", state(), "callbacks", observer.calls)));
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        PlayerDamageWakeObservation.need(args.length == 3, "archive, private directory, class names");
        Locale.setDefault(Locale.ROOT);
        PlayerDamageWakeObservation.archive = Path.of(args[0]).toRealPath();
        PlayerDamageWakeObservation.directory = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new PlayerDamageWakeObservation.Guard());
        PlayerDamageWakeObservation.bind(Path.of(args[2]));
        kq app = new kq(null, null, null, 854, 480, false, null);
        PlayerDamageWakeObservation.app = app;
        try {
            app.k = new gr("BedNightComponent", "-");
            PlayerDamageWakeObservation.put(app, net.minecraft.client.Minecraft.class, "Z",
                PlayerDamageWakeObservation.directory.toFile());
            PlayerDamageWakeObservation.writer = new xi(app.k, PlayerDamageWakeObservation.directory.toFile());
            app.I = PlayerDamageWakeObservation.writer;
            ep.f.a(new kh(app));
            PlayerDamageWakeObservation.workers();
            PlayerDamageWakeObservation.shared = (Random) PlayerDamageWakeObservation.get(null,
                Class.forName("java.lang.Math$RandomNumberGeneratorHolder"), "randomNumberGenerator");
            for (String mode : MODES) run(mode);
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.network == 0 &&
                PlayerDamageWakeObservation.nativeLinks == 0 && PlayerDamageWakeObservation.rejectedWrites == 0 &&
                PlayerDamageWakeObservation.execs == 0, "external effect guards");
        } finally {
            if (PlayerDamageWakeObservation.writer != null) PlayerDamageWakeObservation.workers();
            app.J = false;
            for (Thread thread : Thread.getAllStackTraces().keySet()) {
                if (thread.getName().equals("Timer hack thread")) {
                    thread.interrupt();
                    thread.join(2000);
                    PlayerDamageWakeObservation.need(!thread.isAlive(), "Timer worker retirement");
                }
            }
        }
        System.out.println("RECURSIVE_WAKE_COMPLETE cases=3 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
    }
}
