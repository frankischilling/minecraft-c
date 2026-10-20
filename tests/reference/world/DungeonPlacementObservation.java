import java.io.DataOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.jar.JarFile;

/** One finite prepared original Dungeon placement per fresh
 * copied saved World/process. Compile with unchanged ChunkObservation.java and
 * ChunkStorageObservation.java. No game field/container/Random-state writes. */
public final class DungeonPlacementObservation {
    private static final int X = 8, Y = 64, Z = 8;
    private static final int MAX_SEARCH = 1000000;
    private static final int MAX_COLLECTION = 4096, MAX_CHUNKS = 32;
    private static final int MAX_OUTPUT = 64 * 1024 * 1024;
    private static final String[] STAGES = {"startup", "prepared", "acquired", "before", "call", "after"};

    private static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static Map<String,Object> object(Object... pairs) {
        return ChunkStorageObservation.object(pairs);
    }

    private static String hex(long value, int width) {
        return String.format(java.util.Locale.ROOT, width == 16 ? "%016x" : "%08x",
            width == 16 ? value : value & 0xffffffffL);
    }

    private static String utf16(String value) {
        if (value == null) return null;
        require(value.length() <= 65535, "complete bounded UTF16 value");
        StringBuilder text = new StringBuilder(value.length() * 4);
        for (int index = 0; index < value.length(); ++index)
            text.append(String.format(java.util.Locale.ROOT, "%04x", (int)value.charAt(index)));
        return text.toString();
    }

    private static Object scalar(Object value) {
        if (value == null) return null;
        if (value instanceof Double) return object("type", "double", "raw_bits", hex(Double.doubleToRawLongBits((Double)value), 16));
        if (value instanceof Float) return object("type", "float", "raw_bits", hex(Float.floatToRawIntBits((Float)value), 8));
        if (value instanceof Long) return object("type", "long", "signed_decimal", value.toString(), "raw_bits", hex((Long)value, 16));
        if (value instanceof Number) return object("type", value.getClass().getSimpleName(), "value", ((Number)value).intValue());
        if (value instanceof Boolean) return object("type", "boolean", "value", value);
        if (value instanceof Character) return object("type", "char", "value", (int)(Character)value);
        if (value instanceof String) return object("type", "String", "utf16", utf16((String)value), "length", ((String)value).length());
        throw new IllegalArgumentException("not an admitted scalar");
    }

    private static Object failure(Throwable value) {
        StringWriter text = new StringWriter();
        value.printStackTrace(new PrintWriter(text));
        require(text.getBuffer().length() <= 65536, "complete bounded Throwable");
        return object("status", "threw", "class", value.getClass().getName(),
            "message_utf16", utf16(value.getMessage()), "stack_trace", text.toString());
    }

    private static Throwable unwrap(Throwable value) {
        while ((value instanceof InvocationTargetException || value instanceof ChunkStorageObservation.OriginalFailure)
                && value.getCause() != null) value = value.getCause();
        return value;
    }

    private static final class Case {
        final int index, rx, rz, openings, reject;
        final String name;
        final long seed;
        final boolean override;

        Case(int index, long seed, boolean override) {
            require(index >= 0 && index < 24, "declared case index0..23");
            this.index = index; this.seed = seed; this.override = override;
            rx = index / 12 + 2; rz = (index / 6) % 2 + 2;
            int variant = index % 6;
            openings = variant == 1 ? 5 : variant == 2 ? 0 : variant == 3 ? 6 : 1;
            reject = variant == 4 ? 1 : variant == 5 ? 2 : 0;
            name = "r" + rx + "x" + rz + "_" + (reject == 1 ? "floor_reject" : reject == 2 ? "roof_reject" : "openings_" + openings);
            Random independent = new Random(seed);
            independent.nextGaussian();
            require(independent.nextInt(2) + 2 == rx && independent.nextInt(2) + 2 == rz,
                "declared primed-JDK seed selects the requested radius pair");
        }

        Object view() {
            Random independent = new Random(seed);
            double gaussian = independent.nextGaussian();
            int first = independent.nextInt(2), second = independent.nextInt(2);
            return object("index", index, "name", name, "center", Arrays.asList(X,Y,Z),
                "radius_x", rx, "radius_z", rz, "openings", openings, "reject", reject,
                "seed_hex", hex(seed,16), "seed_signed_decimal", Long.toString(seed),
                "seed_role", override ? "explicit pinned replay INPUT override; no expected output" : "first independent JDK radius-prefix seed",
                "independent_prefix_control", object("role", "seed selection only, not an original placement trace or expected terrain",
                    "one_nextGaussian", scalar(gaussian), "nextInt2_x", first, "nextInt2_z", second));
        }
    }

    private static long[] defaultSeeds() {
        long[] seeds = new long[4]; boolean[] found = new boolean[4]; int remaining = 4;
        for (long seed = 0; seed < MAX_SEARCH && remaining != 0; ++seed) {
            Random random = new Random(seed);
            random.nextGaussian();
            int pair = random.nextInt(2) * 2 + random.nextInt(2);
            if (!found[pair]) { found[pair] = true; seeds[pair] = seed; --remaining; }
        }
        require(remaining == 0, "bounded independent Gaussian/radius seed scan covers all four pairs");
        return seeds;
    }

    private static final class Call {
        final Object value;
        final Throwable thrown;
        Call(Object value, Throwable thrown) { this.value = value; this.thrown = thrown; }
        Object requireReturn() throws Exception {
            if (thrown != null) throw new Exception("actual original setup/acquisition call threw", thrown);
            return value;
        }
    }

    private static final class Observer {
        final ChunkStorageObservation.Observer storage;
        final Class<?> feature, blocks, tile, chest, spawner, stack;
        final Method acquire, place;
        final List<Object> operations = new ArrayList<>();
        boolean observedTile;

        Observer(ChunkStorageObservation.Observer storage) throws Exception {
            this.storage = storage;
            feature = storage.type(storage.client ? "er" : "da");
            blocks = storage.type(storage.client ? "uu" : "na");
            tile = storage.type(storage.client ? "ow" : "jh");
            chest = storage.type(storage.client ? "js" : "gk");
            spawner = storage.type(storage.client ? "cy" : "bx");
            stack = storage.type(storage.client ? "iz" : "fy");
            acquire = storage.bind("actual_World_chunk_acquire", storage.worldType, "c", storage.chunkType, 1, int.class, int.class);
            storage.bind("actual_cache_lookup_route", storage.cache.getClass(), "b", storage.chunkType, 1, int.class, int.class);
            storage.bind("actual_cache_miss_coordinator", storage.cache.getClass(), "c", storage.chunkType, 1, int.class, int.class);
            place = storage.bind("measured_Dungeon_place", feature, "a", boolean.class, 1,
                storage.worldType, Random.class, int.class, int.class, int.class);
            storage.ctor(feature);
            storage.bind("Dungeon_notifying_World_set", storage.worldType, storage.client ? "f" : "e", boolean.class, 1,
                int.class, int.class, int.class, int.class);
            storage.bind("Dungeon_World_material", storage.worldType, storage.client ? "f" : "d",
                storage.type(storage.client ? "ln" : "hj"), 1, int.class, int.class, int.class);
            storage.bind("Dungeon_World_air", storage.worldType, storage.client ? "d" : "f", boolean.class, 1,
                int.class, int.class, int.class);
            storage.bind("Dungeon_World_tile", storage.worldType, "b", tile, 1, int.class, int.class, int.class);
            storage.bind("actual_chest_size", chest, "a", int.class, 1);
            storage.bind("actual_chest_set", chest, "a", void.class, 1, int.class, stack);
            storage.bind("actual_spawner_set_name", spawner, "a", void.class, 1, String.class);
            checkField(blocks, "u", blocks, 25);
            checkField(blocks, "bn", int.class, 17);
            checkField(tile, "a", Map.class, 10); checkField(tile, "b", Map.class, 10);
            checkField(tile, "d", storage.worldType, 1);
            for (String name : new String[] {"e","f","g"}) checkField(tile, name, int.class, 1);
            checkField(tile, "h", boolean.class, 4);
            checkField(spawner, "i", String.class, 2);
            Field inventory = chest.getDeclaredField("a");
            require(inventory.getModifiers() == 2 && inventory.getType().isArray() &&
                inventory.getType().getComponentType() == stack, "actual private chest Stack array");
            require(stack.getDeclaredFields().length == 4, "actual complete four-int Stack layout");
            for (String name : new String[] {"a","b","c","d"})
                checkField(stack, name, int.class, name.equals("d") ? 2 : 1);
        }

        void checkField(Class<?> owner, String name, Class<?> type, int modifiers) throws Exception {
            Field field = owner.getDeclaredField(name);
            require(field.getType() == type && field.getModifiers() == modifiers, "exact declared field " + owner.getName() + "." + name);
            storage.fields.put(owner.getName()+"."+name, object("class", owner.getName(), "name", name,
                "signature", ChunkStorageObservation.descriptor(type), "modifiers", modifiers));
        }

        Object ref(Object value) throws Exception {
            if (value != null && value.getClass().getClassLoader() == storage.loader && !value.getClass().isArray())
                storage.type(value.getClass().getName());
            return storage.ref(value);
        }

        Object value(Object value, int depth) throws Exception {
            require(depth <= 16, "bounded complete selected payload depth");
            if (value == null || value instanceof Number || value instanceof Boolean ||
                value instanceof Character || value instanceof String) return scalar(value);
            if (value instanceof Class<?>) {
                Class<?> type = (Class<?>)value;
                if (type.getClassLoader() == storage.loader) storage.type(type.getName());
                return object("reference", ref(value), "represented_class", type.getName());
            }
            if (value.getClass().isArray()) {
                int length = Array.getLength(value);
                if (value instanceof byte[]) {
                    require(length <= 32768, "complete bounded selected bytes");
                    return object("reference", ref(value), "payload", ChunkStorageObservation.blob((byte[])value));
                }
                require(length <= MAX_COLLECTION, "complete bounded selected array");
                List<Object> values = new ArrayList<>();
                for (int index = 0; index < length; ++index) values.add(value(Array.get(value,index), depth+1));
                return object("reference", ref(value), "array_class", value.getClass().getName(), "length", length, "items", values);
            }
            if (value instanceof Map<?,?>) {
                Map<?,?> map = (Map<?,?>)value; require(map.size() <= MAX_COLLECTION, "complete bounded selected Map");
                List<Object> rows = new ArrayList<>(); int ordinal = 0;
                for (var entry : map.entrySet()) rows.add(object("ordinal", ordinal++, "key", value(entry.getKey(),depth+1),
                    "value", value(entry.getValue(),depth+1)));
                return object("reference", ref(value), "size", map.size(), "entries", rows);
            }
            if (value instanceof Collection<?>) {
                Collection<?> collection = (Collection<?>)value;
                require(collection.size() <= MAX_COLLECTION, "complete bounded selected Collection");
                List<Object> rows = new ArrayList<>();
                for (Object entry : collection) rows.add(value(entry,depth+1));
                return object("reference", ref(value), "size", collection.size(), "entries", rows);
            }
            if (tile.isInstance(value) || value.getClass() == stack) {
                if (tile.isInstance(value)) observedTile = true;
                return originalFields(value,depth+1);
            }
            return ref(value);
        }

        Object originalFields(Object receiver, int depth) throws Exception {
            require(depth <= 16, "bounded original field expansion");
            List<Object> rows = new ArrayList<>();
            for (Class<?> owner = receiver.getClass(); owner != Object.class; owner = owner.getSuperclass()) {
                require(owner != null && owner.getClassLoader() == storage.loader, "actual original field hierarchy");
                storage.type(owner.getName());
                Field[] declared = owner.getDeclaredFields(); require(declared.length <= 128, "bounded actual instance layout");
                Arrays.sort(declared, Comparator.comparing(Field::getName));
                for (Field field : declared) {
                    if (Modifier.isStatic(field.getModifiers())) continue;
                    Object current = storage.field(owner,field.getName(),receiver);
                    rows.add(object("class", owner.getName(), "name", field.getName(),
                        "signature", ChunkStorageObservation.descriptor(field.getType()), "modifiers", field.getModifiers(),
                        "value", value(current,depth+1)));
                }
            }
            return object("object", ref(receiver), "fields", rows);
        }

        Map<String,Object> operation(String label, String owner, String method, String signature) {
            require(operations.size() < 128, "bounded complete explicit operation inventory");
            Map<String,Object> row = object("ordinal", operations.size(), "label", label,
                "class", owner, "method", method, "signature", signature);
            operations.add(row); System.err.println("DUNGEON_PLACE_BEGIN " + row.get("ordinal") + " " + label);
            return row;
        }

        Call call(String label, Method method, Object receiver, Object... arguments) throws Exception {
            StringBuilder signature = new StringBuilder("(");
            for (Class<?> type : method.getParameterTypes()) signature.append(ChunkStorageObservation.descriptor(type));
            signature.append(')').append(ChunkStorageObservation.descriptor(method.getReturnType()));
            Map<String,Object> row = operation(label,method.getDeclaringClass().getName(),method.getName(),signature.toString());
            row.put("receiver",ref(receiver)); List<Object> args = new ArrayList<>();
            for (Object argument : arguments) args.add(argument == null || argument instanceof Number || argument instanceof Boolean
                ? scalar(argument) : ref(argument));
            row.put("arguments",args);
            try {
                Object actual = method.invoke(receiver,arguments);
                row.put("outcome",object("status","returned","value",actual instanceof Boolean || actual instanceof Number ? scalar(actual) : ref(actual)));
                System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
                return new Call(actual,null);
            } catch (InvocationTargetException error) {
                row.put("outcome",failure(error.getCause()));
                System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " threw " + error.getCause().getClass().getName());
                return new Call(null,error.getCause());
            }
        }

        Object constructFeature() throws Exception {
            Constructor<?> constructor = storage.ctor(feature);
            Map<String,Object> row = operation("actual_Dungeon_constructor",feature.getName(),"<init>","()V");
            try {
                Object actual = constructor.newInstance(); require(actual.getClass() == feature,"actual Dungeon instance");
                row.put("outcome",object("status","returned","value",ref(actual)));
                System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
                return actual;
            } catch (InvocationTargetException error) {
                row.put("outcome",failure(error.getCause()));
                System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " threw " + error.getCause().getClass().getName());
                throw error;
            }
        }

        Object prepared(Case test) throws Exception {
            Object stone = storage.field(blocks,"u",null);
            require(stone != null && blocks.isInstance(stone),"actual named stone object");
            int id = (Integer)storage.field(blocks,"bn",stone);
            require(id > 0 && id < 256,"actual stone ID fits original block-byte input");
            byte[] data = new byte[32768], metadata = new byte[16384], sky = new byte[16384], light = new byte[16384], heights = new byte[256];
            Arrays.fill(data,(byte)id); Arrays.fill(heights,(byte)128);
            for (int index = 0; index < test.openings; ++index) {
                int x = index < 2*test.rz+1 ? X-test.rx-1 : X;
                int z = index < 2*test.rz+1 ? Z-test.rz+index : Z-test.rz-1;
                data[(x*16+z)*128+Y] = 0; data[(x*16+z)*128+Y+1] = 0;
            }
            if (test.reject != 0)
                data[((X-test.rx-1)*16+Z-test.rz-1)*128+(test.reject == 1 ? Y-1 : Y+4)] = 0;
            Object root = storage.fresh(), level = storage.fresh();
            storage.put(level,"xPos",int.class,0); storage.put(level,"zPos",int.class,0);
            storage.put(level,"LastUpdate",long.class,0L);
            storage.put(level,"Blocks",byte[].class,data); storage.put(level,"Data",byte[].class,metadata);
            storage.put(level,"SkyLight",byte[].class,sky); storage.put(level,"BlockLight",byte[].class,light);
            storage.put(level,"HeightMap",byte[].class,heights); storage.put(level,"TerrainPopulated",byte.class,(byte)1);
            ChunkStorageObservation.invoke(storage.setTag,level,"Entities",storage.list());
            ChunkStorageObservation.invoke(storage.setTag,level,"TileEntities",storage.list());
            storage.put(root,"Level",storage.compound,level);
            return object("nbt",root,"metadata",object("scope","prepared owned arrays before actual source decoding, not natural terrain or a cache publication",
                "stone_class",stone.getClass().getName(),"stone_object",ref(stone),"stone_field","u","actual_stone_id",id,
                "layout","(x*16+z)*128+y","all_other_blocks","actual stone","metadata_sky_block_light","zero",
                "heightmap_unsigned",128,"TerrainPopulated",true,"Entities_and_TileEntities","empty original Lists"));
        }

        Object rawChunk(Object chunk) throws Exception {
            require(storage.chunkType.isInstance(chunk),"actual Chunk/fallback snapshot");
            Map<String,Object> arrays = new LinkedHashMap<>();
            arrays.put("blocks",ChunkStorageObservation.blob((byte[])storage.get("chunk.b",chunk)));
            arrays.put("blocks_reference",ref(storage.get("chunk.b",chunk)));
            for (String name : new String[] {"e","f","g"}) {
                Object wrapper = storage.get("chunk."+name,chunk);
                arrays.put(name,object("wrapper",ref(wrapper),"bytes",wrapper == null ? null :
                    ChunkStorageObservation.blob((byte[])storage.get("nibble.a",wrapper)),
                    "backing",ref(wrapper == null ? null : storage.get("nibble.a",wrapper))));
            }
            arrays.put("heightmap",ChunkStorageObservation.blob((byte[])storage.get("chunk.h",chunk)));
            arrays.put("heightmap_reference",ref(storage.get("chunk.h",chunk)));
            List<Object> entries = new ArrayList<>();
            Map<?,?> tiles = (Map<?,?>)storage.get("chunk.l",chunk);
            require(tiles.size() <= 16,"complete bounded actual Tile Map");
            for (var entry : tiles.entrySet()) entries.add(object("key",originalFields(entry.getKey(),0),"tile",value(entry.getValue(),0)));
            List<Object> slices = new ArrayList<>(); Object[] entitySlices = (Object[])storage.get("chunk.m",chunk);
            require(entitySlices.length == 8,"actual complete Entity slice array");
            for (Object slice : entitySlices) slices.add(value(slice,0));
            Map<String,Object> flags = new LinkedHashMap<>();
            for (String flag : new String[] {"c","n","o","p","q"}) flags.put(flag,storage.get("chunk."+flag,chunk));
            return object("object",ref(chunk),"world",ref(storage.get("chunk.d",chunk)),
                "coordinates",Arrays.asList(storage.get("chunk.j",chunk),storage.get("chunk.k",chunk)),
                "data",arrays,"minimum_height",storage.get("chunk.i",chunk),"flags",flags,
                "last_save_time_hex",hex((Long)storage.get("chunk.r",chunk),16),
                "Tile_Map",object("object",ref(tiles),"size",tiles.size(),"entries",entries),
                "Entity_slice_array",ref(entitySlices),"Entity_slices",slices);
        }

        Object context() throws Exception {
            List<Object> chunks = new ArrayList<>(); IdentityHashMap<Object,Boolean> seen = new IdentityHashMap<>();
            Map<?,?> map = (Map<?,?>)storage.get("cache.e",storage.cache);
            List<?> inserted = (List<?>)storage.get("cache.f",storage.cache);
            require(map.size() <= MAX_CHUNKS && inserted.size() <= MAX_CHUNKS,"complete actual cache image budget");
            List<Object> targets = new ArrayList<>(map.values()); targets.addAll(inserted); targets.add(storage.get("cache.b",storage.cache));
            for (Object chunk : targets) if (chunk != null && seen.put(chunk,Boolean.TRUE) == null) chunks.add(rawChunk(chunk));
            Map<String,Object> lists = new LinkedHashMap<>();
            for (String name : new String[] {"b","D","c","G","d","e"})
                lists.put(name,value(storage.field(storage.worldType,name,storage.world),0));
            Map<String,Object> scheduled = new LinkedHashMap<>();
            for (String name : new String[] {"E","F"}) {
                Collection<?> collection = (Collection<?>)storage.field(storage.worldType,name,storage.world);
                require(collection.size() <= MAX_COLLECTION,"complete real scheduled collection");
                List<Object> records = new ArrayList<>();
                for (Object record : collection) records.add(record != null && record.getClass().getClassLoader() == storage.loader
                    ? originalFields(record,0) : value(record,0));
                scheduled.put(name,object("object",ref(collection),"size",collection.size(),"native_iteration",records));
            }
            Object names = observedTile ? object("scope","read only after an actual reachable Tile was observed; no eager namespace active use",
                "a",value(storage.field(tile,"a",null),0),"b",value(storage.field(tile,"b",null),0)) :
                object("status","not_read","reason","no actual reachable Tile observed; class initialization state is unobservable");
            return object("established_world_context",storage.context(),"complete_cache_chunks",chunks,
                "World_lists_by_actual_field",lists,"World_scheduled_by_actual_field",scheduled,"Tile_name_Maps",names,
                "World_instance_fields",originalFields(storage.world,0));
        }

        Object prime(Random random) throws Exception {
            Map<String,Object> row = operation("actual_supplied_Random_priming","java.util.Random","nextGaussian","()D");
            row.put("receiver",ref(random)); double value = random.nextGaussian();
            row.put("outcome",object("status","returned","value",scalar(value)));
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            return object("scope","exactly one actual nextGaussian before placement; no Random field reset or cache inspection", "operation",row);
        }

        Object continuation(Random random) throws Exception {
            List<Object> rows = new ArrayList<>();
            Map<String,Object> row = operation("post_placement_nextGaussian","java.util.Random","nextGaussian","()D");
            row.put("receiver",ref(random)); row.put("outcome",object("status","returned","value",scalar(random.nextGaussian()))); rows.add(row);
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            row = operation("post_placement_nextInt11","java.util.Random","nextInt","(I)I");
            row.put("receiver",ref(random)); row.put("bound",11); row.put("outcome",object("status","returned","value",scalar(random.nextInt(11)))); rows.add(row);
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            row = operation("post_placement_nextInt","java.util.Random","nextInt","()I");
            row.put("receiver",ref(random)); row.put("outcome",object("status","returned","value",scalar(random.nextInt()))); rows.add(row);
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            row = operation("post_placement_nextLong","java.util.Random","nextLong","()J");
            row.put("receiver",ref(random)); row.put("outcome",object("status","returned","value",scalar(random.nextLong()))); rows.add(row);
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            row = operation("post_placement_nextDouble","java.util.Random","nextDouble","()D");
            row.put("receiver",ref(random)); row.put("outcome",object("status","returned","value",scalar(random.nextDouble()))); rows.add(row);
            System.err.println("DUNGEON_PLACE_END " + row.get("ordinal") + " returned");
            return object("scope","explicit draws consume the same actual Random after the call, including actual throwing prefixes; no nondestructive-copy claim", "operations",rows);
        }

        void run(Case test, Path output, Map<String,Object> record, Method json) throws Exception {
            record.put("initial_context",context());
            Object featureValue = constructFeature(); record.put("feature_before",originalFields(featureValue,0));
            Map<?,?> prepared = (Map<?,?>)prepared(test); Object nbt = prepared.get("nbt");
            record.put("preparation",prepared.get("metadata"));
            record.put("prepared_input",storage.artifact("prepared-room",nbt));
            write(Path.of(output+".prepared.json"),record,json);
            Object stream = call("prepared_input_actual_region_stream",storage.regionWrite,null,
                storage.root.resolve("world").toFile(),0,0).requireReturn();
            require(stream instanceof DataOutputStream,"real original output stream for prepared input");
            try (DataOutputStream actual = (DataOutputStream)stream) {
                call("prepared_input_actual_NBT_write",storage.write,null,nbt,actual).requireReturn();
            }
            Object chunk = call("actual_World_provider_cache_acquisition",acquire,storage.world,0,0).requireReturn();
            require(chunk != null && chunk.getClass() == storage.chunkType && storage.get("chunk.d",chunk) == storage.world,
                "actual ordinary loaded Chunk bound to the genuine World");
            require((Integer)storage.get("chunk.j",chunk) == 0 && (Integer)storage.get("chunk.k",chunk) == 0,"prepared Chunk coordinates");
            boolean mapOwns = false, listOwns = false;
            for (Object member : ((Map<?,?>)storage.get("cache.e",storage.cache)).values()) mapOwns |= member == chunk;
            for (Object member : (List<?>)storage.get("cache.f",storage.cache)) listOwns |= member == chunk;
            require(mapOwns && listOwns,"actual acquired Chunk appears in real cache Map and insertion List");
            record.put("after_acquisition",context()); write(Path.of(output+".acquired.json"),record,json);
            Random random = new Random(test.seed);
            record.put("supplied_Random",object("class",random.getClass().getName(),"identity",ref(random),"declared_seed",test.view()));
            record.put("priming",prime(random));
            record.put("before_placement",context()); write(Path.of(output+".before.json"),record,json);
            Call measured = call("actual_notifying_whole_Dungeon_place",place,featureValue,storage.world,random,X,Y,Z);
            Object outcome = measured.thrown == null ? object("status","returned","boolean",measured.value) : failure(measured.thrown);
            record.put("placement",outcome);
            write(Path.of(output+".call.json"),object("selected_case",test.view(),"placement",outcome,
                "scope","actual call outcome receipt before full after-image serialization; no complete-image or success claim"),json);
            record.put("after_placement",context()); write(Path.of(output+".after.json"),record,json);
            record.put("continuation",continuation(random));
            record.put("feature_after",originalFields(featureValue,0));
            record.put("prepared_input_after",storage.artifact("prepared-room-after",nbt));
            record.put("persistence",object("status","not_attempted","scope","no direct writer, unload, cache reset, original map clearing or reload success is invented"));
            record.put("unavailable_locals","on an original throw, only actual reachable World/cache/Map/List/Tile/Stack state is captured; inaccessible placement/chest/Stack locals are not reconstructed");
            record.put("terminal",measured.thrown == null ? "captured_actual_return; comparison external" : "captured_actual_throw_and_reachable_prefix; comparison external");
        }
    }

    private static void write(Path output, Object record, Method json) throws Exception {
        byte[] bytes = (((String)json.invoke(null,record))+"\n").getBytes(StandardCharsets.UTF_8);
        require(bytes.length <= MAX_OUTPUT,"complete bounded output; no truncation");
        Files.write(output,bytes,StandardOpenOption.CREATE_NEW);
    }

    private static void paths(Path root, Path output) throws Exception {
        Path marker = root.resolve(".chunk-storage-owned");
        require(Files.isRegularFile(marker,LinkOption.NOFOLLOW_LINKS) && Files.size(marker) == 30 &&
            Files.readString(marker,StandardCharsets.US_ASCII).equals("fresh chunk storage reference\n"),"existing copied-baseline ownership marker");
        require(Files.isRegularFile(root.resolve("world/level.dat"),LinkOption.NOFOLLOW_LINKS),"complete genuine saved baseline");
        try (var walk = Files.walk(root)) {
            List<Path> entries = walk.limit(257).toList(); require(entries.size() <= 256,"complete bounded owned baseline inventory");
            long bytes = 0;
            for (Path entry : entries) {
                require(!Files.isSymbolicLink(entry),"no linked baseline entry");
                if (!Files.isDirectory(entry,LinkOption.NOFOLLOW_LINKS)) {
                    require(Files.isRegularFile(entry,LinkOption.NOFOLLOW_LINKS) && Files.size(entry) <= 32*1024*1024,"bounded baseline file");
                    bytes += Files.size(entry);
                }
            }
            require(bytes <= 32*1024*1024,"bounded complete copied baseline bytes");
        }
        require(output.getParent().equals(root.getParent()) && !output.startsWith(root) &&
            !Files.exists(output,LinkOption.NOFOLLOW_LINKS),"fresh sibling output outside the owned World root");
        for (String stage : STAGES) require(!Files.exists(Path.of(output+"."+stage+".json"),LinkOption.NOFOLLOW_LINKS),"fresh stage receipt");
        for (String suffix : new String[] {".nbt",".threads-before.json",".threads-after.json"})
            require(!Files.exists(Path.of(output+suffix),LinkOption.NOFOLLOW_LINKS),"fresh artifact/thread sidecar");
    }

    public static void main(String[] args) throws Exception {
        System.setOut(System.err);
        require(args.length == 5 || args.length == 6,"jar client|server case_index copied_owned_world_root output_json [seed_hex16]");
        boolean client = args[1].equals("client"); require(client || args[1].equals("server"),"side");
        int index = Integer.parseInt(args[2]); require(index >= 0 && index < 24,"case index0..23");
        Path jarArgument = Path.of(args[0]).toAbsolutePath().normalize(), jar = jarArgument.toRealPath();
        Path rootArgument = Path.of(args[3]).toAbsolutePath().normalize(), root = rootArgument.toRealPath();
        Path output = Path.of(args[4]).toAbsolutePath().normalize();
        require(jarArgument.equals(jar) && rootArgument.equals(root) && Files.isRegularFile(jar,LinkOption.NOFOLLOW_LINKS),"canonical original and copied root");
        paths(root,output);
        String expected = client ? ChunkStorageObservation.CLIENT : ChunkStorageObservation.SERVER;
        require(Files.size(jar) == (client ? 1465375 : 503100) && ChunkStorageObservation.hash(Files.readAllBytes(jar)).equals(expected),"untouched pinned archive");
        long[] defaults = defaultSeeds(); long selected = defaults[index/6];
        if (args.length == 6) { require(args[5].matches("[0-9a-f]{16}"),"canonical16-hex replay seed"); selected = Long.parseUnsignedLong(args[5],16); }
        Case test = new Case(index,selected,args.length == 6);
        List<Object> corpus = new ArrayList<>();
        for (int item = 0; item < 24; ++item) corpus.add(new Case(item,defaults[item/6],false).view());
        Method json = ChunkObservation.class.getDeclaredMethod("json",Object.class);
        require(json.getReturnType() == String.class && json.getModifiers() == 10,"actual project JSON codec descriptor"); json.setAccessible(true);
        Map<String,Object> record = object("schema",1,"observer_scope","finite prepared single-call capture; validation and comparisons are external",
            "side",args[1],"jar_sha256",expected,"selected_case",test.view(),"default_corpus",corpus,
            "scope","one actual whole notifying Dungeon call on declared prepared arrays acquired through genuine World/provider/cache APIs; no natural terrain/population/full-world equality claim",
            "preparation_scope","real saved-World constructor, original NBT/region stream, actual World.c decode/normalization/activation; no game field, cache Map/List, source-phase, owner/count or Random-state assignments",
            "seed_scope","independent JDK nextGaussian then two nextInt(2) values select radii only; no voxel/Chest/loot/mob expected output is predicted or read from C",
            "context_scope","complete bounded reachable Chunk bytes/flags, actual Tile/Stack fields and arrays, World lists/scheduled records, namespace Maps after actual Tile exposure; Random internals/general arbitrary object graphs excluded",
            "operations_scope","explicit Dungeon constructor, region stream/write, acquisition, priming, placement and continuation calls; shared genuine World setup, NBT construction/artifact encoding and source nested notifying callbacks are untraced",
            "startup_status","pending genuine original World construction");
        write(Path.of(output+".startup.json"),record,json);
        Path artifacts = Path.of(output+".nbt"); Files.createDirectory(artifacts);
        Set<String> originalNames = new HashSet<>();
        try (JarFile archive = new JarFile(jar.toFile())) {
            var entries = archive.entries(); while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName(); if (name.endsWith(".class")) originalNames.add(name.substring(0,name.length()-6).replace('/','.'));
            }
        }
        Throwable fatal = null; ChunkStorageObservation.Observer actual = null;
        URL url = jar.toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url},ClassLoader.getPlatformClassLoader())) {
            try {
                actual = new ChunkStorageObservation.Observer(url,loader,root,artifacts,client);
                record.put("startup_status","genuine original World constructor returned; no cold/transitive-work claim");
                record.put("class_origins",actual.origins); record.put("bindings",actual.bindings); record.put("fields",actual.fields);
                Observer observer = new Observer(actual); record.put("operations",observer.operations);
                ChunkStorageObservation.threads(Path.of(output+".threads-before.json"),originalNames);
                observer.run(test,output,record,json);
            } catch (Throwable error) {
                fatal = unwrap(error); record.put("fatal",failure(fatal));
                record.put("terminal","incomplete observer/setup/image capture; do not infer placement completion");
                record.put("unavailable_setup_locals",actual == null ? "constructor did not return; partial original World/Store owners are inaccessible and not fabricated" : "only completed earlier stage receipts and actual reachable owners are available");
            } finally {
                if (actual != null) try { actual.finish(); }
                catch (Throwable error) { record.put("close_failure",failure(unwrap(error))); if (fatal == null) fatal = unwrap(error); }
            }
            if (actual != null) try { ChunkStorageObservation.threads(Path.of(output+".threads-after.json"),originalNames); }
            catch (Throwable error) { record.put("thread_capture_failure",failure(unwrap(error))); if (fatal == null) fatal = unwrap(error); }
        }
        require(Files.size(jar) == (client ? 1465375 : 503100) && ChunkStorageObservation.hash(Files.readAllBytes(jar)).equals(expected),"original archive preserved");
        if (fatal != null) record.put("terminal","incomplete observer/setup/image/close/thread capture; preserve earlier actual call receipts without acceptance");
        write(output,record,json);
        if (fatal != null) throw new Exception("Dungeon placement observer did not complete its declared capture",fatal);
    }
}
