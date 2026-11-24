import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.Buffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.StringJoiner;

/** Vertical-route authored inputs; all rendering is unchanged original CV.g. */
public final class RedstoneVerticalObservation {
    static int active = -1, network, nativeLinks, deniedWrites, allowedWorldWrites, execs;
    static ew observedCache;
    static uu observedBlock;
    static nw observedTess;
    static int[] observedEmissions;
    static uu[] observedBlocks;
    static ln observedMaterial, observedStoneMaterial;
    static uu observedStone, observedUpper, observedRepeaterOff, observedRepeaterOn;

    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }
    static String d(double value) {
        return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value));
    }
    static String f(float value) {
        return String.format(Locale.ROOT, "%08x", Float.floatToRawIntBits(value));
    }
    static String primitive(Object receiver, Class<?> type) throws Exception {
        Field[] fields = type.getDeclaredFields();
        Arrays.sort(fields, Comparator.comparing(Field::getName));
        StringJoiner out = new StringJoiner(",");
        for (Field item : fields) {
            if (!Modifier.isStatic(item.getModifiers()) && item.getType().isPrimitive()) {
                item.setAccessible(true);
                Object value = item.get(receiver);
                out.add(item.getName() + "=" + (value instanceof Float ? f((float)value)
                    : value instanceof Double ? d((double)value) : value.toString()));
            }
        }
        return out.toString();
    }
    static Object[] references(nw tess) throws Exception {
        return new Object[] {field(nw.class, "d").get(tess), field(nw.class, "e").get(tess),
            field(nw.class, "f").get(tess), field(nw.class, "g").get(tess),
            field(nw.class, "y").get(tess)};
    }
    static String buffers(Object[] values) {
        StringJoiner out = new StringJoiner(",");
        for (Object value : values) {
            if (value == null) out.add("NULL");
            else if (value instanceof Buffer) {
                Buffer b = (Buffer)value;
                out.add(value.getClass().getName() + ":" + b.position() + ":" + b.limit()
                    + ":" + b.capacity());
            } else if (value instanceof int[]) out.add("int[]:" + ((int[])value).length);
            else throw new AssertionError("unexpected NW reference");
        }
        return out.toString();
    }
    static String translations(nw tess) throws Exception {
        return d(field(nw.class, "s").getDouble(tess)) + ","
            + d(field(nw.class, "t").getDouble(tess)) + ","
            + d(field(nw.class, "u").getDouble(tess));
    }
    static String bounds(uu block) {
        return d(block.bs) + "," + d(block.bt) + "," + d(block.bu) + ","
            + d(block.bv) + "," + d(block.bw) + "," + d(block.bx);
    }
    static void voxel(lm chunk, int x, int y, int z, int id, int metadata) {
        chunk.b[(x << 11) | (z << 7) | y] = (byte)id;
        chunk.e.a(x, y, z, metadata);
    }
    static void prepare(lm[] chunks, int[] p) {
        for (lm chunk : chunks) {
            Arrays.fill(chunk.b, (byte)0);
            Arrays.fill(chunk.e.a, (byte)0);
            Arrays.fill(chunk.f.a, (byte)0);
            Arrays.fill(chunk.g.a, (byte)0);
        }
        lm center = chunks[0];
        for (int x = 7; x <= 9; ++x)
            for (int y = 63; y <= 65; ++y)
                for (int z = 7; z <= 9; ++z) {
                    center.f.a(x, y, z, p[3] == 1 ? 15 : 0);
                    center.g.a(x, y, z, p[3] == 2 ? 15 : 0);
                }
        voxel(center, 8, 63, 8, 1, 0);
        voxel(center, 8, 64, 8, 55, p[2]);
        int[][] directions = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int side = 0; side < 4; ++side) {
            if ((p[7] & (1 << side)) == 0) continue;
            int x = 8 + directions[side][0], z = 8 + directions[side][1];
            voxel(center, x, 64, z, p[8], 0);
            voxel(center, x, 63, z, p[9], 0);
            voxel(center, x, 65, z, p[10], 0);
        }
        voxel(center, 8, 65, 8, p[11], 0);
    }
    static void deny(Runnable call) {
        try { call.run(); throw new AssertionError("guard denial"); }
        catch (SecurityException expected) { }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        require(args.length == 3, "jar,world,profiles");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        List<String> profiles = Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) { }
            public void checkConnect(String host, int port) { ++network; throw new SecurityException("network denied"); }
            public void checkListen(int port) { ++network; throw new SecurityException("listen denied"); }
            public void checkLink(String name) { ++nativeLinks; throw new SecurityException("native denied"); }
            public void checkExec(String name) { ++execs; throw new SecurityException("exec denied"); }
            public void checkWrite(String path) {
                if (Path.of(path).toAbsolutePath().normalize().startsWith(directory)) { ++allowedWorldWrites; return; }
                ++deniedWrites; throw new SecurityException("outside private world denied");
            }
            public void checkDelete(String path) { checkWrite(path); }
        });
        for (Class<?> type : new Class<?>[] {cv.class, ew.class, fd.class, lm.class,
                nw.class, uu.class, sm.class, ln.class, aq.class, mx.class, eo.class, vm.class, db.class, wo.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "unchanged source origin");
        fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        lm[] chunks = {world.c(0, 0), world.c(0, 1), world.c(1, 0), world.c(1, 1)};
        ew cache = new ew(world, 0, 0, 0, 31, 127, 31);
        field(fd.class, "f").setInt(world, 3);
        nw tess = nw.a;
        tess.b();
        tess.b(-.25, .5, -.75);
        int[] constructed = (int[])field(nw.class, "g").get(tess);
        int[] prior = new int[constructed.length];
        require(constructed.length == 2097152, "actual constructor backing");
        observedCache = cache; observedTess = tess;
        observedEmissions = uu.s; observedBlocks = uu.m;
        require(Modifier.isStatic(field(uu.class, "s").getModifiers())
            && Modifier.isFinal(field(uu.class, "s").getModifiers()), "actual static final emission array");
        System.out.println("VERTICAL_STATIC\tNW.b=" + field(nw.class, "b").getBoolean(null)
            + "\tNW.c=" + field(nw.class, "c").getBoolean(null)
            + "\tUU.s.length=" + uu.s.length + "\tUU.s.final=true");
        int index = 0;
        long unchangedWords = 0;
        for (String line : profiles) {
            if (line.isBlank()) continue;
            String[] columns = line.split(" ");
            require(columns.length == 14, "name plus thirteen signed integers");
            int[] p = Arrays.stream(Arrays.copyOfRange(columns, 1, columns.length))
                .mapToInt(Integer::parseInt).toArray();
            require(p[0] == 0 && p[1] == 55 && p[2] >= 0 && p[2] <= 15
                && p[3] == 0 && p[4] == Integer.MIN_VALUE && p[5] == 0 && p[6] == -1
                && (p[7] == 1 || p[7] == 2 || p[7] == 4 || p[7] == 8 || p[7] == 15)
                && (p[8] == 0 || p[8] == 1) && (p[9] == 0 || p[9] == 55)
                && (p[10] == 0 || p[10] == 55 || p[10] == 75)
                && (p[11] == 0 || p[11] == 1) && p[12] == 0,
                "closed vertical operands");
            uu block = uu.m[55];
            require(block instanceof sm && block.bn == 55, "genuine registered Redstone");
            observedBlock = block;
            observedStone = uu.m[1]; observedUpper = uu.m[75];
            observedRepeaterOff = uu.m[93]; observedRepeaterOn = uu.m[94];
            observedStoneMaterial = observedStone.bA;
            observedMaterial = block.bA;
            int originalBm = block.bm, originalEmission = uu.s[block.bn];
            {
                prepare(chunks, p);
                cv renderer = new cv(cache);
                field(cv.class, "d").setInt(renderer, p[6]);
                int before = field(nw.class, "o").getInt(tess);
                System.arraycopy(constructed, 0, prior, 0, constructed.length);
                Object[] refs = references(tess);
                String bufferBefore = buffers(refs), cvBefore = primitive(renderer, cv.class);
                String boundsBefore = bounds(block), nwBefore = primitive(tess, nw.class);
                int[] emissionBefore = uu.s.clone();
                int enteredBm = block.bm;
                String result = "UNRETURNED", exception = "NONE";
                lm.a = false;
                active = index;
                try { result = Boolean.toString(renderer.g(block, 8, 64, 8)); }
                catch (NullPointerException failure) { exception = "NULL"; }
                catch (ArrayIndexOutOfBoundsException failure) { exception = "INDEX"; }
                finally { active = -1; }
                int after = field(nw.class, "o").getInt(tess);
                require(before <= after && after < constructed.length, "bounded observed writes");
                long untouched = 0;
                for (int i = 0; i < before; ++i) {
                    if (prior[i] != constructed[i]) throw new AssertionError("actual prior emitted word preserved");
                    ++untouched;
                }
                for (int i = after; i < constructed.length; ++i) {
                    if (prior[i] != constructed[i]) throw new AssertionError("actual unused word preserved");
                    ++untouched;
                }
                Object[] afterRefs = references(tess);
                for (int i = 0; i < refs.length; ++i)
                    require(refs[i] == afterRefs[i], "same NW owner reference");
                require(bufferBefore.equals(buffers(afterRefs))
                    && cvBefore.equals(primitive(renderer, cv.class))
                    && boundsBefore.equals(bounds(block))
                    && field(cv.class, "c").get(renderer) == cache, "retained CV/Block/cache/buffer state");
                require(uu.m == observedBlocks && uu.s == observedEmissions
                    && Arrays.equals(uu.s, emissionBefore) && block.bm == enteredBm,
                    "same actual registries and declared data");
                System.out.println("VERTICAL_INPUT\t" + index + "\t" + block.getClass().getName()
                    + "\t" + block.bn + "\t" + originalBm + "\t" + p[5] + "\t" + enteredBm
                    + "\t" + originalEmission + "\t" + uu.s[block.bn] + "\t" + p[2]
                    + "\t" + p[3] + "\t" + p[7] + "\t" + p[8]
                    + "\t" + p[9] + "\t" + p[10] + "\t" + p[11]);
                StringJoiner words = new StringJoiner(",");
                int extent = Math.max(after, before + 100);
                require(extent <= constructed.length, "observed tail extent");
                for (int i = before; i < extent; ++i)
                    words.add(String.format(Locale.ROOT, "%08x", constructed[i]));
                System.out.println("VERTICAL\t" + index + "\t" + columns[0] + "\t" + p[0]
                    + "\t" + result + "\t" + exception + "\t" + before + "\t" + after
                    + "\t" + constructed.length + "\t" + (lm.a ? 1 : 0) + "\t" + translations(tess)
                    + "\t" + boundsBefore + "\t" + cvBefore + "\t" + nwBefore
                    + "\t" + primitive(tess, nw.class) + "\t" + words);
                System.out.println("VERTICAL_BACKING\t" + index + "\t" + untouched + "\t"
                    + Arrays.hashCode(prior) + "\t" + Arrays.hashCode(constructed) + "\t" + bufferBefore);
                unchangedWords += untouched;
            }
            ++index;
        }
        for (int i = 0; i < chunks.length; ++i)
            require(world.c(i / 2, i % 2) == chunks[i], "same canonical World Chunks");
        require(network == 0 && nativeLinks == 0 && deniedWrites == 0 && execs == 0,
            "no source external attempts");
        System.out.println("VERTICAL_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=" + allowedWorldWrites);
        deny(() -> System.getSecurityManager().checkConnect("127.0.0.1", 25565));
        deny(() -> System.getSecurityManager().checkListen(25565));
        deny(() -> System.getSecurityManager().checkLink("redstone-vertical-denial"));
        deny(() -> System.getSecurityManager().checkExec("redstone-vertical-denial"));
        deny(() -> System.getSecurityManager().checkWrite(directory.resolveSibling("redstone-vertical-denial").toString()));
        require(network == 2 && nativeLinks == 1 && deniedWrites == 1 && execs == 1,
            "five denial controls");
        System.out.println("VERTICAL_GUARD controls network=2 native=1 denied-writes=1 execs=1");
        System.out.println("VERTICAL_COMPLETE rows=" + index + " own-unchanged-words=" + unchangedWords
            + " genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0");
    }
}
