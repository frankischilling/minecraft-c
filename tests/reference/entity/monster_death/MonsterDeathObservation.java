import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored death operands; all game constructors and reached methods are unchanged. */
public final class MonsterDeathObservation {
    static int network, nativeLinks, rejectedWrites, rows;
    static fd world;
    static lm chunk;
    static kq app;
    static xi writer;
    static Path outputRoot;
    static final List<hl> items = new ArrayList<>();
    static final List<String> sounds = new ArrayList<>();

    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }
    static String seed(Random random) throws Exception {
        return String.format(Locale.ROOT, "%016x",
            ((AtomicLong)field(Random.class, "seed").get(random)).get());
    }
    static Map<String,Object> map(Object... pairs) {
        Map<String,Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) result.put((String)pairs[i], pairs[i + 1]);
        return result;
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String)
            return "\"" + ((String)value).replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
        if (value instanceof Map) {
            StringBuilder result = new StringBuilder("{");
            for (Object item : ((Map<?,?>)value).entrySet()) {
                Map.Entry<?,?> entry = (Map.Entry<?,?>)item;
                if (result.length() > 1) result.append(',');
                result.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return result.append('}').toString();
        }
        if (value instanceof List) {
            StringBuilder result = new StringBuilder("[");
            for (Object item : (List<?>)value) {
                if (result.length() > 1) result.append(',');
                result.append(json(item));
            }
            return result.append(']').toString();
        }
        return value.toString();
    }
    @SuppressWarnings({"rawtypes", "unchecked"})
    static fd makeWorld() throws Exception {
        fd value = new fd(new wy(), "MonsterDeathReference", xa.a(0), 23L);
        Object provider = field(fd.class, "v").get(value);
        Map cache = (Map)field(ok.class, "e").get(provider);
        List loaded = (List)field(ok.class, "f").get(provider);
        cache.clear(); loaded.clear();
        chunk = new lm(value, new byte[32768], 0, 0);
        chunk.n = true;
        cache.put(yy.a(0, 0), chunk); loaded.add(chunk);
        return value;
    }
    static int membership(sn entity) {
        int result = 0;
        for (List<?> slice : chunk.m)
            for (Object item : slice) if (item == entity) ++result;
        return result;
    }
    static final class Observer implements pm {
        public void a(String n,double x,double y,double z,float volume,float pitch) { sounds.add(n); }
        public void a(sn entity) {
            need(entity instanceof hl && world.b.contains(entity) && membership(entity) == 1,
                 "actual Item publication to World and Chunk");
            need(!items.contains((hl)entity), "one callback per distinct Item");
            items.add((hl)entity);
        }
        public void a(String n,double x,double y,double z,double a,double b,double c) {throw new AssertionError("particle");}
        public void b(sn entity) {throw new AssertionError("removal");}
        public void a(int x,int y,int z) {throw new AssertionError("block");}
        public void b(int x,int y,int z,int a,int b,int c) {throw new AssertionError("range");}
        public void e() {throw new AssertionError("refresh");}
        public void a(String n,int x,int y,int z) {throw new AssertionError("record event");}
        public void a(int x,int y,int z,ow entity) {throw new AssertionError("TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e) {throw new AssertionError("level event");}
    }
    static void run(String kind, String mode, String sourceKind, boolean remote, int inputSeed)
        throws Exception {
        world = makeWorld();
        world.B = remote;
        gz actor = (gz)Class.forName(kind).getDeclaredConstructor(fd.class).newInstance(world);
        sn source = null;
        if (sourceKind.equals("cow")) source = new bx(world);
        if (sourceKind.equals("skeleton")) source = new fr(world);
        if (sourceKind.equals("player")) {
            source = new dc(app, world, app.k, 0);
            app.f = world; app.h = (dc)source;
            ((gs)source).g = 3;
            writer.a(jl.z, -writer.a(jl.z));
            need(!writer.a(ep.r) && !writer.a(ep.s),
                 "buildSword and killEnemy absent before callback");
        }
        actor.e(8.5, 64.5, 8.5);
        if (source != null) source.e(11.5, 64.5, 12.5);
        actor.Y = 4; actor.Z = 17; actor.E = 20; actor.by = 0;
        actor.aa = 3; actor.ab = 4; actor.ad = 7; actor.bb = false; actor.be = false;
        actor.aS = 30; actor.aP = .25; actor.aQ = -.375; actor.aR = -.125;
        field(ls.class, "au").setInt(actor, 0);
        field(ls.class, "av").setInt(actor, 17);
        field(ls.class, "T").setInt(actor, sourceKind.equals("player") ? 7 : -1);
        field(ls.class, "ah").setBoolean(actor, false);
        String constructorSeed = seed(actor.bs);
        actor.bs.setSeed(inputSeed);
        world.r.setSeed(2468);
        items.clear(); sounds.clear(); world.a(new Observer());
        Object answer = null;
        if (mode.equals("fatal")) answer = actor.a(source, 4);
        else actor.b(source);
        List<Object> dropped = new ArrayList<>();
        List<Object> entropy = new ArrayList<>();
        for (hl item : items) {
            dropped.add(List.of(item.a.c, item.a.a, field(iz.class, "d").getInt(item.a),
                                item.c, world.b.contains(item), membership(item)));
            entropy.add(map("private_random", seed(item.bs), "yaw", Float.floatToRawIntBits(item.aS),
                            "hover", Float.floatToRawIntBits(item.d),
                            "velocity", List.of(Double.doubleToRawLongBits(item.aP),
                                                Double.doubleToRawLongBits(item.aQ),
                                                Double.doubleToRawLongBits(item.aR))));
        }
        String profile = kind + "-" + mode + "-" + sourceKind + "-" + (remote ? "remote" : "local") + "-" + inputSeed;
        System.out.println("MONSTER_DEATH " + json(map(
            "profile", profile, "kind", kind, "mode", mode, "source", sourceKind,
            "remote", remote, "seed", inputSeed, "result", answer,
            "dying", field(ls.class, "ah").getBoolean(actor), "dead", actor.be,
            "health", actor.Y, "random", seed(actor.bs), "items", dropped,
            "sounds", new ArrayList<>(sounds),
            "player", sourceKind.equals("player") ? List.of(((gs)source).g, writer.a(jl.z), writer.a((vr)ep.s)) : null)));
        System.out.println("MONSTER_DEATH_ENTROPY " + json(map("profile", profile,
            "constructor_random", constructorSeed, "items", entropy)));
        ++rows;
    }
    static void origin(Class<?> type, Path archive) throws Exception {
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),
             "original archive origin " + type.getName());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length == 2, "original JAR and private output root");
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath();
        outputRoot = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h,int p) {++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p) {++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n) {++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p) {
                if (!Path.of(p).toAbsolutePath().normalize().startsWith(outputRoot)) {
                    ++rejectedWrites;throw new SecurityException("write outside private root denied");
                }
            }
            @Override public void checkDelete(String p) {++rejectedWrites;throw new SecurityException("delete denied");}
        });
        for (Class<?> type : new Class<?>[]{fd.class,wy.class,xa.class,gr.class,sn.class,ls.class,
                 ii.class,gz.class,uz.class,cn.class,fr.class,gb.class,bx.class,hl.class,iz.class,
                 ok.class,lm.class,yy.class,gm.class,dc.class,gs.class,kq.class,xi.class,ep.class,jl.class})
            origin(type, jar);
        new gr("MonsterDeathReference", "");
        try {
            for (String kind : new String[]{"uz", "cn", "fr", "gb"})
                for (int value : new int[]{0, 2, 3}) run(kind, "direct", "null", false, value);
            for (String kind : new String[]{"uz", "cn", "fr", "gb"})
                run(kind, "fatal", "cow", false, 38);
            for (boolean remote : new boolean[]{false, true})
                for (int value : new int[]{0, 2}) run("gb", "direct", "skeleton", remote, value);
            app = new kq(null,null,null,854,480,false,null);
            app.k = new gr("MonsterDeathPlayer", "-");
            field(net.minecraft.client.Minecraft.class, "Z").set(app, outputRoot.toFile());
            writer = new xi(app.k, outputRoot.toFile()); app.I = writer;
            for (String kind : new String[]{"uz", "fr"}) run(kind, "fatal", "player", false, 38);
            need(rows == 22 && network == 0 && nativeLinks == 0 && rejectedWrites == 0,
                 "finite observation and external-effect guards");
            System.out.println("Original Monster death: 22 rows; network=0; native=0; rejected_write=0; genuine_world_chunk=1");
        } finally {
            if (app != null) app.J = false;
            for (Thread thread : Thread.getAllStackTraces().keySet())
                if (thread.getName().equals("Timer hack thread")) {
                    thread.interrupt(); thread.join(2000);
                    need(!thread.isAlive(), "actual Timer worker retirement");
                }
        }
    }
}
