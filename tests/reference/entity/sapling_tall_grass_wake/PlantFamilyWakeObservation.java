import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Prepared operands around unchanged sleeping Player damage and Plant family drops. */
public final class PlantFamilyWakeObservation {
    static final class Profile {
        final String mode;
        final int id, metadata, floor;
        final long worldSeed;
        final boolean observerError;
        Profile(String mode, int id, int metadata, int floor, long seed, boolean error) {
            this.mode = mode; this.id = id; this.metadata = metadata; this.floor = floor;
            this.worldSeed = seed; this.observerError = error;
        }
        Object input() {
            return ConstructorJson.map("mode", mode, "id", id, "metadata", metadata,
                "floor", floor, "world_seed", worldSeed, "observer_error", observerError);
        }
    }
    static final Profile[] PROFILES = {
        new Profile("sapling-supported", 6, 2, 2, 1L, false),
        new Profile("sapling-unsupported", 6, 2, 0, 1L, false),
        new Profile("sapling-publication-error", 6, 2, 0, 1L, true),
        new Profile("tallgrass-supported", 31, 1, 2, 1L, false),
        new Profile("tallgrass-unsupported", 31, 1, 0, 1L, false),
        new Profile("tallgrass-publication-error", 31, 1, 0, 1L, true),
        new Profile("tallgrass-unsupported-seed1234", 31, 1, 0, 4660L, false),
        new Profile("tallgrass-unsupported-seed7fffffff", 31, 1, 0, 2147483647L, false),
    };

    static Object cell(int x, int z) {
        return cellAt(x, 64, z);
    }

    static Object cellAt(int x, int y, int z) {
        lm chunk = PlayerDamageWakeObservation.chunk;
        int index = (x * 16 + z) * 128 + y;
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

    static String descriptor(Class<?> type) {
        if (type == void.class) return "V";
        if (type == int.class) return "I";
        if (type == boolean.class) return "Z";
        if (type == float.class) return "F";
        if (type == double.class) return "D";
        if (type == long.class) return "J";
        if (type == byte.class) return "B";
        if (type == short.class) return "S";
        if (type == char.class) return "C";
        return type.isArray() ? type.getName().replace('.', '/') : "L" + type.getName().replace('.', '/') + ";";
    }

    static Object method(uu receiver, String name, Class<?>... parameters) throws Exception {
        for (Class<?> type = receiver.getClass(); type != null; type = type.getSuperclass()) {
            try {
                java.lang.reflect.Method member = type.getDeclaredMethod(name, parameters);
                String signature = "(";
                for (Class<?> parameter : member.getParameterTypes()) signature += descriptor(parameter);
                signature += ")" + descriptor(member.getReturnType());
                return ConstructorJson.map("class", member.getDeclaringClass().getName(),
                    "method", member.getName(), "descriptor", signature);
            } catch (NoSuchMethodException missing) { /* Actual Java inheritance walk only. */ }
        }
        throw new AssertionError("Missing original declared method " + name);
    }

    static Object receiver(uu block) throws Exception {
        return ConstructorJson.map("class", block.getClass().getName(),
            "neighbor", method(block, "b", fd.class, int.class, int.class, int.class, int.class),
            "survival", method(block, "g", fd.class, int.class, int.class, int.class),
            "substrate", method(block, "d", int.class),
            "drop_wrapper", method(block, "g", fd.class, int.class, int.class, int.class, int.class),
            "drop_chance", method(block, "a", fd.class, int.class, int.class, int.class, int.class, float.class),
            "drop_item", method(block, "a", int.class, Random.class),
            "drop_metadata", method(block, "b_", int.class),
            "drop_count", method(block, "a", Random.class));
    }

    static Object lightInputs() {
        lm chunk = PlayerDamageWakeObservation.chunk;
        int index = (7 * 16 + 8) * 128 + 64, shift = (index & 1) * 4;
        return ConstructorJson.map("position", List.of(7, 64, 8),
            "sky_nibble", (chunk.f.a[index >> 1] >> shift) & 15,
            "block_light_nibble", (chunk.g.a[index >> 1] >> shift) & 15,
            "height", chunk.h[8 * 16 + 7] & 255,
            "world_f_sky_subtraction", PlayerDamageWakeObservation.world.f,
            "fd_m_chunk_subtraction_literal", 0,
            "chunk_sky_seen_static", lm.a);
    }

    static Object state(Observer observer) throws Exception {
        Map<String, Object> result = new LinkedHashMap<>();
        for (Map.Entry<?, ?> entry : ((Map<?, ?>) PlayerDamageWakeObservation.state()).entrySet())
            result.put((String) entry.getKey(), entry.getValue());
        result.put("plant", cell(7, 8));
        result.put("plant_floor", cellAt(7, 63, 8));
        result.put("light_inputs", lightInputs());
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
        final Profile profile;
        final List<Object> events = new ArrayList<>();
        final List<hl> items = new ArrayList<>();
        Observer(Profile profile) { this.profile = profile; }
        public void a(int x, int y, int z) {
            events.add(ConstructorJson.map("kind", "cell", "xyz", List.of(x, y, z),
                "head", cell(8, 8), "plant", cell(7, 8),
                "floor", cellAt(7, 63, 8),
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
                events.add(ConstructorJson.map("kind", "added", "head", cell(8, 8), "plant", cell(7, 8),
                    "floor", cellAt(7, 63, 8),
                    "sleeping", PlayerDamageWakeObservation.actor.u, "item", item(entity),
                    "stack", stack(Thread.currentThread().getStackTrace())));
            } catch (ReflectiveOperationException failure) { throw new AssertionError(failure); }
            catch (Exception failure) { throw new AssertionError(failure); }
            if (profile.observerError)
                throw new IllegalStateException("Plant family Item observer interruption");
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

    static void run(Profile profile) throws Exception {
        PlayerDamageWakeObservation.setup(new int[]{0, 0, 20, 0, 0});
        fd world = PlayerDamageWakeObservation.world;
        List<?> listeners = (List<?>) PlayerDamageWakeObservation.get(world, fd.class, "u");
        PlayerDamageWakeObservation.need(listeners.size() == 1, "one setup observer");
        world.b((pm) listeners.get(0));
        Observer observer = new Observer(profile);
        world.a(observer);
        PlayerDamageWakeObservation.need(listeners.size() == 1 && listeners.get(0) == observer, "one actual observer");
        PlayerDamageWakeObservation.cell(7, 64, 8, profile.id, profile.metadata);
        PlayerDamageWakeObservation.cell(7, 63, 8, profile.floor, 0);
        world.r.setSeed(profile.worldSeed);
        Object actualReceiver = receiver(uu.m[profile.id]);
        Object prepared = ConstructorJson.map("profile", profile.input(),
            "shared_setup", List.of(0, 0, 20, 0, 0),
            "post_setup_cell_assignments", List.of(List.of(7, 64, 8, profile.id, profile.metadata),
                List.of(7, 63, 8, profile.floor, 0)), "world_random_reseed", profile.worldSeed,
                "light_or_sky_overrides", List.of());
        Object before = state(observer), result = null;
        String exception = "NONE", message = null;
        List<Object> exceptionStack = new ArrayList<>();
        try { result = PlayerDamageWakeObservation.actor.a((sn) null, 0); }
        catch (RuntimeException failure) {
            // Preserve any real first failure, including missing light context;
            // no profile/result is repaired to obtain a preferred observation.
            exception = failure.getClass().getName().replace('.', '/');
            message = failure.getMessage();
            exceptionStack = stack(failure.getStackTrace());
        }
        System.out.println("PLANT_FAMILY_WAKE " + ConstructorJson.json(ConstructorJson.map("mode", profile.mode,
            "receiver", actualReceiver, "prepared", prepared, "before", before, "after", state(observer), "result", result, "exception", exception,
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
            for (Profile profile : PROFILES) run(profile);
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
        System.out.println("PLANT_FAMILY_WAKE_COMPLETE cases=8 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
    }
}
