import java.nio.file.Path;
import java.security.Permission;

/** Authored cached-World operands around the unchanged original Ladder body. */
public final class LadderNeighborObservation {
    static final int x = 12, y = 64, z = 8;
    static int rows;

    static void cell(int x, int z, int id, int metadata) {
        lm chunk = MetadataSetObservation.chunk;
        int offset = (x << 11) | (z << 7) | y;
        chunk.b[offset] = (byte)id;
        int packed = offset >> 1;
        chunk.e.a[packed] = (byte)((chunk.e.a[packed] & 0xf0) | (metadata & 15));
    }

    static void run(int metadata, int missing) throws Exception {
        fd world = MetadataSetObservation.world;
        lm chunk = MetadataSetObservation.chunk;
        int[] xs = {12,12,13,11}, zs = {9,7,8,8};
        for (int i = 0; i < 4; ++i) cell(xs[i], zs[i], i == missing ? 0 : 1, 0);
        cell(x, z, 65, metadata);
        chunk.o = false;
        MetadataSetObservation.events = 0;
        long before = MetadataSetObservation.seed(world.r);
        boolean returned = false;
        String exception = null;
        try {
            uu.m[65].b(world, x, y, z, 923);
            returned = true;
        } catch (RuntimeException failure) {
            exception = failure.getClass().getName().replace('.', '/');
        }
        String profile = missing < 0 ? "all-walls-" + metadata : "missing-wall-" + metadata;
        System.out.println("LADDER_NEIGHBOR " + MetadataSetObservation.json(MetadataSetObservation.map(
            "profile", profile, "metadata", metadata, "missing", missing,
            "returned", returned, "exception", exception,
            "id_after", world.a(x,y,z), "metadata_after", world.e(x,y,z),
            "dirty_after", chunk.o, "world_random_unchanged", before == MetadataSetObservation.seed(world.r))));
        System.out.println("LADDER_NEIGHBOR_EVENTS " + MetadataSetObservation.json(MetadataSetObservation.map(
            "profile", profile, "events", MetadataSetObservation.events)));
        ++rows;
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        MetadataSetObservation.need(args.length == 2, "archive and private output root");
        Path jar = Path.of(args[0]).toRealPath();
        Path root = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
            @Override public void checkConnect(String h,int p) {
                ++MetadataSetObservation.network; throw new SecurityException("network denied");
            }
            @Override public void checkListen(int p) {
                ++MetadataSetObservation.network; throw new SecurityException("listen denied");
            }
            @Override public void checkLink(String n) {
                ++MetadataSetObservation.nativeLinks; throw new SecurityException("native denied");
            }
            @Override public void checkWrite(String p) {
                if (!Path.of(p).toAbsolutePath().normalize().startsWith(root)) {
                    ++MetadataSetObservation.rejectedWrites;
                    throw new SecurityException("write outside private root denied");
                }
            }
            @Override public void checkDelete(String p) {
                ++MetadataSetObservation.rejectedWrites; throw new SecurityException("delete denied");
            }
        });
        for (Class<?> type : new Class<?>[]{dp.class,uu.class,fd.class,lm.class,wi.class,ok.class,
                wy.class,xa.class,gr.class,yy.class}) MetadataSetObservation.origin(type,jar);
        new gr("LadderReference", "");
        MetadataSetObservation.prepare();
        MetadataSetObservation.world.B = true;
        for (int metadata = 0; metadata < 16; ++metadata) run(metadata,-1);
        for (int missing = 0; missing < 4; ++missing) run(missing + 2,missing);
        MetadataSetObservation.need(rows == 20 && MetadataSetObservation.network == 0 &&
            MetadataSetObservation.nativeLinks == 0 && MetadataSetObservation.rejectedWrites == 0,
            "finite observations and external-effect guards");
        System.out.println("Original Ladder neighbor: 20 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9");
    }
}
