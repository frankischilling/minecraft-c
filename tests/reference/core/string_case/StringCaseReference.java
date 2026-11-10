import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.Random;

/** Authored operands invoking the selected JDK's unchanged Character and String bodies. */
public final class StringCaseReference {
    private static int fold(int value) {
        return Character.toLowerCase(Character.toUpperCase(value));
    }

    private static void pair(DataOutputStream out, String left, String right) throws Exception {
        out.writeInt(left.length());
        for (int i = 0; i < left.length(); ++i) out.writeChar(left.charAt(i));
        out.writeInt(right.length());
        for (int i = 0; i < right.length(); ++i) out.writeChar(right.charAt(i));
        out.writeBoolean(left.equalsIgnoreCase(right));
    }

    private static String point(int value) {
        return new String(Character.toChars(value));
    }

    private static String quoted(String value) {
        return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    private static String digest(String name) throws Exception {
        try (var input = String.class.getResourceAsStream(name + ".class")) {
            if (input == null) throw new AssertionError("Actual java.base Class bytes: " + name);
            return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(input.readAllBytes()));
        }
    }

    private static String randomString(Random random, int[] interesting) {
        StringBuilder value = new StringBuilder();
        int count = random.nextInt(12);
        for (int i = 0; i < count; ++i) {
            int next = random.nextInt(4) == 0 ? random.nextInt(0x110000)
                       : interesting[random.nextInt(interesting.length)];
            value.appendCodePoint(next);
        }
        return value.toString();
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1 || Runtime.version().feature() != 17)
            throw new IllegalArgumentException("One private output directory and JDK17 required");
        Path root = Path.of(args[0]);
        Files.createDirectories(root);
        if (Character.class.getClassLoader() != null || String.class.getClassLoader() != null ||
            !Character.class.getModule().getName().equals("java.base") ||
            !String.class.getModule().getName().equals("java.base"))
            throw new AssertionError("Unchanged bootstrap java.base classes required");
        Files.writeString(root.resolve("runtime.json"), "{\"runtime\":" +
            quoted(System.getProperty("java.runtime.version")) + ",\"vendor\":" +
            quoted(System.getProperty("java.vendor")) + ",\"Character\":" + quoted(digest("Character")) +
            ",\"String\":" + quoted(digest("String")) + ",\"StringUTF16\":" +
            quoted(digest("StringUTF16")) + ",\"StringLatin1\":" + quoted(digest("StringLatin1")) + "}\n");
        try (DataOutputStream out = new DataOutputStream(new BufferedOutputStream(
                Files.newOutputStream(root.resolve("fold.bin"))))) {
            for (int value = 0; value <= 0x10ffff; ++value) out.writeInt(fold(value));
        }
        int count = 0;
        int[] interesting = {0, 1, 0x41, 0x49, 0x4b, 0x53, 0x61, 0x69, 0x6b, 0x73,
            0x7f, 0x80, 0xdf, 0x130, 0x131, 0x17f, 0x3a3, 0x3c2, 0x3c3, 0x1e9e,
            0x212a, 0xd7ff, 0xd800, 0xd801, 0xdbff, 0xdc00, 0xdc28, 0xdfff, 0xe000,
            0xffff, 0x10000, 0x10400, 0x10428, 0x104b0, 0x104d8, 0x10c80, 0x10cc0,
            0x118a0, 0x118c0, 0x16e40, 0x16e60, 0x1e900, 0x1e922, 0x10ffff};
        try (DataOutputStream out = new DataOutputStream(new BufferedOutputStream(
                Files.newOutputStream(root.resolve("pairs.bin"))))) {
            out.writeInt(0x53434931);
            int expected = 0x110000 + interesting.length * interesting.length +
                           0x800 * interesting.length + 100000;
            out.writeInt(expected);
            for (int value = 0; value <= 0x10ffff; ++value) {
                pair(out, point(value), point(fold(value)));
                ++count;
            }
            for (int left : interesting) for (int right : interesting) {
                pair(out, point(left), point(right));
                ++count;
            }
            for (int surrogate = 0xd800; surrogate <= 0xdfff; ++surrogate) {
                for (int operand : interesting) {
                    pair(out, new String(new char[] {(char)surrogate, 'A', 0xd801, 0xdc00}),
                              point(operand) + "a" + point(0x10428));
                    ++count;
                }
            }
            Random random = new Random(0x53434917L);
            for (int i = 0; i < 100000; ++i) {
                String left = randomString(random, interesting);
                String right;
                switch (i & 3) {
                    case 0:
                        right = left;
                        break;
                    case 1:
                        StringBuilder folded = new StringBuilder();
                        left.codePoints().forEach(value -> folded.appendCodePoint(fold(value)));
                        right = folded.toString();
                        break;
                    case 2:
                        right = randomString(random, interesting);
                        break;
                    default:
                        right = left.isEmpty() ? "A" : left.substring(1);
                        break;
                }
                pair(out, left, right);
                ++count;
            }
            if (count != expected) throw new AssertionError("Complete declared operand inventory");
        }
        System.out.println("JDK17 string case: codepoints=1114112 pairs=" + count);
    }
}
