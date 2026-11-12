import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;
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
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarFile;

/** Finite paired GAME Chunk Entity observations on an independently copied real
 * saved World. Original classes and constructors are untouched. Direct schema
 * and actual region-loader/writer calls are separate measured branches. */
public final class GameEntityStorageObservation {
    private static final String[] CASES = {"item", "two_items_slice_order", "duplicate_records",
        "repeated_same_entity_adoption", "dead_null_stack", "late_null_stack",
        "unknown_only", "unknown_after_item", "boat_after_item", "late_bad_count"};
    private static final int MAX_JSON = ChunkStorageObservation.MAX_BYTES;
    private static final int MAX_ENTITIES = 16;
    private static final String UNKNOWN = "NativeUnknown";

    private static void require(boolean value, String message) {
        ChunkStorageObservation.require(value, message);
    }

    private static Map<String,Object> object(Object... pairs) {
        return ChunkStorageObservation.object(pairs);
    }

    private static String utf16(String value) {
        if (value == null) return null;
        require(value.length() <= 65535, "bounded complete UTF16 observation");
        StringBuilder text = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i)
            text.append(String.format(java.util.Locale.ROOT, "%04x", (int)value.charAt(i)));
        return text.toString();
    }

    private static Object primitive(Object value) {
        if (value == null) return null;
        if (value instanceof Double)
            return object("type", "double", "raw_bits", String.format(java.util.Locale.ROOT,
                "%016x", Double.doubleToRawLongBits((Double)value)));
        if (value instanceof Float)
            return object("type", "float", "raw_bits", String.format(java.util.Locale.ROOT,
                "%08x", Float.floatToRawIntBits((Float)value)));
        if (value instanceof Boolean) return object("type", "boolean", "value", value);
        if (value instanceof Character) return object("type", "char", "value", (int)(Character)value);
        if (value instanceof Number)
            return object("type", value.getClass().getSimpleName(), "value", ((Number)value).longValue());
        if (value instanceof String) return object("type", "String", "utf16", utf16((String)value));
        throw new IllegalArgumentException("not a primitive observation value");
    }

    private static final class Observer {
        final ChunkStorageObservation.Observer storage;
        final Class<?> base, item, stack, tag, watcher;
        final Method adopt, entitySave, tagKind;
        final List<Object> operations = new ArrayList<>();
        int operationCount;

        Observer(ChunkStorageObservation.Observer storage) throws Exception {
            this.storage = storage;
            base = storage.type(storage.client ? "sn" : "lq");
            item = storage.type(storage.client ? "hl" : "ez");
            stack = storage.type(storage.client ? "iz" : "fy");
            tag = storage.tag;
            watcher = storage.type(storage.client ? "ud" : "mo");
            adopt = storage.bind("measured_chunk_adopt", storage.chunkType, "a", void.class, 1, base);
            entitySave = storage.bind("measured_entity_save", base, "c", boolean.class, 1, storage.compound);
            tagKind = storage.bind("observed_tag_kind", tag, "a", byte.class, 1025);
            storage.bind("entity_base_write", base, "d", void.class, 1, storage.compound);
            storage.bind("entity_base_read", base, "e", void.class, 1, storage.compound);
            storage.bind("item_write", item, "b", void.class, 1, storage.compound);
            storage.bind("item_read", item, "a", void.class, 1, storage.compound);
            storage.bind("stack_write", stack, "a", storage.compound, 1, storage.compound);
            storage.bind("stack_read", stack, "b", void.class, 1, storage.compound);
            storage.bind("entity_class_name", base, storage.client ? "aj" : "ag", String.class,
                Modifier.PROTECTED | Modifier.FINAL);
            storage.ctor(item, storage.worldType);
            storage.ctor(stack, storage.compound);
        }

        Object reference(Object value) throws Exception {
            if (value != null) {
                Class<?> type = value.getClass();
                while (type.isArray()) type = type.getComponentType();
                if (!type.isPrimitive() && type.getClassLoader() == storage.loader)
                    storage.type(type.getName());
            }
            return storage.ref(value);
        }

        Object value(Object value) throws Exception {
            if (value == null || value instanceof Number || value instanceof Boolean ||
                    value instanceof Character || value instanceof String)
                return primitive(value);
            return reference(value);
        }

        Object field(Class<?> owner, String name, Object receiver) throws Exception {
            return storage.field(owner, name, receiver);
        }

        Object fields(Object receiver) throws Exception {
            if (receiver == null) return null;
            List<Object> values = new ArrayList<>();
            for (Class<?> owner = receiver.getClass(); owner != Object.class; owner = owner.getSuperclass()) {
                require(owner != null && owner.getClassLoader() == storage.loader, "only original instance fields");
                storage.type(owner.getName());
                Field[] members = owner.getDeclaredFields();
                require(members.length <= 128, "bounded declared field inventory");
                Arrays.sort(members, Comparator.comparing(Field::getName));
                for (Field member : members) {
                    if (Modifier.isStatic(member.getModifiers())) continue;
                    Object current = field(owner, member.getName(), receiver);
                    values.add(object("owner", owner.getName(), "name", member.getName(),
                        "descriptor", ChunkStorageObservation.descriptor(member.getType()),
                        "modifiers", member.getModifiers(), "value", value(current)));
                }
            }
            require(values.size() <= 192, "bounded complete instance field inventory");
            return object("object", reference(receiver), "fields", values);
        }

        Object entity(Object entity) throws Exception {
            require(base.isInstance(entity), "actual Entity slice value");
            Map<String,Object> row = object("instance", fields(entity),
                "world", reference(field(base, storage.client ? "aI" : "aL", entity)),
                "world_is_actual", field(base, storage.client ? "aI" : "aL", entity) == storage.world,
                "bounds", fields(field(base, storage.client ? "aW" : "aZ", entity)));
            Object actualWatcher = field(base, storage.client ? "bD" : "bE", entity);
            row.put("watcher", fields(actualWatcher));
            if (actualWatcher != null) {
                require(actualWatcher.getClass() == watcher, "actual watcher concrete owner");
                Map<?,?> watched = (Map<?,?>)field(watcher, "b", actualWatcher);
                require(watched.size() <= 16, "bounded complete watcher records");
                List<Object> records = new ArrayList<>();
                for (var entry : watched.entrySet())
                    records.add(object("key", value(entry.getKey()), "record", fields(entry.getValue())));
                row.put("watcher_records", object("object", reference(watched), "entries", records));
            }
            if (entity.getClass() == item) {
                Object actualStack = field(item, "a", entity);
                require(actualStack == null || actualStack.getClass() == stack, "actual nullable Item Stack");
                row.put("stack", fields(actualStack));
            }
            return row;
        }

        List<Object> entities(Object chunk) throws Exception {
            List<Object> values = new ArrayList<>();
            if (chunk == null) return values;
            Object[] slices = (Object[])storage.get("chunk.m", chunk);
            require(slices.length == 8, "actual eight Entity slices");
            for (int slice = 0; slice < slices.length; ++slice) {
                List<?> entries = (List<?>)slices[slice];
                require(entries.size() <= MAX_ENTITIES, "bounded complete slice");
                for (int index = 0; index < entries.size(); ++index) {
                    require(values.size() < MAX_ENTITIES, "bounded complete Chunk Entity occurrences");
                    values.add(object("slice", slice, "index", index, "entity", entity(entries.get(index))));
                }
            }
            return values;
        }

        Object chunk(Object chunk) throws Exception {
            return chunk == null ? null : object("chunk", storage.chunk(chunk), "entity_occurrences", entities(chunk));
        }

        List<Object> sourceEntities(Object chunk) throws Exception {
            List<Object> values = new ArrayList<>();
            Object[] slices = (Object[])storage.get("chunk.m", chunk);
            require(slices.length == 8, "actual slice array");
            for (Object slice : slices) {
                List<?> entries = (List<?>)slice;
                require(entries.size() <= MAX_ENTITIES, "bounded source slice");
                for (Object entry : entries) {
                    require(values.size() < MAX_ENTITIES && base.isInstance(entry), "actual bounded Entity entries");
                    values.add(entry);
                }
            }
            return values;
        }

        Object context() throws Exception {
            Map<String,Object> lists = new LinkedHashMap<>();
            for (String name : new String[] {"b", "D"}) {
                List<?> entries = (List<?>)field(storage.worldType, name, storage.world);
                require(entries.size() <= MAX_ENTITIES, "bounded actual World Entity lists");
                List<Object> values = new ArrayList<>();
                for (Object entry : entries) values.add(reference(entry));
                lists.put(name, object("object", reference(entries), "entries", values));
            }
            return object("storage_context", storage.context(), "world_entity_lists", lists);
        }

        Object throwable(Throwable failure) {
            StringWriter text = new StringWriter();
            failure.printStackTrace(new PrintWriter(text));
            require(text.getBuffer().length() <= 65536, "bounded complete Throwable graph");
            return object("status", "exception", "class", failure.getClass().getName(),
                "message_utf16", utf16(failure.getMessage()), "stack_trace", text.toString());
        }

        Object call(String label, Method method, Object receiver, Object... args) throws Exception {
            require(++operationCount <= 256, "bounded complete direct-operation sequence");
            List<Object> arguments = new ArrayList<>();
            for (Object argument : args) arguments.add(value(argument));
            Map<String,Object> event = object("ordinal", operationCount, "label", label,
                "class", method.getDeclaringClass().getName(), "method", method.getName(),
                "receiver", reference(receiver), "arguments", arguments);
            StringBuilder signature = new StringBuilder("(");
            for (Class<?> parameter : method.getParameterTypes())
                signature.append(ChunkStorageObservation.descriptor(parameter));
            signature.append(')').append(ChunkStorageObservation.descriptor(method.getReturnType()));
            event.put("signature", signature.toString());
            operations.add(event);
            System.err.println("GAME_ENTITY_BEGIN " + operationCount + " " + label);
            try {
                Object result = ChunkStorageObservation.invoke(method, receiver, args);
                event.put("outcome", object("status", "returned", "type",
                    ChunkStorageObservation.descriptor(method.getReturnType()), "value", value(result)));
                System.err.println("GAME_ENTITY_END " + operationCount + " returned");
                return result;
            } catch (ChunkStorageObservation.OriginalFailure failure) {
                event.put("outcome", throwable(failure.getCause()));
                System.err.println("GAME_ENTITY_END " + operationCount + " exception " + failure.getCause().getClass().getName());
                throw failure;
            }
        }

        int kind(Object value) throws Exception {
            require(value != null && tag.isInstance(value), "actual NBT value");
            storage.type(value.getClass().getName());
            return ((Byte)ChunkStorageObservation.invoke(tagKind, value)) & 255;
        }

        Object nbt(Object value, int depth, int[] count) throws Exception {
            if (value == null) return null;
            require(depth <= 16 && ++count[0] <= 512, "bounded complete selected NBT tree");
            int kind = kind(value);
            Map<String,Object> row = object("object", reference(value), "type", kind,
                "raw_name_utf16", utf16((String)field(tag, "a", value)));
            Object payload;
            if (kind == 10) {
                Map<String,Object> members = storage.members(value);
                require(members.size() <= 64, "bounded complete selected Compound");
                List<Object> entries = new ArrayList<>();
                for (var entry : members.entrySet()) entries.add(object("key_utf16", utf16(entry.getKey()),
                    "tag", nbt(entry.getValue(), depth + 1, count)));
                payload = entries;
            } else if (kind == 9) {
                List<?> members = (List<?>)field(storage.listType, "a", value);
                require(members.size() <= 32, "bounded complete selected List");
                List<Object> entries = new ArrayList<>();
                for (Object child : members) entries.add(nbt(child, depth + 1, count));
                payload = object("element_type", ((Byte)field(storage.listType, "b", value)) & 255,
                    "items", entries);
            } else if (kind == 7) {
                byte[] bytes = (byte[])field(value.getClass(), "a", value);
                require(bytes != null && bytes.length <= 1024 * 1024, "bounded complete selected byte array");
                payload = ChunkStorageObservation.blob(bytes);
            } else if (kind >= 1 && kind <= 8) {
                payload = primitive(field(value.getClass(), "a", value));
            } else throw new AssertionError("unexpected original NBT value kind " + kind);
            row.put("payload", payload);
            return row;
        }

        Object projection(Object root, boolean whole) throws Exception {
            if (whole) return nbt(root, 1, new int[] {0});
            Map<String,Object> rootMembers = storage.members(root);
            Object level = rootMembers.get("Level");
            if (level == null || level.getClass() != storage.compound) level = root;
            List<Object> headers = new ArrayList<>();
            for (var entry : storage.members(level).entrySet())
                headers.add(object("key_utf16", utf16(entry.getKey()), "type", kind(entry.getValue()),
                    "object", reference(entry.getValue())));
            return object("level_field_headers", headers,
                "Entities", nbt(storage.members(level).get("Entities"), 1, new int[] {0}),
                "TileEntities", nbt(storage.members(level).get("TileEntities"), 1, new int[] {0}));
        }

        Object capture(String label, Object root, boolean whole) throws Exception {
            Object before = projection(root, whole);
            Object artifact = storage.artifact(label, root);
            // Actual sp/lr writing may change an empty List's stored type to Byte.
            return object("typed_before_nbt_write", before, "raw_nbt", artifact,
                "typed_after_nbt_write", projection(root, whole));
        }

        Object itemRecord(boolean second, boolean badCount) throws Exception {
            Object record = storage.fresh();
            storage.put(record, "id", String.class, "Item");
            ChunkStorageObservation.invoke(storage.setTag, record, "Pos", storage.list(
                storage.number(double.class, second ? 9.25 : 8.25), storage.number(double.class, second ? 16.0 : 64.0), storage.number(double.class, 8.25)));
            ChunkStorageObservation.invoke(storage.setTag, record, "Motion", storage.list(
                storage.number(double.class, 0.0), storage.number(double.class, 0.0), storage.number(double.class, 0.0)));
            ChunkStorageObservation.invoke(storage.setTag, record, "Rotation", storage.list(
                storage.number(float.class, 0.0f), storage.number(float.class, 0.0f)));
            storage.put(record, "Health", short.class, (short)7);
            storage.put(record, "Age", short.class, (short)19);
            storage.put(record, "EntityExtra", long.class, second ? 202L : 101L);
            Object itemTag = storage.fresh();
            storage.put(itemTag, "id", short.class, (short)(second ? 263 : 262));
            Object countValue;
            if (badCount) countValue = Short.valueOf((short)6);
            else countValue = Byte.valueOf((byte)(second ? 6 : 5));
            storage.put(itemTag, "Count", badCount ? short.class : byte.class, countValue);
            storage.put(itemTag, "Damage", short.class, (short)(second ? 8 : 7));
            storage.put(itemTag, "ItemExtra", long.class, second ? 1202L : 1101L);
            storage.put(record, "Item", storage.compound, itemTag);
            return record;
        }

        Object input(String name, Object baseline) throws Exception {
            Object root = storage.cloneNBT(baseline);
            Object level = ChunkStorageObservation.invoke(storage.getCompound, root, "Level");
            storage.put(level, "xPos", int.class, 0);
            storage.put(level, "zPos", int.class, 0);
            List<Object> records = new ArrayList<>();
            if (!name.equals("unknown_only")) records.add(itemRecord(false, false));
            if (name.equals("duplicate_records")) records.add(storage.cloneNBT(records.get(0)));
            if (name.equals("late_bad_count")) records.add(unknownRecord(20L));
            if (name.equals("two_items_slice_order") || name.equals("late_null_stack") || name.equals("late_bad_count"))
                records.add(itemRecord(true, name.equals("late_bad_count")));
            if (name.equals("unknown_only") || name.equals("unknown_after_item")) {
                records.add(unknownRecord(name.equals("unknown_only") ? 10L : 20L));
            }
            if (name.equals("boat_after_item")) {
                Object boat = storage.filtered(itemRecord(true, false), "Item", "Health", "Age", "EntityExtra");
                storage.put(boat, "id", String.class, "Boat");
                records.add(boat);
            }
            ChunkStorageObservation.invoke(storage.setTag, level, "Entities", storage.list(records.toArray()));
            ChunkStorageObservation.invoke(storage.setTag, level, "TileEntities", storage.list());
            storage.put(root, "RootEntityExtra", long.class, 901L);
            storage.put(level, "LevelEntityExtra", long.class, 902L);
            return root;
        }

        Object unknownRecord(long marker) throws Exception {
            Object record = storage.fresh();
            storage.put(record, "id", String.class, UNKNOWN);
            storage.put(record, "UnknownExtra", long.class, marker);
            return record;
        }

        final class Controls {
            final List<Field> fields = new ArrayList<>();
            final List<Object> receivers = new ArrayList<>(), oldValues = new ArrayList<>();
            final List<Object> rows = new ArrayList<>();
            boolean restored;

            void set(Class<?> owner, String name, Object receiver, Object next) throws Exception {
                Field member = owner.getDeclaredField(name);
                require(!Modifier.isStatic(member.getModifiers()) && !Modifier.isFinal(member.getModifiers()), "mutable instance source control");
                boolean dead = owner == base && name.equals(storage.client ? "be" : "bh") && member.getType() == boolean.class;
                boolean nullableStack = owner == item && name.equals("a") && member.getType() == stack && next == null;
                require(dead || nullableStack, "only explicit dead/nullable Stack controls");
                member.setAccessible(true);
                Object previous = field(owner, name, receiver);
                rows.add(object("class", owner.getName(), "field", name,
                    "descriptor", ChunkStorageObservation.descriptor(member.getType()),
                    "receiver", reference(receiver), "before", value(previous), "prepared", value(next)));
                fields.add(member); receivers.add(receiver); oldValues.add(previous);
                member.set(receiver, next);
            }

            public void close() throws Exception {
                for (int i = fields.size() - 1; i >= 0; --i) {
                    Field member = fields.get(i);
                    Object receiver = receivers.get(i), previous = oldValues.get(i);
                    member.set(receiver, previous);
                    Object now = member.get(receiver);
                    require(member.getType().isPrimitive() ? previous.equals(now) : previous == now,
                        "complete exact source-control restoration");
                }
                restored = true;
            }
        }

        Object prepare(String name, Object chunk, Controls controls) throws Exception {
            List<Object> entries = sourceEntities(chunk);
            if (name.equals("repeated_same_entity_adoption")) {
                require(!entries.isEmpty() && entries.get(0).getClass() == item, "real loaded Item for direct adoption");
                call("explicit_repeat_original_Chunk_a", adopt, chunk, entries.get(0));
                return object("kind", "original_method_call", "container_edits", false);
            }
            if (name.equals("dead_null_stack") || name.equals("late_null_stack")) {
                int index = name.equals("dead_null_stack") ? 0 : 1;
                require(entries.size() > index && entries.get(index).getClass() == item, "real loaded target Item");
                if (index == 0) controls.set(base, storage.client ? "be" : "bh", entries.get(index), true);
                controls.set(item, "a", entries.get(index), null);
            }
            return object("kind", "prepared_mutable_source_fields", "fields", controls.rows,
                "container_edits", false, "static_or_Random_edits", false);
        }

        Object probe(String label, Object entity) throws Exception {
            Object nbt = storage.fresh();
            storage.put(nbt, "Marker", int.class, 123);
            Map<String,Object> row = object("scope", "separate actual direct c call; not access to the schema's inaccessible local Compound",
                "entity_before", entity(entity));
            try { row.put("outcome", object("status", "returned", "boolean", call(label, entitySave, entity, nbt))); }
            catch (ChunkStorageObservation.OriginalFailure failure) { row.put("outcome", throwable(failure.getCause())); }
            row.put("partial_or_complete_nbt", capture(label, nbt, true));
            row.put("entity_after", entity(entity));
            return row;
        }

        Object staticBranch(String prefix, String name, Object input) throws Exception {
            Map<String,Object> row = object("source", "paired original shared GAME schema; no provider/cache acquisition");
            Object level = ChunkStorageObservation.invoke(storage.getCompound, input, "Level");
            Object chunk;
            try { chunk = call(prefix + "-decode", storage.decode, null, storage.world, level); }
            catch (ChunkStorageObservation.OriginalFailure failure) {
                row.put("decode", throwable(failure.getCause()));
                row.put("partial_chunk", "inaccessible original local after throwing decode; no fabricated object or fields");
                row.put("input_after", capture(prefix + "-input-after", input, false));
                return row;
            }
            row.put("decode", object("status", "returned", "value", reference(chunk)));
            row.put("after_decode", chunk(chunk));
            if (chunk == null) {
                row.put("input_after", capture(prefix + "-input-after", input, false));
                return row;
            }
            Controls controls = new Controls();
            try {
                row.put("preparation", prepare(name, chunk, controls));
                row.put("before_encode", chunk(chunk));
                Object output = storage.fresh();
                storage.put(output, "Marker", int.class, 123);
                storage.put(output, "Entities", String.class, "prepared prior value; replaced only at208");
                storage.put(output, "TileEntities", String.class, "prepared prior value; replaced only at289");
                boolean success = false;
                try {
                    call(prefix + "-encode", storage.encode, null, chunk, storage.world, output);
                    row.put("encode", object("status", "returned")); success = true;
                } catch (ChunkStorageObservation.OriginalFailure failure) { row.put("encode", throwable(failure.getCause())); }
                row.put("after_encode", chunk(chunk));
                row.put("partial_or_complete_output", capture(prefix + "-output", output, false));
                if (name.equals("dead_null_stack") || name.equals("late_null_stack")) {
                    List<Object> values = sourceEntities(chunk);
                    row.put("independent_entity_save_probe", probe(prefix + "-entity-probe", values.get(name.equals("dead_null_stack") ? 0 : 1)));
                }
                if (success) {
                    try { row.put("semantic_reload", object("status", "returned", "chunk", chunk(call(prefix + "-reload", storage.decode,
                        null, storage.world, storage.cloneNBT(output))))); }
                    catch (ChunkStorageObservation.OriginalFailure failure) { row.put("semantic_reload", throwable(failure.getCause())); }
                } else row.put("semantic_reload", object("status", "not_run", "reason", "encode threw"));
            } finally { controls.close(); row.put("controls_restored", controls.restored); }
            row.put("after_restoration", chunk(chunk));
            row.put("input_after", capture(prefix + "-input-after", input, false));
            return row;
        }

        Object readRegion(String label) throws Exception {
            try (DataInputStream stream = (DataInputStream)call(label + "-stream", storage.regionRead, null,
                    storage.root.resolve("world").toFile(), 0, 0)) {
                if (stream == null) return null;
                return call(label + "-nbt", storage.read, null, stream);
            }
        }

        Object regionBranch(String prefix, String name, Object input) throws Exception {
            Map<String,Object> row = object("source", "actual retained ld/hb region loader/writer; direct API, no cache activation",
                "prepared_input", "original NBT writer to actual region stream in copied owned World only");
            try (DataOutputStream stream = (DataOutputStream)call(prefix + "-prepare-stream", storage.regionWrite, null,
                    storage.root.resolve("world").toFile(), 0, 0)) {
                require(stream != null, "actual prepared region output stream");
                call(prefix + "-prepare-nbt", storage.write, null, input, stream);
            }
            Object before = readRegion(prefix + "-disk-before");
            row.put("disk_before_load", before == null ? null : capture(prefix + "-disk-before", before, false));
            Object chunk;
            try { chunk = call(prefix + "-load", storage.load, storage.regionLoader, storage.world, 0, 0); }
            catch (ChunkStorageObservation.OriginalFailure failure) {
                row.put("load", throwable(failure.getCause()));
                row.put("partial_chunk", "inaccessible original local after throwing load; no fabricated object or fields");
                return row;
            }
            row.put("load", object("status", "returned", "value", reference(chunk)));
            row.put("after_load", chunk(chunk));
            if (chunk == null) return row;
            Controls controls = new Controls();
            try {
                row.put("preparation", prepare(name, chunk, controls));
                row.put("before_writer", chunk(chunk));
                try {
                    call(prefix + "-writer", storage.save, storage.regionLoader, storage.world, chunk);
                    row.put("writer", object("status", "returned_void",
                        "caveat", "original writer catches Exception internally; a normal return does not prove a committed write"));
                } catch (ChunkStorageObservation.OriginalFailure failure) { row.put("writer", throwable(failure.getCause())); }
                row.put("after_writer", chunk(chunk));
                Object output = readRegion(prefix + "-disk-after");
                row.put("disk_after_writer", output == null ? null : capture(prefix + "-disk-after", output, false));
                try { row.put("region_reload", object("status", "returned", "chunk", chunk(call(prefix + "-reload", storage.load,
                    storage.regionLoader, storage.world, 0, 0)))); }
                catch (ChunkStorageObservation.OriginalFailure failure) { row.put("region_reload", throwable(failure.getCause())); }
            } finally { controls.close(); row.put("controls_restored", controls.restored); }
            row.put("after_restoration", chunk(chunk));
            return row;
        }

        Object run(Path output) throws Exception {
            Object baseline = storage.regionNBT();
            storage.savedBaseline = storage.artifact("saved-baseline", baseline);
            List<Object> cases = new ArrayList<>();
            for (int index = 0; index < CASES.length; ++index) {
                String name = CASES[index], prefix = String.format(java.util.Locale.ROOT, "%02d-%s", index, name);
                Object input = input(name, baseline);
                Map<String,Object> row = object("index", index, "name", name,
                    "native_boundary", name.equals("boat_after_item")
                        ? "known Boat constructor45 is unsupported in native Item-only factory; original outcome is independent and normally successful"
                        : "compare only the explicitly supported native source contract",
                    "context_before", context(), "input", capture(prefix + "-input", input, false));
                int firstOperation = operations.size();
                row.put("static", staticBranch(prefix + "-static", name, storage.cloneNBT(input)));
                row.put("context_after_static", context());
                row.put("region", regionBranch(prefix + "-region", name, storage.cloneNBT(input)));
                row.put("context_after_region", context());
                row.put("input_after", capture(prefix + "-input-after", input, false));
                row.put("context_after", context());
                row.put("direct_operations", new ArrayList<>(operations.subList(firstOperation, operations.size())));
                cases.add(row);
                // A later host failure cannot erase already completed finite case receipts.
                writeJSON(Path.of(output + ".case-" + index + ".json"), row, storage.json);
            }
            return cases;
        }
    }

    private static void writeJSON(Path output, Object record, Method json) throws Exception {
        byte[] bytes = (((String)ChunkStorageObservation.invoke(json, null, record)) + "\n").getBytes(StandardCharsets.UTF_8);
        require(bytes.length <= MAX_JSON, "bounded complete JSON output");
        Files.write(output, bytes, StandardOpenOption.CREATE_NEW);
    }

    private static void paths(Path jarArgument, Path rootArgument, Path jar, Path root, Path output) throws Exception {
        require(jarArgument.equals(jar) && rootArgument.equals(root), "canonical nonlinked archive and copied owned root");
        Path marker = root.resolve(".chunk-storage-owned");
        require(Files.isRegularFile(marker, LinkOption.NOFOLLOW_LINKS) && Files.size(marker) == 30 &&
            Files.readString(marker, StandardCharsets.US_ASCII).equals("fresh chunk storage reference\n"), "owned fresh copied baseline marker");
        try (var paths = Files.walk(root)) {
            List<Path> entries = paths.limit(257).toList();
            require(entries.size() <= 256, "bounded complete copied-world inventory");
            long total = 0;
            for (Path path : entries) {
                require(!Files.isSymbolicLink(path), "no linked copied-world entries");
                if (!Files.isDirectory(path, LinkOption.NOFOLLOW_LINKS)) {
                    require(Files.isRegularFile(path, LinkOption.NOFOLLOW_LINKS) && Files.size(path) <= MAX_JSON, "bounded copied-world file");
                    total += Files.size(path);
                }
            }
            require(total <= MAX_JSON, "bounded complete copied-world bytes");
        }
        require(!Files.isSymbolicLink(root) && Files.isDirectory(root.getParent()) &&
            !Files.exists(output, LinkOption.NOFOLLOW_LINKS) && Files.isDirectory(output.getParent()) &&
            output.getParent().equals(root.getParent()) && !output.startsWith(root), "fresh sibling output and helper-owned copy");
        require(!Files.exists(Path.of(output + ".nbt"), LinkOption.NOFOLLOW_LINKS) &&
            !Files.exists(Path.of(output + ".threads-before.json"), LinkOption.NOFOLLOW_LINKS) &&
            !Files.exists(Path.of(output + ".threads-after.json"), LinkOption.NOFOLLOW_LINKS), "fresh sidecar outputs");
        for (int index = 0; index < CASES.length; ++index)
            require(!Files.exists(Path.of(output + ".case-" + index + ".json"), LinkOption.NOFOLLOW_LINKS), "fresh per-case receipt");
    }

    public static void main(String[] args) {
        System.setOut(System.err);
        try {
            require(args.length == 4, "jar client|server copied_owned_world_root output_json");
            boolean client = args[1].equals("client");
            require(client || args[1].equals("server"), "paired side");
            Path jarArgument = Path.of(args[0]).toAbsolutePath().normalize();
            Path rootArgument = Path.of(args[2]).toAbsolutePath().normalize();
            Path jar = jarArgument.toRealPath(), root = rootArgument.toRealPath();
            Path output = Path.of(args[3]).toAbsolutePath().normalize();
            paths(jarArgument, rootArgument, jar, root, output);
            String expected = client ? ChunkStorageObservation.CLIENT : ChunkStorageObservation.SERVER;
            require(Files.size(jar) == (client ? 1465375 : 503100) &&
                ChunkStorageObservation.hash(Files.readAllBytes(jar)).equals(expected), "untouched pinned original archive");
            Path artifacts = Path.of(output + ".nbt");
            Files.createDirectory(artifacts);
            Set<String> originalNames = new HashSet<>();
            try (JarFile archive = new JarFile(jar.toFile())) {
                var entries = archive.entries();
                while (entries.hasMoreElements()) {
                    String name = entries.nextElement().getName();
                    if (name.endsWith(".class")) originalNames.add(name.substring(0, name.length() - 6).replace('/', '.'));
                }
            }
            URL url = jar.toUri().toURL();
            try (URLClassLoader loader = new URLClassLoader(new URL[] {url}, ClassLoader.getPlatformClassLoader())) {
                ChunkStorageObservation.Observer actual = new ChunkStorageObservation.Observer(url, loader, root, artifacts, client);
                Observer observer = new Observer(actual);
                Map<String,Object> record = object("schema", 1, "side", args[1], "jar_sha256", expected,
                    "scope", "finite untouched-original shared GAME Chunk Entity schema plus direct retained region loader/writer; actual copied World, no cache publication, activation, ticks or whole-world parity claim",
                    "setup_scope", "existing ChunkStorageObservation genuine constructor/Model setup; transitive class/World work is real and not a cold initialization or deterministic entropy claim",
                    "controls_scope", "prepared NBT; reversible mutable dead/nullable Stack fields only; repeated membership uses original Chunk.a; no static, final, ID, Random, phase, container or bytecode assignments",
                    "field_scope", "all nonstatic instance fields of each observed concrete Entity and original superclasses, bounds, Item Stack, watcher and watcher records; references retained by identity, Random internals and general reachable graphs excluded",
                    "operation_scope", "ordered explicit measured top-level calls; original nested calls and setup are not instrumented",
                    "prepared_chunk_coordinates", Arrays.asList(0, 0),
                    "baseline_template", "genuine saved (1,0) arrays from copied baseline; prepared header/records target (0,0)",
                    "provenance_policy", "EntityExtra/ItemExtra are prepared input only; actual original fresh semantic save discards them. Native preservation is a separate storage policy, not original semantic equivalence.",
                    "compile_sources", Arrays.asList("ChunkObservation.java", "ChunkStorageObservation.java", "GameEntityStorageObservation.java"),
                    "platform_parent", loader.getParent() == ClassLoader.getPlatformClassLoader(),
                    "source_frames", object("decode_factory", 269, "decode_q_store_after_normal_factory", 277,
                        "decode_adopt", 289, "chunk_adopt_q", 2, "chunk_adopt_append", 148,
                        "encode_q_false", 105, "encode_q_true", 167, "encode_entity_save", 183,
                        "encode_entities_put", 208, "entity_wrapper_write", 27, "base_item_hook", 145,
                        "item_stack_write", 37, "factory_read_outside_catch", 66,
                        "interpretation", "paired source map, not an instrumented execution-PC trace"),
                    "before", observer.context(), "registry_before", actual.registry());
                ChunkStorageObservation.threads(Path.of(output + ".threads-before.json"), originalNames);
                try {
                    record.put("cases", observer.run(output));
                    record.put("after", observer.context());
                    record.put("registry_after", actual.registry());
                } finally { actual.finish(); }
                ChunkStorageObservation.threads(Path.of(output + ".threads-after.json"), originalNames);
                require(Files.size(jar) == (client ? 1465375 : 503100) &&
                    ChunkStorageObservation.hash(Files.readAllBytes(jar)).equals(expected), "original archive preserved");
                record.put("class_origins", actual.origins);
                record.put("bindings", actual.bindings);
                record.put("fields", actual.fields);
                record.put("saved_baseline", actual.savedBaseline);
                record.put("case_count", CASES.length);
                record.put("direct_operation_count", observer.operationCount);
                record.put("terminal", "completed finite capture; semantic comparison is external");
                writeJSON(output, record, actual.json);
            }
        } catch (Throwable failure) {
            failure.printStackTrace(System.err);
            System.exit(1);
        }
    }
}
