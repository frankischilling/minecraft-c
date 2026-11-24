import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;

/** Ordinary original translation initialization; no substitute game classes. */
public final class TranslationObservation {
    private static final char[] HEX = "0123456789abcdef".toCharArray();
    private static final StringBuilder OUTPUT = new StringBuilder();
    private static int rows;

    private static void row(Object... columns) {
        if (++rows > 20000) throw new IllegalStateException("row bound");
        for (int i = 0; i < columns.length; i++) {
            if (i != 0) OUTPUT.append('\t');
            OUTPUT.append(columns[i]);
        }
        OUTPUT.append('\n');
        if (OUTPUT.length() > 4 * 1024 * 1024) throw new IllegalStateException("output bound");
    }

    private static String units(String value) {
        if (value.length() > 65536) throw new IllegalStateException("String bound");
        if (value.isEmpty()) return "-";
        char[] encoded = new char[value.length() * 4];
        for (int i = 0; i < value.length(); i++)
            for (int digit = 0; digit < 4; digit++)
                encoded[4 * i + digit] = HEX[(value.charAt(i) >>> (12 - digit * 4)) & 15];
        return new String(encoded);
    }

    private static Object field(Class<?> type, String name, Object receiver) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(receiver);
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2 || !(arguments[0].equals("client") || arguments[0].equals("server")))
            throw new IllegalArgumentException("side jar");
        boolean client = arguments[0].equals("client");
        URL jar = Path.of(arguments[1]).toAbsolutePath().toUri().toURL();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, ClassLoader.getPlatformClassLoader())) {
            // Initialization is ordinary active use and must return normally.
            Class<?> wrapper = Class.forName(client ? "do" : "ch", true, loader);
            Class<?> singletonClass = Class.forName(client ? "nh" : "ii", true, loader);
            Object singleton = field(singletonClass, "a", null);
            Object wrapped = field(wrapper, "a", null);
            if (singleton == null || singleton != wrapped) throw new IllegalStateException("singleton publication");
            Properties properties = (Properties)field(singletonClass, "b", singleton);
            if (properties == null || properties.size() > 4096) throw new IllegalStateException("Properties bound");
            IdentityHashMap<String, Integer> ids = new IdentityHashMap<>();
            List<String> held = new ArrayList<>();
            java.util.function.Function<String, Integer> id = value -> {
                Integer known = ids.get(value);
                if (known == null) {
                    known = held.size() + 1;
                    held.add(value);
                    ids.put(value, known);
                }
                return known;
            };
            List<Map.Entry<Object, Object>> entries = new ArrayList<>(properties.entrySet());
            entries.sort(Comparator.comparing(entry -> (String)entry.getKey()));
            row("TRANSLATION_PROPERTIES_V1");
            row("MAP", entries.size());
            row("SINGLETON", 1);
            for (Map.Entry<Object, Object> entry : entries)
                row("ENTRY", id.apply((String)entry.getKey()), id.apply((String)entry.getValue()));
            Method lookup = wrapper.getDeclaredMethod("a", String.class);
            lookup.setAccessible(true);
            for (String specification : new String[] {"gui.done", "tile.stone.name", "stat.startGame", "minecraft-c.absent.translation.key", ""}) {
                String key = new String(specification);
                String value = (String)lookup.invoke(null, key);
                if (value == null) throw new IllegalStateException("unexpected null result");
                row("QUERY", key.length(), units(key), id.apply(value), value == key ? 1 : 0);
            }
            row("STRINGS", held.size());
            for (int i = 0; i < held.size(); i++)
                row("STRING", i + 1, held.get(i).length(), units(held.get(i)));
            row("END", entries.size(), held.size(), rows);
        }
        System.out.write(OUTPUT.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
        if (System.out.checkError()) throw new java.io.IOException("output failed");
    }
}
