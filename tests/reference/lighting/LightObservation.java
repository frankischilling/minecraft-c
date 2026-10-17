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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Observes original light-region merges and an intact genuine world queue. */
public final class LightObservation {
    private static final int MAX_REGIONS = 32768;
    private static final int ORIGINAL_QUEUE_LIMIT = 1000000;
    private static final int MAX_RECORD_BYTES = 32 * 1024 * 1024;
    private static final long SEED = 0x4c69676874517565L;

    private static final class SetupDependencyUnavailable extends Exception {
        private static final long serialVersionUID = 1L;
        SetupDependencyUnavailable(String message, LinkageError cause) { super(message, cause); }
    }

    /* Each case starts with constructor bounds, followed by merge requests.
     * These are inputs only. Every result comes from the supplied original JAR.
     */
    private static final int[][][] MERGE_CASES = {
        {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0}},
        {{0, 0, 0, 4, 4, 4}, {1, 1, 1, 3, 3, 3}, {-1, 0, 0, 4, 4, 4}, {0, 0, 0, 4, 4, 4}},
        // Growth of exactly two and exactly three in the original exclusive volume.
        {{0, 0, 0, 1, 1, 2}, {0, 0, 0, 2, 1, 2}, {0, 0, 0, 2, 1, 2}},
        {{0, 0, 0, 1, 1, 3}, {0, 0, 0, 2, 1, 3}, {0, 0, 0, 1, 1, 3}},
        {{-2, -2, -2, -1, -1, 0}, {-3, -2, -2, -1, -1, 0}},
        {{-2, -2, -2, -1, -1, 1}, {-3, -2, -2, -1, -1, 1}},
        {{0, 0, 0, 1, 1, 2}, {0, -1, 0, 1, 1, 2}},
        {{0, 0, 0, 1, 1, 3}, {0, -1, 0, 1, 1, 3}},
        {{0, 0, 0, 1, 2, 1}, {0, 0, -1, 1, 2, 1}},
        {{0, 0, 0, 1, 3, 1}, {0, 0, -1, 1, 3, 1}},
        {{0, 0, 0, 2, 1, 1}, {0, 0, 0, 2, 2, 1}},
        {{0, 0, 0, 3, 1, 1}, {0, 0, 0, 3, 2, 1}},
        {{0, 0, 0, 2, 1, 1}, {0, 0, 0, 2, 1, 2}},
        {{0, 0, 0, 3, 1, 1}, {0, 0, 0, 3, 1, 2}},
        // Degenerate slabs, lines, and points retain the original volume quirks.
        {{0, 0, 0, 0, 2, 1}, {0, 0, 0, 1, 2, 1}},
        {{0, 0, 0, 0, 3, 1}, {0, 0, 0, 1, 3, 1}},
        {{0, 0, 0, 5, 0, 5}, {-1, 0, 0, 5, 0, 5}, {-1, 0, 0, 6, 0, 5}},
        {{0, 0, 0, 5, 5, 0}, {0, 0, -1, 5, 5, 0}},
        {{0, 0, 0, 5, 0, 0}, {-1, -1, 0, 6, 0, 0}, {-1, -1, -1, 6, 0, 0}},
        {{0, 0, 0, 0, 5, 0}, {0, -1, -1, 0, 6, 1}},
        {{0, 0, 0, 0, 0, 5}, {-1, 0, -1, 1, 0, 6}},
        {{7, 7, 7, 7, 7, 7}, {6, 6, 6, 8, 8, 8}, {7, 7, 7, 7, 7, 7}},
        {{0, 0, 0, 1, 1, 1}, {-1, 0, 0, 1, 1, 1}, {-2, 0, 0, 1, 1, 1}, {-2, 0, 0, 1, 1, 1}},
        {{0, 0, 0, 1, 1, 1}, {0, 0, 0, 2, 1, 1}, {0, 0, 0, 3, 1, 1}, {0, 0, 0, 4, 1, 1}},
        // Requests beyond one coordinate of padding reject without mutation.
        {{0, 0, 0, 2, 2, 2}, {-2, 0, 0, 2, 2, 2}, {0, -2, 0, 2, 2, 2}, {0, 0, -2, 2, 2, 2}},
        {{0, 0, 0, 2, 2, 2}, {0, 0, 0, 4, 2, 2}, {0, 0, 0, 2, 4, 2}, {0, 0, 0, 2, 2, 4}},
        {{-9, -8, -7, -5, -4, -3}, {-8, -7, -6, -6, -5, -4}, {-10, -8, -7, -5, -4, -3}},
        {{-1, -1, -1, 1, 1, 1}, {-1, -1, -1, 1, 1, 1}, {-2, -1, -1, 1, 1, 1}},
        // Reversed constructor and request bounds are observed, never normalized.
        {{3, 3, 3, 1, 1, 1}, {3, 3, 3, 1, 1, 1}, {2, 2, 2, 2, 2, 2}},
        {{0, 0, 0, 2, 2, 2}, {2, 2, 2, 0, 0, 0}, {3, 3, 3, -1, -1, -1}},
        {{2, 0, 0, 0, 2, 2}, {1, 0, 0, 1, 2, 2}},
        {{0, 2, 0, 2, 0, 2}, {0, 1, 0, 2, 1, 2}},
        {{0, 0, 2, 2, 2, 0}, {0, 0, 1, 2, 2, 1}},
        {{4, 4, 4, -4, -4, -4}, {3, 3, 3, -3, -3, -3}, {4, 4, 4, -4, -4, -4}},
        // Integer edges exercise wrapped padding, extent subtraction, and volume.
        {{Integer.MIN_VALUE, 0, 0, Integer.MIN_VALUE, 0, 0},
         {Integer.MIN_VALUE, 0, 0, Integer.MIN_VALUE, 0, 0},
         {Integer.MIN_VALUE, 0, 0, Integer.MIN_VALUE + 1, 0, 0}},
        {{Integer.MAX_VALUE, 0, 0, Integer.MAX_VALUE, 0, 0},
         {Integer.MAX_VALUE, 0, 0, Integer.MAX_VALUE, 0, 0},
         {Integer.MAX_VALUE - 1, 0, 0, Integer.MAX_VALUE, 0, 0}},
        {{0, Integer.MIN_VALUE, 0, 0, Integer.MIN_VALUE, 0},
         {0, Integer.MIN_VALUE, 0, 0, Integer.MIN_VALUE + 1, 0}},
        {{0, 0, Integer.MAX_VALUE, 0, 0, Integer.MAX_VALUE},
         {0, 0, Integer.MAX_VALUE - 1, 0, 0, Integer.MAX_VALUE}},
        {{Integer.MIN_VALUE, -1, -1, Integer.MAX_VALUE, 1, 1}, {-1, -1, -1, 1, 1, 1}},
        {{Integer.MIN_VALUE + 1, 0, 0, Integer.MAX_VALUE - 1, 2, 2},
         {Integer.MIN_VALUE, 0, 0, Integer.MAX_VALUE - 1, 2, 2}},
        {{-1, Integer.MIN_VALUE + 1, 0, 1, Integer.MAX_VALUE - 1, 2},
         {-1, Integer.MIN_VALUE, 0, 1, Integer.MAX_VALUE - 1, 2}},
        {{0, 0, Integer.MIN_VALUE + 1, 2, 2, Integer.MAX_VALUE - 1},
         {0, 0, Integer.MIN_VALUE, 2, 2, Integer.MAX_VALUE - 1}},
        {{0, 0, 0, 65536, 65536, 1}, {-1, 0, 0, 65536, 65536, 1}},
        {{0, 0, 0, 65536, 65536, 65536}, {0, 0, 0, 65537, 65536, 65536}},
        {{-32768, -32768, -1, 32768, 32768, 1}, {-32769, -32768, -1, 32768, 32768, 1}},
        {{0, 0, 0, 1073741824, 4, 1}, {0, 0, -1, 1073741824, 4, 1}},
        {{Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE,
          Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE},
         {Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE,
          Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE}},
        {{Integer.MIN_VALUE + 1, Integer.MIN_VALUE + 1, Integer.MIN_VALUE + 1,
          Integer.MIN_VALUE + 2, Integer.MIN_VALUE + 2, Integer.MIN_VALUE + 3},
         {Integer.MIN_VALUE, Integer.MIN_VALUE + 1, Integer.MIN_VALUE + 1,
          Integer.MIN_VALUE + 2, Integer.MIN_VALUE + 2, Integer.MIN_VALUE + 3}}
    };

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
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

    private static Exception rethrow(InvocationTargetException error) throws Exception {
        Throwable cause = error.getCause();
        if (cause instanceof Exception) return (Exception) cause;
        if (cause instanceof Error) throw (Error) cause;
        return error;
    }

    private static final class Observer {
        final ClassLoader loader;
        final URL jar;
        final boolean client;
        final Class<?> modelType, unavailableType, worldType, chunkType, cacheType, kindType, regionType;
        final Method sharedInvoke, json, setup, snapshot, get;
        final Method merge, enqueue, available, chunkAt, load, key, empty;
        final Constructor<?> modelConstructor, regionConstructor;
        final Field modelWorld, modelCache, modelProvider, regionKind;
        final Field[] regionBounds = new Field[6];
        Object model, world, cache, provider, sky, block;
        boolean measuring;

        Class<?> original(String name) throws Exception {
            Class<?> type;
            try { type = Class.forName(name, true, loader); }
            catch (LinkageError failure) {
                throw new SetupDependencyUnavailable("original class initialization: " + name, failure);
            }
            require(type.getClassLoader() == loader && type.getProtectionDomain().getCodeSource() != null
                    && jar.equals(type.getProtectionDomain().getCodeSource().getLocation()),
                    "class must originate in supplied JAR: " + name);
            return type;
        }

        Observer(ClassLoader loader, URL jar, boolean client) throws Exception {
            this.loader = loader;
            this.jar = jar;
            this.client = client;
            Class<?> shared = Class.forName("ChunkObservation");
            modelType = Class.forName("ChunkObservation$Model");
            unavailableType = Class.forName("ChunkObservation$SetupUnavailable");
            sharedInvoke = method(shared, "invoke", Object.class, 0x8a, Method.class, Object.class, Object[].class);
            json = method(shared, "json", String.class, 10, Object.class);
            setup = method(modelType, "setup", void.class, 0, Path.class, long.class);
            snapshot = method(modelType, "worldSnapshot", Map.class, 0, int.class, int.class, int.class);
            get = method(modelType, "get", Object.class, 0, String.class, Object.class);
            modelConstructor = modelType.getDeclaredConstructor(ClassLoader.class, boolean.class);
            require(modelConstructor.getModifiers() == 0, "shared model constructor modifiers");
            modelConstructor.setAccessible(true);
            modelWorld = field(modelType, "world", Object.class, 0);
            modelCache = field(modelType, "cache", Object.class, 0);
            modelProvider = field(modelType, "provider", Object.class, 0);
            worldType = original(client ? "fd" : "dj");
            chunkType = original(client ? "lm" : "hi");
            cacheType = original(client ? "ok" : "jb");
            kindType = original(client ? "eb" : "co");
            regionType = original(client ? "st" : "lv");
            for (String name : client ? new String[] {"xa", "rh", "wt", "wi", "cl", "bf", "xv", "uu", "wy", "yy"}
                    : new String[] {"os", "kp", "om", "ob", "bl", "an", "ph", "na", "dq", "qa"}) original(name);
            regionKind = field(regionType, "a", kindType, 17);
            String[] names = {"b", "c", "d", "e", "f", "g"};
            for (int i = 0; i < names.length; ++i) regionBounds[i] = field(regionType, names[i], int.class, 1);
            regionConstructor = regionType.getDeclaredConstructor(kindType, int.class, int.class,
                    int.class, int.class, int.class, int.class);
            require(regionConstructor.getModifiers() == 1, "original region constructor modifiers");
            merge = method(regionType, "a", boolean.class, 1,
                    int.class, int.class, int.class, int.class, int.class, int.class);
            enqueue = method(worldType, "a", void.class, 1, kindType, int.class, int.class,
                    int.class, int.class, int.class, int.class, boolean.class);
            available = method(worldType, client ? "i" : "g", boolean.class, 1, int.class, int.class, int.class);
            chunkAt = method(worldType, "b", chunkType, 1, int.class, int.class);
            load = method(cacheType, "c", chunkType, 1, int.class, int.class);
            key = method(original(client ? "yy" : "qa"), "a", int.class, 9, int.class, int.class);
            empty = method(chunkType, client ? "h" : "g", boolean.class, 1);
        }

        Object invoke(Method method, Object receiver, Object... arguments) throws Exception {
            try { return sharedInvoke.invoke(null, method, receiver, arguments); }
            catch (InvocationTargetException failure) { throw rethrow(failure); }
        }

        Object get(String role, Object receiver) throws Exception { return invoke(get, model, role, receiver); }

        List<Object> region(Object value) throws Exception {
            require(value != null && value.getClass() == regionType, "actual original region class");
            Object kind = regionKind.get(value);
            require(kind == sky || kind == block, "original light kind identity");
            List<Object> result = new ArrayList<>(7);
            result.add(kind == sky ? 0 : 1);
            for (Field field : regionBounds) result.add(field.get(value));
            return result;
        }

        List<Object> queue() throws Exception {
            List<?> source = (List<?>) get("world.C", world);
            require(source.size() <= MAX_REGIONS, "complete queue observation budget exceeded");
            List<Object> result = new ArrayList<>(source.size());
            for (Object value : source) result.add(region(value));
            return result;
        }

        List<Object> merges() throws Exception {
            require(MERGE_CASES.length >= 40, "merge corpus cardinality");
            List<Object> results = new ArrayList<>();
            for (int index = 0; index < MERGE_CASES.length; ++index) {
                int[][] inputs = MERGE_CASES[index];
                for (int[] input : inputs) require(input.length == 6, "merge corpus bounds arity");
                int[] first = inputs[0];
                Object value;
                try {
                    value = regionConstructor.newInstance(index % 2 == 0 ? sky : block,
                            first[0], first[1], first[2], first[3], first[4], first[5]);
                } catch (InvocationTargetException failure) { throw rethrow(failure); }
                List<Object> initial = region(value), requests = new ArrayList<>(), steps = new ArrayList<>();
                for (int i = 1; i < inputs.length; ++i) {
                    int[] bounds = inputs[i];
                    requests.add(bounds(bounds));
                    boolean accepted = (boolean) invoke(merge, value,
                            bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
                    steps.add(object("accepted", accepted, "region", region(value)));
                }
                results.add(object("initial", initial, "requests", requests, "steps", steps));
            }
            return results;
        }

        Map<?, ?> worldSnapshot(int x, int z) throws Exception {
            return (Map<?, ?>) invoke(snapshot, model, x, z, MAX_REGIONS);
        }

        int[] selectedChunk(Map<?, ?> before) {
            for (Object entry : (List<?>) before.get("chunks")) {
                Map<?, ?> chunk = (Map<?, ?>) entry;
                Map<?, ?> identity = (Map<?, ?>) chunk.get("object");
                if (Boolean.FALSE.equals(chunk.get("fallback")) && Boolean.FALSE.equals(chunk.get("empty"))
                        && chunkType.getName().equals(identity.get("class"))) {
                    List<?> coordinates = (List<?>) chunk.get("coordinates");
                    int x = (int) coordinates.get(0), z = (int) coordinates.get(1);
                    require(x >= -1900000 && x <= 1900000 && z >= -1900000 && z <= 1900000,
                            "selected ordinary cached chunk coordinate budget");
                    return new int[] {x, z};
                }
            }
            throw new IllegalStateException("genuine setup has no ordinary nonempty cached chunk");
        }

        boolean admitted(int kind, int[] bounds) throws Exception {
            if (kind == 0 && (boolean) get("provider.e", provider)) return false;
            if ((int) get("world.A", null) + 1 == 50) return false;
            // Java addition intentionally preserves the original midpoint wrap.
            int midX = (bounds[3] + bounds[0]) / 2;
            int midZ = (bounds[5] + bounds[2]) / 2;
            if (!(boolean) invoke(available, world, midX, 64, midZ)) return false;
            Map<?, ?> actualCache = (Map<?, ?>) get("cache.e", cache);
            int cacheKey = (int) invoke(key, null, midX >> 4, midZ >> 4);
            Object cached = actualCache.get(cacheKey);
            require(actualCache.containsKey(cacheKey) && cached != null,
                    "available midpoint must already have an actual cached chunk");
            Object actual = invoke(chunkAt, world, midX, midZ);
            require(actual == cached, "midpoint lookup retained actual cached chunk");
            return !(boolean) invoke(empty, actual);
        }

        Map<String, Object> request(int[] input, int chunkX, int chunkZ) throws Exception {
            require(input.length == 8, "world corpus request arity");
            require(((List<?>) get("world.C", world)).size() < MAX_REGIONS,
                    "world queue has no room for bounded observation");
            int[] bounds = Arrays.copyOfRange(input, 1, 7);
            require((input[0] == 0 || input[0] == 1) && (input[7] == 0 || input[7] == 1)
                    && bounds[0] <= bounds[3] && bounds[1] <= bounds[4] && bounds[2] <= bounds[5]
                    && bounds[1] >= 0 && bounds[4] < 128, "legitimate world request bounds");
            boolean coalesce = input[7] != 0;
            Map<?, ?> eligibilityBefore = worldSnapshot(chunkX, chunkZ);
            boolean admitted = admitted(input[0], bounds);
            require(eligibilityBefore.equals(worldSnapshot(chunkX, chunkZ)),
                    "eligibility checks must preserve complete world/cache/queue observation");
            List<Object> requestRegion = new ArrayList<>(7);
            requestRegion.add(input[0]);
            requestRegion.addAll(bounds(bounds));
            Map<String, Object> result = object("region", requestRegion, "coalesce", coalesce, "admitted", admitted);
            invoke(enqueue, world, input[0] == 0 ? sky : block,
                    bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5], coalesce);
            require(((List<?>) get("world.C", world)).size() <= MAX_REGIONS,
                    "complete queue observation budget exceeded");
            return result;
        }

        Map<String, Object> limit(int x, int z, int chunkX, int chunkZ) throws Exception {
            List<?> actualQueue = (List<?>) get("world.C", world);
            int initialCount = actualQueue.size();
            require(initialCount > 0 && initialCount <= MAX_REGIONS, "bounded nonempty queue before limit observation");
            List<Object> retainedObjects = new ArrayList<>(actualQueue);
            List<Object> retainedRegions = queue();
            int initialDepth = (int) get("world.A", null);
            int[] bounds = {x, 64, z, x, 64, z};
            Map<?, ?> eligibilityBefore = worldSnapshot(chunkX, chunkZ);
            require(admitted(1, bounds), "limit observation requires an admitted actual cached midpoint");
            require(eligibilityBefore.equals(worldSnapshot(chunkX, chunkZ)),
                    "limit eligibility checks must preserve complete world/cache/queue observation");
            Object[] arguments = {block, x, 64, z, x, 64, z, false};
            int appended = ORIGINAL_QUEUE_LIMIT - initialCount;
            require(appended >= 0 && appended + 1 <= 1000001, "limit observation request budget");
            // Only original enqueue calls extend the queue. The preserved baseline
            // remains intact until the original automatic overflow clear occurs.
            for (int count = 0; count < appended; ++count) {
                require(actualQueue.size() == initialCount + count, "original pre-append queue size progression");
                invoke(enqueue, world, arguments);
                require(get("world.C", world) == actualQueue && actualQueue.size() == initialCount + count + 1,
                        "original post-append queue size progression and identity");
                require((int) get("world.A", null) == initialDepth, "original enqueue depth restored after append");
            }
            int atLimitCount = actualQueue.size();
            require(atLimitCount == ORIGINAL_QUEUE_LIMIT, "original queue reaches exact million-entry limit");
            for (int i = 0; i < retainedObjects.size(); ++i) {
                require(actualQueue.get(i) == retainedObjects.get(i)
                        && region(actualQueue.get(i)).equals(retainedRegions.get(i)),
                        "original bounded baseline retained at queue limit");
            }
            List<Object> firstRegion = region(actualQueue.get(0));
            List<Object> lastRegion = region(actualQueue.get(atLimitCount - 1));
            // This request crosses the original limit and invokes its own warning
            // and automatic clear. No observer list mutation or queue drain occurs.
            invoke(enqueue, world, arguments);
            int countAfterOverflow = actualQueue.size();
            boolean queueIdentityRetained = get("world.C", world) == actualQueue;
            boolean enqueueDepthRestored = (int) get("world.A", null) == initialDepth;
            require(countAfterOverflow == 0 && queueIdentityRetained && enqueueDepthRestored,
                    "original overflow clears retained queue and restores enqueue depth");
            return object("initial_count", initialCount, "appended_to_limit", appended,
                    "at_limit_count", atLimitCount, "first_region", firstRegion, "last_region", lastRegion,
                    "count_after_overflow", countAfterOverflow, "queue_identity_retained", queueIdentityRetained,
                    "enqueue_depth_restored", enqueueDepthRestored);
        }

        Map<String, Object> observe(Path root) throws Exception {
            try { model = modelConstructor.newInstance(loader, client); }
            catch (InvocationTargetException failure) { throw rethrow(failure); }
            sky = get("kind.a", null);
            block = get("kind.b", null);
            require(sky != null && block != null && sky != block, "original enum identities");
            // The shared model performs genuine constructors and fresh-root checks.
            invoke(setup, model, root, SEED);
            world = modelWorld.get(model);
            cache = modelCache.get(model);
            provider = modelProvider.get(model);
            Map<?, ?> before = worldSnapshot(0, 0);
            if (((Map<?, ?>) get("cache.e", cache)).isEmpty()) {
                invoke(load, cache, 0, 0);
                before = worldSnapshot(0, 0);
            }
            int[] coordinates = selectedChunk(before);
            before = worldSnapshot(coordinates[0], coordinates[1]);
            int x = coordinates[0] * 16 + 8, z = coordinates[1] * 16 + 8;
            int missingX = 0, missingZ = 0;
            boolean foundMissing = false;
            Map<?, ?> actualCache = (Map<?, ?>) get("cache.e", cache);
            for (int offset = 64; offset <= 1024; ++offset) {
                int cx = coordinates[0] + offset, cz = coordinates[1] + 89;
                int cacheKey = (int) invoke(key, null, cx, cz);
                if (!actualCache.containsKey(cacheKey)) {
                    missingX = cx * 16 + 8;
                    missingZ = cz * 16 + 8;
                    foundMissing = true;
                    break;
                }
            }
            require(foundMissing, "missing midpoint corpus coordinate");
            measuring = true;
            List<Object> mergeResults = merges();
            List<Object> setupRequests = new ArrayList<>();
            // Preserve a nonempty baseline made only by genuine world requests.
            setupRequests.add(request(new int[] {0, x - 5, 40, z - 5, x - 5, 40, z - 5, 0},
                    coordinates[0], coordinates[1]));
            setupRequests.add(request(new int[] {1, x - 5, 50, z - 5, x - 5, 50, z - 5, 0},
                    coordinates[0], coordinates[1]));
            before = worldSnapshot(coordinates[0], coordinates[1]);
            require(!((List<?>) ((Map<?, ?>) before.get("queue")).get("entries")).isEmpty(),
                    "genuine baseline queue must be nonempty");
            // kind, six bounds, coalesce: all world requests have ordered bounds.
            int[][] corpus = {
                {0, x, 64, z, x, 64, z, 0},
                {0, x, 64, z, x, 64, z, 1},
                {0, x + 1, 64, z, x + 1, 64, z, 1},
                {0, x, 65, z, x, 65, z, 1},
                {1, x, 64, z, x, 64, z, 0},
                {1, x, 64, z, x, 64, z, 1},
                {1, x + 1, 64, z, x + 1, 64, z, 1},
                {1, x, 64, z + 1, x, 64, z + 1, 0},
                {0, x - 1, 63, z - 1, x + 1, 65, z + 1, 0},
                {0, x, 64, z, x, 64, z, 1},
                {1, x - 1, 63, z - 1, x + 1, 65, z + 1, 1},
                {1, x - 1, 63, z - 1, x + 1, 65, z + 1, 1},
                {0, x + 3, 62, z + 3, x + 3, 62, z + 3, 0},
                {1, x + 3, 62, z + 3, x + 3, 62, z + 3, 0},
                {0, x + 4, 62, z + 3, x + 4, 62, z + 3, 0},
                {1, x + 4, 62, z + 3, x + 4, 62, z + 3, 0},
                {0, x + 5, 62, z + 3, x + 5, 62, z + 3, 0},
                {1, x + 5, 62, z + 3, x + 5, 62, z + 3, 0},
                {0, x + 3, 62, z + 3, x + 3, 62, z + 3, 1},
                {1, x + 3, 62, z + 3, x + 3, 62, z + 3, 1},
                {0, x - 2, 0, z - 2, x - 2, 0, z - 2, 0},
                {1, x - 2, 127, z - 2, x - 2, 127, z - 2, 1},
                {0, x - 2, 0, z - 2, x - 2, 1, z - 2, 1},
                {1, x - 2, 126, z - 2, x - 2, 127, z - 2, 1},
                {0, missingX, 64, missingZ, missingX, 64, missingZ, 0},
                {1, missingX, 64, missingZ, missingX, 64, missingZ, 1},
                {0, missingX - 1, 63, missingZ - 1, missingX + 1, 65, missingZ + 1, 1},
                {1, missingX - 1, 63, missingZ - 1, missingX + 1, 65, missingZ + 1, 0},
                {0, x - 3, 70, z, x - 1, 71, z + 1, 0},
                {0, x - 3, 70, z, x - 1, 71, z + 1, 1},
                {1, x - 3, 70, z, x - 1, 71, z + 1, 0},
                {1, x - 3, 70, z, x - 1, 71, z + 1, 1},
                // Four foreign kinds leave the matching region fifth from newest.
                {0, x - 4, 74, z + 4, x - 4, 74, z + 4, 0},
                {1, x - 4, 81, z + 4, x - 4, 81, z + 4, 0},
                {1, x - 4, 82, z + 4, x - 4, 82, z + 4, 0},
                {1, x - 4, 83, z + 4, x - 4, 83, z + 4, 0},
                {1, x - 4, 84, z + 4, x - 4, 84, z + 4, 0},
                {0, x - 4, 74, z + 4, x - 4, 74, z + 4, 1},
                // A fifth foreign kind moves that same region outside the scan.
                {1, x - 4, 85, z + 4, x - 4, 85, z + 4, 0},
                {0, x - 4, 74, z + 4, x - 4, 74, z + 4, 1},
                // Both same-kind regions qualify; the newest one must win.
                {0, x - 3, 90, z - 3, x - 3, 90, z - 3, 0},
                {0, x - 2, 90, z - 3, x - 2, 90, z - 3, 0},
                {0, x - 3, 90, z - 3, x - 3, 90, z - 3, 1}
            };
            List<Object> requests = new ArrayList<>(), steps = new ArrayList<>();
            for (int[] input : corpus) {
                requests.add(request(input, coordinates[0], coordinates[1]));
                steps.add(queue());
            }
            // Freeze complete bounded observations before the separate large-queue
            // experiment. No complete snapshot is taken while the queue is large.
            Map<?, ?> after = worldSnapshot(coordinates[0], coordinates[1]);
            Map<String, Object> limitResult = limit(x, z, coordinates[0], coordinates[1]);
            return object("schema", 1, "side", client ? "client" : "server", "merges", mergeResults,
                    "queue", object("before_world", before, "requests", requests,
                            "after_world", after, "steps", steps,
                            "setup_requests", setupRequests, "eligibility_preserved", true), "limit", limitResult);
        }

        String encode(Object value) throws Exception { return (String) invoke(json, null, value); }
    }

    private static List<Object> bounds(int[] values) {
        List<Object> result = new ArrayList<>(values.length);
        for (int value : values) result.add(value);
        return result;
    }

    public static void main(String[] args) {
        PrintStream protocol = System.out;
        System.setOut(System.err);
        int status = 1;
        Observer observer = null;
        try {
            require(args.length == 3, "arguments: jar side new_world_root");
            require(args[1].equals("client") || args[1].equals("server"), "side");
            Path jar = Path.of(args[0]).toRealPath();
            require(Files.isRegularFile(jar), "original JAR must be a regular file");
            URL jarUrl = jar.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {jarUrl}, ClassLoader.getPlatformClassLoader())) {
                require(loader.getParent() == ClassLoader.getPlatformClassLoader(), "isolated original loader parent");
                observer = new Observer(loader, jarUrl, args[1].equals("client"));
                Map<String, Object> record = observer.observe(Path.of(args[2]).toAbsolutePath().normalize());
                byte[] encoded = (observer.encode(record) + "\n").getBytes(StandardCharsets.UTF_8);
                require(encoded.length <= MAX_RECORD_BYTES, "complete record output budget exceeded");
                protocol.write(encoded, 0, encoded.length);
                protocol.flush();
                require(!protocol.checkError(), "protocol output failed");
                status = 0;
            }
        } catch (Throwable failure) {
            boolean unavailable = failure instanceof SetupDependencyUnavailable
                    || observer != null && !observer.measuring
                    && (observer.unavailableType.isInstance(failure) || failure instanceof LinkageError);
            System.err.println(unavailable ? "UNAVAILABLE: genuine original world setup" : "FAILURE: light observation");
            failure.printStackTrace(System.err);
            if (unavailable) status = 77;
        } finally { System.setOut(protocol); }
        System.exit(status);
    }
}
