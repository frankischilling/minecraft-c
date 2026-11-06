import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/** Observes the common fields after ordinary untouched original Stats initialization. */
public final class RegistryObservation {
    private static final int COMMON = 23;
    private static final int MAX_UNITS = 65536;
    private static final int MAX_COLLECTION = 1000000;
    private static final int MAX_OUTPUT = 16 * 1024 * 1024;
    private static final char[] HEX = "0123456789abcdef".toCharArray();

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static void origin(Class<?> type, URLClassLoader loader, Path jar) throws Exception {
        require(type.getClassLoader() == loader, "supplied original class loader: " + type.getName());
        require(type.getProtectionDomain().getCodeSource() != null
                && Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                        .toRealPath().equals(jar),
                "supplied original JAR origin: " + type.getName());
    }

    private static Field field(Class<?> owner, String name, Class<?> type, int modifiers)
            throws Exception {
        Field result = owner.getDeclaredField(name);
        require(result.getType() == type && result.getModifiers() == modifiers,
                "original field descriptor/modifiers: " + owner.getName() + "." + name);
        result.setAccessible(true);
        return result;
    }

    private static String units(String value) {
        if (value == null) return "NULL";
        require(value.length() <= MAX_UNITS, "String unit bound");
        if (value.isEmpty()) return "-";
        char[] result = new char[value.length() * 4];
        for (int offset = 0; offset < value.length(); offset++) {
            int unit = value.charAt(offset);
            for (int digit = 0; digit < 4; digit++) {
                result[4 * offset + digit] = HEX[(unit >>> (12 - 4 * digit)) & 15];
            }
        }
        return new String(result);
    }

    private static int occurrences(List<?> list, Object wanted) {
        require(list.size() <= MAX_COLLECTION, "actual list inspection bound");
        int count = 0;
        for (Object value : list) {
            if (value == wanted) count++;
        }
        return count;
    }

    private static void row(StringBuilder output, Object... columns) {
        for (int index = 0; index < columns.length; index++) {
            if (index != 0) output.append('\t');
            String text = String.valueOf(columns[index]);
            for (int offset = 0; offset < text.length(); offset++) {
                char unit = text.charAt(offset);
                require(unit >= 32 && unit <= 126, "ASCII protocol column");
            }
            output.append(text);
        }
        output.append('\n');
        require(output.length() <= MAX_OUTPUT, "protocol output bound");
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2
                || !(arguments[0].equals("client") || arguments[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: RegistryObservation client|server original.jar");
        }
        boolean client = arguments[0].equals("client");
        Path jar = Path.of(arguments[1]).toRealPath();
        StringBuilder output = new StringBuilder();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar.toUri().toURL()},
                ClassLoader.getPlatformClassLoader())) {
            // Normal original initialization may print its own achievement output.
            // Leave that output untouched; the helper header follows initialization.
            Class<?> statistics = Class.forName(client ? "jl" : "gg", true, loader);
            Class<?> base = Class.forName(client ? "vr" : "ns", false, loader);
            Class<?> general = Class.forName(client ? "yd" : "pm", false, loader);
            Class<?> strategy = Class.forName(client ? "wk" : "oe", false, loader);
            origin(statistics, loader, jar);
            origin(base, loader, jar);
            origin(general, loader, jar);
            origin(strategy, loader, jar);
            require(statistics.getModifiers() == Modifier.PUBLIC
                    && base.getModifiers() == Modifier.PUBLIC
                    && general.getModifiers() == Modifier.PUBLIC
                    && general.getSuperclass() == base,
                    "actual Stats/Base/General declarations");
            require(strategy.isInterface(), "actual formatting interface");

            Field id = field(base, "e", int.class, Modifier.PUBLIC | Modifier.FINAL);
            Field name = field(base, "f", String.class, Modifier.PUBLIC | Modifier.FINAL);
            Field flag = field(base, "g", boolean.class, Modifier.PUBLIC);
            Field key = field(base, "h", String.class, Modifier.PUBLIC);
            Field formatting = field(base, "a", strategy, Modifier.PRIVATE | Modifier.FINAL);
            Object[] formatters = new Object[3];
            String[] formatterNames = {"i", "j", "k"};
            String[] formatterClasses = client
                    ? new String[] {"bq", "bo", "bn"} : new String[] {"ax", "av", "at"};
            IdentityHashMap<Object, Boolean> formatterIdentities = new IdentityHashMap<>();
            for (int index = 0; index < formatters.length; index++) {
                Object value = field(base, formatterNames[index], strategy,
                        Modifier.PUBLIC | Modifier.STATIC).get(null);
                require(value != null && formatterIdentities.put(value, Boolean.TRUE) == null
                        && value.getClass().getName().equals(formatterClasses[index])
                        && strategy.isInstance(value), "distinct actual formatter owner");
                origin(value.getClass(), loader, jar);
                formatters[index] = value;
            }

            Object storedMap = field(statistics, "a", Map.class,
                    Modifier.PROTECTED | Modifier.STATIC).get(null);
            Object storedAll = field(statistics, "b", List.class,
                    Modifier.PUBLIC | Modifier.STATIC).get(null);
            Object storedGeneral = field(statistics, "c", List.class,
                    Modifier.PUBLIC | Modifier.STATIC).get(null);
            require(storedMap != null && storedMap.getClass() == HashMap.class
                    && storedAll != null && storedAll.getClass() == ArrayList.class
                    && storedGeneral != null && storedGeneral.getClass() == ArrayList.class
                    && storedAll != storedGeneral, "actual map and distinct list owners");
            Map<?, ?> map = (Map<?, ?>)storedMap;
            List<?> all = (List<?>)storedAll;
            List<?> generalList = (List<?>)storedGeneral;
            require(map.size() <= MAX_COLLECTION, "actual map inspection bound");
            IdentityHashMap<Object, Boolean> identities = new IdentityHashMap<>();
            Object[] held = new Object[COMMON];
            row(output, "STATS_COMMON_V1", arguments[0]);
            for (int index = 0; index < COMMON; index++) {
                String fieldName = String.valueOf((char)(index < 21 ? 'f' + index : 'A' + index - 21));
                Object statistic = field(statistics, fieldName, base,
                        Modifier.PUBLIC | Modifier.STATIC).get(null);
                require(statistic != null && statistic.getClass() == general
                        && identities.put(statistic, Boolean.TRUE) == null,
                        "distinct actual common General statistic");
                held[index] = statistic;
                int numeric = id.getInt(statistic);
                require(map.get(Integer.valueOf(numeric)) == statistic
                        && occurrences(all, statistic) == 1
                        && occurrences(generalList, statistic) == 1,
                        "common object identity in actual map and both actual lists");
                Object selected = formatting.get(statistic);
                int formatter = -1;
                for (int candidate = 0; candidate < formatters.length; candidate++) {
                    if (selected == formatters[candidate]) formatter = candidate;
                }
                require(formatter >= 0, "actual common formatter reference");
                row(output, "STAT", index, numeric, units((String)name.get(statistic)),
                        flag.getBoolean(statistic) ? 1 : 0, units((String)key.get(statistic)),
                        formatterNames[formatter]);
            }
            require(held.length == identities.size(), "all common owners retained");
            row(output, "END", COMMON);
        }
        // A failed original initializer or validation emits no helper protocol.
        System.out.write(output.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
        if (System.out.checkError()) throw new java.io.IOException("statistic observation output failed");
    }
}
