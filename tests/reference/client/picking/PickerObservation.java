import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import sun.reflect.ReflectionFactory;

/** Prepared PX/App body observation; all invoked game bodies remain unchanged. */
public final class PickerObservation {
    static final String JAR_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static fd world;
    static lm[] chunks;
    static dc camera, player;
    static yl cart;
    static ju falling, second;
    static hl item;
    static kq app;
    static px renderer;
    static os ordinary;
    static pj creative;
    static final IdentityHashMap<Object, String> actors = new IdentityHashMap<>();
    static int networkAttempts;
    static int nativeLinkAttempts;

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    static Field field(Class<?> owner, String name) throws Exception {
        Field result = owner.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    static <T> T prepared(Class<T> type) throws Exception {
        Constructor<Object> object = Object.class.getDeclaredConstructor();
        Constructor<?> constructor = ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type, object);
        constructor.setAccessible(true);
        return type.cast(constructor.newInstance());
    }

    static String bits(double value) { return String.format("%016x", Double.doubleToRawLongBits(value)); }
    static String fbits(float value) { return String.format("%08x", Float.floatToRawIntBits(value)); }
    static List<String> xyz(double x, double y, double z) { return List.of(bits(x), bits(y), bits(z)); }
    static Map<String, Object> map(Object... pairs) {
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) result.put((String)pairs[i], pairs[i + 1]);
        return result;
    }

    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) return "\"" + ((String)value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        if (value instanceof Number || value instanceof Boolean) return value.toString();
        List<String> parts = new ArrayList<>();
        if (value instanceof Map) {
            for (Object entry : ((Map<?, ?>)value).entrySet()) {
                Map.Entry<?, ?> pair = (Map.Entry<?, ?>)entry;
                parts.add(json(pair.getKey()) + ":" + json(pair.getValue()));
            }
            return "{" + String.join(",", parts) + "}";
        }
        if (value instanceof List) {
            for (Object entry : (List<?>)value) parts.add(json(entry));
            return "[" + String.join(",", parts) + "]";
        }
        throw new AssertionError("unsupported observer JSON " + value.getClass());
    }

    static String actor(Object value) {
        if (value == null) return null;
        String name = actors.get(value);
        require(name != null, "only authored genuine Entity identities");
        return name;
    }

    static int vectorIndex(Object value) throws Exception {
        if (value == null) return -1;
        List<?> pool = (List<?>)field(bt.class, "d").get(null);
        for (int i = 0; i < pool.size(); ++i) if (pool.get(i) == value) return i;
        throw new AssertionError("actual pooled result Vec3 identity");
    }

    static Object hit(vf value) throws Exception {
        return value == null ? null : map("kind", value.a.name(),
            "coords", List.of(value.b, value.c, value.d), "face", value.e,
            "vector_slot", vectorIndex(value.f), "vector_bits", xyz(value.f.a, value.f.b, value.f.c),
            "entity", actor(value.g));
    }

    static Object actorState(sn value) {
        return map("name", actor(value), "class", value.getClass().getName(), "dead", value.be,
            "pickable", value.h_(), "border_bits", fbits(value.m_()),
            "position_bits", xyz(value.aM, value.aN, value.aO),
            "bounds_bits", List.of(bits(value.aW.a), bits(value.aW.b), bits(value.aW.c),
                                   bits(value.aW.d), bits(value.aW.e), bits(value.aW.f)));
    }

    static void reset() throws Exception {
        for (lm chunk : chunks) {
            Arrays.fill(chunk.b, (byte)0);
            Arrays.fill(chunk.e.a, (byte)0);
            for (List<?> slice : chunk.m) slice.clear();
        }
        world.b.clear();
        world.d.clear();
        ((List<?>)field(fd.class, "R").get(world)).clear();
        for (sn entity : new sn[]{camera, player, cart, falling, second, item}) {
            entity.be = false;
            entity.bF = false;
            entity.e(8.5, 64.5, 10.5);
        }
        camera.bf = 0;
        camera.e(8.5, 64.5, 8.5);
        camera.aJ = camera.aM; camera.aK = camera.aN; camera.aL = camera.aO;
        camera.aS = camera.aT = camera.aU = camera.aV = 0;
        app.i = camera; app.h = camera; app.f = world; app.c = ordinary;
        app.y = null;
        field(px.class, "m").set(renderer, cart);
        eq.b(); bt.b();
    }

    static void block(int z) {
        chunks[0].b[(8 << 11) | (z << 7) | 64] = 1;
    }

    static void publish(sn... entities) {
        for (sn entity : entities) require(world.b(entity), "actual World insertion returns true");
    }

    static void row(String name, float partial) throws Exception {
        List<Object> inputs = new ArrayList<>();
        for (sn entity : new sn[]{camera, player, cart, falling, second, item}) inputs.add(actorState(entity));
        int before = field(bt.class, "e").getInt(null);
        Object oldHit = app.y;
        Object oldEntity = field(px.class, "m").get(renderer);
        renderer.a(partial); // The unchanged original PX.a(F), never an observer algorithm.
        List<?> pool = (List<?>)field(bt.class, "d").get(null);
        int cursor = field(bt.class, "e").getInt(null);
        List<Object> vectors = new ArrayList<>();
        for (int i = 0; i < cursor; ++i) {
            bt value = (bt)pool.get(i);
            vectors.add(map("slot", i, "bits", xyz(value.a, value.b, value.c)));
        }
        List<Object> nearby = new ArrayList<>();
        for (Object entity : (List<?>)field(fd.class, "R").get(world)) nearby.add(actor(entity));
        System.out.println(json(map("case", name, "partial_bits", fbits(partial),
            "controller", app.c.getClass().getName(), "reach_bits", fbits(app.c.b()),
            "camera_null", app.i == null, "world_null", app.f == null,
            "actors", inputs, "nearby_order", nearby, "app_y", hit(app.y),
            "px_m", actor(field(px.class, "m").get(renderer)),
            "same_hit_as_before", app.y == oldHit,
            "same_m_as_before", field(px.class, "m").get(renderer) == oldEntity,
            "bt_cursor_before", before, "bt_cursor_after", cursor, "bt_pool_size", pool.size(),
            "bt_pool_identity", System.identityHashCode(pool), "vectors", vectors,
            "start_slot0_after_bits", cursor == 0 ? null : xyz(((bt)pool.get(0)).a, ((bt)pool.get(0)).b, ((bt)pool.get(0)).c))));
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 2, "JAR and private copied authored World");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        String before = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)));
        require(before.equals(JAR_SHA), "original JAR exact pin");
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++networkAttempts; throw new SecurityException("observer denies network"); }
            @Override public void checkListen(int port) { ++networkAttempts; throw new SecurityException("observer denies listeners"); }
            @Override public void checkLink(String library) { ++nativeLinkAttempts; throw new SecurityException("observer denies native library links: " + library); }
            @Override public void checkWrite(String file) {
                require(Path.of(file).toAbsolutePath().normalize().startsWith(directory), "writes confined to private copied World");
            }
        });
        world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        chunks = new lm[]{world.c(0, 0), world.c(0, 1), world.c(1, 0), world.c(1, 1)};
        require(((Map<?, ?>)field(world.v.getClass(), "e").get(world.v)).size() == 4, "four genuine loaded Chunks");
        gr session = new gr(null, null);
        camera = new dc(null, world, session, 0);
        player = new dc(null, world, session, 0);
        cart = new yl(world, 8.5, 64.5, 10.5, 0);
        falling = new ju(world, 8.5, 64.5, 10.5, 13);
        second = new ju(world, 8.5, 64.5, 10.5, 13);
        item = new hl(world, 8.5, 64.5, 10.5, new iz(1, 1, 0));
        actors.put(camera, "camera"); actors.put(player, "player"); actors.put(cart, "cart");
        actors.put(falling, "fallingA"); actors.put(second, "fallingB"); actors.put(item, "item");
        app = prepared(kq.class);
        renderer = prepared(px.class);
        field(px.class, "j").set(renderer, app);
        ordinary = new os(app);
        creative = new pj(app);
        for (Class<?> type : new Class<?>[]{fd.class, lm.class, dc.class, gs.class, ls.class, sn.class,
                yl.class, ju.class, hl.class, gr.class, kq.class, net.minecraft.client.Minecraft.class,
                px.class, ob.class, os.class, pj.class, bt.class, eq.class, vf.class, jg.class, uu.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "unchanged archive origin " + type);
        reset(); row("empty", 1);
        reset(); block(10); row("block_near", 1);
        reset(); block(12); row("block_os4_beyond_entity3", 1);
        reset(); block(13); row("block_beyond_os4", 1);
        reset(); publish(falling); row("falling_hit", 1);
        reset(); publish(cart); row("minecart_hit", 1);
        reset(); player.bf = 0; player.e(8.5, 64.5, 10.5); publish(player); row("player_hit", 1);
        reset(); block(10); falling.e(8.5, 64.5, 11.5); publish(falling); row("entity_behind_block", 1);
        reset(); block(10); falling.e(8.5, 64.5, 9.5); publish(falling); row("entity_before_block", 1);
        reset(); falling.e(8.5, 64.5, 8.5); publish(falling); row("inside_box", 1);
        reset(); publish(camera); row("excluded_self", 1);
        reset(); falling.be = true; publish(falling); row("dead_not_pickable", 1);
        reset(); publish(item); row("item_not_pickable", 1);
        reset(); publish(falling, second); row("equal_distance_first_A", 1);
        reset(); publish(second, falling); row("equal_distance_first_B", 1);
        reset(); publish(falling, second, falling); row("duplicate_order_ABA", 1);
        reset(); falling.e(8.5, 64.5, 8.5); second.e(8.5, 64.5, 8.5); publish(falling, second); row("inside_order_AB", 1);
        reset(); falling.e(8.5, 64.5, 8.5); second.e(8.5, 64.5, 8.5); publish(second, falling); row("inside_order_BA", 1);
        reset(); falling.e(8.5, 64.5, 8.5); publish(falling, second); row("inside_then_far", 1);
        reset(); falling.e(8.5, 64.5, 8.5); publish(second, falling); row("far_then_inside", 1);
        reset(); falling.e(8.5, 64.5, 12.0); publish(falling); row("entity_before_cap3", 1);
        reset(); falling.e(8.5, 64.5, 12.25); publish(falling); row("entity_beyond_cap3", 1);
        reset(); block(12); falling.e(8.5, 64.5, 12.25); publish(falling); row("block4_entity3_cap", 1);
        reset(); app.c = creative; falling.e(8.5, 64.5, 22.0); publish(falling); row("pj_m_without_entity_vf", 1);
        reset(); camera.aJ = 7.5; camera.aU = 90; row("finite_partial_half", .5f);
        reset(); app.y = new vf(falling); app.i = null; row("early_null_camera", 1);
        reset(); app.y = new vf(falling); app.f = null; row("early_null_world", 1);
        require(networkAttempts == 0, "no network or listener attempts");
        require(nativeLinkAttempts == 0, "no native library link attempts");
        List<Object> loadedChunks = new ArrayList<>();
        for (Object value : ((Map<?, ?>)field(world.v.getClass(), "e").get(world.v)).values()) {
            lm chunk = (lm)value;
            loadedChunks.add(List.of(chunk.j, chunk.k));
        }
        String after = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)));
        require(before.equals(after), "unchanged JAR after original invocation");
        System.out.println(json(map("complete", true, "cases", 27, "jar_sha256", after,
            "java", System.getProperty("java.runtime.version"), "network_attempts", networkAttempts,
            "native_link_attempts", nativeLinkAttempts,
            "loaded_chunks_after", loadedChunks,
            "scope", "unchanged PX.a(F) prepared PX/kq body; genuine World/Chunks/Player/targets; no App/PX constructor or normal-startup claim")));
    }
}
