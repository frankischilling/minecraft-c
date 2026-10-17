import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Observes actual constructors and full finishing calls in genuine fresh worlds.
 * Local arrays are compared separately from complete, side-specific world effects.
 * No provider replacement, cache insertion, queue drain, save, or original field
 * assignment occurs. Between passes only the caller-owned block bytes change.
 */
public final class ChunkObservation {
    private static final int BLOCK_BYTES = 32768;
    private static final int MAX_RECORD_BYTES = 32 * 1024 * 1024;
    private static final int MAX_SETUP_REGIONS = 16384;
    private static final int MAX_REGIONS_PER_PASS = 1024;
    private static final String OPACITY_SHA =
            "f15de154e2937ae549a09e588e04ae8b4e5c21a99b3649cbf45aa2e067fe63ce";
    private static boolean measuring;

    private static final class SetupUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupUnavailable(String message, Throwable cause) { super(message, cause); }
    }

    private static void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }

    private static String hex(byte[] value) {
        char[] result = new char[value.length * 2];
        char[] alphabet = "0123456789abcdef".toCharArray();
        for (int i = 0; i < value.length; ++i) {
            result[i * 2] = alphabet[(value[i] & 255) >>> 4];
            result[i * 2 + 1] = alphabet[value[i] & 15];
        }
        return new String(result);
    }

    private static String hash(byte[] bytes) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static Map<String, Object> object(Object... values) {
        Map<String, Object> result = new LinkedHashMap<>();
        require(values.length % 2 == 0, "object pairs");
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }

    private static Map<String, Object> bytes(byte[] value) {
        return object("length", value.length, "values", hex(value));
    }

    private static Map<String, Object> fingerprint(byte[] value) throws Exception {
        return value == null ? object("length", null, "sha256", null)
                : object("length", value.length, "sha256", hash(value));
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof String) {
            StringBuilder text = new StringBuilder("\"");
            for (char c : ((String) value).toCharArray()) {
                if (c == '"' || c == '\\') text.append('\\').append(c);
                else if (c < 32 || c > 126) text.append(String.format(Locale.ROOT, "\\u%04x", (int) c));
                else text.append(c);
            }
            return text.append('"').toString();
        }
        if (value instanceof Boolean || value instanceof Integer || value instanceof Long) return value.toString();
        if (value instanceof Map<?, ?>) {
            StringBuilder text = new StringBuilder("{");
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                if (text.length() > 1) text.append(',');
                text.append(json(entry.getKey())).append(':').append(json(entry.getValue()));
            }
            return text.append('}').toString();
        }
        if (value instanceof List<?>) {
            StringBuilder text = new StringBuilder("[");
            for (Object entry : (List<?>) value) {
                if (text.length() > 1) text.append(',');
                text.append(json(entry));
            }
            return text.append(']').toString();
        }
        throw new IllegalStateException("unsupported JSON value type");
    }

    private static Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
        try { return method.invoke(receiver, arguments); }
        catch (InvocationTargetException error) {
            Throwable cause = error.getCause();
            if (cause instanceof Exception) throw (Exception) cause;
            if (cause instanceof Error) throw (Error) cause;
            throw error;
        }
    }

    private static final class Model {
        final boolean client;
        final ClassLoader loader;
        final Class<?> worldType, providerType, storageType, normalType, chunkType;
        final Class<?> nibbleType, cacheType, kindType, regionType;
        final Map<String, Field> fields = new LinkedHashMap<>();
        final IdentityHashMap<Object, String> references = new IdentityHashMap<>();
        final Method seedMethod, finishMethod, emptyMethod, presenceMethod, keyMethod;
        final Constructor<?> chunkConstructor;
        final Field opacityField;
        final int[] opacity;
        final int[] initialOpacity;
        final Object skyKind, blockKind;
        Object world, provider, storage, chunk, cache;
        byte[] blocks;
        Object[] localIdentities;

        Class<?> type(String name) throws Exception { return Class.forName(name, true, loader); }

        Field bind(String role, Class<?> owner, String name, Class<?> expected, int modifiers) throws Exception {
            Field field = owner.getDeclaredField(name);
            require(field.getType() == expected && field.getModifiers() == modifiers,
                    "field descriptor/modifiers changed: " + owner.getName() + "." + name);
            field.setAccessible(true);
            fields.put(role, field);
            return field;
        }

        Object get(String role, Object receiver) throws Exception { return fields.get(role).get(receiver); }

        Method method(Class<?> owner, String name, Class<?> result, int modifiers, Class<?>... arguments)
                throws Exception {
            Method method = owner.getDeclaredMethod(name, arguments);
            require(method.getReturnType() == result && method.getModifiers() == modifiers,
                    "method descriptor/modifiers changed: " + owner.getName() + "." + name);
            method.setAccessible(true);
            return method;
        }

        Model(ClassLoader loader, boolean client) throws Exception {
            this.loader = loader;
            this.client = client;
            worldType = type(client ? "fd" : "dj");
            providerType = type(client ? "xa" : "os");
            normalType = type(client ? "rh" : "kp");
            storageType = type(client ? "wt" : "om");
            chunkType = type(client ? "lm" : "hi");
            nibbleType = type(client ? "wi" : "ob");
            cacheType = type(client ? "ok" : "jb");
            kindType = type(client ? "eb" : "co");
            regionType = type(client ? "st" : "lv");
            Class<?> generator = type(client ? "cl" : "bl");
            Class<?> loaderType = type(client ? "bf" : "an");
            Class<?> manager = type(client ? "xv" : "ph");
            bind("chunk.a", chunkType, "a", boolean.class, 9);
            bind("chunk.b", chunkType, "b", byte[].class, 1);
            bind("chunk.d", chunkType, "d", worldType, 1);
            for (String name : new String[] {"e", "f", "g"}) bind("chunk." + name, chunkType, name, nibbleType, 1);
            bind("chunk.h", chunkType, "h", byte[].class, 1);
            bind("chunk.i", chunkType, "i", int.class, 1);
            for (String name : new String[] {"j", "k"}) bind("chunk." + name, chunkType, name, int.class, 17);
            bind("chunk.l", chunkType, "l", Map.class, 1);
            bind("chunk.m", chunkType, "m", List[].class, 1);
            for (String name : new String[] {"c", "n", "o", "p", "q"}) bind("chunk." + name, chunkType, name, boolean.class, 1);
            bind("chunk.r", chunkType, "r", long.class, 1);
            bind("nibble.a", nibbleType, "a", byte[].class, 17);
            bind("world.t", worldType, "t", providerType, 17);
            bind("world.v", worldType, "v", generator, 4);
            bind("world.w", worldType, "w", storageType, 20);
            bind("world.C", worldType, "C", List.class, 2);
            bind("world.A", worldType, "A", int.class, 8);
            bind("world.M", worldType, "M", int.class, 2);
            bind("provider.a", providerType, "a", worldType, 1);
            bind("provider.b", providerType, "b", manager, 1);
            bind("provider.e", providerType, "e", boolean.class, 1);
            bind("cache.a", cacheType, "a", Set.class, 2);
            bind("cache.b", cacheType, "b", chunkType, 2);
            bind("cache.c", cacheType, "c", generator, 2);
            bind("cache.d", cacheType, "d", loaderType, 2);
            bind("cache.e", cacheType, "e", Map.class, 2);
            bind("cache.f", cacheType, "f", List.class, 2);
            bind("cache.g", cacheType, "g", worldType, 2);
            bind("region.a", regionType, "a", kindType, 17);
            for (String name : new String[] {"b", "c", "d", "e", "f", "g"}) bind("region." + name, regionType, name, int.class, 1);
            bind("kind.a", kindType, "a", kindType, 0x4019);
            bind("kind.b", kindType, "b", kindType, 0x4019);
            bind("kind.c", kindType, "c", int.class, 17);
            skyKind = get("kind.a", null);
            blockKind = get("kind.b", null);
            require(skyKind != null && blockKind != null && skyKind != blockKind, "light enum identities");
            seedMethod = method(worldType, client ? "s" : "l", long.class, 1);
            finishMethod = method(chunkType, client ? "c" : "b", void.class, 1);
            emptyMethod = method(chunkType, client ? "h" : "g", boolean.class, 1);
            presenceMethod = method(cacheType, "a", boolean.class, 1, int.class, int.class);
            keyMethod = method(type(client ? "yy" : "qa"), "a", int.class, 9, int.class, int.class);
            chunkConstructor = chunkType.getDeclaredConstructor(worldType, byte[].class, int.class, int.class);
            require(chunkConstructor.getModifiers() == 1, "chunk constructor access");
            opacityField = type(client ? "uu" : "na").getDeclaredField("q");
            require(opacityField.getType() == int[].class &&
                    java.lang.reflect.Modifier.isStatic(opacityField.getModifiers()), "opacity field binding");
            opacityField.setAccessible(true);
            opacity = (int[]) opacityField.get(null);
            require(opacity.length == 256, "opacity table length");
            initialOpacity = opacity.clone();
            require(OPACITY_SHA.equals(hash(opacityBytes())), "actual initialized opacity table hash");
        }

        byte[] opacityBytes() {
            byte[] result = new byte[1024];
            for (int i = 0; i < opacity.length; ++i) {
                int value = opacity[i];
                require(value >= 0, "negative actual opacity");
                for (int byteIndex = 0; byteIndex < 4; ++byteIndex)
                    result[i * 4 + byteIndex] = (byte) (value >>> (24 - byteIndex * 8));
            }
            return result;
        }

        String ref(Object value) {
            if (value == null) return null;
            String result = references.get(value);
            if (result == null) {
                result = "r" + references.size();
                references.put(value, result);
            }
            return result;
        }

        Map<String, Object> reference(Object value) {
            return object("ref", ref(value), "class", value == null ? null : value.getClass().getName());
        }

        void setup(Path root, long seed) throws Exception {
            require(!Files.exists(root) && !Files.isSymbolicLink(root) && Files.isDirectory(root.getParent()),
                    "setup root must be new beneath an existing owned parent");
            Files.createDirectory(root);
            try {
                provider = normalType.getDeclaredConstructor().newInstance();
                if (client) {
                    storage = type("wy").getDeclaredConstructor().newInstance();
                    world = worldType.getDeclaredConstructor(storageType, String.class, providerType, long.class)
                            .newInstance(storage, "chunk-reference", provider, seed);
                } else {
                    storage = type("dq").getDeclaredConstructor(File.class, String.class, boolean.class)
                            .newInstance(root.toFile(), "world", false);
                    world = worldType.getDeclaredConstructor(storageType, String.class, long.class, providerType)
                            .newInstance(storage, "chunk-reference", seed, provider);
                }
            } catch (InvocationTargetException | LinkageError failure) {
                throw new SetupUnavailable("genuine original constructor failed", failure);
            }
            require((long) invoke(seedMethod, world) == seed, "actual selected world seed");
            require(get("world.t", world) == provider && get("world.w", world) == storage,
                    "world ownership relationships");
            cache = get("world.v", world);
            require(cache != null && cache.getClass() == cacheType, "actual normal chunk cache");
            require(get("provider.a", provider) == world && get("cache.g", cache) == world,
                    "attached world relationships");
            require(!(boolean) get("provider.e", provider), "ordinary normal provider has sky");
            ref(world); ref(provider); ref(storage); ref(cache);
        }

        byte[] packed(Object target, String name) throws Exception {
            Object wrapper = get("chunk." + name, target);
            return wrapper == null ? null : (byte[]) get("nibble.a", wrapper);
        }

        Object[] identities(Object target) throws Exception {
            List<Object> result = new ArrayList<>();
            result.add(get("chunk.d", target));
            result.add(get("chunk.b", target));
            for (String name : new String[] {"e", "f", "g"}) {
                Object wrapper = get("chunk." + name, target);
                result.add(wrapper);
                result.add(get("nibble.a", wrapper));
            }
            result.add(get("chunk.h", target));
            result.add(get("chunk.l", target));
            Object[] entities = (Object[]) get("chunk.m", target);
            result.add(entities);
            result.addAll(Arrays.asList(entities));
            return result.toArray();
        }

        Map<String, Object> identityControls() throws Exception {
            Object[] current = identities(chunk);
            List<Object> retained = new ArrayList<>();
            for (int i = 0; i < current.length; ++i) retained.add(current[i] == localIdentities[i]);
            IdentityHashMap<Object, Boolean> unique = new IdentityHashMap<>();
            boolean distinct = true;
            for (Object identity : current) distinct &= identity != null && unique.put(identity, Boolean.TRUE) == null;
            return object("roles_retained", retained, "roles_pairwise_distinct", distinct,
                    "input_retained", get("chunk.b", chunk) == blocks,
                    "world_retained", get("chunk.d", chunk) == world);
        }

        Map<String, Object> flags(Object target) throws Exception {
            Map<String, Object> result = new LinkedHashMap<>();
            for (String name : new String[] {"c", "n", "o", "p", "q"}) result.put(name, get("chunk." + name, target));
            return result;
        }

        Map<String, Object> local() throws Exception {
            byte[] localBlocks = (byte[]) get("chunk.b", chunk);
            byte[] metadata = packed(chunk, "e"), sky = packed(chunk, "f"), blockLight = packed(chunk, "g");
            byte[] heights = (byte[]) get("chunk.h", chunk);
            require(localBlocks.length == 32768 && metadata.length == 16384 && sky.length == 16384 &&
                    blockLight.length == 16384 && heights.length == 256, "normal local array lengths");
            Map<?, ?> map = (Map<?, ?>) get("chunk.l", chunk);
            Object[] entities = (Object[]) get("chunk.m", chunk);
            require(entities.length == 8, "entity slice cardinality");
            List<Object> slices = new ArrayList<>();
            for (Object entry : entities) {
                require(entry instanceof List<?>, "actual entity slice list");
                slices.add(object("class", entry.getClass().getName(), "size", ((List<?>) entry).size()));
            }
            return object("data", object("blocks", bytes(localBlocks), "metadata", bytes(metadata),
                            "skylight", bytes(sky), "block_light", bytes(blockLight), "heightmap", bytes(heights),
                            "minimum_height", get("chunk.i", chunk)),
                    "coordinates", Arrays.asList(get("chunk.j", chunk), get("chunk.k", chunk)),
                    "flags", flags(chunk), "time", String.format(Locale.ROOT, "%016x", (long) get("chunk.r", chunk)),
                    "map", object("class", map.getClass().getName(), "size", map.size()),
                    "entity_slices", slices, "identities", identityControls());
        }

        Map<String, Object> cachedChunk(Object value, Object fallback) throws Exception {
            require(value != null && chunkType.isInstance(value), "actual cached chunk type");
            List<Object> arrayReferences = new ArrayList<>();
            arrayReferences.add(reference(get("chunk.b", value)));
            for (String name : new String[] {"e", "f", "g"}) {
                Object wrapper = get("chunk." + name, value);
                arrayReferences.add(reference(wrapper));
                arrayReferences.add(reference(wrapper == null ? null : get("nibble.a", wrapper)));
            }
            arrayReferences.add(reference(get("chunk.h", value)));
            return object("object", reference(value), "coordinates", Arrays.asList(get("chunk.j", value), get("chunk.k", value)),
                    "heightmap", bytes((byte[]) get("chunk.h", value)), "minimum_height", get("chunk.i", value),
                    "flags", flags(value), "time", String.format(Locale.ROOT, "%016x", (long) get("chunk.r", value)),
                    "measured", value == chunk, "fallback", value == fallback,
                    "empty", invoke(emptyMethod, value), "array_references", arrayReferences,
                    "blocks", fingerprint((byte[]) get("chunk.b", value)),
                    "metadata", fingerprint(packed(value, "e")), "skylight", fingerprint(packed(value, "f")),
                    "block_light", fingerprint(packed(value, "g")));
        }

        Map<String, Object> worldSnapshot(int x, int z, int queueLimit) throws Exception {
            require(get("world.v", world) == cache, "retained cache identity");
            Map<?, ?> map = (Map<?, ?>) get("cache.e", cache);
            List<?> list = (List<?>) get("cache.f", cache);
            Set<?> pending = (Set<?>) get("cache.a", cache);
            List<?> queue = (List<?>) get("world.C", world);
            if (map.size() > 512 || list.size() > 512 || pending.size() > 512 || queue.size() > queueLimit) {
                String counts = ": map=" + map.size() + "/512, insertion_list=" + list.size()
                        + "/512, pending_set=" + pending.size() + "/512, queue=" + queue.size()
                        + "/" + queueLimit;
                if (!measuring) throw new SetupUnavailable("genuine setup exceeds complete observation budget" + counts, null);
                throw new IllegalStateException("complete world collection observation budget exceeded" + counts);
            }
            require(map.getClass() == java.util.HashMap.class && list.getClass() == ArrayList.class &&
                    pending.getClass() == java.util.HashSet.class && queue.getClass() == ArrayList.class,
                    "actual world collection implementations");
            Object fallback = get("cache.b", cache);
            IdentityHashMap<Object, Boolean> seen = new IdentityHashMap<>();
            List<Object> chunks = new ArrayList<>(), entries = new ArrayList<>(), inserted = new ArrayList<>();
            List<Object> pendingEntries = new ArrayList<>(), regions = new ArrayList<>(), presence = new ArrayList<>();
            List<Object> values = new ArrayList<>();
            values.add(fallback);
            int ordinal = 0;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                require(entry.getKey() instanceof Integer, "actual signed Integer cache key");
                entries.add(object("ordinal", ordinal++, "key", entry.getKey(), "value", ref(entry.getValue())));
                if (entry.getValue() != null) values.add(entry.getValue());
            }
            ordinal = 0;
            for (Object value : list) {
                inserted.add(object("ordinal", ordinal++, "value", ref(value)));
                values.add(value);
            }
            ordinal = 0;
            for (Object value : pending) {
                require(value instanceof Integer, "actual pending signed Integer key");
                pendingEntries.add(object("ordinal", ordinal++, "key", value));
            }
            for (Object value : values) {
                if (seen.put(value, Boolean.TRUE) == null) chunks.add(cachedChunk(value, fallback));
            }
            require(chunks.size() <= 1025, "deduplicated cached chunk budget");
            int[][] offsets = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (int[] offset : offsets) {
                int cx = x + offset[0], cz = z + offset[1];
                int key = (int) invoke(keyMethod, null, cx, cz);
                presence.add(object("coordinates", Arrays.asList(cx, cz), "key", key,
                        "present", invoke(presenceMethod, cache, cx, cz), "contains", map.containsKey(key),
                        "value", ref(map.get(key))));
            }
            ordinal = 0;
            for (Object region : queue) {
                require(region != null && region.getClass() == regionType, "actual queued region type");
                Object kind = get("region.a", region);
                require(kind == skyKind || kind == blockKind, "actual queued light kind identity");
                Enum<?> enumKind = (Enum<?>) kind;
                List<Object> bounds = new ArrayList<>();
                for (String name : new String[] {"b", "c", "d", "e", "f", "g"}) bounds.add(get("region." + name, region));
                regions.add(object("ordinal", ordinal++, "object", reference(region),
                        "kind", object("identity", kind == skyKind ? "a" : "b", "name", enumKind.name(),
                                "ordinal", enumKind.ordinal(), "default", get("kind.c", kind)), "bounds", bounds));
            }
            return object("world", reference(world), "provider", reference(get("world.t", world)),
                    "storage", reference(get("world.w", world)), "cache", reference(cache),
                    "provider_world", reference(get("provider.a", provider)),
                    "manager", reference(get("provider.b", provider)), "no_sky", get("provider.e", provider),
                    "cache_world", reference(get("cache.g", cache)), "generator", reference(get("cache.c", cache)),
                    "loader", reference(get("cache.d", cache)),
                    "map", object("object", reference(map), "entries", entries),
                    "insertion_list", object("object", reference(list), "entries", inserted),
                    "pending_set", object("object", reference(pending), "entries", pendingEntries),
                    "fallback", ref(fallback), "chunks", chunks, "presence", presence,
                    "queue", object("object", reference(queue), "entries", regions),
                    "enqueue_depth", get("world.A", null), "drain_depth", get("world.M", world),
                    "chunk_static_a", get("chunk.a", null));
        }

        Map<String, Object> snapshot(int x, int z, int queueLimit) throws Exception {
            return object("local", local(), "world", worldSnapshot(x, z, queueLimit),
                    "opacity", bytes(opacityBytes()), "opacity_identity_retained", opacityField.get(null) == opacity);
        }

        Map<String, Object> observe(long seed, int x, int z, byte[] input, byte[] repeated, Path root)
                throws Exception {
            System.err.println("SETUP_BEGIN");
            setup(root, seed);
            // Capture complete real setup context before the measured constructor.
            Map<String, Object> setupWorld = worldSnapshot(x, z, MAX_SETUP_REGIONS);
            int setupQueueSize = ((List<?>) get("world.C", world)).size();
            System.err.println("SETUP_DONE");
            measuring = true;
            System.err.println("MEASUREMENT_BEGIN");
            blocks = input.clone();
            chunk = chunkConstructor.newInstance(world, blocks, x, z);
            require(chunk.getClass() == chunkType, "actual measured chunk class");
            localIdentities = identities(chunk);
            Map<String, Object> before = snapshot(x, z, setupQueueSize);
            invoke(finishMethod, chunk);
            Map<String, Object> after = snapshot(x, z, setupQueueSize + MAX_REGIONS_PER_PASS);
            int afterQueueSize = ((List<?>) get("world.C", world)).size();
            boolean initialRetained = Arrays.equals(blocks, input);
            System.arraycopy(repeated, 0, blocks, 0, BLOCK_BYTES);
            Map<String, Object> repeatBefore = snapshot(x, z, afterQueueSize);
            invoke(finishMethod, chunk);
            Map<String, Object> repeatAfter = snapshot(x, z, afterQueueSize + MAX_REGIONS_PER_PASS);
            boolean repeatRetained = Arrays.equals(blocks, repeated);
            boolean opacityRetained = opacityField.get(null) == opacity && Arrays.equals(opacity, initialOpacity);
            boolean staticUnchanged = true;
            for (Map<String, Object> stage : Arrays.asList(before, after, repeatBefore, repeatAfter)) {
                Map<?, ?> context = (Map<?, ?>) stage.get("world");
                staticUnchanged &= setupWorld.get("chunk_static_a").equals(context.get("chunk_static_a"));
            }
            System.err.println("MEASUREMENT_DONE");
            return object("schema", 2, "side", client ? "client" : "server",
                    "seed", String.format(Locale.ROOT, "%016x", seed), "coordinates", Arrays.asList(x, z),
                    "bindings", object("world", worldType.getName(), "chunk", chunkType.getName(),
                            "finish", finishMethod.getName(), "nibble", nibbleType.getName(),
                            "provider", normalType.getName(), "cache", cacheType.getName()),
                    "input_sha256", hash(input), "repeat_input_sha256", hash(repeated),
                    "setup_world", setupWorld, "before", before, "after", after,
                    "repeat_before", repeatBefore, "repeat_after", repeatAfter,
                    "controls", object("initial_blocks_unchanged", initialRetained,
                            "repeat_blocks_unchanged", repeatRetained, "opacity_unchanged", opacityRetained,
                            "chunk_static_a_unchanged", staticUnchanged));
        }
    }

    private static byte[] input(Path path) throws Exception {
        require(Files.isRegularFile(path) && !Files.isSymbolicLink(path) && Files.size(path) == BLOCK_BYTES,
                "input must be exactly 32768 regular bytes");
        byte[] result = Files.readAllBytes(path);
        require(result.length == BLOCK_BYTES, "input byte count changed");
        return result;
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out;
        System.setOut(System.err);
        int status = 1;
        try {
            require(args.length == 8, "arguments: jar side seed_hex x z initial_file repeat_file new_world_root");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            require(args[2].matches("[0-9a-f]{16}"), "seed hex");
            long seed = Long.parseUnsignedLong(args[2], 16);
            int x = Integer.parseInt(args[3]), z = Integer.parseInt(args[4]);
            require(x >= -2048 && x <= 2048 && z >= -2048 && z <= 2048, "reviewed chunk coordinate bounds");
            Path jar = Path.of(args[0]).toRealPath();
            byte[] initial = input(Path.of(args[5])), repeated = input(Path.of(args[6]));
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                    ClassLoader.getPlatformClassLoader())) {
                Model model;
                try { model = new Model(loader, args[1].equals("client")); }
                catch (LinkageError failure) {
                    System.err.println("UNAVAILABLE: original class initialization");
                    failure.printStackTrace(System.err);
                    System.exit(77);
                    return;
                }
                Map<String, Object> record;
                try {
                    record = model.observe(seed, x, z, initial, repeated, Path.of(args[7]).toAbsolutePath().normalize());
                } catch (Throwable failure) {
                    if (!measuring && failure instanceof SetupUnavailable) {
                        System.err.println("UNAVAILABLE: genuine original world setup");
                        failure.printStackTrace(System.err);
                        System.exit(77);
                        return;
                    }
                    throw failure;
                }
                byte[] encoded = (json(record) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_RECORD_BYTES, "complete record output budget");
                protocol.write(encoded, 0, encoded.length);
                protocol.flush();
                require(!protocol.checkError(), "protocol output failed");
                status = 0;
            }
        } catch (Throwable failure) {
            System.err.println("FAILURE: chunk observation");
            failure.printStackTrace(System.err);
        } finally { System.setOut(protocol); }
        System.exit(status);
    }
}
