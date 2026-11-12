import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Prepared operands around unchanged sleeping Player damage and Ladder drops. */
public final class LadderWakeObservation {
    static final String[] MODES = {"success", "publication-error"};

    static Object cell(int x, int z) {
        lm chunk = PlayerDamageWakeObservation.chunk;
        int index = (x * 16 + z) * 128 + 64;
        return List.of(chunk.b[index] & 255,
            (chunk.e.a[index >> 1] >> ((index & 1) * 4)) & 15);
    }

    static Object item(hl value) throws Exception {
        return ConstructorJson.map("position", List.of(PlayerDamageWakeObservation.d(value.aM),
            PlayerDamageWakeObservation.d(value.aN), PlayerDamageWakeObservation.d(value.aO)),
            "motion", List.of(PlayerDamageWakeObservation.d(value.aP),
            PlayerDamageWakeObservation.d(value.aQ), PlayerDamageWakeObservation.d(value.aR)),
            "dimensions", List.of(PlayerDamageWakeObservation.f(value.bg),
            PlayerDamageWakeObservation.f(value.bh), PlayerDamageWakeObservation.f(value.bf)),
            "bounds", PlayerDamageWakeObservation.bounds(value.aW),
            "yaw", PlayerDamageWakeObservation.f(value.aS),
            "phase", PlayerDamageWakeObservation.f(value.d), "delay", value.c,
            "age", value.b, "health", PlayerDamageWakeObservation.get(value, hl.class, "f"),
            "stack", List.of(value.a.c, value.a.a, value.a.i()),
            "inserted", value.bF, "removed", value.be,
            "world_same", value.aI == PlayerDamageWakeObservation.world);
    }

    static Object state(Observer observer) throws Exception {
        Map<String, Object> result = new LinkedHashMap<>();
        for (Map.Entry<?, ?> entry : ((Map<?, ?>) PlayerDamageWakeObservation.state()).entrySet())
            result.put((String) entry.getKey(), entry.getValue());
        result.put("ladder", cell(7, 8));
        result.put("ladder_support", cell(7, 9));
        result.put("foot", cell(8, 7));
        result.put("world_random", PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.world.r));
        result.put("player_random", PlayerDamageWakeObservation.random(PlayerDamageWakeObservation.actor.bs));
        List<Object> items = new ArrayList<>();
        for (hl value : observer.items) items.add(item(value));
        result.put("items", items);
        return result;
    }

    static List<Object> stack(StackTraceElement[] trace) {
        List<Object> result = new ArrayList<>();
        for (StackTraceElement frame : trace)
            result.add(ConstructorJson.map("class", frame.getClassName(), "method", frame.getMethodName(),
                "file", frame.getFileName(), "line", frame.getLineNumber()));
        return result;
    }

    static final class Observer implements pm {
        final String mode;
        final List<Object> events = new ArrayList<>();
        final List<hl> items = new ArrayList<>();
        Observer(String mode) { this.mode = mode; }
        public void a(int x, int y, int z) {
            events.add(ConstructorJson.map("kind", "cell", "xyz", List.of(x, y, z),
                "head", cell(8, 8), "ladder", cell(7, 8),
                "sleeping", PlayerDamageWakeObservation.actor.u,
                "stack", stack(Thread.currentThread().getStackTrace())));
        }
        public void a(sn value) {
            PlayerDamageWakeObservation.need(value instanceof hl && items.isEmpty(), "one real Item publication");
            hl entity = (hl) value;
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.world.b.contains(value) &&
                entity.bF && entity.a != null, "published before observer");
            items.add(entity);
            try {
                events.add(ConstructorJson.map("kind", "added", "head", cell(8, 8), "ladder", cell(7, 8),
                    "sleeping", PlayerDamageWakeObservation.actor.u, "item", item(entity),
                    "stack", stack(Thread.currentThread().getStackTrace())));
            } catch (ReflectiveOperationException failure) { throw new AssertionError(failure); }
            catch (Exception failure) { throw new AssertionError(failure); }
            if (mode.equals("publication-error"))
                throw new IllegalStateException("Ladder Item observer interruption");
        }
        public void b(sn value) { throw new AssertionError("unexpected removal"); }
        public void a(String n,double x,double y,double z,float v,float p) { throw new AssertionError("unexpected sound"); }
        public void a(String n,double x,double y,double z,double a,double b,double c) { throw new AssertionError("unexpected particle"); }
        public void b(int x,int y,int z,int a,int b,int c) {}
        public void e() { throw new AssertionError("unexpected refresh"); }
        public void a(String n,int x,int y,int z) { throw new AssertionError("unexpected record"); }
        public void a(int x,int y,int z,ow tile) { throw new AssertionError("unexpected TileEntity"); }
        public void a(gs p,int a,int b,int c,int d,int e) { throw new AssertionError("unexpected event"); }
    }

    static void run(String mode) throws Exception {
        PlayerDamageWakeObservation.setup(new int[]{0, 0, 20, 0, 0});
        fd world = PlayerDamageWakeObservation.world;
        List<?> listeners = (List<?>) PlayerDamageWakeObservation.get(world, fd.class, "u");
        PlayerDamageWakeObservation.need(listeners.size() == 1, "one setup observer");
        world.b((pm) listeners.get(0));
        Observer observer = new Observer(mode);
        world.a(observer);
        PlayerDamageWakeObservation.need(listeners.size() == 1 && listeners.get(0) == observer, "one actual observer");
        PlayerDamageWakeObservation.cell(7, 64, 8, 65, 2);
        PlayerDamageWakeObservation.need(cell(7, 9).equals(List.of(0, 0)), "Ladder support is absent");
        Object before = state(observer), result = null;
        String exception = "NONE", message = null;
        List<Object> exceptionStack = new ArrayList<>();
        try { result = PlayerDamageWakeObservation.actor.a((sn) null, 0); }
        catch (IllegalStateException failure) {
            PlayerDamageWakeObservation.need(mode.equals("publication-error") &&
                "Ladder Item observer interruption".equals(failure.getMessage()), "declared callback failure");
            exception = failure.getClass().getName().replace('.', '/');
            message = failure.getMessage();
            exceptionStack = stack(failure.getStackTrace());
        }
        System.out.println("LADDER_WAKE " + ConstructorJson.json(ConstructorJson.map("mode", mode,
            "before", before, "after", state(observer), "result", result, "exception", exception,
            "exception_message", message, "exception_stack", exceptionStack, "events", observer.events)));
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
        System.out.println("LADDER_WAKE_COMPLETE cases=2 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
    }
}
