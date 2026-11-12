import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;

/** Finite ordinary JDK PrintStream calls; result bytes never come from native code. */
public final class PrintlnObservation {
    private static final char[] HEX = "0123456789abcdef".toCharArray();
    private static final BufferedWriter PROTOCOL = new BufferedWriter(
            new OutputStreamWriter(System.out, StandardCharsets.US_ASCII));
    private static int rows;
    private static int protocolUnits;

    private record Case(String name, String text, int failAfter, boolean closeBefore) {}

    /** Explicit stream input: retain successful bytes, then throw ordinary IOExceptions. */
    private static final class FaultOutput extends OutputStream {
        private final ByteArrayOutputStream bytes;
        private final int failAfter;
        private int written;
        private boolean closed;

        FaultOutput(ByteArrayOutputStream bytes, int failAfter) {
            this.bytes = bytes;
            this.failAfter = failAfter;
        }

        @Override
        public void write(int value) throws IOException {
            if (closed) throw new IOException("chosen closed output");
            if (failAfter >= 0 && written == failAfter) {
                throw new IOException("chosen output limit " + failAfter);
            }
            bytes.write(value);
            ++written;
        }

        @Override
        public void write(byte[] source, int offset, int length) throws IOException {
            if (source == null) throw new NullPointerException("source");
            if (offset < 0 || length < 0 || offset > source.length - length) {
                throw new IndexOutOfBoundsException("source span");
            }
            for (int i = 0; i < length; ++i) write(source[offset + i]);
        }

        @Override
        public void flush() throws IOException {
            if (closed) throw new IOException("chosen closed output");
        }

        @Override
        public void close() {
            closed = true;
        }
    }

    private static String units(String value) {
        StringBuilder result = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i) {
            int unit = value.charAt(i);
            result.append(HEX[(unit >>> 12) & 15]);
            result.append(HEX[(unit >>> 8) & 15]);
            result.append(HEX[(unit >>> 4) & 15]);
            result.append(HEX[unit & 15]);
        }
        return result.toString();
    }

    private static String bytes(byte[] value) {
        StringBuilder result = new StringBuilder(value.length * 2);
        for (byte unit : value) {
            result.append(HEX[(unit >>> 4) & 15]);
            result.append(HEX[unit & 15]);
        }
        return result.toString();
    }

    private static void row(String text) throws IOException {
        if (text.length() > 1024 || protocolUnits + text.length() + 1 > 8192) {
            throw new IOException("finite observer output bound");
        }
        for (int i = 0; i < text.length(); ++i) {
            char unit = text.charAt(i);
            if (unit != '\t' && (unit < 0x20 || unit > 0x7e)) {
                throw new IOException("ASCII protocol field");
            }
        }
        PROTOCOL.write(text);
        PROTOCOL.write('\n');
        PROTOCOL.flush();
        protocolUnits += text.length() + 1;
        ++rows;
    }

    private static void observe(Case input) throws IOException {
        ByteArrayOutputStream captured = new ByteArrayOutputStream();
        OutputStream target = captured;
        FaultOutput fault = null;
        if (input.failAfter() >= 0 || input.closeBefore()) {
            fault = new FaultOutput(captured, input.failAfter());
            target = fault;
        }
        try (PrintStream stream = new PrintStream(target, true, StandardCharsets.US_ASCII)) {
            if (input.closeBefore()) fault.close();
            stream.println(input.text());
            boolean error = stream.checkError();
            byte[] actual = captured.toByteArray();
            if (actual.length > 128) throw new IOException("observed byte bound");
            // Observation boundary is after println/checkError, before cleanup close.
            row("RESULT\t" + input.name() + "\t" + actual.length + "\t" + bytes(actual)
                    + "\t" + (error ? "1" : "0"));
        }
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 0) throw new IllegalArgumentException("no arguments");
        Case[] inputs = {
            new Case("normal", "hello", -1, false),
            new Case("empty", "", -1, false),
            new Case("ascii64", "0123456789abcdef0123456789abcdef"
                    + "0123456789abcdef0123456789abcdef", -1, false),
            new Case("nul", new String(new char[] {'A', 0, 'B'}), -1, false),
            new Case("failing", "hello", 2, false),
            new Case("closed", "hello", -1, true)
        };
        String separator = System.lineSeparator();
        if (separator.length() < 1 || separator.length() > 2) {
            throw new IllegalStateException("finite line-separator profile");
        }
        row("PRINTLN_OBSERVATION_V1");
        row("POLICY\tUS-ASCII\tAUTOFLUSH_TRUE");
        row("SEPARATOR\t" + separator.length() + "\t" + units(separator) + "\t"
                + bytes(separator.getBytes(StandardCharsets.US_ASCII)));
        for (Case input : inputs) {
            row("CASE\t" + input.name() + "\t"
                    + (input.failAfter() >= 0 || input.closeBefore() ? "FAULT_STREAM" : "BYTE_ARRAY")
                    + "\t" + input.failAfter() + "\t" + (input.closeBefore() ? "1" : "0"));
            row("INPUT\t" + input.name() + "\t" + input.text().length() + "\t"
                    + units(input.text()) + "\t" + bytes(input.text().getBytes(StandardCharsets.US_ASCII)));
        }
        for (Case input : inputs) observe(input);
        row("END\t" + inputs.length + "\t" + rows);
        if (System.out.checkError()) throw new IOException("observer transport failed");
    }
}
