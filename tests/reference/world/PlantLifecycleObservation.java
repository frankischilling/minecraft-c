import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import javax.tools.JavaCompiler;
import javax.tools.ToolProvider;

/** Finite dependency-fixture experiment over unchanged original plant Blocks.
 * The generated World subclass supplies prepared arrays, light/sky answers and
 * setter/publication services and actual original Block lifecycle callbacks.
 * Its constructor never runs. This does not test
 * original World, Chunk, observer, neighbor, lighting or publication behavior.
 * Item drops expose the selected deterministic stack/delay/position projection;
 * item Math.random fields and default-constructor clock/profile effects are not
 * measured. Sapling-selected tree constructors and bodies remain original. */
public final class PlantLifecycleObservation {
    private static final int MIN = -16, WIDTH = 48, HEIGHT = 128;
    private static final int CELLS = WIDTH * WIDTH * HEIGHT;
    private static final String CLIENT_SHA256 =
            "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    private static final String SERVER_SHA256 =
            "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
    private static final String FIXTURE = "PlantLifecycleFixtureWorld";

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalArgumentException(message);
    }

    private static Map<String, Object> object(Object... pairs) {
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) {
            String key = (String) pairs[i];
            require(!result.containsKey(key), "duplicate key " + key);
            result.put(key, pairs[i + 1]);
        }
        return result;
    }

    private static String hex(long value) {
        return String.format(Locale.ROOT, "%016x", value);
    }

    private static String digest(Path path) throws Exception {
        byte[] sum = MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path));
        StringBuilder encoded = new StringBuilder(64);
        for (byte unit : sum) encoded.append(String.format(Locale.ROOT, "%02x", unit & 255));
        return encoded.toString();
    }

    private static Field field(Class<?> owner, String name) throws Exception {
        Field result = owner.getDeclaredField(name);
        result.setAccessible(true);
        return result;
    }

    private static boolean bool(String text) {
        require(text.equals("true") || text.equals("false"), "boolean true|false");
        return Boolean.parseBoolean(text);
    }

    private static int bounded(String text, int minimum, int maximum) {
        int value = Integer.parseInt(text);
        require(value >= minimum && value <= maximum, "integer range " + minimum + ".." + maximum);
        return value;
    }

    private static int index(int x, int y, int z) {
        require(x >= MIN && x < MIN + WIDTH && z >= MIN && z < MIN + WIDTH &&
                y >= 0 && y < HEIGHT, "prepared cell domain");
        return ((x - MIN) * WIDTH + z - MIN) * HEIGHT + y;
    }

    private static final class Request {
        final String name, action;
        final int id, x, y, z, metadata, plantLight, growthLight, soilId, soilMeta;
        final long seed, worldSeed;
        final boolean remote, sky;
        final List<int[]> overrides = new ArrayList<>();

        Request(String row) {
            String[] parts = row.split("\t", -1);
            require(parts.length == 16, "request requires 16 tab-separated fields");
            name = parts[0]; action = parts[1];
            require(!name.isEmpty() && name.length() <= 128, "case name");
            require(action.equals("tick") || action.equals("neighbor") || action.equals("grow"),
                    "action tick|neighbor|grow");
            id = bounded(parts[2], 1, 255);
            x = bounded(parts[3], MIN, MIN + WIDTH - 1);
            y = bounded(parts[4], 1, HEIGHT - 1);
            z = bounded(parts[5], MIN, MIN + WIDTH - 1);
            metadata = bounded(parts[6], 0, 15);
            seed = Long.parseLong(parts[7]); worldSeed = Long.parseLong(parts[8]);
            remote = bool(parts[9]);
            plantLight = Integer.parseInt(parts[10]); growthLight = Integer.parseInt(parts[11]);
            sky = bool(parts[12]);
            soilId = bounded(parts[13], 0, 255); soilMeta = bounded(parts[14], 0, 15);
            if (!parts[15].equals("-")) {
                for (String cell : parts[15].split(";", -1)) {
                    String[] values = cell.split(",", -1);
                    require(values.length == 5, "override x,y,z,id,metadata");
                    int[] entry = {Integer.parseInt(values[0]), Integer.parseInt(values[1]),
                            Integer.parseInt(values[2]), bounded(values[3], 0, 255),
                            bounded(values[4], 0, 15)};
                    index(entry[0], entry[1], entry[2]);
                    require(overrides.size() < 4096, "explicit fixture override limit");
                    overrides.add(entry);
                }
            }
        }

        Object view() {
            List<Object> rows = new ArrayList<>();
            for (int[] entry : overrides)
                rows.add(Arrays.asList(entry[0], entry[1], entry[2], entry[3], entry[4]));
            return object("name", name, "action", action, "block", id,
                    "position", Arrays.asList(x, y, z), "metadata", metadata,
                    "seed", Long.toString(seed), "world_seed", Long.toString(worldSeed),
                    "remote", remote, "plant_light", plantLight, "growth_light", growthLight,
                    "sky_visible", sky, "soil", Arrays.asList(soilId, soilMeta), "cells", rows);
        }
    }

    private static List<Request> requests(Path input) throws Exception {
        List<Request> result = new ArrayList<>();
        for (String row : Files.readAllLines(input, StandardCharsets.UTF_8)) {
            if (row.isBlank() || row.startsWith("#")) continue;
            require(result.size() < 64, "finite experiment limit 64 requests");
            Request request = new Request(row);
            for (Request previous : result)
                require(!previous.name.equals(request.name), "unique case names");
            result.add(request);
        }
        require(!result.isEmpty(), "at least one request");
        return result;
    }

    /* This is project-owned dependency-fixture source, not original World or
     * Chunk implementation. Writes run original Block removal/addition bodies
     * at the paired Chunk setter's ID/metadata and remote-guard boundaries.
     * Height, light propagation, observers and neighbor dispatch remain prepared
     * services. Presence declares four loaded Chunks, X/Z0..1, independently of
     * the larger inspection arrays. Every World boundary call is recorded. */
    private static String fixtureSource(boolean client) {
        String source = """
                import java.util.ArrayList;
                import java.util.List;
                public final class PlantLifecycleFixtureWorld extends @WORLD@ {
                    public int[] ids, metadata;
                    public List<String> trace, setters;
                    public List<Object> published;
                    public int preparedLight, preparedGrowth;
                    public boolean preparedSky;
                    public PlantLifecycleFixtureWorld() {
                        super((@STORE@) null, "unused", 0L, (@DIMENSION@) null);
                        throw new AssertionError("fixture constructor must never execute");
                    }
                    private int slot(int x,int y,int z) {
                        if (y < 0 || y >= 128) return -1;
                        if (x < -16 || x >= 32 || z < -16 || z >= 32)
                            throw new AssertionError("fixture array boundary");
                        return ((x+16)*48+z+16)*128+y;
                    }
                    private void note(List<String> target,String row) {
                        if (target.size() >= 262144) throw new AssertionError("fixture event limit");
                        target.add(row);
                    }
                    private String coordinates(int x,int y,int z) {
                        return x+","+y+","+z;
                    }
                    private int raw(int x,int y,int z) {
                        int slot=slot(x,y,z); return slot < 0 ? 0 : ids[slot];
                    }
                    private boolean write(String kind,int x,int y,int z,int id,int data,boolean withMeta) {
                        int slot=slot(x,y,z), narrowed=id&255, packed=data&15;
                        int old=slot < 0 ? 0 : ids[slot];
                        boolean changed=slot >= 0 && (old!=id ||
                            (withMeta && metadata[slot]!=data));
                        String event=kind+":"+coordinates(x,y,z)+":"+id+":"+data+":"+changed;
                        note(setters,event); note(trace,"setter:"+event);
                        if (!changed) return false;
                        // Paired lm/hi.a: ID store96/111 precedes old callback117/142.
                        ids[slot]=narrowed;
                        if (old!=0 && (!withMeta || !this.B)) {
                            note(trace,"removed:"+coordinates(x,y,z)+":"+old);
                            @BLOCK@.m[old].b(this,x,y,z);
                        }
                        // ID-only resets at128; metadata overload stores at154 and271.
                        // Prepared height/light services between those stores change no cells.
                        metadata[slot]=withMeta ? packed : 0;
                        if (withMeta) metadata[slot]=packed;
                        // New callback253 is remote-guarded; callback294 is unconditional.
                        // Select m[id] now: callbacks may have changed the actual cell earlier.
                        if (id!=0 && (withMeta || !this.B)) {
                            note(trace,"added:"+coordinates(x,y,z)+":"+id);
                            @BLOCK@.m[id].c(this,x,y,z);
                        }
                        return changed;
                    }
                    private boolean preparedRegion(int minX,int minY,int minZ,int maxX,int maxY,int maxZ) {
                        // fd/dj.a(IIIIII)Z: Y guard before all six ordered chunk shifts.
                        if (maxY < 0 || minY >= 128) return false;
                        minX >>= 4; minY >>= 4; minZ >>= 4;
                        maxX >>= 4; maxY >>= 4; maxZ >>= 4;
                        for (int cx=minX; cx<=maxX; ++cx)
                            for (int cz=minZ; cz<=maxZ; ++cz)
                                if (cx<0 || cx>1 || cz<0 || cz>1) return false;
                        return true;
                    }
                    @Override public boolean a(int minX,int minY,int minZ,int maxX,int maxY,int maxZ) {
                        boolean value=preparedRegion(minX,minY,minZ,maxX,maxY,maxZ);
                        note(trace,"present:"+coordinates(minX,minY,minZ)+":"+
                            coordinates(maxX,maxY,maxZ)+":"+value);
                        return value;
                    }
                    @Override public int a(int x,int y,int z) {
                        int value=raw(x,y,z); note(trace,"block:"+coordinates(x,y,z)+":"+value); return value;
                    }
                    @Override public int @META@(int x,int y,int z) {
                        int slot=slot(x,y,z), value=slot < 0 ? 0 : metadata[slot];
                        note(trace,"metadata:"+coordinates(x,y,z)+":"+value); return value;
                    }
                    @Override public boolean @AIR@(int x,int y,int z) {
                        boolean value=raw(x,y,z)==0; note(trace,"air:"+coordinates(x,y,z)+":"+value); return value;
                    }
                    @Override public @MATERIAL@ @MATERIAL_GET@(int x,int y,int z) {
                        int id=raw(x,y,z); note(trace,"material:"+coordinates(x,y,z)+":"+id);
                        return id==0 ? @MATERIAL@.a : @BLOCK@.m[id].bA;
                    }
                    @Override public int @PLANT_LIGHT@(int x,int y,int z) {
                        note(trace,"plant_light:"+coordinates(x,y,z)+":"+preparedLight); return preparedLight;
                    }
                    @Override public int @GROWTH_LIGHT@(int x,int y,int z) {
                        note(trace,"growth_light:"+coordinates(x,y,z)+":"+preparedGrowth); return preparedGrowth;
                    }
                    @Override public boolean @SKY@(int x,int y,int z) {
                        note(trace,"sky:"+coordinates(x,y,z)+":"+preparedSky); return preparedSky;
                    }
                    @Override public boolean @RAIN@(int x,int y,int z) {
                        note(trace,"rain:"+coordinates(x,y,z)+":false"); return false;
                    }
                    @Override public boolean @QUIET_SET@(int x,int y,int z,int id) {
                        return write("quiet_block",x,y,z,id,0,false);
                    }
                    @Override public boolean a(int x,int y,int z,int id,int data) {
                        return write("quiet_block_metadata",x,y,z,id,data,true);
                    }
                    @Override public boolean @NOTIFY_SET@(int x,int y,int z,int id) {
                        return write("block",x,y,z,id,0,false);
                    }
                    @Override public boolean b(int x,int y,int z,int id,int data) {
                        return write("block_metadata",x,y,z,id,data,true);
                    }
                    @Override public void @NOTIFY_META@(int x,int y,int z,int data) {
                        int slot=slot(x,y,z); String event="metadata:"+coordinates(x,y,z)+":"+data;
                        note(setters,event); note(trace,"setter:"+event);
                        if (slot >= 0) metadata[slot]=data&15;
                    }
                    @Override public boolean @QUIET_META@(int x,int y,int z,int data) {
                        int slot=slot(x,y,z); String event="quiet_metadata:"+coordinates(x,y,z)+":"+data;
                        note(setters,event); note(trace,"setter:"+event);
                        // fd.e/dj.d70 invokes Chunk.b14; equal nibble writes still return true.
                        if (slot >= 0) metadata[slot]=data&15; return slot >= 0;
                    }
                    @Override public boolean b(@ENTITY@ entity) {
                        if (published.size() >= 4096) throw new AssertionError("fixture publication limit");
                        published.add(entity); note(trace,"publish:"+(published.size()-1)); return true;
                    }
                }
                """;
        String[][] names = {{"WORLD", "fd", "dj"}, {"STORE", "wt", "om"},
                {"DIMENSION", "xa", "os"}, {"BLOCK", "uu", "na"},
                {"MATERIAL", "ln", "hj"}, {"ENTITY", "sn", "lq"},
                {"META", "e", "c"}, {"AIR", "d", "f"}, {"MATERIAL_GET", "f", "d"},
                {"PLANT_LIGHT", "m", "k"}, {"GROWTH_LIGHT", "n", "l"},
                {"SKY", "l", "j"}, {"RAIN", "t", "s"}, {"QUIET_SET", "c", "b"},
                {"NOTIFY_SET", "f", "e"}, {"NOTIFY_META", "d", "c"},
                {"QUIET_META", "e", "d"}};
        for (String[] row : names) source = source.replace("@" + row[0] + "@", row[client ? 1 : 2]);
        require(!source.contains("@WORLD@"), "complete generated fixture binding");
        return source;
    }

    private static Object allocate(Class<?> type) throws Exception {
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Object unsafe = field(unsafeType, "theUnsafe").get(null);
        return unsafeType.getMethod("allocateInstance", Class.class).invoke(unsafe, type);
    }

    private static Object snapshot(int[] ids, int[] metadata) {
        List<Object> result = new ArrayList<>();
        for (int x = MIN; x < MIN + WIDTH; ++x)
            for (int z = MIN; z < MIN + WIDTH; ++z)
                for (int y = 0; y < HEIGHT; ++y) {
                    int index = index(x, y, z);
                    if (ids[index] != 0 || metadata[index] != 0)
                        result.add(Arrays.asList(x, y, z, ids[index], metadata[index]));
                }
        return result;
    }

    private static Object tail(Random random) {
        return object("next_long_hex", hex(random.nextLong()), "next_int31", random.nextInt(31),
                "next_double_bits", hex(Double.doubleToRawLongBits(random.nextDouble())),
                "next_gaussian_bits", hex(Double.doubleToRawLongBits(random.nextGaussian())));
    }

    private static Object drops(List<Object> published, boolean client, ClassLoader loader)
            throws Exception {
        Class<?> item = Class.forName(client ? "hl" : "ez", false, loader);
        Class<?> stack = Class.forName(client ? "iz" : "fy", false, loader);
        Class<?> entity = Class.forName(client ? "sn" : "lq", false, loader);
        List<Object> rows = new ArrayList<>();
        for (Object value : published) {
            require(value != null && value.getClass() == item, "publication is actual original ItemEntity");
            Object payload = field(item, "a").get(value);
            require(payload != null && payload.getClass() == stack, "actual original item stack");
            rows.add(object("stack", Arrays.asList(field(stack, "a").getInt(payload),
                    field(stack, "b").getInt(payload), field(stack, "c").getInt(payload),
                    field(stack, "d").getInt(payload)), "pickup_delay", field(item, "c").getInt(value),
                    "position_bits", Arrays.asList(
                            hex(Double.doubleToRawLongBits(field(entity, client ? "aM" : "aP").getDouble(value))),
                            hex(Double.doubleToRawLongBits(field(entity, client ? "aN" : "aQ").getDouble(value))),
                            hex(Double.doubleToRawLongBits(field(entity, client ? "aO" : "aR").getDouble(value))))));
        }
        return rows;
    }

    private static Object observe(Request request, boolean client, Class<?> fixture,
                                  Class<?> world, Class<?> blocks, ClassLoader loader) throws Exception {
        int[] ids = new int[CELLS], metadata = new int[CELLS];
        for (int x = MIN; x < MIN + WIDTH; ++x)
            for (int z = MIN; z < MIN + WIDTH; ++z) {
                int cell = index(x, request.y - 1, z);
                ids[cell] = request.soilId; metadata[cell] = request.soilMeta;
            }
        int center = index(request.x, request.y, request.z);
        ids[center] = request.id; metadata[center] = request.metadata;
        for (int[] row : request.overrides) {
            int cell = index(row[0], row[1], row[2]); ids[cell] = row[3]; metadata[cell] = row[4];
        }
        Object before = snapshot(ids, metadata);
        List<String> trace = new ArrayList<>(), setters = new ArrayList<>();
        List<Object> published = new ArrayList<>();
        Random supplied = new Random(request.seed), worldRandom = new Random(request.worldSeed);
        Object prepared = allocate(fixture);
        field(fixture, "ids").set(prepared, ids); field(fixture, "metadata").set(prepared, metadata);
        field(fixture, "trace").set(prepared, trace); field(fixture, "setters").set(prepared, setters);
        field(fixture, "published").set(prepared, published);
        field(fixture, "preparedLight").setInt(prepared, request.plantLight);
        field(fixture, "preparedGrowth").setInt(prepared, request.growthLight);
        field(fixture, "preparedSky").setBoolean(prepared, request.sky);
        field(world, "B").setBoolean(prepared, request.remote); field(world, "r").set(prepared, worldRandom);
        Object receiver = ((Object[]) field(blocks, "m").get(null))[request.id];
        require(receiver != null && receiver.getClass().getClassLoader() == loader,
                "actual original registry receiver");
        String[] plants = client ? new String[] {"wb", "he", "ru", "jb", "tl", "ni", "ri", "or", "vl"}
                : new String[] {"nw", "eu", "kz", "fz", "md", "ij", "ko", "je", "no"};
        require(Arrays.asList(plants).contains(receiver.getClass().getName()), "declared plant receiver");
        Method method;
        Object[] arguments;
        if (request.action.equals("neighbor")) {
            method = receiver.getClass().getMethod("b", world, int.class, int.class, int.class, int.class);
            arguments = new Object[] {prepared, request.x, request.y, request.z, 0};
        } else {
            require(!request.action.equals("grow") || receiver.getClass().getName().equals(client ? "he" : "eu"),
                    "grow action requires actual sapling");
            method = receiver.getClass().getMethod(request.action.equals("grow") ? (client ? "c" : "b") : "a",
                    world, int.class, int.class, int.class, Random.class);
            arguments = new Object[] {prepared, request.x, request.y, request.z, supplied};
        }
        require(method.getDeclaringClass().getClassLoader() == loader &&
                method.getDeclaringClass().getProtectionDomain().getCodeSource().getLocation().equals(
                        blocks.getProtectionDomain().getCodeSource().getLocation()), "method belongs to original JAR");
        Object outcome;
        try {
            method.invoke(receiver, arguments);
            outcome = object("status", "returned");
        } catch (InvocationTargetException wrapped) {
            Throwable cause = wrapped.getCause();
            outcome = object("status", "threw", "class", cause.getClass().getName(), "message", cause.getMessage());
        }
        String descriptor = "(L" + world.getName() + ";" +
                (request.action.equals("neighbor") ? "IIII)V" : "IIILjava/util/Random;)V");
        return object("request", request.view(), "receiver_class", receiver.getClass().getName(),
                "method_class", method.getDeclaringClass().getName(), "method", method.getName(),
                "descriptor", descriptor, "outcome", outcome, "before", before, "after", snapshot(ids, metadata),
                "trace", trace, "setters", setters, "supplied_random_tail", tail(supplied),
                "world_random_tail", tail(worldRandom), "drops", drops(published, client, loader));
    }

    private static void json(StringBuilder out, Object value) {
        if (value == null) out.append("null");
        else if (value instanceof String) {
            out.append('"');
            for (char unit : ((String) value).toCharArray()) {
                if (unit == '"' || unit == '\\') out.append('\\').append(unit);
                else if (unit < 32 || unit > 126) out.append(String.format(Locale.ROOT, "\\u%04x", (int) unit));
                else out.append(unit);
            }
            out.append('"');
        } else if (value instanceof Number || value instanceof Boolean) out.append(value);
        else if (value instanceof List<?>) {
            out.append('['); boolean comma = false;
            for (Object item : (List<?>) value) {
                if (comma) out.append(','); json(out, item); comma = true;
            }
            out.append(']');
        } else if (value instanceof Map<?, ?>) {
            out.append('{'); boolean comma = false;
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                if (comma) out.append(','); json(out, entry.getKey()); out.append(':');
                json(out, entry.getValue()); comma = true;
            }
            out.append('}');
        } else throw new IllegalArgumentException("unsupported JSON value");
    }

    /** Arguments: client|server original.jar fresh_generated_dir requests.tsv output.json.
     * Each noncomment TSV row has: name action blockId x y z metadata seed
     * worldSeed remote plantLight growthLight skyVisible soilId soilMeta cells.
     * cells is '-' or semicolon-separated x,y,z,id,metadata overrides. Preparation
     * fills the y-1 plane, sets the requested cell, then applies overrides.
     * The complete sparse maps use x/z[-16,31], y[0,127], default ID/metadata0;
     * row order is x, z, y. Every other in-domain cell is explicitly zero. */
    public static void main(String[] args) throws Exception {
        require(args.length == 5, "client|server original.jar fresh_generated_dir requests.tsv output.json");
        require(args[0].equals("client") || args[0].equals("server"), "side");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath(), input = Path.of(args[3]).toRealPath();
        Path generated = Path.of(args[2]).toAbsolutePath().normalize();
        Path output = Path.of(args[4]).toAbsolutePath().normalize();
        List<Request> declared = requests(input);
        String expected = client ? CLIENT_SHA256 : SERVER_SHA256;
        require(digest(jar).equals(expected), "verified original JAR SHA-256");
        require(!Files.exists(generated) && !Files.exists(output), "fresh fixture directory and output");
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        require(compiler != null, "full JDK required for project-owned World fixture compilation");
        Files.createDirectory(generated);
        Path source = generated.resolve(FIXTURE + ".java");
        Files.writeString(source, fixtureSource(client), StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW);
        require(compiler.run(null, System.err, System.err, "-classpath", jar.toString(), "-d",
                generated.toString(), source.toString()) == 0, "generated fixture compilation");
        PrintStream console = System.out;
        List<Object> rows = new ArrayList<>();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL(), generated.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            System.setOut(System.err);
            Class<?> blocks = Class.forName(client ? "uu" : "na", true, loader);
            Class<?> world = Class.forName(client ? "fd" : "dj", false, loader);
            Class<?> fixture = Class.forName(FIXTURE, true, loader);
            require(fixture.getSuperclass() == world, "fixture extends actual original World");
            for (Request request : declared) rows.add(observe(request, client, fixture, world, blocks, loader));
        } finally {
            System.setOut(console);
        }
        require(digest(jar).equals(expected), "original JAR unchanged after observation");
        StringBuilder encoded = new StringBuilder();
        json(encoded, object("schema", "plant-dependency-fixture-v1", "side", args[0],
                "jar_sha256", expected, "requests_sha256", digest(input),
                "fixture_source_sha256", digest(source),
                "scope", "original Block lifecycle, mutation callbacks and selected tree bodies with prepared World services",
                "not_measured", "original World/Chunk, observer/neighbor and light propagation parity; default constructor profile/time; item Math.random fields",
                "extent", Arrays.asList(MIN, MIN + WIDTH - 1, 0, HEIGHT - 1, MIN, MIN + WIDTH - 1),
                "prepared_loaded_chunks", Arrays.asList(0, 1, 0, 1),
                "sparse_default", Arrays.asList(0, 0), "cases", rows));
        encoded.append('\n');
        require(encoded.length() <= 128 * 1024 * 1024, "finite transcript byte bound");
        Files.writeString(output, encoded, StandardCharsets.US_ASCII, StandardOpenOption.CREATE_NEW);
    }
}
