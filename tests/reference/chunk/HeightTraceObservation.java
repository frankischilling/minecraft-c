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
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Captures two genuine full finishing passes without replacing world state. */
public final class HeightTraceObservation {
    private static final long SEED = 23L;
    private static final int BLOCK_BYTES = 32768;
    private static final int MAX_REGIONS = 32768;
    private static final int MAX_RECORD_BYTES = 32 * 1024 * 1024;

    private static final class OriginalDependencyUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        OriginalDependencyUnavailable(String message, LinkageError cause) { super(message, cause); }
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

    private static Method method(Class<?> owner, String name, Class<?> result, int modifiers,
                                 Class<?>... arguments) throws Exception {
        Method method = owner.getDeclaredMethod(name, arguments);
        require(method.getReturnType() == result && method.getModifiers() == modifiers,
                "method descriptor/modifiers changed: " + owner.getName() + "." + name);
        method.setAccessible(true);
        return method;
    }

    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers) throws Exception {
        Field field = owner.getDeclaredField(name);
        require(field.getType() == type && field.getModifiers() == modifiers,
                "field descriptor/modifiers changed: " + owner.getName() + "." + name);
        field.setAccessible(true);
        return field;
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException failure) { throw unwrap(failure); }
    }

    private static Exception unwrap(InvocationTargetException failure) throws Exception {
        Throwable cause = failure.getCause();
        if (cause instanceof Exception) return (Exception) cause;
        if (cause instanceof Error) throw (Error) cause;
        return failure;
    }

    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static byte[] input(Path path) throws Exception {
        require(!Files.isSymbolicLink(path) && Files.isRegularFile(path, LinkOption.NOFOLLOW_LINKS)
                && Files.size(path) == BLOCK_BYTES, "input must be exactly 32768 regular bytes");
        byte[] result = Files.readAllBytes(path);
        require(result.length == BLOCK_BYTES, "input byte count changed");
        return result;
    }

    private static void origin(Class<?> type, ClassLoader loader, URL jar, Map<String, Object> origins) {
        require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                "class must originate in supplied JAR: " + type.getName());
        origins.put(type.getName(), jar.toString());
    }

    private static Class<?> originalType(String name, ClassLoader loader) throws Exception {
        try { return Class.forName(name, true, loader); }
        catch (LinkageError failure) {
            throw new OriginalDependencyUnavailable("original class initialization: " + name, failure);
        }
    }

    // Ordinary calls provide exact debugger boundaries. They do not call the world.
    private static void begin(Object world, Object measuredChunk, int pass) {
        System.err.println("HEIGHT_TRACE_BEGIN " + pass);
    }

    private static void end(int pass) { System.err.println("HEIGHT_TRACE_END " + pass); }

    public static void main(String[] args) {
        PrintStream originalOut = System.out;
        System.setOut(System.err);
        int status = 1;
        boolean measuring = false;
        Class<?> unavailableType = null;
        try {
            require(args.length == 8,
                    "arguments: jar side new_world_root snapshot_json initial_file repeat_file chunk_x chunk_z");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            boolean client = args[1].equals("client");
            int chunkX = Integer.parseInt(args[6]), chunkZ = Integer.parseInt(args[7]);
            byte[] initial = input(Path.of(args[4])), repeated = input(Path.of(args[5]));
            Path jarPath = Path.of(args[0]).toRealPath();
            require(Files.isRegularFile(jarPath), "original JAR must be a regular file");
            Path output = Path.of(args[3]).toAbsolutePath().normalize();
            require(!Files.exists(output, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(output)
                    && Files.isDirectory(output.getParent()), "new snapshot output beneath existing parent required");
            URL jar = jarPath.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
                require(loader.getParent() == ClassLoader.getPlatformClassLoader(), "isolated original loader parent");
                Class<?> shared = Class.forName("ChunkObservation");
                Class<?> modelType = Class.forName("ChunkObservation$Model");
                unavailableType = Class.forName("ChunkObservation$SetupUnavailable");
                Constructor<?> modelConstructor = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
                require(modelConstructor.getModifiers() == 0, "shared model constructor modifiers");
                modelConstructor.setAccessible(true);
                Object model;
                try { model = modelConstructor.newInstance(loader, client); }
                catch (InvocationTargetException failure) {
                    if (failure.getCause() instanceof LinkageError) {
                        throw new OriginalDependencyUnavailable("original model class initialization", (LinkageError) failure.getCause());
                    }
                    throw unwrap(failure);
                }
                Method setup = method(modelType, "setup", void.class, 0, Path.class, long.class);
                Method get = method(modelType, "get", Object.class, 0, String.class, Object.class);
                Method snapshot = method(modelType, "snapshot", Map.class, 0, int.class, int.class, int.class);
                Method worldSnapshot = method(modelType, "worldSnapshot", Map.class, 0, int.class, int.class, int.class);
                Method identities = method(modelType, "identities", Object[].class, 0, Object.class);
                Method json = method(shared, "json", String.class, 10, Object.class);
                Map<String, Object> origins = new LinkedHashMap<>();
                for (String role : new String[] {"worldType", "providerType", "storageType", "normalType", "chunkType",
                        "nibbleType", "cacheType", "kindType", "regionType"}) {
                    origin((Class<?>) field(modelType, role, Class.class, 16).get(model), loader, jar, origins);
                }
                for (String name : client ? new String[] {"ei", "cl", "bf", "xv", "uu", "yy", "wy"}
                        : new String[] {"ct", "bl", "an", "ph", "na", "qa", "dq"}) {
                    origin(originalType(name, loader), loader, jar, origins);
                }
                invoke(setup, model, Path.of(args[2]).toAbsolutePath().normalize(), SEED);
                Object world = field(modelType, "world", Object.class, 0).get(model);
                Object cache = field(modelType, "cache", Object.class, 0).get(model);
                Class<?> worldType = (Class<?>) field(modelType, "worldType", Class.class, 16).get(model);
                Class<?> chunkType = (Class<?>) field(modelType, "chunkType", Class.class, 16).get(model);
                Class<?> cacheType = (Class<?>) field(modelType, "cacheType", Class.class, 16).get(model);
                require(world.getClass() == worldType && cache.getClass() == cacheType, "exact genuine world/cache classes");
                Class<?> metadataType = originalType(client ? "ei" : "ct", loader);
                Object metadata = field(worldType, "x", metadataType, 4).get(world);
                List<Object> spawn = new ArrayList<>();
                for (String getter : new String[] {"c", "d", "e"}) {
                    spawn.add(invoke(method(metadataType, getter, int.class, 1), metadata));
                }
                Map<?, ?> constructorCache = (Map<?, ?>) invoke(get, model, "cache.e", cache);
                List<Object> cacheEntries = new ArrayList<>();
                for (Map.Entry<?, ?> entry : constructorCache.entrySet()) {
                    Object cached = entry.getValue();
                    require(entry.getKey() instanceof Integer && cached != null, "actual constructor cache entry");
                    cacheEntries.add(object("key", entry.getKey(), "class", cached.getClass().getName(),
                            "coordinates", Arrays.asList(invoke(get, model, "chunk.j", cached), invoke(get, model, "chunk.k", cached))));
                }
                List<?> insertion = (List<?>) invoke(get, model, "cache.f", cache);
                Set<?> pending = (Set<?>) invoke(get, model, "cache.a", cache);
                List<?> queue = (List<?>) invoke(get, model, "world.C", world);
                Map<String, Object> setupMetadata = object("spawn", spawn, "cache_entries", cacheEntries,
                        "map_count", constructorCache.size(), "insertion_count", insertion.size(),
                        "pending_count", pending.size(), "queue_count", queue.size(),
                        "expected_spawn_random_walk_iterations", client ? null : 0);
                if (!client) {
                    Object originChunk = constructorCache.get(0);
                    require(spawn.equals(Arrays.asList(0, 64, 0)) && constructorCache.size() == 1
                            && originChunk != null && originChunk.getClass() == chunkType
                            && (int) invoke(get, model, "chunk.j", originChunk) == 0
                            && (int) invoke(get, model, "chunk.k", originChunk) == 0
                            && insertion.size() == 1 && insertion.get(0) == originChunk
                            && pending.isEmpty() && queue.isEmpty(), "fixed original server setup context changed");
                } else if (constructorCache.isEmpty()) {
                    Method load = method(cacheType, "c", chunkType, 1, int.class, int.class);
                    try { invoke(load, cache, 0, 0); }
                    catch (LinkageError failure) {
                        throw new OriginalDependencyUnavailable("original client setup cache generation", failure);
                    }
                }
                Map<?, ?> setupWorld = (Map<?, ?>) invoke(worldSnapshot, model, chunkX, chunkZ, MAX_REGIONS);
                measuring = true;
                field(shared, "measuring", boolean.class, 10).setBoolean(null, true);
                byte[] blocks = initial.clone();
                Constructor<?> chunkConstructor = (Constructor<?>) field(modelType, "chunkConstructor", Constructor.class, 16).get(model);
                Object chunk;
                try { chunk = chunkConstructor.newInstance(world, blocks, chunkX, chunkZ); }
                catch (InvocationTargetException failure) { throw unwrap(failure); }
                require(chunk.getClass() == chunkType, "exact measured original chunk class");
                // These fields belong to the caller-owned observation model. No
                // original game field, queue, provider, or cache is assigned.
                field(modelType, "chunk", Object.class, 0).set(model, chunk);
                field(modelType, "blocks", byte[].class, 0).set(model, blocks);
                field(modelType, "localIdentities", Object[].class, 0).set(model, invoke(identities, model, chunk));
                Method finish = (Method) field(modelType, "finishMethod", Method.class, 16).get(model);
                Map<?, ?> before = (Map<?, ?>) invoke(snapshot, model, chunkX, chunkZ, MAX_REGIONS);
                begin(world, chunk, 0);
                invoke(finish, chunk);
                end(0);
                Map<?, ?> after = (Map<?, ?>) invoke(snapshot, model, chunkX, chunkZ, MAX_REGIONS);
                boolean initialRetained = Arrays.equals(blocks, initial);
                // Only the caller-owned block input changes between full passes.
                System.arraycopy(repeated, 0, blocks, 0, BLOCK_BYTES);
                Map<?, ?> repeatBefore = (Map<?, ?>) invoke(snapshot, model, chunkX, chunkZ, MAX_REGIONS);
                begin(world, chunk, 1);
                invoke(finish, chunk);
                end(1);
                Map<?, ?> repeatAfter = (Map<?, ?>) invoke(snapshot, model, chunkX, chunkZ, MAX_REGIONS);
                int[] opacity = (int[]) field(modelType, "opacity", int[].class, 16).get(model);
                int[] initialOpacity = (int[]) field(modelType, "initialOpacity", int[].class, 16).get(model);
                Field opacityField = (Field) field(modelType, "opacityField", Field.class, 16).get(model);
                Map<String, Object> record = object("schema", 2, "side", args[1], "seed_hex", "0000000000000017",
                        "coordinates", Arrays.asList(chunkX, chunkZ), "jar_sha256", hash(Files.readAllBytes(jarPath)),
                        "input_sha256", hash(initial), "repeat_input_sha256", hash(repeated),
                        "class_origins", origins, "platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader(),
                        "bindings", object("world", worldType.getName(), "chunk", chunkType.getName(), "finish", finish.getName()),
                        "setup_metadata", setupMetadata, "setup_world", setupWorld,
                        "before", before, "after", after, "repeat_before", repeatBefore, "repeat_after", repeatAfter,
                        "controls", object("initial_blocks_unchanged", initialRetained,
                                "repeat_blocks_unchanged", Arrays.equals(blocks, repeated),
                                "opacity_unchanged", opacityField.get(null) == opacity && Arrays.equals(opacity, initialOpacity)));
                byte[] encoded = (((String) invoke(json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_RECORD_BYTES, "complete snapshot output budget");
                Files.write(output, encoded, StandardOpenOption.CREATE_NEW);
                status = 0;
            }
        } catch (Throwable failure) {
            boolean unavailable = !measuring && (failure instanceof OriginalDependencyUnavailable
                    || unavailableType != null && unavailableType.isInstance(failure));
            System.err.println(unavailable ? "UNAVAILABLE: genuine original setup" : "FAILURE: height observation");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally { System.setOut(originalOut); }
        System.exit(status);
    }
}
