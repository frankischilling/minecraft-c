import java.io.InputStream;
import java.net.URL;
import java.security.MessageDigest;

/** Read the resource bytes exposed by each selected, actually loaded Class. */
public final class ClassResourcePins {
    private static String quote(String value) {
        StringBuilder out = new StringBuilder("\"");
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            if (c == '"' || c == '\\') out.append('\\').append(c);
            else if (c >= 32 && c <= 126) out.append(c);
            else {
                out.append("\\u");
                String hex = Integer.toHexString(c);
                for (int pad = hex.length(); pad < 4; ++pad) out.append('0');
                out.append(hex);
            }
        }
        return out.append('"').toString();
    }

    public static String json(Class<?> originalKey) throws Exception {
        Class<?>[] selected = {originalKey, java.util.HashSet.class, java.util.HashMap.class,
            Class.forName("java.util.HashMap$Node", false, null),
            Class.forName("java.util.HashMap$TreeNode", false, null), java.util.ArrayList.class,
            java.lang.System.class};
        StringBuilder out = new StringBuilder("[");
        for (int i = 0; i < selected.length; ++i) {
            Class<?> cls = selected[i];
            String resource = "/" + cls.getName().replace('.', '/') + ".class";
            URL url = cls.getResource(resource);
            if (url == null) throw new IllegalStateException("missing selected Class resource");
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            int total = 0;
            try (InputStream input = cls.getResourceAsStream(resource)) {
                if (input == null) throw new IllegalStateException("missing selected Class stream");
                byte[] buffer = new byte[8192];
                for (int count; (count = input.read(buffer)) != -1;) {
                    total += count;
                    if (total > 2 * 1024 * 1024) throw new IllegalStateException("selected Class byte bound");
                    digest.update(buffer, 0, count);
                }
            }
            StringBuilder hex = new StringBuilder();
            for (byte value : digest.digest()) {
                int unsigned = value & 255;
                if (unsigned < 16) hex.append('0');
                hex.append(Integer.toHexString(unsigned));
            }
            if (i != 0) out.append(',');
            out.append("{\"class\":").append(quote(cls.getName())).append(",\"module\":")
                .append(cls.getModule().isNamed() ? quote(cls.getModule().getName()) : "null")
                .append(",\"bootstrap_loader\":").append(cls.getClassLoader() == null)
                .append(",\"resource\":").append(quote(url.toExternalForm()))
                .append(",\"bytes\":").append(total).append(",\"sha256\":").append(quote(hex.toString())).append('}');
        }
        return out.append(']').toString();
    }
}
