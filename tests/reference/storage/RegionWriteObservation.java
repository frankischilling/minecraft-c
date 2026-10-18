import java.io.*;
import java.lang.ref.Reference;
import java.lang.reflect.*;
import java.net.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

public final class RegionWriteObservation {
    static final String CLIENT_SHA = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static final String SERVER_SHA = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    static final int MAX_FILES = 300, MAX_FILE_BYTES = 4 * 1024 * 1024;
    static final long MAX_INVENTORY_BYTES = 8 * 1024 * 1024;
    final boolean client, observe;
    final Path storage, snapshots;
    final URLClassLoader loader;
    final Class<?> cache, region, buffer;
    final Method open, input, output, growth, flush, commit, contains;
    final Field cacheEntries, regionFile, regionHandle, locations, timestamps, free, counter;
    final IdentityHashMap<Object, Integer> identities = new IdentityHashMap<>();
    final ArrayList<Object> held = new ArrayList<>();
    final ArrayList<Object> events = new ArrayList<>();
    final LinkedHashMap<String, byte[]> expected = new LinkedHashMap<>();
    int snapshotNumber;

    RegionWriteObservation(Path jar, boolean client, boolean observe, Path storage, Path snapshots) throws Exception {
        this.client = client; this.observe = observe;
        this.storage = storage.toAbsolutePath().normalize(); this.snapshots = snapshots.toAbsolutePath().normalize();
        check(Files.isRegularFile(jar, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(jar) &&
            digest(Files.readAllBytes(jar)).equals(client ? CLIENT_SHA : SERVER_SHA), "pinned original archive before class loading");
        check(!Files.exists(this.storage, LinkOption.NOFOLLOW_LINKS) && !Files.exists(this.snapshots, LinkOption.NOFOLLOW_LINKS), "fresh helper-owned storage roots");
        Files.createDirectory(this.storage); Files.createDirectory(this.snapshots);
        loader = new URLClassLoader(new URL[] {jar.toUri().toURL()}, ClassLoader.getPlatformClassLoader());
        cache = Class.forName(client ? "rj" : "kq", true, loader);
        region = Class.forName(client ? "qj" : "jx", true, loader);
        buffer = Class.forName(client ? "hz" : "fh", false, loader);
        for (Class<?> type : new Class<?>[] {cache, region, buffer})
            check(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar.toRealPath()), "untouched supplied class origin");
        open = cache.getDeclaredMethod("a", File.class, int.class, int.class);
        input = cache.getDeclaredMethod("c", File.class, int.class, int.class);
        output = cache.getDeclaredMethod("d", File.class, int.class, int.class);
        growth = cache.getDeclaredMethod("b", File.class, int.class, int.class);
        flush = cache.getDeclaredMethod("a");
        commit = region.getDeclaredMethod("a", int.class, int.class, byte[].class, int.class);
        commit.setAccessible(true);
        contains = region.getDeclaredMethod("c", int.class, int.class);
        cacheEntries = field(cache, "a"); regionFile = field(region, "b"); regionHandle = field(region, "c");
        locations = field(region, "d"); timestamps = field(region, "e"); free = field(region, "f"); counter = field(region, "g");
        check(open.getReturnType() == region && input.getReturnType() == DataInputStream.class &&
              output.getReturnType() == DataOutputStream.class && growth.getReturnType() == int.class &&
              flush.getReturnType() == void.class && commit.getReturnType() == void.class && contains.getReturnType() == boolean.class &&
              Modifier.isStatic(open.getModifiers()) && Modifier.isSynchronized(open.getModifiers()) &&
              Modifier.isStatic(flush.getModifiers()) && Modifier.isSynchronized(flush.getModifiers()) &&
              Modifier.isProtected(commit.getModifiers()) && Modifier.isSynchronized(commit.getModifiers()), "actual region method modifiers");
        check(cache.getClassLoader() == loader && region.getClassLoader() == loader && buffer.getClassLoader() == loader &&
              loader.getParent() == ClassLoader.getPlatformClassLoader() && region.getSuperclass() == Object.class &&
              buffer.getSuperclass() == ByteArrayOutputStream.class && cacheEntries.getType() == Map.class &&
              regionFile.getType() == File.class && regionHandle.getType() == RandomAccessFile.class &&
              locations.getType() == int[].class && timestamps.getType() == int[].class && free.getType() == ArrayList.class &&
              counter.getType() == int.class, "exact supplied class and field ownership");
    }

    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void check(boolean condition, String label) { if (!condition) throw new IllegalStateException(label); }
    static Object call(Method method, Object owner, Object... arguments) throws Exception {
        try { return method.invoke(owner, arguments); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof Exception value) throw value;
            if (cause instanceof Error value) throw value;
            throw failure;
        }
    }
    static Map<String, Object> object(Object... values) {
        LinkedHashMap<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String)values[i], values[i + 1]);
        return result;
    }
    static String digest(byte[] value) throws Exception { return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value)); }
    static Map<String, Object> blob(byte[] value) throws Exception { return object("bytes", value.length, "sha256", digest(value)); }
    Map<String, Object> bindings() throws Exception {
        Map<String, Object> methods = object();
        for (Object[] row : new Object[][]{{"open",open},{"input",input},{"output",output},{"growth",growth},
                {"flush",flush},{"commit",commit},{"contains",contains},{"buffer_close",buffer.getDeclaredMethod("close")}}) {
            Method method = (Method)row[1];
            methods.put((String)row[0], object("owner",method.getDeclaringClass().getName(),"name",method.getName(),
                "descriptor",java.lang.invoke.MethodType.methodType(method.getReturnType(),method.getParameterTypes()).toMethodDescriptorString(),
                "modifiers",method.getModifiers()));
        }
        Map<String, Object> fields = object();
        for (Object[] row : new Object[][]{{"cache_entries",cacheEntries},{"region_file",regionFile},{"region_handle",regionHandle},
                {"locations",locations},{"timestamps",timestamps},{"free",free},{"counter",counter}}) {
            Field field = (Field)row[1];
            fields.put((String)row[0],object("owner",field.getDeclaringClass().getName(),"name",field.getName(),
                "descriptor",field.getType().descriptorString(),"modifiers",field.getModifiers()));
        }
        return object("methods",methods,"fields",fields);
    }
    int identity(Object value) {
        Integer existing = identities.get(value);
        if (existing != null) return existing;
        int result = identities.size(); identities.put(value, result); held.add(value); return result;
    }
    String relative(Path value) { return storage.relativize(value.toAbsolutePath().normalize()).toString().replace('\\', '/'); }
    Path root(String name) throws Exception {
        Path value = storage.resolve(name); Files.createDirectory(value); return value;
    }
    Object region(Path root, int x, int z, String label) throws Exception {
        long before = System.currentTimeMillis(); Object result = call(open, null, root.toFile(), x, z); long after = System.currentTimeMillis();
        int id = identity(result);
        events.add(object("kind", "open", "label", label, "root", relative(root), "coordinates", new int[]{x,z}, "before_ms", before, "after_ms", after, "object", id));
        return result;
    }
    int consume(Path root, int x, int z, String label) throws Exception {
        int result = (Integer)call(growth, null, root.toFile(), x, z);
        events.add(object("kind", "growth", "label", label, "root", relative(root), "coordinates", new int[]{x,z}, "result", result));
        return result;
    }
    void flush(String label) throws Exception {
        call(flush, null); events.add(object("kind", "flush", "label", label));
        capture(label);
    }
    Map<String, Object> state() throws Exception {
        ArrayList<Object> entries = new ArrayList<>(), regions = new ArrayList<>();
        Map<?, ?> actual = (Map<?, ?>)cacheEntries.get(null);
        check(actual.getClass() == HashMap.class && actual.size() <= 256 && held.size() <= 300, "bounded original cache state");
        for (Map.Entry<?, ?> item : actual.entrySet()) {
            Object value = ((Reference<?>)item.getValue()).get();
            entries.add(object("path", relative(((File)item.getKey()).toPath()), "live", value != null,
                "object", value == null ? null : identity(value)));
        }
        for (Object value : held) {
            RandomAccessFile handle = (RandomAccessFile)regionHandle.get(value);
            check(((int[])locations.get(value)).length == 1024 && ((int[])timestamps.get(value)).length == 1024 &&
                  ((List<?>)free.get(value)).size() <= MAX_FILE_BYTES / 4096, "bounded original region arrays");
            regions.add(object("object", identities.get(value), "path", relative(((File)regionFile.get(value)).toPath()),
                "handle_valid", handle != null && handle.getFD().valid(), "locations", ((int[])locations.get(value)).clone(),
                "timestamps", ((int[])timestamps.get(value)).clone(), "free", new ArrayList<>((List<?>)free.get(value)), "growth", counter.getInt(value)));
        }
        return object("cache_count", actual.size(), "cache_entries_raw_order", entries, "held_regions", regions);
    }
    Map<String, Object> inventory(Path copyRoot) throws Exception {
        ArrayList<Path> files = new ArrayList<>();
        try (var walk = Files.walk(storage)) {
            walk.forEach(p -> { if (Files.isRegularFile(p, LinkOption.NOFOLLOW_LINKS)) files.add(p); });
        }
        files.sort(Comparator.comparing(this::relative)); check(files.size() <= MAX_FILES, "bounded region inventory count");
        long bytes = 0; LinkedHashMap<String, Object> result = new LinkedHashMap<>();
        for (Path file : files) {
            check(!Files.isSymbolicLink(file), "owned regular file");
            long size = Files.size(file); check(size <= MAX_FILE_BYTES, "bounded individual region bytes"); bytes += size;
            check(bytes <= MAX_INVENTORY_BYTES, "bounded region inventory bytes"); byte[] raw = Files.readAllBytes(file);
            String name = relative(file); result.put(name, blob(raw));
            if (copyRoot != null) { Path dest = copyRoot.resolve(name); Files.createDirectories(dest.getParent()); Files.write(dest, raw, StandardOpenOption.CREATE_NEW); }
        }
        return result;
    }
    void capture(String label) throws Exception {
        if (!observe) return;
        Path destination = snapshots.resolve(String.format(Locale.ROOT, "%03d-%s", snapshotNumber++, label)); Files.createDirectory(destination);
        events.add(object("kind", "snapshot", "label", label, "directory", destination.getFileName().toString(), "state", state(), "files", inventory(destination)));
    }
    static byte[] plain(int length, int salt) {
        byte[] value = new byte[length];
        for (int i = 0; i < length; ++i) value[i] = (byte)((i * 73) ^ (i >>> 7) ^ salt);
        return value;
    }
    void retainInput(byte[] value, String suffix) throws Exception {
        Path directory = snapshots.resolve("inputs"); Files.createDirectories(directory);
        Path path = directory.resolve(digest(value) + suffix);
        if (Files.exists(path)) check(Arrays.equals(Files.readAllBytes(path), value), "retained complete input bytes");
        else Files.write(path, value, StandardOpenOption.CREATE_NEW);
    }
    static byte[] compressed(byte[] plain) throws Exception {
        Deflater deflater = new Deflater(Deflater.NO_COMPRESSION);
        try {
            ByteArrayOutputStream result = new ByteArrayOutputStream();
            try (DeflaterOutputStream stream = new DeflaterOutputStream(result, deflater)) { stream.write(plain); }
            return result.toByteArray();
        } finally { deflater.end(); }
    }
    static byte[][] storedLength(int target) throws Exception {
        int low = 0, high = target;
        for (int tries = 0; tries < 24 && low <= high; ++tries) {
            int n = low + (high - low) / 2; byte[] plain = plain(n, 93), encoded = compressed(plain);
            if (encoded.length == target) {
                try (InflaterInputStream stream = new InflaterInputStream(new ByteArrayInputStream(encoded))) {
                    check(Arrays.equals(stream.readAllBytes(), plain), "valid exact-length stored zlib payload");
                }
                return new byte[][]{plain, encoded};
            }
            if (encoded.length < target) low = n + 1; else high = n - 1;
        }
        throw new IllegalStateException("JDK did not produce requested exact compressed length: " + target);
    }
    void read(Path root, int x, int z, byte[] expected, String label) throws Exception {
        DataInputStream stream = (DataInputStream)call(input, null, root.toFile(), x, z);
        check(stream != null, "original stream exists: " + label);
        byte[] actual;
        try (stream) { actual = stream.readNBytes(expected.length + 1); }
        check(Arrays.equals(actual, expected), "complete original decompressed payload: " + label);
        events.add(object("kind", "read", "label", label, "coordinates", new int[]{x,z}, "payload", blob(actual)));
    }
    void direct(Path root, Object target, int x, int z, byte[][] data, String label, boolean accepted) throws Exception {
        retainInput(data[0], ".plain"); retainInput(data[1], ".zlib");
        long before = System.currentTimeMillis(); call(commit, target, x, z, data[1], data[1].length); long after = System.currentTimeMillis();
        events.add(object("kind", "direct_commit", "label", label, "root", relative(root), "coordinates", new int[]{x,z},
            "before_ms", before, "after_ms", after, "plain", blob(data[0]), "compressed", blob(data[1]), "expected_accepted", accepted,
            "scope", "constructed valid-zlib protected-method input; not a world/NBT save"));
        if (accepted) { expected.put(x+","+z, data[0]); read(root, x, z, data[0], label); }
        consume(root, 0, 0, label + "-growth"); consume(root, 0, 0, label + "-reset"); capture(label);
    }
    void stream(Path root, Object target, int x, int z, byte[] data, String label, boolean initial) throws Exception {
        retainInput(data, ".plain");
        DataOutputStream stream = (DataOutputStream)call(output, null, root.toFile(), x, z); stream.write(data);
        boolean present = (Boolean)call(contains, target, x, z);
        events.add(object("kind", "before_close", "label", label, "coordinates", new int[]{x,z}, "present", present));
        check(present != initial, "original output buffers until close"); capture(label + "-before-close");
        long before = System.currentTimeMillis(); stream.close(); long after = System.currentTimeMillis();
        events.add(object("kind", "stream_close", "label", label, "root", relative(root), "coordinates", new int[]{x,z},
            "before_ms", before, "after_ms", after, "plain", blob(data)));
        expected.put(x+","+z, data); read(root, x, z, data, label);
        consume(root,0,0,label+"-growth"); consume(root,0,0,label+"-reset"); capture(label);
    }
    Map<String, Object> run(Path jar) throws Exception {
        Path mapping = root("mapping");
        int[][] coordinates = {{0,0},{31,31},{32,32},{-1,-1},{-32,-32},{-33,-33},{-1,32},{Integer.MIN_VALUE,Integer.MAX_VALUE},{Integer.MAX_VALUE,Integer.MIN_VALUE}};
        for (int i=0;i<coordinates.length;++i) region(mapping,coordinates[i][0],coordinates[i][1],"mapping-"+i);
        capture("mapping-open"); flush("mapping-flush");

        Path allocation = root("allocation"); Object target = region(allocation,0,0,"allocation-empty"); capture("allocation-empty");
        consume(allocation,0,0,"initial-growth"); consume(allocation,0,0,"initial-reset");
        stream(allocation,target,0,0,plain(1024,1),"A-first-stream",true);
        byte[][] two = storedLength(4091), one = storedLength(100), three = storedLength(8190), four = storedLength(12283), below = storedLength(4090), oversized = storedLength(1044475);
        direct(allocation,target,1,0,two,"B-exact-two",true);
        direct(allocation,target,2,0,three,"C-three",true);
        direct(allocation,target,1,0,one,"B-shrink-gap",true);
        direct(allocation,target,3,0,one,"D-reuse-gap",true);
        stream(allocation,target,0,0,plain(2048,2),"A-same-sector-rewrite",false);
        direct(allocation,target,2,0,four,"C-grow-four",true);
        flush("allocation-midpoint-flush"); target=region(allocation,0,0,"allocation-midpoint-reopen");
        consume(allocation,0,0,"midpoint-reopen-growth"); capture("allocation-midpoint-reopened");
        direct(allocation,target,4,0,two,"E-reuse-two",true);
        direct(allocation,target,5,0,below,"F-below-multiple",true);
        direct(allocation,target,6,0,two,"G-exact-grow-two",true);
        direct(allocation,target,7,0,oversized,"reject-256-new",false);
        check(!(Boolean)call(contains,target,7,0),"oversized new record absent");
        direct(allocation,target,1,0,oversized,"reject-256-existing",false);
        for (Map.Entry<String, byte[]> item : expected.entrySet()) {
            String[] parts=item.getKey().split(",");read(allocation,Integer.parseInt(parts[0]),Integer.parseInt(parts[1]),item.getValue(),"retained-"+item.getKey());
        }
        capture("allocation-retained"); flush("allocation-flush");
        Object reopened=region(allocation,0,0,"allocation-reopen");check(reopened!=target,"closed cache gives fresh region object");
        consume(allocation,0,0,"reopen-growth");
        for (Map.Entry<String,byte[]> item:expected.entrySet()) {String[] parts=item.getKey().split(",");read(allocation,Integer.parseInt(parts[0]),Integer.parseInt(parts[1]),item.getValue(),"reopen-"+item.getKey());}
        capture("allocation-reopened"); flush("allocation-reopen-flush");

        Path cacheRoot=root("cache"); ArrayList<Object> first=new ArrayList<>();
        for(int i=0;i<256;++i) first.add(region(cacheRoot,i*32,0,"cache-fill-"+i));
        capture("cache-full-256");Object hit=region(cacheRoot,0,0,"cache-hit-at-256");check(hit==first.get(0),"live cache hit identity retained at capacity");capture("cache-hit-at-256");
        Object overflow=region(cacheRoot,256*32,0,"cache-miss-257");check(!first.contains(overflow),"fresh overflow region identity");capture("cache-miss-257");flush("cache-final-flush");
        Path finalCopy=snapshots.resolve("final");Files.createDirectory(finalCopy);
        Map<String,Object> finalFiles=inventory(finalCopy),finalState=state();
        LinkedHashMap<String,Object> origins=new LinkedHashMap<>();for(Class<?> type:new Class<?>[]{cache,region,buffer})origins.put(type.getName(),type.getProtectionDomain().getCodeSource().getLocation().toExternalForm());
        return object("schema",1,"scope","untouched original low-level region methods; helper-owned files only","side",client?"client":"server","mode",observe?"observed":"control",
            "jar_sha256",digest(Files.readAllBytes(jar)),"platform_parent",loader.getParent()==ClassLoader.getPlatformClassLoader(),
            "class_origins",origins,"bindings",bindings(),"held_references","Every returned region is strongly retained by helper; no SoftReference collection claim.",
            "limits",object("heap_mib",512,"files",MAX_FILES,"file_bytes",MAX_FILE_BYTES,"inventory_bytes",MAX_INVENTORY_BYTES),
            "events",events,"final_state",finalState,"final_files",finalFiles);
    }

    static void json(Object value,StringBuilder out) {
        if(value==null){out.append("null");return;}
        if(value instanceof String text){out.append('"');for(int i=0;i<text.length();++i){char c=text.charAt(i);switch(c){case '"'->out.append("\\\"");case '\\'->out.append("\\\\");case '\n'->out.append("\\n");case '\r'->out.append("\\r");case '\t'->out.append("\\t");default->{if(c<32)out.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else out.append(c);}}}out.append('"');return;}
        if(value instanceof Number||value instanceof Boolean){out.append(value);return;}
        if(value instanceof Map<?,?> map){out.append('{');boolean first=true;for(var item:map.entrySet()){if(!first)out.append(',');first=false;json(item.getKey().toString(),out);out.append(':');json(item.getValue(),out);}out.append('}');return;}
        if(value instanceof Iterable<?> items){out.append('[');boolean first=true;for(Object item:items){if(!first)out.append(',');first=false;json(item,out);}out.append(']');return;}
        if(value.getClass().isArray()){out.append('[');for(int i=0;i<Array.getLength(value);++i){if(i!=0)out.append(',');json(Array.get(value,i),out);}out.append(']');return;}
        throw new IllegalArgumentException("Unknown JSON value "+value.getClass());
    }
    public static void main(String[] args) throws Exception {
        check(args.length==6,"args: jar side observed|control fresh_storage output_json fresh_snapshots");
        Path jar=Path.of(args[0]).toAbsolutePath().normalize(),result=Path.of(args[4]);
        check(args[1].equals("client")||args[1].equals("server"),"side");check(args[2].equals("observed")||args[2].equals("control"),"mode");
        RegionWriteObservation observer=new RegionWriteObservation(jar,args[1].equals("client"),args[2].equals("observed"),Path.of(args[3]),Path.of(args[5]));
        try {
            Map<String,Object> record=observer.run(jar);StringBuilder out=new StringBuilder();json(record,out);out.append('\n');
            check(digest(Files.readAllBytes(jar)).equals(observer.client ? CLIENT_SHA : SERVER_SHA), "original archive preserved after all calls");
            check(out.length()<=32*1024*1024,"bounded observation JSON");Files.writeString(result,out,StandardOpenOption.CREATE_NEW);
        } finally {call(observer.flush,null);observer.loader.close();}
    }
}
