import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Genuine persistent-world load and natural population callback boundaries.
 * Original game state changes only through real constructors and methods.
 */
public final class CacheCallbacksObservation {
    private static final int MAX_BYTES = 32 * 1024 * 1024;
    private static boolean measuring;
    private static final class SetupUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupUnavailable(String message, Throwable cause) { super(message, cause); }
    }
    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private static Map<String, Object> object(Object... values) {
        require(values.length % 2 == 0, "object pairs");
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }
    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers) throws Exception {
        Field result = owner.getDeclaredField(name);
        require(result.getType() == type && result.getModifiers() == modifiers,
                "field descriptor/modifiers: " + owner.getName() + "." + name);
        result.setAccessible(true);
        return result;
    }
    private static Method method(Class<?> owner, String name, Class<?> result, int modifiers,
                                 Class<?>... arguments) throws Exception {
        Method value = owner.getDeclaredMethod(name, arguments);
        require(value.getReturnType() == result && value.getModifiers() == modifiers,
                "method descriptor/modifiers: " + owner.getName() + "." + name);
        value.setAccessible(true);
        return value;
    }
    private static Exception unwrap(InvocationTargetException failure) throws Exception {
        Throwable cause = failure.getCause();
        if (cause instanceof Exception) return (Exception) cause;
        if (cause instanceof Error) throw (Error) cause;
        return failure;
    }
    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) { throw unwrap(failure); }
    }
    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    private static void origin(Class<?> type, ClassLoader loader, URL jar, Map<String, Object> origins) {
        require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                "actual supplied archive class: " + type.getName());
        origins.put(type.getName(), jar.toString());
    }
    private static void begin(Object world, Object cache, int index) { }
    private static void end(Object result, int index) { }
    private static void identity(Object value, String reference) { }
    private static void timeBegin(Object world, long value) { }
    private static void timeEnd(Object world, long value) { }
    private static Object blob(byte[] values) {
        return values == null ? null : object("length", values.length, "values", HexFormat.of().formatHex(values));
    }
    private static Constructor<?> constructor(Class<?> type, Class<?>... args) throws Exception {
        Constructor<?> result = type.getDeclaredConstructor(args);
        require(result.getModifiers() == 1, "exact public original constructor " + type.getName());
        return result;
    }
    private static Object construct(Constructor<?> constructor, Object... args) throws Exception {
        try { return constructor.newInstance(args); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (!measuring && (cause instanceof Exception || cause instanceof LinkageError))
                throw new SetupUnavailable("genuine original constructor failed", cause);
            throw unwrap(failure);
        } catch (LinkageError failure) { throw new SetupUnavailable("original constructor dependency", failure); }
    }

    private static final class Observer {
        final boolean client;
        final URL jar;
        final ClassLoader loader;
        final Class<?> modelType, worldType, chunkType, cacheType, infoType;
        final Object model, world, provider, storage, cache, saveFormat;
        final Method get, worldSnapshot, reference, json, acquire, save, close, time, setTime;
        final Map<String, Object> origins = new LinkedHashMap<>();
        final Map<String, Class<?>> classes = new LinkedHashMap<>();
        final IdentityHashMap<Object, Boolean> tagged = new IdentityHashMap<>();
        final List<Integer> spawn = new ArrayList<>();
        final Map<String, Object> setupMetadata;
        Class<?> type(String name) throws Exception {
            Class<?> result = Class.forName(name, true, loader); origin(result, loader, jar, origins);
            classes.put(name, result); return result;
        }
        void observerReference(String name, Object value) throws Exception {
            require(Arrays.asList("world", "provider", "storage", "cache").contains(name)
                    && modelType.getClassLoader() != loader, "only project-owned observer reference bindings");
            field(modelType, name, Object.class, 0).set(model, value);
        }
        Object get(String name, Object receiver) throws Exception { return invoke(get, model, name, receiver); }
        Object ref(Object value) throws Exception {
            Object result = invoke(reference, model, value);
            if (value != null && !tagged.containsKey(value)) {
                require(tagged.size() < 1024, "bounded first-encounter identity tags"); tagged.put(value, true);
                identity(value, (String) ((Map<?, ?>) result).get("ref"));
            }
            return result;
        }
        Observer(URL jar, ClassLoader loader, Path root, boolean client, String scenario) throws Exception {
            this.jar = jar; this.loader = loader; this.client = client;
            modelType = Class.forName("ChunkObservation$Model");
            Constructor<?> shared = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
            require(shared.getModifiers() == 0, "shared model constructor"); shared.setAccessible(true);
            try { model = shared.newInstance(loader, client); }
            catch (InvocationTargetException failure) {
                if (failure.getCause() instanceof LinkageError) throw new SetupUnavailable("original initialization dependency", failure.getCause());
                throw unwrap(failure);
            }
            get = method(modelType, "get", Object.class, 0, String.class, Object.class);
            worldSnapshot = method(modelType, "worldSnapshot", Map.class, 0, int.class, int.class, int.class);
            reference = method(modelType, "reference", Map.class, 0, Object.class);
            json = method(ChunkObservation.class, "json", String.class, 10, Object.class);
            worldType = type(client ? "fd" : "dj"); chunkType = type(client ? "lm" : "hi");
            cacheType = type(client ? "ok" : "jb"); infoType = type(client ? "ei" : "ct");
            Class<?> storageType = type(client ? "wt" : "om"), providerType = type(client ? "xa" : "os");
            for (String name : client ? new String[] {"hi", "mx", "fm", "ld", "bf", "rh", "yf", "rj", "qj", "yy", "wi", "st", "eb", "uu", "li", "xv", "kd", "fv", "sq"}
                    : new String[] {"ie", "dq", "hb", "an", "kp", "pk", "kq", "jx", "qa", "ob", "lv", "co", "na", "hg", "ph", "gs", "dy", "lt"}) type(name);
            acquire = method(cacheType, "c", chunkType, 1, int.class, int.class);
            save = method(worldType, "a", void.class, 1, boolean.class, type(client ? "yb" : "pj"));
            time = method(worldType, client ? "t" : "m", long.class, 1);
            setTime = method(worldType, "a", void.class, 1, long.class);
            if (scenario.equals("load")) {
                require(Files.isDirectory(root, LinkOption.NOFOLLOW_LINKS)
                        && Files.isRegularFile(root.resolve("world/level.dat"), LinkOption.NOFOLLOW_LINKS), "fresh copied genuine saved world root");
            } else Files.createDirectory(root);
            if (client) {
                saveFormat = construct(constructor(classes.get("hi"), java.io.File.class), root.toFile());
                storage = invoke(method(classes.get("hi"), "a", storageType, 1, String.class, boolean.class), saveFormat, "world", false);
                require(storage.getClass() == classes.get("mx"), "actual persistent region save handler");
                world = construct(constructor(worldType, storageType, String.class, long.class), storage, "cache-callback-reference", 23L);
                close = method(classes.get("hi"), "c", void.class, 1);
            } else {
                saveFormat = null;
                storage = construct(constructor(classes.get("ie"), java.io.File.class, String.class, boolean.class), root.toFile(), "world", false);
                Object normal = construct(constructor(classes.get("kp")));
                world = construct(constructor(worldType, storageType, String.class, long.class, providerType), storage, "cache-callback-reference", 23L, normal);
                close = method(classes.get("ie"), "e", void.class, 1);
            }
            provider = get("world.t", world); cache = get("world.v", world);
            for (var pair : object("world", world, "provider", provider, "storage", storage, "cache", cache).entrySet()) observerReference(pair.getKey(), pair.getValue());
            require(world.getClass() == worldType && cache.getClass() == cacheType && get("cache.g", cache) == world
                    && get("provider.a", provider) == world && get("world.w", world) == storage
                    && get("cache.d", cache).getClass() == classes.get(client ? "ld" : "hb")
                    && get("cache.c", cache).getClass() == classes.get(client ? "yf" : "pk"), "actual persistent base-world ownership");
            require((long) invoke(method(worldType, client ? "s" : "l", long.class, 1), world) == 23L, "actual seed23");
            Object info = field(worldType, "x", infoType, 4).get(world);
            for (String name : new String[] {"c", "d", "e"}) spawn.add((Integer) invoke(method(infoType, name, int.class, 1), info));
            require(spawn.equals(Arrays.asList(0,64,0)), "qualifying actual origin spawn");
            Map<?, ?> map = (Map<?, ?>) get("cache.e", cache); List<?> list = (List<?>) get("cache.f", cache);
            require(((Set<?>) get("cache.a", cache)).isEmpty() && ((List<?>) get("world.C", world)).isEmpty(), "genuine empty pending/queue setup");
            if (scenario.equals("load")) require(map.isEmpty() && list.isEmpty(), "copied-world constructor does not preload cache");
            else {
                Object origin = map.get(0);
                require(map.size() == 1 && list.size() == 1 && origin != null && origin.getClass() == chunkType
                        && list.get(0) == origin && (int) get("chunk.j", origin) == 0 && (int) get("chunk.k", origin) == 0, "genuine constructor origin baseline");
            }
            setupMetadata = object("spawn", spawn, "constructor_map_count", map.size(), "constructor_insertion_count", list.size(),
                    "constructor_pending_count", 0, "constructor_queue_count", 0, "provider_numerical_state",
                    "outside cache-callback seam; per-VM Random seeds retained by read-only debugger");
        }
        Object fullChunk(Object value) throws Exception {
            require(value != null && value.getClass() == chunkType, "actual ordinary cached chunk");
            Map<String, Object> flags = new LinkedHashMap<>();
            for (String name : new String[] {"c", "n", "o", "p", "q"}) flags.put(name, get("chunk." + name, value));
            Map<?, ?> tiles = (Map<?, ?>) get("chunk.l", value);
            require(tiles.isEmpty(), "finite experiment has no tile objects");
            Object[] entities = (Object[]) get("chunk.m", value); require(entities.length == 8, "eight actual entity slices");
            List<Object> slices = new ArrayList<>(), identities = new ArrayList<>();
            for (Object slice : entities) {
                require(slice instanceof List<?> && ((List<?>) slice).isEmpty(), "finite experiment has empty entity slices");
                slices.add(object("object", ref(slice), "size", 0));
            }
            byte[] blocks = (byte[]) get("chunk.b", value), heights = (byte[]) get("chunk.h", value);
            require(blocks.length == 32768 && heights.length == 256, "complete ordinary chunk arrays");
            identities.add(ref(blocks));
            Map<String, Object> data = new LinkedHashMap<>(); data.put("blocks", blob(blocks));
            String[] names = {"e", "f", "g"}, roles = {"metadata", "skylight", "block_light"};
            for (int i = 0; i < names.length; ++i) {
                Object wrapper = get("chunk." + names[i], value); byte[] array = (byte[]) get("nibble.a", wrapper);
                require(array.length == 16384, "complete actual packed array");
                data.put(roles[i], blob(array)); identities.add(ref(wrapper)); identities.add(ref(array));
            }
            data.put("heightmap", blob(heights)); data.put("minimum_height", get("chunk.i", value)); identities.add(ref(heights));
            return object("object", ref(value), "world", ref(get("chunk.d", value)),
                    "coordinates", Arrays.asList(get("chunk.j", value), get("chunk.k", value)), "data", data,
                    "flags", flags, "time", String.format(java.util.Locale.ROOT, "%016x", (long) get("chunk.r", value)),
                    "array_references", identities, "tile_map", object("object", ref(tiles), "size", 0), "entity_slices", slices);
        }


        Object snapshot(int x, int z) throws Exception {
            Map<?, ?> map = (Map<?, ?>) get("cache.e", cache); List<?> list = (List<?>) get("cache.f", cache);
            require(map.size() <= 32 && list.size() <= 64 && ((Set<?>) get("cache.a", cache)).size() <= 64
                    && ((List<?>) get("world.C", world)).size() <= 32768, "bounded complete cache callback snapshot");
            ref(world); ref(cache); ref(get("cache.b", cache)); ref(get("cache.c", cache)); ref(get("cache.d", cache));
            ref(map); ref(list); ref(get("cache.a", cache)); ref(get("world.C", world));
            Object worldState = invoke(worldSnapshot, model, x, z, 32768);
            Object fallback = get("cache.b", cache);
            ref(get("chunk.b", fallback));
            for (String name : new String[] {"e", "f", "g"}) {
                Object wrapper = get("chunk." + name, fallback);
                ref(wrapper); ref(get("nibble.a", wrapper));
            }
            ref(get("chunk.h", fallback));
            IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>(); List<Object> full = new ArrayList<>();
            List<Object> values = new ArrayList<>(map.values()); values.addAll(list); values.add(get("cache.b", cache));
            for (Object chunk : values) if (chunk != null && chunk.getClass() == chunkType && seen.put(chunk, true) == null) full.add(fullChunk(chunk));
            int[] opacity = (int[]) field(modelType, "opacity", int[].class, 16).get(model);
            int[] initialOpacity = (int[]) field(modelType, "initialOpacity", int[].class, 16).get(model);
            boolean identity = ((Field) field(modelType, "opacityField", Field.class, 16).get(model)).get(null) == opacity;
            require(identity && Arrays.equals(opacity, initialOpacity), "original opacity table retained");
            return object("world", worldState, "cached_locals", full, "world_time", String.format("%016x", (long) invoke(time, world)),
                    "world_force_load", field(worldType, "y", boolean.class, 1).get(world), "provider_force_load", null, "spawn", spawn,
                    "opacity", blob((byte[]) invoke(method(modelType, "opacityBytes", byte[].class, 0), model)),
                    "opacity_identity_retained", identity, "opacity_values_retained", true);
        }
        void close() throws Exception { invoke(close, client ? saveFormat : storage); }
    }
    public static void main(String[] args) {
        PrintStream originalOut = System.out; System.setOut(System.err); int status = 1;
        try {
            require(args.length == 6, "jar side prepare|load|population seed_hex owned_world_root snapshots_json");
            boolean client = args[1].equals("client"); require(client || args[1].equals("server"), "side");
            String scenario = args[2]; require(Arrays.asList("prepare","load","population").contains(scenario) && args[3].equals("0000000000000017"), "fixed scenario and seed23");
            Path jarPath = Path.of(args[0]).toRealPath(), root = Path.of(args[4]).toAbsolutePath().normalize(), output = Path.of(args[5]).toAbsolutePath().normalize();
            require(!Files.isSymbolicLink(root) && Files.isDirectory(root.getParent()) && (scenario.equals("load") || !Files.exists(root, LinkOption.NOFOLLOW_LINKS))
                    && !Files.exists(output, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(output) && Files.isDirectory(output.getParent()) && !output.startsWith(root), "owned root and separate new snapshot output");
            String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c" : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
            require(Files.isRegularFile(jarPath) && hash(Files.readAllBytes(jarPath)).equals(expected), "pinned supplied original archive");
            URL jar = jarPath.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                Observer observer = new Observer(jar, loader, root, client, scenario);
                measuring = true; field(ChunkObservation.class, "measuring", boolean.class, 10).setBoolean(null,true);
                Object timeAPI = null;
                if (scenario.equals("load")) {
                    long before = (long) invoke(observer.time, observer.world);
                    timeBegin(observer.world,1234L); invoke(observer.setTime, observer.world,1234L); timeEnd(observer.world,1234L);
                    require((long) invoke(observer.time, observer.world) == 1234L, "actual public world-time API result");
                    timeAPI = object("class", observer.worldType.getName(), "method", "a", "signature", "(J)V", "modifiers", 1,
                            "value", 1234, "before", String.format("%016x", before), "after", "00000000000004d2");
                }
                Object initial = observer.snapshot(0,0); List<Object> operations = new ArrayList<>();
                int[][] coords = scenario.equals("load") ? new int[][] {{1,0},{1,0}} : scenario.equals("population")
                        ? new int[][] {{1,0},{0,1},{1,1},{1,1}} : new int[][] {{1,0}};
                for (int index = 0; index < coords.length; ++index) {
                    int x = coords[index][0], z = coords[index][1]; Object before = observer.snapshot(x,z);
                    begin(observer.world,observer.cache,index); Object result = invoke(observer.acquire,observer.cache,x,z); end(result,index);
                    operations.add(object("index",index,"method","c","coordinates",Arrays.asList(x,z),"result",observer.ref(result),"before",before,"after",observer.snapshot(x,z)));
                }
                if (scenario.equals("prepare")) invoke(observer.save,observer.world,true,null);
                Object saved = scenario.equals("prepare") ? observer.snapshot(1,0) : null;
                observer.close();
                Map<String,Object> record = object("schema",1,"scope","cache-callback-boundary","side",args[1],"scenario",scenario,"seed_hex",args[3],"jar_sha256",expected,
                        "platform_parent",loader.getParent()==ClassLoader.getPlatformClassLoader(),"class_origins",observer.origins,
                        "bindings",object("world",observer.worldType.getName(),"cache",observer.cacheType.getName(),"storage",observer.storage.getClass().getName(),"loader",observer.get("cache.d",observer.cache).getClass().getName(),"generator",observer.get("cache.c",observer.cache).getClass().getName()),
                        "setup_metadata",observer.setupMetadata,"time_api",timeAPI,"initial",initial,"operations",operations,"saved",saved,
                        "external_effects","Original loader, generation and terrain-feature internals remain opaque; only cache coordinator transitions are compared.");
                require(hash(Files.readAllBytes(jarPath)).equals(expected),"original archive preserved");
                byte[] encoded = (((String) invoke(observer.json,null,record))+"\n").getBytes(StandardCharsets.UTF_8); require(encoded.length<=MAX_BYTES,"complete snapshot output budget");
                Files.write(output,encoded,StandardOpenOption.CREATE_NEW); status=0;
            }
        } catch(Throwable failure) {
            boolean unavailable = !measuring && (failure instanceof SetupUnavailable || failure.getClass().getName().equals("ChunkObservation$SetupUnavailable"));
            System.err.println(unavailable ? "UNAVAILABLE: genuine callback setup" : "FAILURE: callback observation"); failure.printStackTrace(System.err); if(unavailable)status=77;
        } finally { System.setOut(originalOut); }
        System.exit(status);
    }
}
