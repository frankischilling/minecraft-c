import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;
import net.minecraft.client.Minecraft;

/** Unchanged type12 dispatcher/body, with explicit CPU-only application prestate. */
public final class LeverObservation {
    static int network, links, execs;
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
            .invoke(field(unsafe, "theUnsafe").get(null), type);
    }
    static void require(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
    static String d(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        require(args.length == 2, "jar and private world");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            public void checkListen(int port) { ++network; throw new SecurityException(); }
            public void checkLink(String name) { ++links; throw new SecurityException(); }
            public void checkExec(String name) { ++execs; throw new SecurityException(); }
            public void checkWrite(String name) { require(Path.of(name).toAbsolutePath().normalize().startsWith(directory), "private World writes"); }
            public void checkDelete(String name) { checkWrite(name); }
        });
        for (Class<?> type : new Class<?>[]{cv.class, ew.class, fd.class, lm.class, nw.class,
                uu.class, xr.class, bt.class, in.class, Minecraft.class, kq.class, kv.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "unchanged class origin");
        fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        lm[] chunks = {world.c(0,0), world.c(0,1), world.c(1,0), world.c(1,1)};
        ew cache = new ew(world, 0, 0, 0, 31, 127, 31);
        field(fd.class, "f").setInt(world, 3);
        Minecraft app = (Minecraft)raw(kq.class);
        kv settings = (kv)raw(kv.class); settings.k = false; app.z = settings;
        field(Minecraft.class, "a").set(null, app);
        nw tess = nw.a; tess.b();
        uu block = uu.m[69]; require(block instanceof xr, "genuine Lever");
        for (int metadata = 0; metadata < 16; ++metadata) for (int override : new int[]{-1,17}) {
            Arrays.fill(chunks[0].b, (byte)0); Arrays.fill(chunks[0].e.a, (byte)0);
            Arrays.fill(chunks[0].f.a, (byte)255); Arrays.fill(chunks[0].g.a, (byte)0);
            chunks[0].b[(8 << 11) | (8 << 7) | 64] = 69;
            chunks[0].e.a(8,64,8,metadata);
            cv renderer = new cv(cache);
            field(cv.class,"f").setBoolean(renderer,true);
            field(cv.class,"d").setInt(renderer,override);
            int before = field(nw.class,"o").getInt(tess), cursor = field(bt.class,"e").getInt(null);
            boolean returned = renderer.b(block,8,64,8);
            int after = field(nw.class,"o").getInt(tess);
            StringBuilder row = new StringBuilder("LEVER\t" + metadata + "\t" + override + "\t" + returned
                + "\t" + field(cv.class,"d").getInt(renderer) + "\t" + field(cv.class,"m").getBoolean(renderer)
                + "\t" + (field(bt.class,"e").getInt(null)-cursor) + "\t" + d(block.bs)+","+d(block.bt)+","+d(block.bu)+","+d(block.bv)+","+d(block.bw)+","+d(block.bx) + "\t");
            int[] words = (int[])field(nw.class,"g").get(tess);
            for (int i=before;i<after;++i) { if(i!=before)row.append(',');row.append(String.format(Locale.ROOT,"%08x",words[i])); }
            System.out.println(row);
        }
        require(network == 0 && links == 0 && execs == 0, "no network/native/exec");
        System.out.println("LEVER_COMPLETE calls=32 network=0 native=0 exec=0");
    }
}
