import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

/** Deterministic Java 17 scalar observations, with no clock sampling. */
public final class NanotimeConversionObservation {
    private static final int MAX_VECTORS = 19999;
    private static final int PATTERN_STEPS = 2048;
    private static final long BILLION = 1000000000L;
    private static final long TWO_POW_53 = 1L << 53;

    private static final long[] COUNTS = {
        Long.MIN_VALUE, Long.MIN_VALUE + 1, Long.MIN_VALUE + 511,
        Long.MIN_VALUE + 512, Long.MIN_VALUE + 513, Long.MIN_VALUE + 1023,
        Long.MIN_VALUE + 1024,
        -TWO_POW_53 - 2, -TWO_POW_53 - 1, -TWO_POW_53,
        -TWO_POW_53 + 1, -TWO_POW_53 + 2,
        -1000000001L, -1000000000L, -999999999L, -3, -1,
        0, 1, 3, 999999999L, 1000000000L, 1000000001L,
        TWO_POW_53 - 2, TWO_POW_53 - 1, TWO_POW_53,
        TWO_POW_53 + 1, TWO_POW_53 + 2,
        Long.MAX_VALUE - 1024, Long.MAX_VALUE - 1023, Long.MAX_VALUE - 513,
        Long.MAX_VALUE - 512, Long.MAX_VALUE - 511, Long.MAX_VALUE - 1,
        Long.MAX_VALUE
    };
    private static final long[] FREQUENCIES = {
        Long.MIN_VALUE, -1, 0, 1, 2, 3,
        999999L, 1000000L, 1000001L,
        9999999L, 10000000L, 10000001L,
        999999999L, 1000000000L, 1000000001L, Long.MAX_VALUE
    };
    private static final long[] SECONDS = {
        Long.MIN_VALUE, Long.MIN_VALUE + 1,
        -9223372038L, -9223372037L, -9223372036L, -9223372035L,
        -1, 0, 1,
        9223372035L, 9223372036L, 9223372037L, 9223372038L,
        Long.MAX_VALUE - 1, Long.MAX_VALUE
    };
    private static final long[] NANOSECONDS = {
        Long.MIN_VALUE, -1, 0, 1,
        145224191L, 145224192L, 145224193L,
        854775806L, 854775807L, 854775808L,
        999999998L, 999999999L, 1000000000L, Long.MAX_VALUE
    };

    private final BufferedWriter output;
    private int counterCount;
    private int timespecCount;

    private NanotimeConversionObservation(BufferedWriter output) {
        this.output = output;
    }

    private static String hex64(long value) {
        String digits = Long.toHexString(value);
        return "0000000000000000".substring(digits.length()) + digits;
    }

    private void row(char kind, long first, long second, boolean ok, long expected)
            throws IOException {
        if (counterCount + timespecCount >= MAX_VECTORS) {
            throw new IllegalStateException("vector limit");
        }
        if (kind == 'C') {
            ++counterCount;
        } else if (kind == 'S') {
            ++timespecCount;
        } else {
            throw new IllegalArgumentException("vector kind");
        }
        output.write(kind);
        output.write('\t');
        output.write(hex64(first));
        output.write('\t');
        output.write(hex64(second));
        output.write(ok ? "\tOK\t" : "\tRANGE\t");
        output.write(hex64(ok ? expected : 0L));
        output.write('\n');
    }

    private void counter(long count, long frequency) throws IOException {
        boolean ok = false;
        long expected = 0L;
        if (frequency > 0) {
            double counterValue = (double)count;
            double frequencyValue = (double)frequency;
            double quotient = counterValue / frequencyValue;
            double nanoseconds = quotient * 1000000000.0;
            if (Double.isFinite(nanoseconds) &&
                nanoseconds >= -0x1.0p63 && nanoseconds < 0x1.0p63) {
                expected = (long)nanoseconds;
                ok = true;
            }
        }
        row('C', count, frequency, ok, expected);
    }

    private void timespec(long sec, long nsec) throws IOException {
        boolean ok = false;
        long expected = 0L;
        if (nsec >= 0 && nsec < BILLION) {
            try {
                long wholeSeconds = Math.multiplyExact(sec, BILLION);
                expected = Math.addExact(wholeSeconds, nsec);
                ok = true;
            } catch (ArithmeticException outOfRange) {
                // RANGE is the declared native policy, not a Java clock error.
            }
        }
        row('S', sec, nsec, ok, expected);
    }

    private void vectors() throws IOException {
        for (long count : COUNTS) {
            for (long frequency : FREQUENCIES) {
                counter(count, frequency);
            }
        }
        for (long sec : SECONDS) {
            for (long nsec : NANOSECONDS) {
                timespec(sec, nsec);
            }
        }

        // Fixed integer patterns select INPUTS only. Java long wrapping,
        // shifts and unsigned remainders make this corpus deterministic.
        long pattern = 0x0123456789abcdefL;
        for (int index = 0; index < PATTERN_STEPS; ++index) {
            pattern += 0x9e3779b97f4a7c15L;
            long bits = Long.rotateLeft(pattern, 17) ^ (pattern >>> 23);
            long frequency = (index & 1) == 0
                ? FREQUENCIES[(index >>> 1) % FREQUENCIES.length]
                : (Long.rotateLeft(bits, 31) & Long.MAX_VALUE) | 1L;
            counter(bits, frequency);

            long sec;
            switch (index & 3) {
                case 0:
                    sec = bits;
                    break;
                case 1:
                    sec = Long.remainderUnsigned(bits, 18446744075L) - 9223372037L;
                    break;
                case 2:
                    sec = bits >> 32;
                    break;
                default:
                    sec = bits % 9223372037L;
                    break;
            }
            long nsec = (index & 7) == 0
                ? NANOSECONDS[(index >>> 3) % NANOSECONDS.length]
                : Long.remainderUnsigned(Long.rotateLeft(bits, 29), BILLION);
            timespec(sec, nsec);
        }
    }

    public static void main(String[] arguments) throws IOException {
        if (arguments.length != 0) throw new IllegalArgumentException("no arguments");
        BufferedWriter output = new BufferedWriter(
            new OutputStreamWriter(System.out, StandardCharsets.US_ASCII));
        NanotimeConversionObservation observation = new NanotimeConversionObservation(output);
        output.write("NANO_CONVERSION_V1\n");
        observation.vectors();
        output.write("END\t" + observation.counterCount + "\t" + observation.timespecCount + "\n");
        output.flush();
        if (System.out.checkError()) throw new IOException("output failure");
    }
}
