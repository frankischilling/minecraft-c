import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.StringJoiner;
import java.util.concurrent.atomic.AtomicLong;

/** Declared raw Chunk inputs around untouched E predicates on a genuine FD World. */
public final class ChestPlacementObservation {
    static fd world;
    static lm[] chunks;
    static int network, natives, execs, deniedWrites, worldWrites;
    static final String[] CORE = {"e", "fd", "lm", "mx", "uu", "xp", "pe", "st", "eb", "pm", "ok", "wi", "xa"};

    static void need(boolean value, String message) { if (!value) throw new AssertionError(message); }
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static Map<String,Object> map(Object... pairs) {
        Map<String,Object> value = new LinkedHashMap<String,Object>();
        for (int i = 0; i < pairs.length; i += 2) value.put((String)pairs[i], pairs[i + 1]);
        return value;
    }
    static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) return "\"" + ((String)value).replace("\\", "\\\\")
            .replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t") + "\"";
        if (value instanceof Map) {
            StringJoiner out = new StringJoiner(",", "{", "}");
            for (Map.Entry<?,?> entry : ((Map<?,?>)value).entrySet()) out.add(json(entry.getKey()) + ":" + json(entry.getValue()));
            return out.toString();
        }
        if (value instanceof Iterable) {
            StringJoiner out = new StringJoiner(",", "[", "]");
            for (Object item : (Iterable<?>)value) out.add(json(item));
            return out.toString();
        }
        return value.toString();
    }
    static String sha(byte[] bytes) throws Exception {
        StringBuilder out = new StringBuilder();
        for (byte value : MessageDigest.getInstance("SHA-256").digest(bytes)) out.append(String.format(Locale.ROOT, "%02x", value & 255));
        return out.toString();
    }
    static Object primitive(Field field, Object owner) throws Exception {
        if (field.getType() == float.class) return Float.floatToRawIntBits(field.getFloat(owner));
        if (field.getType() == double.class) return Double.doubleToRawLongBits(field.getDouble(owner));
        return field.get(owner);
    }
    static final class Snapshot {
        final Object owner;
        final Field[] fields;
        final Object[] values;
        Snapshot(Object owner) throws Exception {
            this.owner = owner;
            fields = Arrays.stream(owner.getClass().getDeclaredFields()).filter(f -> !Modifier.isStatic(f.getModifiers()))
                .sorted((a,b) -> a.getName().compareTo(b.getName())).toArray(Field[]::new);
            values = new Object[fields.length];
            for (int i = 0; i < fields.length; ++i) { fields[i].setAccessible(true); values[i] = primitive(fields[i], owner); }
        }
        boolean same() throws Exception {
            for (int i = 0; i < fields.length; ++i) {
                Object value = primitive(fields[i], owner);
                if (fields[i].getType().isPrimitive() ? !values[i].equals(value) : values[i] != value) return false;
            }
            return true;
        }
    }
    static List<Object> random(Random value) throws Exception {
        return List.of(String.format(Locale.ROOT, "%012x", ((AtomicLong)field(Random.class, "seed").get(value)).get()),
            field(Random.class, "haveNextNextGaussian").getBoolean(value),
            String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(field(Random.class, "nextNextGaussian").getDouble(value))));
    }
    static List<Object> queues() throws Exception {
        List<Object> result = new ArrayList<Object>();
        for (Object raw : (List<?>)field(fd.class, "C").get(world)) {
            st item = (st)raw; need(item.a == eb.a || item.a == eb.b, "actual light kind");
            result.add(List.of(item.a == eb.a ? 0 : 1, item.b, item.c, item.d, item.e, item.f, item.g));
        }
        return result;
    }
    static byte[][] arrays() {
        byte[][] result = new byte[chunks.length * 5][];
        for (int i = 0; i < chunks.length; ++i) {
            result[i * 5] = chunks[i].b; result[i * 5 + 1] = chunks[i].e.a;
            result[i * 5 + 2] = chunks[i].f.a; result[i * 5 + 3] = chunks[i].g.a; result[i * 5 + 4] = chunks[i].h;
        }
        return result;
    }
    static void put(int x, int y, int z, int id) {
        if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127) return;
        chunks[(x >> 4) * 2 + (z >> 4)].b[((x & 15) << 11) | ((z & 15) << 7) | y] = (byte)id;
    }
    static int cell(int x, int y, int z) {
        if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127) return 0;
        return chunks[(x >> 4) * 2 + (z >> 4)].b[((x & 15) << 11) | ((z & 15) << 7) | y] & 255;
    }
    static void begin(int index) { /* Passive debugger boundary outside game execution. */ }
    static void end(int index) { /* Passive debugger boundary outside game execution. */ }
    static final class Observer implements pm {
        final List<Object> events = new ArrayList<Object>();
        public void a(int x,int y,int z) { events.add(List.of("cell",x,y,z)); }
        public void b(int a,int b,int c,int d,int e,int f) { events.add(List.of("range",a,b,c,d,e,f)); }
        public void a(sn value) { events.add(List.of("add",value == null ? "NULL" : value.getClass().getName())); }
        public void b(sn value) { events.add(List.of("remove",value == null ? "NULL" : value.getClass().getName())); }
        public void e() { events.add(List.of("refresh")); }
        public void a(String name,double x,double y,double z,float volume,float pitch) { events.add(List.of("sound",name)); }
        public void a(String name,double x,double y,double z,double a,double b,double c) { events.add(List.of("particle",name)); }
        public void a(String name,int x,int y,int z) { events.add(List.of("record",name,x,y,z)); }
        public void a(int x,int y,int z,ow value) { events.add(List.of("tile",x,y,z,value == null ? "NULL" : value.getClass().getName())); }
        public void a(gs value,int a,int b,int c,int d,int e) { events.add(List.of("level",a,b,c,d,e)); }
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT); need(args.length == 3, "jar, private world, declared profiles");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        List<String> profiles = Files.readAllLines(Path.of(args[2]));
        Map<String,Object> origins = new LinkedHashMap<String,Object>();
        for (String name : CORE) {
            Class<?> type = Class.forName(name, false, ChestPlacementObservation.class.getClassLoader());
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "original class origin");
            try (InputStream input = type.getResourceAsStream("/" + name + ".class")) { origins.put(name, sha(input.readAllBytes())); }
        }
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host, int port) { ++network; throw new SecurityException("network"); }
            public void checkListen(int port) { ++network; throw new SecurityException("listen"); }
            public void checkLink(String name) { ++natives; throw new SecurityException("native"); }
            public void checkExec(String name) { ++execs; throw new SecurityException("exec"); }
            public void checkWrite(String name) {
                if (Path.of(name).toAbsolutePath().normalize().startsWith(directory)) { ++worldWrites; return; }
                ++deniedWrites; throw new SecurityException("outside private world");
            }
            public void checkDelete(String name) { checkWrite(name); }
        });
        world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        chunks = new lm[] {world.c(0,0), world.c(0,1), world.c(1,0), world.c(1,1)};
        uu block = uu.m[54]; need(block != null && block.getClass() == e.class && block.bn == 54, "registered original Chest");
        uu[] registry = uu.m.clone();
        Method helper = e.class.getDeclaredMethod("h", fd.class, int.class, int.class, int.class); helper.setAccessible(true);
        Observer observer = new Observer(); world.a(observer);
        System.out.println("CHEST_ORIGINS " + json(map("jar_sha256", sha(Files.readAllBytes(jar)), "classes", origins)));
        int index = 0;
        for (String profile : profiles) {
            String[] words = profile.split("\t", -1); need(words.length == 8, "eight declared fields");
            int operation = Integer.parseInt(words[1]), x = Integer.parseInt(words[2]), y = Integer.parseInt(words[3]),
                z = Integer.parseInt(words[4]), target = Integer.parseInt(words[5]);
            boolean nil = Boolean.parseBoolean(words[6]);
            for (lm chunk : chunks) {
                Arrays.fill(chunk.b, (byte)0); Arrays.fill(chunk.e.a, (byte)0); Arrays.fill(chunk.f.a, (byte)15);
                Arrays.fill(chunk.g.a, (byte)0); Arrays.fill(chunk.h, (byte)0); chunk.i = 0; chunk.n = true;
            }
            observer.events.clear();
            put(x,y,z,target);
            List<Object> declaredCells = new ArrayList<Object>();
            if (!words[7].equals("-")) for (String raw : words[7].split(";")) {
                String[] xyz = raw.split(","); int cx = Integer.parseInt(xyz[0]), cy = Integer.parseInt(xyz[1]), cz = Integer.parseInt(xyz[2]);
                put(cx,cy,cz,54); declaredCells.add(List.of(cx,cy,cz,cell(cx,cy,cz)));
            }
            byte[][] current = arrays(), before = new byte[current.length][];
            for (int i = 0; i < before.length; ++i) before[i] = current[i].clone();
            Snapshot worldBefore = new Snapshot(world); Snapshot[] chunkBefore = new Snapshot[chunks.length];
            for (int i = 0; i < chunks.length; ++i) chunkBefore[i] = new Snapshot(chunks[i]);
            List<Object> randomBefore = random(world.r), queueBefore = queues();
            String result = "UNRETURNED", exception = "NONE";
            begin(index);
            try {
                fd actual = nil ? null : world;
                result = operation == 0 ? Boolean.toString(block.a(actual,x,y,z)) : helper.invoke(block,actual,x,y,z).toString();
            } catch (Throwable failure) {
                Throwable reached = failure instanceof InvocationTargetException ? failure.getCause() : failure;
                if (!(reached instanceof NullPointerException)) throw failure;
                exception = reached.getClass().getName();
            }
            end(index);
            int changes = 0; byte[][] after = arrays();
            for (int i = 0; i < before.length; ++i) {
                need(before[i].length == after[i].length, "same array extent");
                for (int j = 0; j < before[i].length; ++j) if (before[i][j] != after[i][j]) ++changes;
            }
            boolean chunkSame = true; for (Snapshot value : chunkBefore) chunkSame &= value.same();
            List<Object> randomAfter = random(world.r), queueAfter = queues();
            System.out.println("CHEST_PLACEMENT " + json(map("index", index, "name", words[0], "operation", operation,
                "x",x,"y",y,"z",z,"target_id",target,"null_world",nil,"cells",declaredCells,
                "target_cell",cell(x,y,z),"result",result,"exception",exception,"array_changes",changes,
                "world_fields_unchanged",worldBefore.same(),"chunk_fields_unchanged",chunkSame,
                "queues_before",queueBefore,"queues_after",queueAfter,"events",observer.events,
                "rng_unchanged",randomBefore.equals(randomAfter))));
            System.out.println("CHEST_RANDOM " + json(map("index",index,"name",words[0],"before",randomBefore,"after",randomAfter)));
            ++index;
        }
        need(Arrays.equals(registry,uu.m), "same registered owners");
        for (int i = 0; i < chunks.length; ++i) need(world.c(i/2,i%2) == chunks[i], "same canonical saved Chunks");
        need(network == 0 && natives == 0 && execs == 0 && deniedWrites == 0, "no external attempts");
        System.out.println("CHEST_COMPLETE " + json(map("rows",index,"network",network,"native",natives,"execs",execs,
            "denied_writes",deniedWrites,"world_writes",worldWrites,"canonical_chunks",4,"registry_unchanged",true,"remote",world.B)));
    }
}
