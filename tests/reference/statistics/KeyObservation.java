import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/** Ordinary untouched original b initialization and complete actual key lookups. */
public final class KeyObservation {
    private static final int MAX_ENTRIES = 4096;
    private static final int MAX_UNITS = 65536;
    private static final int MAX_ROWS = 20000;
    private static final int MAX_OUTPUT = 4 * 1024 * 1024;
    private static final char[] HEX = "0123456789abcdef".toCharArray();

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static String units(String value) {
        require(value.length() <= MAX_UNITS, "String unit bound");
        if (value.isEmpty()) return "-";
        char[] result = new char[value.length() * 4];
        for (int index = 0; index < value.length(); index++) {
            int unit = value.charAt(index);
            for (int digit = 0; digit < 4; digit++) {
                result[4 * index + digit] = HEX[(unit >>> (12 - 4 * digit)) & 15];
            }
        }
        return new String(result);
    }

    private static final class Rows {
        final StringBuilder text = new StringBuilder();
        int count;

        void add(Object... columns) {
            require(++count <= MAX_ROWS, "row bound");
            for (int index = 0; index < columns.length; index++) {
                if (index != 0) text.append('\t');
                String column = String.valueOf(columns[index]);
                for (int offset = 0; offset < column.length(); offset++) {
                    char unit = column.charAt(offset);
                    require(unit >= 32 && unit <= 126, "ASCII output columns");
                }
                text.append(column);
            }
            text.append('\n');
            require(text.length() <= MAX_OUTPUT, "output byte bound");
        }
    }

    private static final class Strings {
        final IdentityHashMap<String, Integer> identities = new IdentityHashMap<>();
        final List<String> held = new ArrayList<>();

        String token(String value) {
            if (value == null) return "NULL";
            Integer known = identities.get(value);
            if (known == null) {
                require(held.size() < MAX_ENTRIES, "String identity bound");
                known = held.size() + 1;
                held.add(value);
                identities.put(value, known);
            }
            return "R:" + known;
        }
    }

    private static final class Cell {
        final int key;
        final String value;

        Cell(int key, String value) {
            this.key = key;
            this.value = value;
        }
    }

    private static String query(Method lookup, Map<?, ?> map, int key) throws Exception {
        Object observed = lookup.invoke(null, key);
        require(observed == null || observed instanceof String, "lookup return type");
        require(observed == map.get(Integer.valueOf(key)), "lookup/map reference identity");
        return (String)observed;
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 1) {
            throw new IllegalArgumentException("Usage: KeyObservation original.jar");
        }
        URL jar = Path.of(arguments[0]).toRealPath().toUri().toURL();
        Rows out = new Rows();
        try (URLClassLoader loader = new URLClassLoader(new URL[] {jar}, null)) {
            // This is normal original initialization: no substitute classes/resources.
            Class<?> type = Class.forName("b", true, loader);
            require(type.getClassLoader() == loader
                    && type.getProtectionDomain().getCodeSource() != null
                    && type.getProtectionDomain().getCodeSource().getLocation().equals(jar),
                    "exact supplied-JAR b origin");
            require(type.getModifiers() == Modifier.PUBLIC, "original public b class");
            Field singletonField = type.getDeclaredField("a");
            require(singletonField.getType() == type
                    && singletonField.getModifiers() == (Modifier.PUBLIC | Modifier.STATIC),
                    "original singleton field descriptor");
            Field mapField = type.getDeclaredField("b");
            require(mapField.getType() == Map.class && mapField.getModifiers() == Modifier.PRIVATE,
                    "original map field descriptor");
            mapField.setAccessible(true);
            Object singleton = singletonField.get(null);
            require(singleton != null && singleton.getClass() == type, "published singleton");
            Object stored = mapField.get(singleton);
            require(stored != null && stored.getClass() == HashMap.class, "actual HashMap owner");
            Map<?, ?> map = (Map<?, ?>)stored;
            require(map.size() <= MAX_ENTRIES, "complete map bound");
            Method lookup = type.getDeclaredMethod("a", int.class);
            require(lookup.getReturnType() == String.class
                    && lookup.getModifiers() == (Modifier.PUBLIC | Modifier.STATIC),
                    "original static lookup descriptor");
            List<Cell> cells = new ArrayList<>();
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                require(entry.getKey() instanceof Integer && entry.getValue() instanceof String,
                        "actual map Integer/String entry types");
                cells.add(new Cell(((Integer)entry.getKey()).intValue(), (String)entry.getValue()));
                require(cells.size() <= MAX_ENTRIES, "entry enumeration bound");
            }
            require(cells.size() == map.size(), "complete map enumeration");
            // Diagnostic ordering only: this is not a HashMap traversal observation.
            cells.sort(Comparator.comparingInt((Cell cell) -> cell.key));
            Strings strings = new Strings();
            out.add("KEY_MAP_V1");
            out.add("INITIALIZATION", "COMPLETE");
            out.add("ORDER", "SIGNED_INT_ASCENDING");
            out.add("MAP", cells.size());
            out.add("SINGLETON", 1);
            boolean first = true;
            int priorKey = 0;
            for (Cell cell : cells) {
                require(first || priorKey < cell.key, "unique sorted integer keys");
                first = false;
                priorKey = cell.key;
                out.add("ENTRY", cell.key, strings.token(cell.value));
            }
            int queryCount = 0;
            for (Cell cell : cells) {
                String observed = query(lookup, map, cell.key);
                require(observed == cell.value, "snapshot/lookup reference identity");
                out.add("QUERY", cell.key, strings.token(observed));
                queryCount++;
            }
            // Explicit ordinary lookup inputs; report actual presence or absence.
            for (int key : new int[] {Integer.MIN_VALUE, Integer.MAX_VALUE, 0, -1, 1,
                    32767, -32768}) {
                out.add("QUERY", key, strings.token(query(lookup, map, key)));
                queryCount++;
            }
            require(singletonField.get(null) == singleton && mapField.get(singleton) == map,
                    "same singleton/map after all actual lookups");
            require(map.size() == cells.size(), "map count retained after lookups");
            for (Cell cell : cells) {
                require(map.get(Integer.valueOf(cell.key)) == cell.value,
                        "map entry identity retained after lookups");
            }
            out.add("STRINGS", strings.held.size());
            for (int index = 0; index < strings.held.size(); index++) {
                String value = strings.held.get(index);
                out.add("STRING", index + 1, value.length(), units(value));
            }
            out.add("SUMMARY", "PASS");
            out.add("END", cells.size(), queryCount, strings.held.size(), out.count);
        }
        // A failed original call, descriptor check, bound or loader close emits no protocol.
        System.out.write(out.text.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
        if (System.out.checkError()) throw new java.io.IOException("key observation output failed");
    }
}
