import java.io.PrintWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HashMap;
import java.util.HexFormat;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.Map;

/** Original WorldInfo object graphs only; sampled output LastPlayed is not a fixture expectation. */
public final class WorldInfoObservation {
    private static final String SELECTORS = "abcdefghijklmno";
    private static final char[] FIELD_TYPES = {'J','I','I','I','J','J','J','C','I','S','I','Z','I','Z','I'};
    private static final String[] KEYS = {"RandomSeed", "SpawnX", "SpawnY", "SpawnZ", "Time",
            "LastPlayed", "SizeOnDisk", "LevelName", "version", "rainTime", "raining",
            "thunderTime", "thundering", "Player"};
    private static final char[] TAG_TYPES = {'J','I','I','I','J','J','J','S','I','I','B','I','B','C'};
    private static final IdentityHashMap<Object, Integer> IDS = new IdentityHashMap<>();
    private static final IdentityHashMap<Object, Boolean> SERIAL_ROOTS = new IdentityHashMap<>();

    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static int identity(Object object) {
        if (object == null) return 0;
        Integer present = IDS.get(object);
        if (present != null) return present;
        int next = IDS.size() + 1;
        IDS.put(object, next); // Keep every reported identity strongly live through END.
        return next;
    }

    private static String hex(long value, int width) {
        if (width < 16) value &= (1L << (width * 4)) - 1;
        String digits = Long.toHexString(value);
        return "0".repeat(width - digits.length()) + digits;
    }

    private static String units(String value) {
        if (value == null || value.isEmpty()) return "-";
        require(value.length() <= 64, "bounded UTF-16 observation");
        StringBuilder out = new StringBuilder();
        for (int index = 0; index < value.length(); ++index) out.append(hex(value.charAt(index), 4));
        return out.toString();
    }

    private static void row(StringBuilder out, Object... columns) {
        for (int index = 0; index < columns.length; ++index) {
            if (index != 0) out.append('\t');
            out.append(columns[index]);
        }
        out.append('\n');
        require(out.length() <= 131072, "bounded complete output");
    }

    private static Class<?> original(URLClassLoader loader, URL jar, String name) throws Exception {
        Class<?> result = Class.forName(name, false, loader);
        require(result.getClassLoader() == loader
                && result.getProtectionDomain().getCodeSource().getLocation().equals(jar),
                "exact supplied-JAR class origin: " + name);
        return result;
    }

    private static Method method(Class<?> owner, String name, Class<?> result,
            Class<?>... parameters) throws Exception {
        Method method = owner.getDeclaredMethod(name, parameters);
        require(method.getModifiers() == Modifier.PUBLIC && method.getReturnType() == result,
                "public original method descriptor: " + name);
        method.setAccessible(true);
        return method;
    }

    private static Constructor<?> constructor(Class<?> owner, Class<?>... parameters) throws Exception {
        Constructor<?> result = owner.getDeclaredConstructor(parameters);
        require(result.getModifiers() == Modifier.PUBLIC, "public original constructor");
        result.setAccessible(true);
        return result;
    }

    private static final class Binding {
        final boolean client;
        final Class<?> info, compound, base, byteTag, intTag, longTag, stringTag;
        final Constructor<?> fresh, loaded, copy, newCompound;
        final Method putInt, putLong, putByte, putString, putCompound, nameTag, readName;
        final Method rename, time, size, spawn, version, rainTime, raining, thunderTime, thundering;
        final Method setPlayer, serialize, tagType, getString;
        final Field compoundMap;
        final Map<Character, Field> fields = new LinkedHashMap<>();
        final Map<Character, Method> getters = new LinkedHashMap<>();
        int snapshots, serializations, refusedSerializations;

        Binding(URLClassLoader loader, URL jar, boolean isClient) throws Exception {
            client = isClient;
            info = original(loader, jar, client ? "ei" : "ct");
            compound = original(loader, jar, client ? "nu" : "iq");
            base = original(loader, jar, client ? "ij" : "fo");
            byteTag = original(loader, jar, client ? "qp" : "kb");
            intTag = original(loader, jar, client ? "pp" : "jq");
            longTag = original(loader, jar, client ? "mi" : "hv");
            stringTag = original(loader, jar, client ? "xb" : "ou");
            require(info.getSuperclass() == Object.class && compound.getSuperclass() == base,
                    "actual Info/NBT inheritance");
            fresh = constructor(info, long.class, String.class);
            loaded = constructor(info, compound);
            copy = constructor(info, info);
            newCompound = constructor(compound);
            for (int index = 0; index < SELECTORS.length(); ++index) {
                char selector = SELECTORS.charAt(index);
                Field field = info.getDeclaredField(String.valueOf(selector));
                char tag = FIELD_TYPES[index];
                Class<?> expected = tag == 'J' ? long.class : tag == 'I' ? int.class
                        : tag == 'Z' ? boolean.class : tag == 'S' ? String.class : compound;
                require(field.getModifiers() == Modifier.PRIVATE && field.getType() == expected,
                        "exact fifteen-field declaration");
                field.setAccessible(true);
                fields.put(selector, field);
            }
            require(info.getDeclaredFields().length == 15, "complete Info fields");
            compoundMap = compound.getDeclaredField("a");
            require(compoundMap.getModifiers() == Modifier.PRIVATE
                    && compoundMap.getType() == Map.class, "actual compound map field");
            compoundMap.setAccessible(true);
            putInt = method(compound, "a", void.class, String.class, int.class);
            putLong = method(compound, "a", void.class, String.class, long.class);
            putByte = method(compound, "a", void.class, String.class, byte.class);
            putString = method(compound, "a", void.class, String.class, String.class);
            putCompound = method(compound, "a", void.class, String.class, compound);
            nameTag = method(base, "a", base, String.class);
            readName = method(base, "b", String.class);
            getString = method(compound, "i", String.class, String.class);
            tagType = base.getDeclaredMethod("a");
            require(Modifier.isPublic(tagType.getModifiers()) && Modifier.isAbstract(tagType.getModifiers())
                    && tagType.getReturnType() == byte.class, "abstract actual NBT type method");
            tagType.setAccessible(true);
            rename = method(info, "a", void.class, String.class);
            time = method(info, "a", void.class, long.class);
            size = method(info, "b", void.class, long.class);
            spawn = method(info, "a", void.class, int.class, int.class, int.class);
            version = method(info, client ? "d" : "a", void.class, int.class);
            rainTime = method(info, client ? "f" : "c", void.class, int.class);
            raining = method(info, "b", void.class, boolean.class);
            thunderTime = method(info, client ? "e" : "b", void.class, int.class);
            thundering = method(info, "a", void.class, boolean.class);
            serialize = method(info, "a", compound);
            if (client) setPlayer = method(info, "a", void.class, compound);
            else {
                Method absent = null;
                try { absent = info.getDeclaredMethod("a", compound); }
                catch (NoSuchMethodException expected) { /* This client method is absent on server. */ }
                require(absent == null, "server Player setter must be absent");
                setPlayer = null;
            }
            getter('a', "b", long.class); getter('b', "c", int.class);
            getter('c', "d", int.class); getter('d', "e", int.class);
            getter('e', "f", long.class); getter('g', "g", long.class);
            getter('i', client ? "i" : "h", int.class);
            getter('k', client ? "k" : "i", int.class);
            getter('l', client ? "o" : "l", boolean.class);
            getter('m', client ? "p" : "m", int.class);
            getter('n', client ? "m" : "j", boolean.class);
            getter('o', client ? "n" : "k", int.class);
            if (client) {
                getter('f', "l", long.class); getter('h', "h", compound);
                getter('j', "j", String.class);
            }
        }

        void getter(char field, String name, Class<?> result) throws Exception {
            getters.put(field, method(info, name, result));
        }

        Object value(Object object, char selector) throws Exception { return fields.get(selector).get(object); }

        Map<?, ?> map(Object object) throws Exception {
            require(object != null && object.getClass() == compound, "actual NBT compound identity");
            Object map = compoundMap.get(object);
            require(map.getClass() == HashMap.class, "ordinary original compound HashMap");
            return (Map<?, ?>)map;
        }

        Object tagValue(Object tag, char kind) throws Exception {
            Class<?> expected = kind == 'J' ? longTag : kind == 'I' ? intTag
                    : kind == 'B' ? byteTag : stringTag;
            require(tag != null && tag.getClass() == expected, "actual scalar tag class");
            int type = kind == 'J' ? 4 : kind == 'I' ? 3 : kind == 'B' ? 1 : 8;
            require((Byte)tagType.invoke(tag) == type, "actual scalar tag type");
            Field member = expected.getDeclaredField("a");
            Class<?> descriptor = kind == 'J' ? long.class : kind == 'I' ? int.class
                    : kind == 'B' ? byte.class : String.class;
            require(member.getType() == descriptor && !Modifier.isStatic(member.getModifiers()),
                    "actual scalar payload descriptor");
            member.setAccessible(true);
            return member.get(tag);
        }

        void text(StringBuilder out, String kind, int step, String text) {
            row(out, kind, step, identity(text), text == null ? -1 : text.length(), units(text));
        }

        void player(StringBuilder out, int step, Object player) throws Exception {
            if (player == null) { row(out, "PLAYER", step, 0, 0, -1, "-", 0, "-", 0, "-"); return; }
            Map<?, ?> members = map(player);
            require(members.size() == 2 && members.containsKey("Dimension") && members.containsKey("Keep"),
                    "finite controlled Player children");
            Object dimension = members.get("Dimension"), keep = members.get("Keep");
            String name = (String)readName.invoke(player);
            row(out, "PLAYER", step, identity(player), identity(name), name.length(), units(name),
                    identity(dimension), hex((Integer)tagValue(dimension, 'I'), 8),
                    identity(keep), hex((Integer)tagValue(keep, 'I'), 8));
        }

        void snapshot(StringBuilder out, int step, Object object) throws Exception {
            require(object.getClass() == info, "actual Info class");
            ++snapshots;
            row(out, "SNAPSHOT", step, identity(object));
            for (int index = 0; index < SELECTORS.length(); ++index) {
                char selector = SELECTORS.charAt(index), kind = FIELD_TYPES[index];
                Object actual = value(object, selector);
                Method getter = getters.get(selector);
                if (getter != null) {
                    Object returned = getter.invoke(object);
                    require(kind == 'S' || kind == 'C' ? returned == actual : returned.equals(actual),
                            "getter/field identity or value");
                }
                Object encoded = kind == 'J' ? hex((Long)actual, 16)
                        : kind == 'I' ? hex((Integer)actual, 8)
                        : kind == 'Z' ? ((Boolean)actual ? "1" : "0") : identity(actual);
                row(out, "FIELD", step, selector, kind, encoded);
            }
            text(out, "NAME", step, (String)value(object, 'j'));
            player(out, step, value(object, 'h'));
        }

        void serialize(StringBuilder out, int serial, Object object) throws Exception {
            Object[] before = new Object[15];
            for (int index = 0; index < before.length; ++index) before[index] = value(object, SELECTORS.charAt(index));
            Object result = serialize.invoke(object);
            require(result.getClass() == compound && SERIAL_ROOTS.put(result, Boolean.TRUE) == null,
                    "fresh actual serializer result");
            Map<?, ?> members = map(result);
            Object player = value(object, 'h');
            require(members.size() == (player == null ? 13 : 14)
                    && members.get("Player") == player && result != player,
                    "serializer retained same optional Player");
            row(out, "SERIAL", serial, identity(object), identity(result), members.size());
            for (int index = 0; index < KEYS.length; ++index) {
                char kind = TAG_TYPES[index];
                Object tag = members.get(KEYS[index]);
                if (kind == 'C') {
                    row(out, "SERIAL_FIELD", serial, index, 10, identity(tag));
                    if (tag != null) require(readName.invoke(tag).equals("Player"), "same Player renamed by insertion");
                    continue;
                }
                Object actual = tagValue(tag, kind);
                if (index == 5) {
                    // Retain the actual sample separately; no expected-time equality or helper clock read.
                    row(out, "SERIAL_FIELD", serial, index, 4, "SAMPLED");
                    row(out, "CLOCK_OUTPUT", serial, hex((Long)actual, 16));
                } else if (kind == 'S') {
                    row(out, "SERIAL_FIELD", serial, index, 8, identity(actual));
                    text(out, "SERIAL_NAME", serial, (String)actual);
                } else {
                    row(out, "SERIAL_FIELD", serial, index, kind == 'J' ? 4 : kind == 'I' ? 3 : 1,
                            hex(((Number)actual).longValue(), kind == 'J' ? 16 : kind == 'I' ? 8 : 2));
                }
            }
            for (int index = 0; index < before.length; ++index) {
                Object after = value(object, SELECTORS.charAt(index));
                char kind = FIELD_TYPES[index];
                require(kind == 'C' || kind == 'S' ? after == before[index] : after.equals(before[index]),
                        "serialization changed live Info fields including LastPlayed");
            }
            ++serializations;
        }

        void refuseNullName(StringBuilder out, int serial, Object object) throws Exception {
            require(value(object, 'j') == null, "actual null-name serialization input");
            Object[] before = new Object[15];
            for (int index = 0; index < before.length; ++index) before[index] = value(object, SELECTORS.charAt(index));
            Object player = value(object, 'h');
            require(player != null, "nonnull Player distinguishes failure before insertion");
            String playerName = (String)readName.invoke(player);
            require(playerName.equals("BeforeNull"), "controlled pre-failure Player name");
            Object result = null;
            Throwable cause = null;
            try { result = serialize.invoke(object); }
            catch (InvocationTargetException failure) { cause = failure.getCause(); }
            boolean exactCause = cause != null && cause.getClass() == IllegalArgumentException.class
                    && "Empty string not allowed".equals(cause.getMessage());
            boolean unchanged = true;
            for (int index = 0; index < before.length; ++index) {
                Object after = value(object, SELECTORS.charAt(index));
                char kind = FIELD_TYPES[index];
                unchanged &= kind == 'C' || kind == 'S' ? after == before[index] : after.equals(before[index]);
            }
            boolean playerUnchanged = value(object, 'h') == player && readName.invoke(player) == playerName;
            boolean noReturnedData = result == null;
            require(exactCause && unchanged && playerUnchanged && noReturnedData,
                    "original null-name failure leaves all Info fields and Player name unchanged, no returned Data");
            row(out, "EXCEPTION", serial, identity(object), cause.getClass().getName(),
                    exactCause ? 1 : 0, unchanged ? 1 : 0, playerUnchanged ? 1 : 0, noReturnedData ? 1 : 0);
            ++refusedSerializations;
        }

        Object player(int dimension, int marker, String name) throws Exception {
            Object player = newCompound.newInstance();
            putInt.invoke(player, "Dimension", dimension);
            putInt.invoke(player, "Keep", marker);
            require(nameTag.invoke(player, name) == player, "same tag after name setter");
            return player;
        }
    }

    public static void main(String[] args) throws Exception {
        require(args.length == 2 && (args[0].equals("client") || args[0].equals("server")),
                "Usage: WorldInfoObservation client|server jar");
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        require(Files.size(jar) == (client ? 1465375 : 503100), "pinned original size");
        require(HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)))
                .equals(expected), "pinned original JAR");
        URL url = jar.toUri().toURL();
        StringBuilder out = new StringBuilder();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url}, ClassLoader.getPlatformClassLoader())) {
            Binding b = new Binding(loader, url, client);
            row(out, "WORLD_INFO_OBSERVATION_V1", args[0]);
            row(out, "POLICY", "OBJECT_GRAPH_ONLY", "EXCLUDE_ONLY_CLOCK_OUTPUT_FROM_STABLE_FIXTURE");
            row(out, "CAPABILITY", "CLIENT_PLAYER_SET", client ? 1 : 0);
            String nameA = new String(new char[] {'A', 0, '\u03a9', '\ud800'});
            String nameB = new String(new char[] {'A', 0, '\u03a9', '\ud800'});
            require(nameA != nameB && nameA.equals(nameB), "equal text with distinct name identities");
            Object fresh = b.fresh.newInstance(Long.MIN_VALUE + 12345L, nameA);
            Object nullName = b.fresh.newInstance(Long.MAX_VALUE, null);
            require(b.value(fresh, 'j') == nameA && b.value(nullName, 'j') == null, "fresh nullable exact name");
            b.snapshot(out, 0, fresh); b.snapshot(out, 1, nullName);
            Object data = b.newCompound.newInstance();
            b.putLong.invoke(data, "RandomSeed", 0xfedcba9876543210L);
            b.putInt.invoke(data, "SpawnX", Integer.MIN_VALUE); b.putInt.invoke(data, "SpawnY", 257);
            b.putInt.invoke(data, "SpawnZ", Integer.MAX_VALUE);
            b.putLong.invoke(data, "Time", 0x0102030405060708L);
            b.putLong.invoke(data, "LastPlayed", 0x1112131415161718L);
            b.putLong.invoke(data, "SizeOnDisk", -17L);
            b.putString.invoke(data, "LevelName", nameA); b.putInt.invoke(data, "version", 0x76543210);
            b.putInt.invoke(data, "rainTime", -7); b.putByte.invoke(data, "raining", (byte)0x80);
            b.putInt.invoke(data, "thunderTime", Integer.MAX_VALUE); b.putByte.invoke(data, "thundering", (byte)7);
            Object originalPlayer = b.player(-1, 0x12345678, "Initial");
            b.putCompound.invoke(data, "Player", originalPlayer);
            Object loaded = b.loaded.newInstance(data), copy = b.copy.newInstance(loaded);
            require(loaded != copy && b.value(loaded, 'h') == originalPlayer
                    && b.value(copy, 'h') == originalPlayer && b.value(loaded, 'j') == nameA
                    && b.value(copy, 'j') == nameA, "actual loaded/copy name and Player aliases");
            b.snapshot(out, 2, loaded); b.snapshot(out, 3, copy);
            b.time.invoke(copy, Long.MIN_VALUE + 1); b.size.invoke(copy, Long.MAX_VALUE);
            b.spawn.invoke(copy, -3, -64, 257); b.version.invoke(copy, 7);
            b.rainTime.invoke(copy, 17); b.raining.invoke(copy, false);
            b.thunderTime.invoke(copy, 19); b.thundering.invoke(copy, false);
            require((Long)b.value(loaded, 'e') == 0x0102030405060708L
                    && (Long)b.value(loaded, 'g') == -17L && (Integer)b.value(loaded, 'b') == Integer.MIN_VALUE
                    && (Integer)b.value(loaded, 'k') == 0x76543210 && (Boolean)b.value(loaded, 'l')
                    && (Boolean)b.value(loaded, 'n'), "copy scalar edits did not affect source");
            b.snapshot(out, 4, copy); b.snapshot(out, 5, loaded);
            b.rename.invoke(copy, nameB);
            require(b.value(copy, 'j') == nameB && b.value(loaded, 'j') == nameA, "rename is reference replacement");
            b.snapshot(out, 6, copy);
            b.rename.invoke(copy, new Object[] {null}); b.snapshot(out, 7, copy);
            b.rename.invoke(copy, nameB);
            b.nameTag.invoke(originalPlayer, "Before"); b.snapshot(out, 8, copy);
            b.serialize(out, 0, copy); b.snapshot(out, 9, copy); b.snapshot(out, 10, loaded);
            if (client) {
                Object replacement = b.player(9, 0x6543210, "Replacement");
                b.setPlayer.invoke(copy, replacement);
                require(b.value(copy, 'h') == replacement && b.value(loaded, 'h') == originalPlayer
                        && (Integer)b.value(copy, 'i') == -1, "Player replacement leaves cached dimension stale");
                b.snapshot(out, 11, copy); b.serialize(out, 1, copy); b.snapshot(out, 12, copy);
                b.snapshot(out, 13, loaded);
                b.setPlayer.invoke(copy, new Object[] {null});
                require(b.value(copy, 'h') == null && (Integer)b.value(copy, 'i') == -1,
                        "null Player assignment retains cached dimension");
                b.snapshot(out, 14, copy); b.serialize(out, 2, copy);
            }
            Object emptyData = b.newCompound.newInstance(), empty = b.loaded.newInstance(emptyData);
            Object emptyCopy = b.copy.newInstance(empty);
            require(b.value(empty, 'j') != null && b.value(empty, 'j') == b.value(emptyCopy, 'j')
                    && b.value(empty, 'j') == b.getString.invoke(emptyData, "Missing")
                    && ((String)b.value(empty, 'j')).isEmpty(), "loaded missing name is shared empty, not null");
            b.snapshot(out, 15, empty); b.snapshot(out, 16, emptyCopy);
            b.serialize(out, 3, fresh); b.serialize(out, 4, empty);
            require(b.nameTag.invoke(originalPlayer, "BeforeNull") == originalPlayer, "same pre-failure Player");
            b.rename.invoke(loaded, new Object[] {null}); b.snapshot(out, 17, loaded);
            b.refuseNullName(out, 5, loaded); b.snapshot(out, 18, loaded);
            row(out, "END", b.snapshots, b.serializations, b.serializations, b.refusedSerializations);
        }
        require(Files.size(jar) == (client ? 1465375 : 503100)
                && HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)))
                        .equals(expected), "original JAR unchanged");
        PrintWriter writer = new PrintWriter(System.out, false, StandardCharsets.US_ASCII);
        writer.print(out); writer.flush(); require(!writer.checkError(), "output failure");
    }
}
