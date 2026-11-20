import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Prepared operands around unchanged sleeping Player damage and upright plant neighbors. */
public final class UprightPlantsWakeObservation {
    static final class Profile {
        final String mode;
        final int id, metadata = 0, floor, light = 15, columns, waterDirection, waterId, publicationErrorAt, removalErrorY;
        final long worldSeed = 1L;
        final boolean rangeError;
        Profile(String mode, int id, int floor, int columns, int waterDirection, int waterId, int publicationErrorAt, int removalErrorY, boolean rangeError) {
            this.mode = mode; this.id = id; this.floor = floor; this.columns = columns;
            this.waterDirection = waterDirection; this.waterId = waterId;
            this.publicationErrorAt = publicationErrorAt; this.removalErrorY = removalErrorY; this.rangeError = rangeError;
        }
        Object input() {
            return ConstructorJson.map("mode", mode, "id", id, "metadata", metadata,
                "floor", floor, "light", light, "world_seed", worldSeed, "columns", columns,
                "water_direction", waterDirection, "water_id", waterId,
                "publication_error_at", publicationErrorAt, "removal_error_y", removalErrorY, "range_error", rangeError);
        }
    }
    static final Profile[] PROFILES = {
        new Profile("cactus-bed", 81, 12, 1, -1, 0, 0, 0, false),
        new Profile("cactus-publication-error", 81, 12, 1, -1, 0, 1, 0, false),
        new Profile("cactus-removal-error", 81, 12, 1, -1, 0, 0, 64, false),
        new Profile("cactus-column3", 81, 12, 3, -1, 0, 0, 0, false),
        new Profile("cactus-column3-publication-error-second", 81, 12, 3, -1, 0, 2, 0, false),
        new Profile("cactus-column3-removal-error-middle", 81, 12, 3, -1, 0, 0, 65, false),
        new Profile("reeds-sand", 83, 12, 1, -1, 0, 0, 0, false),
        new Profile("reeds-air", 83, 0, 1, -1, 0, 0, 0, false),
        new Profile("reeds-dry-grass", 83, 2, 1, -1, 0, 0, 0, false),
        new Profile("reeds-west-water", 83, 2, 1, 0, 9, 0, 0, false),
        new Profile("reeds-east-water", 83, 2, 1, 1, 9, 0, 0, false),
        new Profile("reeds-north-water", 83, 2, 1, 2, 9, 0, 0, false),
        new Profile("reeds-south-water", 83, 2, 1, 3, 9, 0, 0, false),
        new Profile("reeds-flowing-water", 83, 3, 1, 0, 8, 0, 0, false),
        new Profile("reeds-stacked", 83, 83, 1, -1, 0, 0, 0, false),
        new Profile("reeds-publication-error", 83, 2, 1, -1, 0, 1, 0, false),
        new Profile("reeds-removal-error", 83, 2, 1, -1, 0, 0, 64, false),
        new Profile("reeds-column3", 83, 2, 3, -1, 0, 0, 0, false),
        new Profile("reeds-column3-publication-error-second", 83, 2, 3, -1, 0, 2, 0, false),
        new Profile("reeds-column3-removal-error-middle", 83, 2, 3, -1, 0, 0, 65, false),
        new Profile("reeds-east-water-range-error", 83, 2, 1, 1, 9, 0, 0, true),
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
            "placement", method(block, "a", fd.class, int.class, int.class, int.class),
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

    static Object column() {
        return List.of(cellAt(7, 64, 8), cellAt(7, 65, 8), cellAt(7, 66, 8));
    }

    static Object waterCell(Profile profile) {
        if (profile.waterDirection == -1) return null;
        int[][] offsets = {{-1,0},{1,0},{0,-1},{0,1}};
        int x = 7 + offsets[profile.waterDirection][0], z = 8 + offsets[profile.waterDirection][1];
        List<?> cell = (List<?>) cellAt(x, 63, z);
        return List.of(x, 63, z, cell.get(0), cell.get(1));
    }

    static String longBits(long value) {
        return String.format(Locale.ROOT, "%016x", value);
    }

    static Object tick(qy value) throws Exception {
        return ConstructorJson.map("xyz", List.of(value.a, value.b, value.c), "block", value.d,
            "time", longBits(value.e), "ordinal", longBits((Long)PlayerDamageWakeObservation.get(value, qy.class, "g")));
    }

    static Object scheduled() throws Exception {
        // A passive static-field read avoids initializing/resetting the original
        // scheduler class merely to inspect its sequence before first active use.
        Class<?> type = Class.forName("sun.misc.Unsafe");
        Object unsafe = PlayerDamageWakeObservation.get(null, type, "theUnsafe");
        java.lang.reflect.Field counter = qy.class.getDeclaredField("f");
        boolean initialized = !(Boolean)type.getMethod("shouldBeInitialized", Class.class).invoke(unsafe, qy.class);
        Object base = type.getMethod("staticFieldBase", java.lang.reflect.Field.class).invoke(unsafe, counter);
        long offset = (Long)type.getMethod("staticFieldOffset", java.lang.reflect.Field.class).invoke(unsafe, counter);
        long sequence = (Long)type.getMethod("getLong", Object.class, long.class).invoke(unsafe, base, offset);
        List<Object> ordered = new ArrayList<>(), members = new ArrayList<>();
        for (Object entry : (java.util.Set<?>)PlayerDamageWakeObservation.get(PlayerDamageWakeObservation.world, fd.class, "E"))
            ordered.add(tick((qy)entry));
        for (Object entry : (java.util.Set<?>)PlayerDamageWakeObservation.get(PlayerDamageWakeObservation.world, fd.class, "F"))
            members.add(tick((qy)entry));
        return ConstructorJson.map("initialized", initialized, "sequence", longBits(sequence), "ordered", ordered, "members", members);
    }

    static Object state(Observer observer) throws Exception {
        Map<String, Object> result = new LinkedHashMap<>();
        for (Map.Entry<?, ?> entry : ((Map<?, ?>) PlayerDamageWakeObservation.state()).entrySet())
            result.put((String) entry.getKey(), entry.getValue());
        result.put("plant", cell(7, 8));
        result.put("column", column());
        result.put("world_o", PlayerDamageWakeObservation.world.o);
        result.put("water_cell", waterCell(observer.profile));
        result.put("scheduled", scheduled());
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
                "head", cell(8, 8), "plant", cell(7, 8), "column", column(),
                "world_o", PlayerDamageWakeObservation.world.o, "water_cell", waterCell(profile),
                "floor", cellAt(7, 63, 8),
                "sleeping", PlayerDamageWakeObservation.actor.u,
                "stack", stack(Thread.currentThread().getStackTrace())));
            if (profile.removalErrorY != 0 && x == 7 && y == profile.removalErrorY && z == 8)
                throw new IllegalStateException("Upright plants cell observer interruption");
        }
        public void a(sn value) {
            PlayerDamageWakeObservation.need(value instanceof hl && items.size() < 4, "bounded real Item publication");
            hl entity = (hl) value;
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.world.b.contains(value) &&
                entity.bF && entity.a != null, "published before observer");
            items.add(entity);
            try {
                events.add(ConstructorJson.map("kind", "added", "head", cell(8, 8), "plant", cell(7, 8), "column", column(),
                    "world_o", PlayerDamageWakeObservation.world.o, "water_cell", waterCell(profile),
                    "floor", cellAt(7, 63, 8),
                    "sleeping", PlayerDamageWakeObservation.actor.u, "item", item(entity),
                    "stack", stack(Thread.currentThread().getStackTrace())));
            } catch (ReflectiveOperationException failure) { throw new AssertionError(failure); }
            catch (Exception failure) { throw new AssertionError(failure); }
            if (profile.publicationErrorAt != 0 && items.size() == profile.publicationErrorAt)
                throw new IllegalStateException("Upright plants Item observer interruption");
        }
        public void b(sn value) { throw new AssertionError("unexpected removal"); }
        public void a(String n,double x,double y,double z,float v,float p) { throw new AssertionError("unexpected sound"); }
        public void a(String n,double x,double y,double z,double a,double b,double c) { throw new AssertionError("unexpected particle"); }
        public void b(int x,int y,int z,int a,int b,int c) {
            events.add(ConstructorJson.map("kind", "range", "xyz", List.of(x,y,z,a,b,c),
                "head", cell(8, 8), "plant", cell(7, 8), "column", column(),
                "floor", cellAt(7,63,8), "sleeping", PlayerDamageWakeObservation.actor.u,
                "world_o", PlayerDamageWakeObservation.world.o, "water_cell", waterCell(profile),
                "stack", stack(Thread.currentThread().getStackTrace())));
            if (profile.rangeError) throw new IllegalStateException("Upright plants range observer interruption");
        }
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
        List<Object> assignments = new ArrayList<>();
        for (int height = 0; height < profile.columns; ++height) {
            PlayerDamageWakeObservation.cell(7, 64 + height, 8, profile.id, profile.metadata);
            assignments.add(List.of(7, 64 + height, 8, profile.id, profile.metadata));
        }
        PlayerDamageWakeObservation.cell(7, 63, 8, profile.floor, 0);
        assignments.add(List.of(7, 63, 8, profile.floor, 0));
        if (profile.waterDirection != -1) {
            int[][] waterOffsets = {{-1,0},{1,0},{0,-1},{0,1}};
            int x = 7 + waterOffsets[profile.waterDirection][0], z = 8 + waterOffsets[profile.waterDirection][1];
            PlayerDamageWakeObservation.cell(x, 63, z, profile.waterId, 0);
            assignments.add(List.of(x, 63, z, profile.waterId, 0));
        }
        world.r.setSeed(profile.worldSeed);
        Object actualReceiver = receiver(uu.m[profile.id]);
        Object prepared = ConstructorJson.map("profile", profile.input(),
            "shared_setup", List.of(0, 0, 20, 0, 0),
            "post_setup_cell_assignments", assignments, "world_random_reseed", profile.worldSeed,
            "light_or_sky_getter_overrides", List.of());
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
        System.out.println("UPRIGHT_PLANTS_WAKE " + ConstructorJson.json(ConstructorJson.map("mode", profile.mode,
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
        System.out.println("UPRIGHT_PLANTS_WAKE_COMPLETE cases=21 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
    }
}
