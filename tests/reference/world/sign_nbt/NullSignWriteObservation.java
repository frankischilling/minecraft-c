import java.io.FileDescriptor;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;

/** Supplemental authored caller: public per-line nulls, actual YK writer. */
public final class NullSignWriteObservation {
    static int denied;
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        SignNbtObservation.need(args.length == 1, "one unchanged archive");
        SignNbtObservation.archive = Path.of(args[0]).toRealPath();
        SignNbtObservation.need(SignNbtObservation.sha(Files.readAllBytes(SignNbtObservation.archive))
            .equals(SignNbtObservation.JAR_SHA), "unchanged archive hash");
        for (Class<?> c : new Class<?>[] {yk.class, ow.class, nu.class, ij.class, xb.class})
            SignNbtObservation.origin(c);
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission p) { }
            public void checkPermission(Permission p, Object context) { }
            void deny() { ++denied; throw new SecurityException("external action denied"); }
            public void checkConnect(String host, int port) { deny(); }
            public void checkConnect(String host, int port, Object context) { deny(); }
            public void checkListen(int port) { deny(); }
            public void checkAccept(String host, int port) { deny(); }
            public void checkLink(String name) { deny(); }
            public void checkExec(String command) { deny(); }
            public void checkWrite(String name) { deny(); }
            public void checkWrite(FileDescriptor descriptor) {
                if (descriptor != FileDescriptor.out && descriptor != FileDescriptor.err) deny();
            }
            public void checkDelete(String name) { deny(); }
            public void checkExit(int status) { deny(); }
        });
        for (int i = 0; i < 4; ++i) {
            String name = "public-null-line-" + i;
            SignNbtObservation.beginCase(name, "Public field prestate, original component writer only");
            yk tile = SignNbtObservation.construct(name, "constructor");
            tile.e = 12; tile.f = -30; tile.g = 51;
            tile.a[i] = null; // Declared public field prestate, never a Source-private write.
            SignNbtObservation.row("PUBLIC_PRESTATE", SignNbtObservation.map("case", name,
                "null_index", i, "after", SignNbtObservation.state(tile)));
            nu output = SignNbtObservation.seededOutput();
            SignNbtObservation.need(!SignNbtObservation.write(name, "write", tile, output),
                "null String rejected by actual XB constructor");
        }
        SignNbtObservation.need(denied == 0, "no external action attempts");
        SignNbtObservation.row("COMPLETE", SignNbtObservation.map("completed_script", true,
            "cases", List.copyOf(SignNbtObservation.cases), "constructor_calls", 4,
            "write_calls", 4, "denied", denied,
            "class_origins", SignNbtObservation.origins));
    }
}
