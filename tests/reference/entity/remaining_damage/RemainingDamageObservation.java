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

/** Authored operands around unchanged original constructors and damage methods. */
public final class RemainingDamageObservation {
    static int network, nativeLinks, rejectedWrites, rows;
    static fd world;
    static final List<lm> chunks = new ArrayList<>();
    static final List<hl> items = new ArrayList<>();
    static final List<Object> sounds = new ArrayList<>();
    static Path outputRoot;

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
    static String fword(float value) {
        return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value));
    }
    static String dword(double value) {
        return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value));
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
        fd value = new fd(new wy(), "RemainingDamageReference", xa.a(0), 23L);
        Object provider = field(fd.class, "v").get(value);
        Map cache = (Map)field(ok.class, "e").get(provider);
        List loaded = (List)field(ok.class, "f").get(provider);
        cache.clear(); loaded.clear(); chunks.clear();
        for (int x = -1; x <= 1; ++x) for (int z = -1; z <= 1; ++z) {
            lm chunk = new lm(value, new byte[32768], x, z);
            chunk.n = true;
            cache.put(yy.a(x, z), chunk); loaded.add(chunk); chunks.add(chunk);
        }
        need(cache.size() == 9 && loaded.size() == 9, "nine genuine cached Chunks");
        return value;
    }
    static int membership(sn entity) {
        int result = 0;
        for (lm chunk : chunks) for (List<?> slice : chunk.m)
            for (Object item : slice) if (item == entity) ++result;
        return result;
    }
    static final class Observer implements pm {
        public void a(String n,double x,double y,double z,float volume,float pitch) {
            sounds.add(map("name", n, "volume", fword(volume), "pitch", fword(pitch)));
        }
        public void a(sn entity) {
            need(entity instanceof hl && world.b.contains(entity) && membership(entity) == 1,
                 "actual Item publication to World and one Chunk");
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
    static void run(String kind, int size, int amount) throws Exception {
        world = makeWorld();
        world.B = false; world.q = 1;
        ls actor = (ls)Class.forName(kind).getDeclaredConstructor(fd.class).newInstance(world);
        if (size != 0) ((uw)actor).e(size);
        bx source = new bx(world);
        actor.e(8.5, 64.5, 8.5); source.e(11.5, 64.5, 12.5);
        actor.Y = 20; actor.Z = 17; actor.E = 20; actor.by = 0;
        actor.aa = 3; actor.ab = 4; actor.ad = 7; actor.bb = false; actor.be = false;
        actor.aS = 30; actor.aP = .25; actor.aQ = -.375; actor.aR = -.125;
        field(ls.class, "au").setInt(actor, 0);
        field(ls.class, "av").setInt(actor, 17);
        field(ls.class, "T").setInt(actor, -1);
        field(ls.class, "ah").setBoolean(actor, false);
        String constructorSeed = seed(actor.bs);
        actor.bs.setSeed(38); world.r.setSeed(2468);
        items.clear(); sounds.clear(); world.a(new Observer());
        boolean answer = actor.a(source, amount);
        List<Object> dropped = new ArrayList<>();
        List<Object> entropy = new ArrayList<>();
        for (hl item : items) {
            dropped.add(List.of(item.a.c, item.a.a, field(iz.class, "d").getInt(item.a),
                                item.c, world.b.contains(item), membership(item)));
            entropy.add(map("private_random", seed(item.bs), "yaw", fword(item.aS),
                            "hover", fword(item.d),
                            "velocity", List.of(dword(item.aP),dword(item.aQ),dword(item.aR))));
        }
        String profile = kind + "-size" + size + "-damage" + amount;
        System.out.println("REMAINING_DAMAGE " + json(map(
            "profile", profile, "kind", kind, "size", size, "amount", amount, "seed", 38,
            "result", answer, "health", actor.Y, "previous_health", actor.Z,
            "resistance", actor.by, "last_damage", field(ls.class,"au").getInt(actor),
            "hurt", actor.aa, "max_hurt", actor.ab, "angle", fword(actor.ac),
            "velocity", List.of(dword(actor.aP),dword(actor.aQ),dword(actor.aR)),
            "random", seed(actor.bs), "dying", field(ls.class,"ah").getBoolean(actor),
            "dead", actor.be, "sounds", new ArrayList<>(sounds), "items", dropped)));
        System.out.println("REMAINING_DAMAGE_ENTROPY " + json(map("profile", profile,
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
                 ii.class,bg.class,ar.class,wq.class,gi.class,uw.class,xt.class,bp.class,bx.class,
                 hl.class,iz.class,ok.class,lm.class,yy.class,gm.class}) origin(type, jar);
        new gr("RemainingDamageReference", "");
        for (int size : new int[]{1,4}) for (int amount : new int[]{4,20}) run("uw",size,amount);
        for (String kind : new String[]{"xt","bp"}) for (int amount : new int[]{4,20}) run(kind,0,amount);
        run("gi",0,4); run("gi",0,40);
        need(rows == 10 && network == 0 && nativeLinks == 0 && rejectedWrites == 0,
             "finite observation and external-effect guards");
        System.out.println("Original remaining damage: 10 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9");
    }
}
