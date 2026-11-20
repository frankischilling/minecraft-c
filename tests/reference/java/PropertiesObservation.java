import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Properties;

/** Finite actual JDK byte-stream loads and String-only property observations. */
public final class PropertiesObservation {
    private static final int MAX_INPUT = 262144;
    private static final int MAX_STRING = 65536;
    private static final int MAX_ROWS = 65536;
    private static final int MAX_OUTPUT = 16 * 1024 * 1024;
    private static final char[] HEX = "0123456789abcdef".toCharArray();
    private static final String BS = Character.toString((char)92);

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static String units(String value) {
        if (value == null) return "NULL";
        require(value.length() <= MAX_STRING, "String unit bound");
        if (value.isEmpty()) return "-";
        char[] result = new char[value.length() * 4];
        for (int i = 0; i < value.length(); i++) {
            int unit = value.charAt(i);
            for (int digit = 0; digit < 4; digit++) {
                result[4 * i + digit] = HEX[(unit >>> (12 - 4 * digit)) & 15];
            }
        }
        return new String(result);
    }

    private static String hex(byte[] value) {
        if (value.length == 0) return "-";
        char[] result = new char[value.length * 2];
        for (int i = 0; i < value.length; i++) {
            int octet = value[i] & 255;
            result[2 * i] = HEX[octet >>> 4];
            result[2 * i + 1] = HEX[octet & 15];
        }
        return new String(result);
    }

    private static byte[] bytes(String value) {
        require(value.length() <= MAX_INPUT, "input byte bound");
        byte[] result = new byte[value.length()];
        for (int i = 0; i < value.length(); i++) {
            require(value.charAt(i) <= 255, "explicit Latin1 input units required");
            result[i] = (byte)value.charAt(i);
        }
        return result;
    }

    private static String escape(String tail) { return BS + tail; }

    private static final class Rows {
        final StringBuilder text = new StringBuilder();
        int count;
        void add(Object... fields) {
            require(++count <= MAX_ROWS, "output row bound");
            for (int i = 0; i < fields.length; i++) {
                if (i != 0) text.append('\t');
                String field = String.valueOf(fields[i]);
                for (int j = 0; j < field.length(); j++) {
                    char unit = field.charAt(j);
                    require(unit >= 32 && unit <= 126, "ASCII output columns");
                }
                text.append(field);
            }
            text.append('\n');
            require(text.length() <= MAX_OUTPUT, "output byte bound");
        }
    }

    private static final class FiniteStream extends InputStream {
        final byte[] input;
        final int chunk, failAfter;
        int consumed, singleCalls, bulkCalls, closeCalls;
        boolean closed;

        FiniteStream(byte[] input, int chunk, int failAfter) {
            require(input.length <= MAX_INPUT && chunk > 0 && failAfter >= -1
                    && failAfter <= input.length, "finite stream inputs");
            this.input = input;
            this.chunk = chunk;
            this.failAfter = failAfter;
        }

        private void admit() throws IOException {
            if (closed) throw new IOException("properties-observer stream is closed");
            // The declared fault precedes EOF, including failAfter==input.length.
            if (failAfter >= 0 && consumed >= failAfter) {
                throw new IOException("properties-observer read failure at byte " + failAfter);
            }
        }

        @Override public int read() throws IOException {
            singleCalls++;
            admit();
            if (consumed == input.length) return -1;
            return input[consumed++] & 255;
        }

        @Override public int read(byte[] target, int offset, int length) throws IOException {
            bulkCalls++;
            Objects.checkFromIndexSize(offset, length, target.length);
            if (length == 0) return 0;
            admit();
            if (consumed == input.length) return -1;
            int count = Math.min(Math.min(length, chunk), input.length - consumed);
            if (failAfter >= 0) count = Math.min(count, failAfter - consumed);
            require(count > 0, "positive finite-stream read");
            System.arraycopy(input, consumed, target, offset, count);
            consumed += count;
            return count;
        }

        @Override public void close() {
            closeCalls++;
            closed = true;
        }
    }

    private static final class Query {
        final String key, fallback, mode;
        Query(String key, String mode, String fallback) {
            require(key != null && (mode.equals("ONE") || mode.equals("TWO_NULL")
                    || mode.equals("TWO_VALUE")), "query mode/key");
            require(mode.equals("TWO_VALUE") == (fallback != null), "query fallback presence");
            // These are explicit fresh API inputs, never chosen from map output.
            this.key = new String(key);
            this.mode = mode;
            this.fallback = fallback == null ? null : new String(fallback);
        }
    }

    private static Query one(String key) { return new Query(key, "ONE", null); }
    private static Query nullable(String key) { return new Query(key, "TWO_NULL", null); }
    private static Query fallback(String key, String value) {
        return new Query(key, "TWO_VALUE", value);
    }

    private static final class Step {
        final byte[] input;
        final int failAfter;
        final Query[] queries;
        Step(String input, int failAfter, Query... queries) {
            this.input = bytes(input);
            require(failAfter >= -1 && failAfter <= this.input.length, "fault position");
            this.failAfter = failAfter;
            this.queries = queries;
        }
    }

    private static final class Case {
        final String label;
        final Step[] steps;
        Case(String label, Step... steps) {
            require(label.matches("[a-z0-9-]{1,64}") && steps.length >= 1
                    && steps.length <= 3, "fixed case label/step bound");
            this.label = label;
            this.steps = steps;
        }
    }

    private static Step load(String input, Query... queries) { return new Step(input, -1, queries); }

    private static void single(List<Case> cases, String label, String input, String key) {
        cases.add(new Case(label, load(input, one(key), nullable("absent"),
                fallback("absent", "fallback"), fallback(key, "fallback"))));
    }

    private static List<Case> cases() {
        List<Case> result = new ArrayList<>();
        single(result, "empty-input", "", "");
        StringBuilder high = new StringBuilder("latin=");
        for (int value = 128; value <= 255; value++) high.append((char)value);
        high.append('\n');
        single(result, "latin-high-bytes", high.toString(), "latin");
        single(result, "latin-key-and-nul", new String(new char[] {
                (char)0xe9, (char)0xff, 0, '=', (char)0x80, (char)0xa0, (char)0xff, 0, '\n'}),
                new String(new char[] {(char)0xe9, (char)0xff, 0}));
        single(result, "raw-value-controls", "controls=a" + (char)0 + "b" + (char)11
                + (char)26 + (char)127 + "\n", "controls");
        single(result, "all-line-ends", "lf=one\ncr=two\rpair=three\r\nlast=four", "last");
        single(result, "whitespace-comments", " \t\f\r\n  #ignored\r\t!ignored\n"
                + " \t\fkey \t\f= \t\fvalue  \t\f\n", "key");
        single(result, "comment-backslash", "#not-continued" + BS + "\nvisible=yes\n"
                + "!also-not-continued" + BS + "\r\nother=value\n", "visible");
        single(result, "delimiters", "equals=value\ncolon:value\nwhite value\n"
                + "mixed \t\f: \t\f=value\nonly-key\n", "mixed");
        single(result, "escaped-keys-values", "a" + escape("=") + "b" + escape(":")
                + "c" + escape(" ") + "d=left" + escape(":") + "right"
                + escape("=") + "tail" + escape(" ") + "with" + BS + BS + "slash\n",
                "a=b:c d");
        single(result, "escaped-leading-space", escape(" ") + "key" + escape(" ")
                + "=" + escape(" ") + "value" + escape(" ") + "\n", " key ");
        single(result, "control-and-unknown-escapes", "esc=" + escape("t") + escape("n")
                + escape("r") + escape("f") + escape("b") + escape("v") + escape("z")
                + escape("#") + escape("!") + "\n", "esc");
        single(result, "unicode-exact-units", "unicode=" + escape("u0041") + escape("u00e9")
                + escape("uD83D") + escape("uDE00") + escape("uD800") + escape("u0000")
                + escape("uFFFF") + "\n", "unicode");
        single(result, "unicode-key-separator", escape("u006b") + escape("u003d")
                + "=v\n", "k=");
        single(result, "unicode-fifth-digit", "unicode=" + escape("u00411") + "\n", "unicode");
        single(result, "uppercase-u-not-unicode", "unicode=" + escape("U0041") + "\n", "unicode");
        for (String tail : new String[] {"u", "u0", "u00", "u000", "uG123", "u12G4",
                "u123G", "uu0041", "u-001", "u+001"}) {
            single(result, "malformed-value-" + result.size(),
                    "before=retained\nbad=" + escape(tail) + "\nafter=unreached\n", "before");
        }
        single(result, "malformed-key", "before=retained\n" + escape("u12G4")
                + "=bad\nafter=unreached\n", "before");
        for (int slashes = 1; slashes <= 5; slashes++) {
            single(result, "continuation-slashes-" + slashes, "joined=start" + BS.repeat(slashes)
                    + "\r\n \t\fnext" + BS + "\n end\nlast=done\n", "joined");
        }
        single(result, "continuation-comment-marker", "joined=first" + BS
                + "\n  #part-of-value\nlast=done\n", "joined");
        single(result, "continuation-empty-lines", "joined=first" + BS
                + "\n \t\f\r\nlast=done\n", "joined");
        for (int slashes = 1; slashes <= 3; slashes++) {
            single(result, "eof-slashes-" + slashes, "last=value" + BS.repeat(slashes), "last");
        }
        single(result, "only-backslash-eof", BS, "");
        single(result, "backslash-final-cr", "last=value" + BS + "\r", "last");
        single(result, "backslash-final-crlf", "last=value" + BS + "\r\n", "last");
        for (char whitespace : new char[] {' ', '\t', '\f'}) {
            single(result, "space-after-backslash-" + (int)whitespace,
                    "first=value" + BS + whitespace + "\nnext=value\n", "first");
        }
        single(result, "empty-duplicate-keys-values", "a=first\na:last\n=empty-first\n"
                + ":empty-last\nblank=\nkey-only\n\n", "");
        result.add(new Case("second-load-overrides",
                load("same=old\nstay=keep\n=empty-old\n", one("same"), one("stay"), one("")),
                load("same=new\nadded=second\n=empty-new\nsame=newest\n",
                        one("same"), one("stay"), one(""), fallback("same", "newest"),
                        fallback("absent", "newest"))));
        result.add(new Case("second-load-malformed-prefix",
                load("same=old\nstay=keep\n", one("same"), one("stay")),
                load("same=changed\nok=before\nbad=" + escape("u12G4") + "\nafter=no\n",
                        one("same"), one("ok"), one("stay"), one("after"))));
        String io = "before=one\npending=two\nafter=three\n";
        for (int at : new int[] {0, 5, 10, 11, 12, 22, io.length()}) {
            result.add(new Case("io-boundary-" + at,
                    new Step(io, at, one("before"), one("pending"), one("after"),
                            fallback("absent", "fallback"))));
        }
        result.add(new Case("io-comment-boundary",
                new Step("#comment\nkey=value\n", 9, one("key"), one(""))));
        result.add(new Case("second-load-io-prefix",
                load("same=old\nstay=keep\n", one("same"), one("stay")),
                new Step("same=changed\npending=two\n", 13,
                        one("same"), one("stay"), one("pending"))));
        for (int length : new int[] {8191, 8192, 8193}) {
            single(result, "large-value-" + length, "large=" + "v".repeat(length) + "\n", "large");
        }
        String key = "k".repeat(8193);
        single(result, "large-key-8193", key + "=value\n", key);
        single(result, "large-logical-continuation", "large=" + "a".repeat(8190) + BS
                + "\r\n \t\f" + "b".repeat(8195) + "\n", "large");
        single(result, "large-comment", "#" + "c".repeat(16385) + "\r\nvisible=yes\n", "visible");
        require(result.size() <= 80, "fixed corpus bound");
        return result;
    }

    private static final class Identities {
        final IdentityHashMap<String, Integer> ids = new IdentityHashMap<>();
        final List<String> held = new ArrayList<>();
        String token(String value) {
            if (value == null) return "NULL";
            Integer known = ids.get(value);
            if (known == null) {
                require(held.size() < 512, "per-owner String identity bound");
                known = held.size() + 1;
                held.add(value);
                ids.put(value, known);
            }
            return "R:" + known;
        }
    }

    private static final class Cell {
        final String key, value;
        Cell(String key, String value) { this.key = key; this.value = value; }
    }

    private static void snapshot(Rows out, int owner, int step, Properties properties,
            Identities identities, Query[] queries) {
        List<Cell> cells = new ArrayList<>();
        for (Map.Entry<Object, Object> entry : properties.entrySet()) {
            require(entry.getKey() instanceof String && entry.getValue() instanceof String,
                    "actual String-only entries");
            cells.add(new Cell((String)entry.getKey(), (String)entry.getValue()));
        }
        cells.sort(Comparator.comparing((Cell cell) -> cell.key));
        require(cells.size() == properties.size(), "complete actual map snapshot");
        out.add("SNAPSHOT", owner, step, cells.size());
        for (Cell cell : cells) {
            out.add("ENTRY", owner, step, identities.token(cell.key), identities.token(cell.value));
        }
        for (int index = 0; index < queries.length; index++) {
            Query query = queries[index];
            String returned = query.mode.equals("ONE") ? properties.getProperty(query.key)
                    : properties.getProperty(query.key, query.fallback);
            out.add("GET", owner, step, index, identities.token(returned),
                    query.mode.equals("ONE") ? "NA" : returned == query.fallback ? "1" : "0");
        }
        out.add("STRINGS", owner, step, identities.held.size());
        for (int index = 0; index < identities.held.size(); index++) {
            String value = identities.held.get(index);
            out.add("STRING", owner, step, index + 1, value.length(), units(value));
        }
        out.add("END_LOAD", owner, step);
    }

    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 0, "no arguments");
        Rows out = new Rows();
        out.add("PROPERTIES_OBSERVATION_V1");
        out.add("POLICY", "ACTUAL_JDK_PROPERTIES_LOAD_INPUT_STREAM", "DEFAULTS_NULL",
                "SORT_MAP_KEYS_ONLY", "OBSERVED_STRING_IDENTITY", "FINITE_DECLARED_STREAMS");
        List<Case> cases = cases();
        int owners = 0, loads = 0, queryCount = 0;
        for (Case test : cases) {
            for (int shape : new int[] {Integer.MAX_VALUE, 1, 7, 8192}) {
                int owner = owners++;
                require(owners <= 320, "owner count bound");
                Properties properties = new Properties();
                Identities identities = new Identities();
                out.add("CASE", owner, test.label,
                        shape == Integer.MAX_VALUE ? "ALL" : Integer.toString(shape),
                        test.steps.length);
                for (int step = 0; step < test.steps.length; step++) {
                    Step input = test.steps[step];
                    Query[] queries = new Query[input.queries.length];
                    for (int index = 0; index < queries.length; index++) {
                        Query specification = input.queries[index];
                        queries[index] = new Query(specification.key, specification.mode,
                                specification.fallback);
                    }
                    out.add("INPUT", owner, step, input.failAfter, input.input.length, hex(input.input));
                    for (int index = 0; index < queries.length; index++) {
                        Query query = queries[index];
                        out.add("QUERY", owner, step, index, query.key.length(), units(query.key),
                                query.mode, query.fallback == null ? -1 : query.fallback.length(),
                                units(query.fallback));
                    }
                    FiniteStream stream = new FiniteStream(input.input, shape, input.failAfter);
                    Exception observed = null;
                    try {
                        properties.load(stream);
                    } catch (IOException | IllegalArgumentException failure) {
                        observed = failure;
                    }
                    String type = observed == null ? null : observed.getClass().getName();
                    String message = observed == null ? null : observed.getMessage();
                    out.add("LOAD", owner, step, observed == null ? "OK" : "THREW",
                            type == null ? -1 : type.length(), units(type),
                            message == null ? -1 : message.length(), units(message), stream.consumed,
                            stream.singleCalls + stream.bulkCalls, stream.singleCalls, stream.bulkCalls,
                            stream.closeCalls, stream.closed ? 1 : 0);
                    snapshot(out, owner, step, properties, identities, queries);
                    loads++;
                    queryCount += queries.length;
                }
                out.add("END_CASE", owner);
            }
        }
        out.add("END", owners, loads, queryCount, out.count);
        byte[] actual = out.text.toString().getBytes(StandardCharsets.US_ASCII);
        System.out.write(actual);
        System.out.flush();
        if (System.out.checkError()) throw new IOException("properties observation output failed");
    }
}
