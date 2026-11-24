import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;

/** Real type14 dispatcher and CPU NW output on explicitly prepared original owners. */
public final class BedObservation {
    static int network, links, execs;
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void need(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String word(double value) {
        return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length == 2, "jar private-world");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            public void checkListen(int port) { ++network; throw new SecurityException(); }
            public void checkLink(String name) { ++links; throw new SecurityException(); }
            public void checkExec(String name) { ++execs; throw new SecurityException(); }
            public void checkWrite(String name) {
                need(Path.of(name).toAbsolutePath().normalize().startsWith(directory), "private World writes");
            }
            public void checkDelete(String name) { checkWrite(name); }
        });
        for (Class<?> type : new Class<?>[]{cv.class, ew.class, fd.class, lm.class, nw.class,
                uu.class, ve.class, jj.class, in.class})
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath()
                 .equals(jar), "unchanged class origin");
        fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        lm[] chunks = {world.c(0,0), world.c(0,1), world.c(1,0), world.c(1,1)};
        ew cache = new ew(world, 0, 0, 0, 31, 127, 31);
        field(fd.class, "f").setInt(world, 3);
        nw tess = nw.a; tess.b();
        uu block = uu.m[26]; need(block instanceof ve, "genuine Bed");
        String[] colors = {"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "aa"};
        for (int metadata = 0; metadata < 16; ++metadata)
            for (int override : new int[]{-1,17})
                for (int profile = 0; profile < 6; ++profile) {
                    Arrays.fill(chunks[0].b, (byte)0); Arrays.fill(chunks[0].e.a, (byte)0);
                    Arrays.fill(chunks[0].f.a, (byte)255); Arrays.fill(chunks[0].g.a, (byte)0);
                    chunks[0].b[(8 << 11) | (8 << 7) | 64] = 26;
                    chunks[0].e.a(8,64,8,metadata);
                    if (profile == 1 || profile == 2)
                        for (int[] neighbor : new int[][]{{8,7},{8,9},{7,8},{9,8}})
                            chunks[0].b[(neighbor[0] << 11) | (neighbor[1] << 7) | 64] = 1;
                    cv renderer = new cv(cache);
                    field(cv.class,"f").setBoolean(renderer,profile == 2);
                    field(cv.class,"d").setInt(renderer,override);
                    field(cv.class,"e").setBoolean(renderer,true);
                    field(cv.class,"m").setBoolean(renderer,profile == 3);
                    for (int i = 0; i < colors.length; ++i)
                        field(cv.class, colors[i]).setFloat(renderer, (i+1)/16.0f);
                    int[] saved = null; int savedTop = 0;
                    if (profile == 4) { saved = jj.a.clone(); Arrays.fill(jj.a,0); }
                    if (profile == 5) { savedTop = jj.c[metadata & 3][1]; jj.c[metadata & 3][1] = 2; }
                    int before = field(nw.class,"o").getInt(tess);
                    boolean returned = renderer.b(block,8,64,8);
                    if (profile == 4) System.arraycopy(saved,0,jj.a,0,4);
                    if (profile == 5) jj.c[metadata & 3][1] = savedTop;
                    int after = field(nw.class,"o").getInt(tess);
                    int count = (after-before)/8;
                    need(returned && count == (profile == 1 ? 12 : profile == 4 ? 36 : 30),
                         "exact NW triangle words metadata="+metadata+" profile="+profile+" count="+count);
                    StringBuilder row = new StringBuilder("BED\t" + metadata + "\t" + override + "\t" + profile
                        + "\t" + returned + "\t" + field(cv.class,"d").getInt(renderer)
                        + "\t" + field(cv.class,"e").getBoolean(renderer)
                        + "\t" + field(cv.class,"m").getBoolean(renderer) + "\t" + count + "\t"
                        + word(block.bs)+","+word(block.bt)+","+word(block.bu)+","+word(block.bv)+","+word(block.bw)+","+word(block.bx) + "\t");
                    int[] words = (int[])field(nw.class,"g").get(tess);
                    for (int i=before;i<after;++i) {
                        if(i!=before) row.append(',');
                        row.append(String.format(Locale.ROOT,"%08x",words[i]));
                    }
                    System.out.println(row);
                }
        need(network == 0 && links == 0 && execs == 0, "no external effects");
        System.out.println("BED_COMPLETE calls=192 network=0 native=0 exec=0");
    }
}
