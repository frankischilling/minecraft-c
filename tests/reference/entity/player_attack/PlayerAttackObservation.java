import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;
import java.util.Map;

/** Authored scenes and observations of unchanged original Player attack calls. */
public final class PlayerAttackObservation {
    static kq app;
    static xi writer;
    static int network, nativeLinks, drops;
    static fd world;
    static lm chunk;

    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name);
        value.setAccessible(true);
        return value;
    }
    static void require(boolean value, String text) {
        if (!value) throw new AssertionError(text);
    }
    static int bit(boolean value) { return value ? 1 : 0; }
    @SuppressWarnings({"rawtypes", "unchecked"})
    static fd makeWorld() throws Exception {
        fd value = new fd(new wy(), "PlayerAttackReference", xa.a(0), 23L);
        Object provider = field(fd.class, "v").get(value);
        Map cache = (Map)field(ok.class, "e").get(provider);
        List loaded = (List)field(ok.class, "f").get(provider);
        cache.clear(); loaded.clear();
        chunk = new lm(value, new byte[32768], 0, 0);
        chunk.n = true;
        cache.put(yy.a(0, 0), chunk); loaded.add(chunk);
        return value;
    }
    static final class Observer implements pm {
        public void a(String n,double x,double y,double z,float volume,float pitch) {}
        public void a(sn value) {
            require(value instanceof hl && world.b.contains(value), "actual published dropped Item");
            ++drops;
        }
        public void a(String n,double x,double y,double z,double a,double b,double c) {throw new AssertionError("particle");}
        public void b(sn value) {throw new AssertionError("removal");}
        public void a(int x,int y,int z) {throw new AssertionError("block");}
        public void b(int x,int y,int z,int a,int b,int c) {throw new AssertionError("range");}
        public void e() {throw new AssertionError("refresh");}
        public void a(String n,int x,int y,int z) {throw new AssertionError("record");}
        public void a(int x,int y,int z,ow value) {throw new AssertionError("TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e) {throw new AssertionError("event");}
    }
    static void run(String profile) throws Exception {
        world = makeWorld(); world.B = false;
        dc player = new dc(app, world, app.k, 0);
        app.f = world; app.h = player;
        player.e(.5, 64, .5); player.aP = player.aQ = player.aR = 0;
        player.g = 3;
        ls target = profile.equals("sheep-fatal") ? new dl(world) : new bx(world);
        target.e(1.5, 64, .5); target.aS = 0;
        target.aP = target.aQ = target.aR = 0;
        target.E = 20; target.Y = profile.equals("sheep-fatal") ? 1 : 10;
        target.Z = 10; target.by = 0; target.aa = target.ab = 0;
        field(ls.class,"au").setInt(target,0);
        field(ls.class,"av").setInt(target,0);
        field(ls.class,"ah").setBoolean(target,false);
        field(ls.class,"T").setInt(target,profile.equals("sheep-fatal") ? 7 : 0);
        target.bs.setSeed(38); world.r.setSeed(38);
        if (profile.equals("cow-falling")) player.aQ = -.1;
        if (profile.equals("cow-resisted")) {
            target.by = 20;
            field(ls.class,"au").setInt(target,1);
        }
        gi wolf = null;
        if (profile.equals("cow-with-owned-wolf")) {
            wolf = new gi(world); wolf.e(2.5,64,.5);
            wolf.d(true); wolf.b(false); wolf.a(player.l);
            require(world.b(wolf), "ordinary Wolf publication");
        }
        world.a(new Observer()); drops = 0;
        writer.a(jl.w, -writer.a(jl.w)); writer.a(jl.z, -writer.a(jl.z));
        require(player.c.b() == null, "actual empty selected Stack");
        new ob(app).b(player, target);
        System.out.println("PLAYER_ATTACK profile=" + profile +
            " health=" + target.Y + " previous-health=" + target.Z +
            " hurt=" + target.by + " hurt-time=" + target.aa + " max-hurt=" + target.ab +
            " last-damage=" + field(ls.class,"au").getInt(target) +
            " idle=" + field(ls.class,"av").getInt(target) +
            " dying=" + bit(field(ls.class,"ah").getBoolean(target)) +
            " score=" + player.g + " mob-kills=" + writer.a(jl.z) +
            " damage-dealt=" + writer.a(jl.w) + " drops=" + drops +
            " wolf-flags=" + (wolf == null ? -1 : ((Number)wolf.bD.a(16)).intValue()) +
            " wolf-target=" + (wolf == null ? -1 : bit(wolf.G() == target)));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toAbsolutePath();
        Files.createDirectories(directory);
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listener");}
            public void checkLink(String library) {++nativeLinks;throw new SecurityException("native link");}
        });
        for (String name : new String[]{"kq","dc","gs","ix","ob","fd","ok","lm","ls","sn","bx","dl","gi","xi","jl"}) {
            Class<?> type = Class.forName(name, false, PlayerAttackObservation.class.getClassLoader());
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "unchanged archive origin " + name);
        }
        for (String name : new String[]{"cn","uz","fr","gb","uw","wh","dl","bx","ww","gi","xt"}) {
            Class<?> type = Class.forName(name, false, PlayerAttackObservation.class.getClassLoader());
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "original Mob class origin " + name);
            require(type.getMethod("h_").getDeclaringClass() == ls.class &&
                    type.getMethod("m_").getDeclaringClass() == sn.class,
                    "actual inherited Mob pickability resolution " + name);
        }
        app = new kq(null,null,null,854,480,false,null);
        try {
            require(uu.ac.getClass() == ee.class &&
                    ee.class.getMethod("b", int.class).getDeclaringClass() == uu.class &&
                    ee.class.getMethod("g").getDeclaringClass() == uu.class,
                    "actual inherited Wool inventory color and shape methods");
            for (int metadata = 0; metadata < 16; ++metadata)
                require(uu.ac.b(metadata) == 0xffffff, "actual Wool inventory color " + metadata);
            uu.ac.g();
            require(gm.c[35].getClass() == bi.class &&
                    bi.class.getMethod("f", int.class).getDeclaringClass() == gm.class &&
                    bi.class.getMethod("b").getDeclaringClass() == gm.class &&
                    bi.class.getMethod("c").getDeclaringClass() == gm.class,
                    "actual inherited Wool Item render methods");
            StringBuilder icons = new StringBuilder("WOOL_ITEM_ICONS ");
            for (int metadata = 0; metadata < 16; ++metadata) {
                if (metadata != 0) icons.append(',');
                icons.append(gm.c[35].a(metadata));
                require(gm.c[35].f(metadata) == 0xffffff, "actual Wool Item color " + metadata);
            }
            System.out.println(icons);
            app.k = new gr("AttackPlayer", "-");
            field(net.minecraft.client.Minecraft.class,"Z").set(app,directory.toFile());
            writer = new xi(app.k,directory.toFile()); app.I = writer;
            for (String profile : new String[]{"cow-normal","cow-falling","cow-resisted","sheep-fatal","cow-with-owned-wolf"})
                run(profile);
            require(network == 0 && nativeLinks == 0, "CPU observation external effects");
            System.out.println("Original Player attack: 5 rows; network=0; native=0; genuine_owners=1");
        } finally {
            app.J = false;
            for (Thread thread : Thread.getAllStackTraces().keySet())
                if (thread.getName().equals("Timer hack thread")) {
                    thread.interrupt(); thread.join(2000);
                    require(!thread.isAlive(), "Timer worker retirement");
                }
        }
    }
}
