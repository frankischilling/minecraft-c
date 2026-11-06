import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.DuplicateFormatFlagsException;
import java.util.FormatFlagsConversionMismatchException;
import java.util.IllegalFormatCodePointException;
import java.util.IllegalFormatConversionException;
import java.util.IllegalFormatException;
import java.util.IllegalFormatFlagsException;
import java.util.IllegalFormatPrecisionException;
import java.util.IllegalFormatWidthException;
import java.util.List;
import java.util.Locale;
import java.util.MissingFormatArgumentException;
import java.util.MissingFormatWidthException;
import java.util.UnknownFormatConversionException;
import java.util.UnknownFormatFlagsException;

/** Finite public String.format observation. Inputs are synthetic source cases;
 * only the actual JDK operation supplies results. No native implementation or
 * handwritten format model is invoked. All protocol text is ASCII plus LF;
 * Java String payloads are length-tagged hexadecimal UTF16 code units. */
public final class FormatObservation {
    private static final String HEX = "0123456789abcdef";
    private static final int MAX_CASES = 300;
    private static final int MAX_RESULT_UNITS = 8192;
    private static final int MAX_BYTES = 4 * 1024 * 1024;
    private static int emittedBytes;

    private static final class Case {
        final String pattern;
        final String[] arguments;
        Case(String pattern, String[] arguments) {
            this.pattern = pattern;
            this.arguments = arguments == null ? null : arguments.clone();
        }
    }

    private static void add(List<Case> cases, String pattern, String... arguments) {
        cases.add(new Case(pattern, arguments));
    }

    private static String units(int... values) {
        char[] chars = new char[values.length];
        for (int i = 0; i < values.length; ++i) {
            if (values[i] < 0 || values[i] > 0xffff)
                throw new AssertionError("Bad declared UTF16 unit");
            chars[i] = (char)values[i];
        }
        return new String(chars);
    }

    private static List<Case> corpus() {
        List<Case> cases = new ArrayList<>();
        add(cases, null, (String[])null);
        add(cases, null);
        add(cases, "");
        add(cases, "literal");
        add(cases, "plain\ttext");
        add(cases, "%%|%n|%%");
        add(cases, "[%5%][%-3%]");
        add(cases, "%1$%/%1$n");
        add(cases, "%s|%s", "", (String)null);
        add(cases, "%s|%s|%2$s|%<s", (String[])null);
        add(cases, "%2147483647$s", (String[])null);

        String[] indexing = {
            "%s", "%2$s", "%1$s/%1$s", "%s/%<S/%s/%<s", "%2$s/%s/%<s/%1$s",
            "%2$s/%1$<s", "%2$s/%s/%s", "%<s", "%s/%3$s", "%s/%<s/%s",
            "%01$s", "%1$s/%2$s/%1$S", "%%/%<s", "%s/%%/%<s"
        };
        for (String pattern : indexing) {
            add(cases, pattern, "one", "two");
            add(cases, pattern, (String[])null);
        }

        String[] general = {
            "[%7.3s]", "[%-7.3s]", "[%.0s]", "[%1$.2S]", "%.1b", "%6.3B",
            "%h", "%H", "[%10.4h]", "%#s", "%s/%<S", "%b/%B"
        };
        for (String pattern : general) {
            add(cases, pattern, "false", "");
            add(cases, pattern, (String)null, (String)null);
            add(cases, pattern);
        }

        // Every ordinary conversion receives both a String and a null. This
        // includes unsupported VALUE types without inventing numeric values.
        String conversions = "sSbBhHcCdoxXeEfgGaA";
        for (int i = 0; i < conversions.length(); ++i) {
            String pattern = "%" + conversions.charAt(i);
            add(cases, pattern, "word");
            add(cases, pattern, (String)null);
        }

        // Exact DateTime.isValid suffix domain. No Date/Calendar/timezone or
        // sampled clock is created: only actual String/null type handling.
        String dateSuffixes = "HIklMNLQpsSTzZaAbBCdehjmyYrRcDF";
        for (int i = 0; i < dateSuffixes.length(); ++i) {
            add(cases, "%t" + dateSuffixes.charAt(i), "date");
            add(cases, "%T" + dateSuffixes.charAt(i), (String)null);
        }

        String[] invalid = {
            "%", "%q", "%.s", "%N", "%F", "%D", "%O", "%i", "%t", "%tq", "%Tq",
            "%--s", "%00d", "%1$<<s", "%##s", "%++d", "%  d", "%+ d", "%-05d",
            "%#%", "%<%", "%+s", "%0s", "%#b", "%#h", "%#d", "%,x", "%,e", "%#g", "%(a",
            "%-s", "%0d", "%-0d", "%1n", "%1.1n", "%#n", "%-n", "%<n",
            "%2147483648s", "%2147483648n", "%.2147483648s", "%.1c", "%.1d", "%.1n", "%.1tq",
            "%0$s", "%2147483648$s", "%999999999999999999999$s", "%0$--s", "%1$2147483648s",
            "%1$.2147483648s", "%2147483647d", "%.2147483647s"
        };
        for (String pattern : invalid)
            add(cases, pattern, "word");

        // Full-pattern parse/validation precedes missing arguments and print
        // type checks. Valid huge widths occur only on guaranteed error paths.
        add(cases, "%2147483647s");
        add(cases, "%s %q");
        add(cases, "%d %q", "word");
        add(cases, "%#s %--s", "word");
        add(cases, "%s %-s");
        add(cases, "%d %0$s", "word");
        add(cases, "%#s %.1d");
        add(cases, "%2147483647s %q", "word");
        add(cases, "%#s");
        add(cases, "%#s", (String)null);
        add(cases, "%08d", (String)null);
        add(cases, "%6.2A", (String)null);
        add(cases, "%8.2E", (String)null);

        String[] unicode = {
            units('s', 't', 'r', 'a', 0xdf, 'e'), units(0xfb03), units('i', 0x131, 0x130, 0x307),
            units(0xd801, 0xdc28), units(0xd800, 'a', 0, 0xdc00), units(0x149),
            units(0x390), units(0x3c2, 0xb5), units(0xffff, 0xffff, 0xffff, 0xffff, 0xffff),
            units('A', 0, 'B')
        };
        for (String argument : unicode) {
            add(cases, "%S", argument);
            add(cases, "%.1S", argument);
            add(cases, "%h/%<H", argument);
        }
        add(cases, "%3.1S", units(0xdf, 'x'));
        add(cases, "%-3.1S", units(0xdf, 'x'));
        add(cases, "%2S", units(0xfb03));
        add(cases, "%.2S", units(0xd801, 0xdc28));
        if (cases.size() < 150 || cases.size() > MAX_CASES)
            throw new AssertionError("Corpus size outside the declared finite domain: " + cases.size());
        return cases;
    }

    private static String hex(String value) {
        if (value.isEmpty())
            return "-";
        StringBuilder encoded = new StringBuilder(value.length() * 4);
        for (int i = 0; i < value.length(); ++i) {
            int unit = value.charAt(i);
            encoded.append(HEX.charAt((unit >>> 12) & 15));
            encoded.append(HEX.charAt((unit >>> 8) & 15));
            encoded.append(HEX.charAt((unit >>> 4) & 15));
            encoded.append(HEX.charAt(unit & 15));
        }
        return encoded.toString();
    }

    private static void emit(String... fields) throws IOException {
        StringBuilder line = new StringBuilder();
        for (int i = 0; i < fields.length; ++i) {
            if (i != 0)
                line.append('\t');
            String field = fields[i];
            for (int j = 0; j < field.length(); ++j)
                if (field.charAt(j) < 32 || field.charAt(j) > 126)
                    throw new AssertionError("Non-ASCII protocol field");
            line.append(field);
        }
        line.append('\n');
        byte[] bytes = line.toString().getBytes(StandardCharsets.US_ASCII);
        if (bytes.length > 65536 || emittedBytes > MAX_BYTES - bytes.length)
            throw new AssertionError("Observation output bound");
        System.out.write(bytes, 0, bytes.length);
        emittedBytes += bytes.length;
    }

    private static void inputText(String id, String kind, String value) throws IOException {
        if (value == null)
            emit("INPUT", id, kind, "NULL");
        else
            emit("INPUT", id, kind, "TEXT", Integer.toString(value.length()), hex(value));
    }

    private static void detailText(String id, String kind, String value) throws IOException {
        if (value == null)
            emit("DETAIL", id, kind, "NULL");
        else
            emit("DETAIL", id, kind, "TEXT", Integer.toString(value.length()), hex(value));
    }

    private static void exception(String id, RuntimeException error) throws IOException {
        emit("RESULT", id, "ERROR", error.getClass().getName());
        detailText(id, "MESSAGE", error.getMessage());
        if (error instanceof DuplicateFormatFlagsException)
            detailText(id, "FLAGS", ((DuplicateFormatFlagsException)error).getFlags());
        else if (error instanceof IllegalFormatFlagsException)
            detailText(id, "FLAGS", ((IllegalFormatFlagsException)error).getFlags());
        else if (error instanceof FormatFlagsConversionMismatchException) {
            FormatFlagsConversionMismatchException e = (FormatFlagsConversionMismatchException)error;
            detailText(id, "FLAGS", e.getFlags());
            emit("DETAIL", id, "CONVERSION", "UNIT", Integer.toString(e.getConversion()));
        } else if (error instanceof IllegalFormatWidthException)
            emit("DETAIL", id, "WIDTH", "INT", Integer.toString(((IllegalFormatWidthException)error).getWidth()));
        else if (error instanceof IllegalFormatPrecisionException)
            emit("DETAIL", id, "PRECISION", "INT", Integer.toString(((IllegalFormatPrecisionException)error).getPrecision()));
        else if (error instanceof MissingFormatArgumentException)
            detailText(id, "SPECIFIER", ((MissingFormatArgumentException)error).getFormatSpecifier());
        else if (error instanceof MissingFormatWidthException)
            detailText(id, "SPECIFIER", ((MissingFormatWidthException)error).getFormatSpecifier());
        else if (error instanceof UnknownFormatConversionException)
            detailText(id, "CONVERSION", ((UnknownFormatConversionException)error).getConversion());
        else if (error instanceof UnknownFormatFlagsException)
            detailText(id, "FLAGS", ((UnknownFormatFlagsException)error).getFlags());
        else if (error instanceof IllegalFormatConversionException) {
            IllegalFormatConversionException e = (IllegalFormatConversionException)error;
            emit("DETAIL", id, "CONVERSION", "UNIT", Integer.toString(e.getConversion()));
            emit("DETAIL", id, "ARGUMENT_CLASS", e.getArgumentClass().getName());
        } else if (error instanceof IllegalFormatCodePointException)
            emit("DETAIL", id, "CODE_POINT", "INT", Integer.toString(((IllegalFormatCodePointException)error).getCodePoint()));
        else if (error.getClass().getName().equals("java.util.IllegalFormatArgumentIndexException"))
            emit("DETAIL", id, "ARGUMENT_INDEX", "UNAVAILABLE_NONPUBLIC");
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 0)
            throw new IllegalArgumentException("No command-line input is accepted");
        List<Case> cases = corpus();
        emit("FORMAT_STRINGS_V1");
        emit("ENV", "LOCALE", "java.util.Locale.ENGLISH");
        String separator = System.lineSeparator();
        emit("ENV", "LINE_SEPARATOR", "TEXT", Integer.toString(separator.length()), hex(separator));
        for (int i = 0; i < cases.size(); ++i) {
            Case c = cases.get(i);
            String id = Integer.toString(i);
            emit("CASE", id);
            inputText(id, "PATTERN", c.pattern);
            if (c.arguments == null)
                emit("INPUT", id, "ARGUMENTS", "NULL");
            else {
                emit("INPUT", id, "ARGUMENTS", "COUNT", Integer.toString(c.arguments.length));
                for (int j = 0; j < c.arguments.length; ++j)
                    inputText(id, "ARGUMENT_" + j, c.arguments[j]);
            }
            try {
                String observed = String.format(Locale.ENGLISH, c.pattern, (Object[])c.arguments);
                if (observed.length() > MAX_RESULT_UNITS)
                    throw new AssertionError("Unexpected large result in a bounded synthetic case");
                emit("RESULT", id, "OK", "TEXT", Integer.toString(observed.length()), hex(observed));
            } catch (IllegalFormatException | NullPointerException observed) {
                exception(id, observed);
            }
        }
        emit("END", Integer.toString(cases.size()));
        System.out.flush();
        if (System.out.checkError())
            throw new IOException("Observation stdout failed");
    }
}
