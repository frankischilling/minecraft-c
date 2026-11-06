import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Objects;

/** Finite actual JDK byte decoding, readLine, retained lines and close observations. */
public final class ReaderObservation {
    private static final int MAX_INPUT = 32768;
    private static final int MAX_UNITS = 32768;
    private static final int MAX_ROWS = 65536;
    private static final int MAX_OUTPUT = 4 * 1024 * 1024;
    private static final int MAX_OWNERS = 1024;
    private static final int MAX_CONTROLS = 16;
    private static final char[] HEX = "0123456789abcdef".toCharArray();
    private static final String[] CHARSETS = {
        "UTF-8", "windows-1252", "ISO-8859-1", "US-ASCII"
    };

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static String units(String value) {
        if (value == null) return "NULL";
        require(value.length() <= MAX_UNITS, "UTF16 unit bound");
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

    private static String hex(byte[] input) {
        if (input.length == 0) return "-";
        char[] result = new char[input.length * 2];
        for (int index = 0; index < input.length; index++) {
            int value = input[index] & 255;
            result[2 * index] = HEX[value >>> 4];
            result[2 * index + 1] = HEX[value & 15];
        }
        return new String(result);
    }

    private static byte[] latin1(String input) {
        require(input.length() <= MAX_INPUT, "input byte bound");
        byte[] result = new byte[input.length()];
        for (int index = 0; index < input.length(); index++) {
            require(input.charAt(index) <= 255, "explicit byte input required");
            result[index] = (byte)input.charAt(index);
        }
        return result;
    }

    private static byte[] octets(int... input) {
        require(input.length <= MAX_INPUT, "input byte bound");
        byte[] result = new byte[input.length];
        for (int index = 0; index < input.length; index++) {
            require(input[index] >= 0 && input[index] <= 255, "literal octet range");
            result[index] = (byte)input[index];
        }
        return result;
    }

    private static byte[] range(int start, int end) {
        require(start >= 0 && start <= end && end <= 255, "literal byte range");
        byte[] result = new byte[end - start + 1];
        for (int index = 0; index < result.length; index++) {
            result[index] = (byte)(start + index);
        }
        return result;
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

    private static final class FiniteStream extends InputStream {
        final byte[] input;
        final int shape, failAfter;
        final String availability;
        final boolean closeFault;
        int consumed, singleCalls, bulkCalls, availableCalls, closeCalls;
        boolean closed;

        FiniteStream(byte[] input, int shape, String availability,
                int failAfter, boolean closeFault) {
            require(input.length <= MAX_INPUT && shape > 0 && failAfter >= -1
                    && failAfter <= input.length, "finite stream declaration");
            require(availability.equals("ZERO") || availability.equals("REMAINING")
                    || availability.equals("ERROR"), "availability declaration");
            this.input = input;
            this.shape = shape;
            this.availability = availability;
            this.failAfter = failAfter;
            this.closeFault = closeFault;
        }

        private void admit() throws IOException {
            if (closed) throw new IOException("reader-observer stream is closed");
            // The declared fault precedes EOF, including failAfter==input.length.
            if (failAfter >= 0 && consumed >= failAfter) {
                throw new IOException("reader-observer read failure at byte " + failAfter);
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
            int count = Math.min(Math.min(length, shape), input.length - consumed);
            if (failAfter >= 0) count = Math.min(count, failAfter - consumed);
            require(count > 0, "positive finite-stream read");
            System.arraycopy(input, consumed, target, offset, count);
            consumed += count;
            return count;
        }

        @Override public int available() throws IOException {
            availableCalls++;
            if (closed) throw new IOException("reader-observer stream is closed");
            if (availability.equals("ERROR")) {
                throw new IOException("reader-observer available failure");
            }
            if (availability.equals("ZERO")) return super.available();
            return input.length - consumed;
        }

        @Override public void close() throws IOException {
            closeCalls++;
            // Declared resource effect precedes the optional actual IOException.
            closed = true;
            if (closeFault) throw new IOException("reader-observer close failure");
        }
    }

    private static final class TestCase {
        final String label;
        final byte[] input;
        final int reads, failAfter;
        final boolean closeFault, utf8Only, availableFault;

        TestCase(String label, byte[] input, int reads, int failAfter,
                boolean closeFault, boolean utf8Only, boolean availableFault) {
            require(label.matches("[a-z0-9-]{1,64}") && input.length <= MAX_INPUT
                    && reads >= 0 && reads + 4 <= MAX_CONTROLS
                    && failAfter >= -1 && failAfter <= input.length, "finite case declaration");
            this.label = label;
            this.input = input;
            this.reads = reads;
            this.failAfter = failAfter;
            this.closeFault = closeFault;
            this.utf8Only = utf8Only;
            this.availableFault = availableFault;
        }
    }

    private static void common(List<TestCase> cases, String label, byte[] input) {
        cases.add(new TestCase(label, input, 6, -1, false, false, false));
    }

    private static void utf8(List<TestCase> cases, String label, int... bytes) {
        cases.add(new TestCase(label, octets(bytes), 4, -1, false, true, false));
    }

    private static List<TestCase> cases() {
        List<TestCase> result = new ArrayList<>();
        common(result, "empty-input", octets());
        common(result, "final-unterminated", latin1("final"));
        common(result, "lf-lines", latin1("a\nb\n"));
        common(result, "cr-lines", latin1("a\rb\r"));
        common(result, "crlf-lines", latin1("a\r\nb\r\n"));
        result.add(new TestCase("mixed-lineends", latin1("a\rb\nc\r\nd\n\re"),
                8, -1, false, false, false));
        result.add(new TestCase("empty-lines", latin1("\n\r\n\r\r\n\n"),
                8, -1, false, false, false));
        common(result, "equal-lines-distinct-owners", latin1("same\nsame\n\n\n"));
        common(result, "nul-lines", octets(0, 97, 0, 10, 0, 13, 10, 122, 0));
        common(result, "all-byte-values", range(0, 255));
        common(result, "all-high-byte-values", range(128, 255));
        result.add(new TestCase("close-before-read", latin1("unread\n"),
                0, -1, false, false, false));
        result.add(new TestCase("early-close-retained", latin1("first\nsecond\n"),
                1, -1, false, false, false));
        result.add(new TestCase("close-io-retained", latin1("first\nsecond\n"),
                1, -1, true, false, false));
        result.add(new TestCase("read-io-before-first", latin1("first\nrest\n"),
                4, 0, false, false, false));
        result.add(new TestCase("read-io-after-prefix", latin1("first\nsecond\nlast"),
                6, 8, false, false, false));
        byte[] atEof = latin1("first\nlast");
        result.add(new TestCase("read-io-at-eof", atEof,
                6, atEof.length, false, false, false));
        result.add(new TestCase("read-io-skip-lf", latin1("a\r\nb\n"),
                6, 2, false, false, false));
        result.add(new TestCase("available-io", latin1("first\nlast\n"),
                6, -1, false, false, true));

        utf8(result, "utf8-valid-scalars", 0, 0x7f, 0xc2, 0x80, 0xdf, 0xbf,
                0xe0, 0xa0, 0x80, 0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80,
                0xef, 0xbf, 0xbf, 0xf0, 0x90, 0x80, 0x80,
                0xf4, 0x8f, 0xbf, 0xbf, 10);
        utf8(result, "utf8-bom-and-supplementary", 0xef, 0xbb, 0xbf, 97,
                0xf0, 0x9f, 0x98, 0x80, 13, 10, 0xc2, 0xa2);
        utf8(result, "utf8-stray-continuations", 0x80, 0xbf, 97, 10);
        utf8(result, "utf8-overlong-two", 0xc0, 0x80, 0xc1, 0xbf, 97, 10);
        utf8(result, "utf8-two-bad-continuation", 0xc2, 65, 10);
        utf8(result, "utf8-two-incomplete", 0xc2);
        utf8(result, "utf8-three-overlong", 0xe0, 0x80, 0x80, 0xe0, 0x9f, 0xbf, 10);
        utf8(result, "utf8-surrogates", 0xed, 0xa0, 0x80, 0xed, 0xbf, 0xbf, 10);
        utf8(result, "utf8-three-bad-second", 0xe2, 65, 0x80, 10);
        utf8(result, "utf8-three-bad-third", 0xe2, 0x82, 65, 10);
        utf8(result, "utf8-three-incomplete-one", 0xe2);
        utf8(result, "utf8-three-incomplete-two", 0xe2, 0x82);
        utf8(result, "utf8-four-overlong", 0xf0, 0x80, 0x80, 0x80,
                0xf0, 0x8f, 0xbf, 0xbf, 10);
        utf8(result, "utf8-four-out-of-range", 0xf4, 0x90, 0x80, 0x80,
                0xf5, 0x80, 0x80, 0x80, 10);
        utf8(result, "utf8-four-bad-second", 0xf0, 65, 0x80, 0x80, 10);
        utf8(result, "utf8-four-bad-third", 0xf0, 0x90, 65, 0x80, 10);
        utf8(result, "utf8-four-bad-fourth", 0xf0, 0x90, 0x80, 65, 10);
        utf8(result, "utf8-four-incomplete-one", 0xf0);
        utf8(result, "utf8-four-incomplete-two", 0xf0, 0x90);
        utf8(result, "utf8-four-incomplete-three", 0xf0, 0x90, 0x80);
        utf8(result, "utf8-illegal-long-leads", 0xf8, 0x88, 0x80, 0x80, 0x80,
                0xfc, 0x84, 0x80, 0x80, 0x80, 0x80, 0xfe, 0xff, 10);
        result.add(new TestCase("utf8-read-io-incomplete", octets(111, 107, 10,
                0xc2, 0xa2, 108, 97, 115, 116, 10), 5, 4, false, true, false));

        for (int length : new int[] {8191, 8192, 8193}) {
            result.add(new TestCase("long-crlf-" + length,
                    latin1("q".repeat(length) + "\r\nlast\n"),
                    4, -1, false, true, false));
        }
        result.add(new TestCase("long-no-lf-16385", latin1("z".repeat(16385)),
                3, -1, false, true, false));
        require(result.size() <= 64, "finite case count");
        return result;
    }

    private static final class Lines {
        final IdentityHashMap<String, Integer> identities = new IdentityHashMap<>();
        final List<String> held = new ArrayList<>();

        String token(String line) {
            if (line == null) return "NULL";
            Integer known = identities.get(line);
            if (known == null) {
                require(held.size() < MAX_CONTROLS, "retained line count");
                known = held.size() + 1;
                held.add(line);
                identities.put(line, known);
            }
            return "R:" + known;
        }
    }

    private static void result(Rows out, int owner, int index, String operation,
            BufferedReader reader, FiniteStream stream, Lines lines) {
        IOException observed = null;
        String line = null;
        try {
            if (operation.equals("READ_LINE")) line = reader.readLine();
            else reader.close();
        } catch (IOException failure) {
            observed = failure;
        }
        String status = observed != null ? "THREW"
                : operation.equals("READ_LINE") && line == null ? "EOF" : "OK";
        String type = observed == null ? null : observed.getClass().getName();
        String message = observed == null ? null : observed.getMessage();
        out.add("RESULT", owner, index, operation, status, lines.token(line),
                type == null ? -1 : type.length(), units(type),
                message == null ? -1 : message.length(), units(message), stream.consumed,
                stream.singleCalls + stream.bulkCalls, stream.singleCalls, stream.bulkCalls,
                stream.availableCalls, stream.closeCalls, stream.closed ? 1 : 0);
    }

    public static void main(String[] arguments) throws Exception {
        require(arguments.length == 0, "no arguments");
        Rows out = new Rows();
        out.add("READER_OBSERVATION_V1");
        out.add("POLICY", "ACTUAL_JDK_BUFFERED_READER_INPUT_STREAM_READER",
                "EXPLICIT_CHARSETS", "REPLACE_MALFORMED", "FINITE_DECLARED_STREAMS",
                "HELD_LINE_IDENTITIES");
        int owners = 0, actions = 0, readCalls = 0, closeCalls = 0;
        for (TestCase test : cases()) {
            for (String charsetName : CHARSETS) {
                if (test.utf8Only && !charsetName.equals("UTF-8")) continue;
                Charset charset = Charset.forName(charsetName);
                for (int shape : new int[] {Integer.MAX_VALUE, 1, 7, 8192}) {
                    String[] modes = test.availableFault ? new String[] {"ERROR"}
                            : new String[] {"ZERO", "REMAINING"};
                    for (String availability : modes) {
                        int owner = owners++;
                        require(owners <= MAX_OWNERS, "owner bound");
                        List<String> controls = new ArrayList<>();
                        for (int index = 0; index < test.reads; index++) {
                            controls.add("READ_LINE");
                        }
                        controls.add("CLOSE");
                        controls.add("READ_LINE");
                        controls.add("CLOSE");
                        controls.add("READ_LINE");
                        require(controls.size() <= MAX_CONTROLS, "control bound");
                        out.add("CASE", owner, test.label, charsetName,
                                shape == Integer.MAX_VALUE ? "ALL" : Integer.toString(shape),
                                availability, controls.size());
                        out.add("INPUT", owner, test.failAfter, test.closeFault ? 1 : 0,
                                test.input.length, hex(test.input));
                        for (int index = 0; index < controls.size(); index++) {
                            out.add("QUERY", owner, index, controls.get(index));
                        }
                        FiniteStream stream = new FiniteStream(test.input, shape, availability,
                                test.failAfter, test.closeFault);
                        BufferedReader reader = new BufferedReader(
                                new InputStreamReader(stream, charset));
                        Lines lines = new Lines();
                        out.add("CREATE", owner, stream.consumed,
                                stream.singleCalls + stream.bulkCalls, stream.singleCalls,
                                stream.bulkCalls, stream.availableCalls, stream.closeCalls,
                                stream.closed ? 1 : 0);
                        for (int index = 0; index < controls.size(); index++) {
                            String operation = controls.get(index);
                            result(out, owner, index, operation, reader, stream, lines);
                            actions++;
                            if (operation.equals("READ_LINE")) readCalls++;
                            else closeCalls++;
                        }
                        // Read all old line objects after later reads, failure and close.
                        out.add("STRINGS", owner, lines.held.size());
                        for (int index = 0; index < lines.held.size(); index++) {
                            String line = lines.held.get(index);
                            out.add("STRING", owner, index + 1, line.length(), units(line));
                        }
                        out.add("END_CASE", owner);
                    }
                }
            }
        }
        out.add("END", owners, actions, readCalls, closeCalls, out.count);
        byte[] actual = out.text.toString().getBytes(StandardCharsets.US_ASCII);
        System.out.write(actual);
        System.out.flush();
        if (System.out.checkError()) throw new IOException("reader observation output failed");
    }
}
