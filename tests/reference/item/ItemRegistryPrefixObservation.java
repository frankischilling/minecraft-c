import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Starts untouched original Block initialization; stops nested Item at2664. */
public final class ItemRegistryPrefixObservation {
    public static volatile URLClassLoader originalLoader;
    public static volatile String originalJarUrl;
    public static volatile String verifiedJarSha256;
    public static volatile String originalEntry;

    public static void main(String[] args) throws Exception {
        if (args.length != 2 || !(args[0].equals("client") || args[0].equals("server"))) {
            throw new IllegalArgumentException("Usage: ItemRegistryPrefixObservation client|server jar");
        }
        boolean client = args[0].equals("client");
        Path jar = Path.of(args[1]).toRealPath();
        String expected = client
                ? "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
                : "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";
        if (Files.size(jar) != (client ? 1465375L : 503100L)) {
            throw new IllegalStateException("original JAR size");
        }
        String actual = HexFormat.of().formatHex(
                MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)));
        if (!actual.equals(expected)) throw new IllegalStateException("original JAR SHA-256");
        URL url = jar.toUri().toURL();
        originalJarUrl = url.toExternalForm();
        verifiedJarSha256 = actual;
        try (URLClassLoader loader = new URLClassLoader(new URL[] {url},
                ClassLoader.getPlatformClassLoader())) {
            originalLoader = loader;
            originalEntry = client ? "uu" : "na";
            Class.forName(originalEntry, true, loader);
            throw new IllegalStateException("UNEXPECTED_ORIGINAL_BLOCK_INITIALIZER_COMPLETION");
        }
    }
}
