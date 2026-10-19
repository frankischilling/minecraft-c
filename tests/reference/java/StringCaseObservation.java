import java.nio.charset.StandardCharsets;
import java.util.Locale;

/** Records the complete single-code-point English full-uppercase mapping. */
public final class StringCaseObservation {
    private static final char[] HEX = "0123456789abcdef".toCharArray();

    private static String units(String text) {
        char[] result = new char[text.length() * 4];
        for (int i = 0; i < text.length(); i++) {
            int value = text.charAt(i);
            for (int j = 0; j < 4; j++) result[4 * i + j] = HEX[(value >>> (12 - 4 * j)) & 15];
        }
        return new String(result);
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 0) throw new IllegalArgumentException("no arguments");
        StringBuilder output = new StringBuilder("STRING_UPPER_EN_V1\n");
        int changed = 0, maximum = 0, observed = 0;
        for (int point = 0; point <= Character.MAX_CODE_POINT; point++) {
            String input = new String(Character.toChars(point));
            String upper = input.toUpperCase(Locale.ENGLISH);
            observed++;
            if (!input.equals(upper)) {
                output.append("MAP\t").append(point).append('\t').append(units(upper)).append('\n');
                changed++;
                maximum = Math.max(maximum, upper.length());
            }
        }
        output.append("END\t").append(observed).append('\t').append(changed).append('\t')
                .append(maximum).append('\n');
        System.out.write(output.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.flush();
        if (System.out.checkError()) throw new java.io.IOException("case observation output failed");
    }
}
