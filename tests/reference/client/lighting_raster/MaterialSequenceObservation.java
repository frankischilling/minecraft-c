import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.PixelFormat;

/** Runs the unchanged frozen scene before observing any material/current state. */
public final class MaterialSequenceObservation {
    static int network, nativeAttempts;
    static final List<String> links = new ArrayList<>();
    static final String[] PREFIX_CALLS = {
        "CM(1032,4608)+Enable(2903)",
        "Color(3e800000,3f000000,3f400000,3f000000)",
        "CM(1032,4609)",
        "Color(3f400000,3e800000,3f000000,3f400000)",
        "CM(1032,5632)",
        "Color(3e000000,3e800000,3e000000,3f800000)",
        "Disable(2903)",
        "Color(3f600000,3f000000,3e800000,3e000000)"
    };
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String bounded(String text) {
        require(text != null, "actual driver/native string");
        String single = text.replace('\n',' ').replace('\r',' ');
        return single.substring(0,Math.min(single.length(),240));
    }
    static void prefix(int count) {
        // Authored prefixes of the eight contiguous scene32 call groups.
        // The only state observation is after this method has returned.
        RasterObservation.setup(false);
        if (count >= 1) {
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_AMBIENT);
            GL11.glEnable(GL11.GL_COLOR_MATERIAL);
        }
        if (count >= 2) GL11.glColor4f(.25f,.5f,.75f,.5f);
        if (count >= 3) GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_DIFFUSE);
        if (count >= 4) GL11.glColor4f(.75f,.25f,.5f,.75f);
        if (count >= 5) GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_EMISSION);
        if (count >= 6) GL11.glColor4f(.125f,.25f,.125f,1);
        if (count >= 7) GL11.glDisable(GL11.GL_COLOR_MATERIAL);
        if (count >= 8) GL11.glColor4f(.875f,.5f,.25f,.125f);
    }
    static String prefixInput(int count) {
        StringBuilder text = new StringBuilder("MATERIAL_SEQUENCE_PREFIX ").append(count)
            .append(" setup=RasterObservation.setup(false) calls=");
        if (count == 0) text.append("none");
        for (int index = 0; index < count; ++index) {
            if (index != 0) text.append(';');
            text.append(PREFIX_CALLS[index]);
        }
        return text.toString();
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2 || args.length == 3,
                "native directory, pinned library filename and optional prefix0..8 arguments");
        int selectedPrefix = args.length == 3 ? Integer.parseInt(args[2]) : -1;
        require(args.length == 2 || selectedPrefix >= 0 && selectedPrefix <= 8, "declared prefix0..8");
        Locale.setDefault(Locale.ROOT);
        Path natives = Path.of(args[0]).toRealPath();
        require(args[1].equals("lwjgl64.dll") || args[1].equals("liblwjgl64.so"),
                "declared native platform");
        Path allowed = natives.resolve(args[1]);
        Map<String,Path> jdkNatives = new LinkedHashMap<>();
        if (args[1].equals("liblwjgl64.so")) {
            Path jdkLib = Path.of(System.getProperty("java.home"),"lib").toRealPath();
            for (String name : new String[]{"awt","awt_xawt","awt_headless","jawt","fontmanager"}) {
                Path library = jdkLib.resolve(System.mapLibraryName(name)).toRealPath();
                require(library.getParent().equals(jdkLib), "exact JDK native containment");
                jdkNatives.put(name,library);
            }
        }
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("network forbidden");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("listener forbidden");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("file write forbidden");
            }
            @Override public void checkLink(String library) {
                System.out.println("NATIVE_LINK_ATTEMPT " + bounded(library));
                require(++nativeAttempts <= 32, "bounded native-link attempt inventory");
                Path requested = Path.of(library);
                if (requested.isAbsolute() && requested.normalize().equals(allowed)) {
                    links.add(library); return;
                }
                for (Map.Entry<String,Path> entry : jdkNatives.entrySet())
                    if (library.equals(entry.getKey()) || requested.isAbsolute() &&
                            requested.normalize().equals(entry.getValue())) {
                        System.out.println("JDK_NATIVE_LINK " + entry.getKey() + " " + entry.getValue());
                        return;
                    }
                throw new SecurityException("native outside exact LWJGL/JDK allowlist: " + bounded(library));
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64));
        Display.setTitle("Frozen material sequence then state query");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("MATERIAL_SEQUENCE_DRIVER " + bounded(GL11.glGetString(GL11.GL_VENDOR)) +
                " | " + bounded(GL11.glGetString(GL11.GL_RENDERER)) + " | " +
                bounded(GL11.glGetString(GL11.GL_VERSION)));
            StringBuilder format = new StringBuilder("MATERIAL_SEQUENCE_FORMAT");
            for (int parameter : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,
                    GL11.GL_ALPHA_BITS,GL11.GL_DEPTH_BITS,GL11.GL_STENCIL_BITS})
                format.append(' ').append(GL11.glGetInteger(parameter));
            System.out.println(format);
            require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual driver/format queries");

            if (selectedPrefix < 0) {
                // No intervening material/current queries or replacement scene body.
                // scene(32) includes its genuine draw, full pixels and RASTER_STATE.
                RasterObservation.scene(32);
                MaterialObservation.state("after_contiguous_draw");
            } else {
                System.out.println(prefixInput(selectedPrefix));
                prefix(selectedPrefix);
                MaterialObservation.state("after_prefix_" + selectedPrefix);
            }
            require(network == 0, "no network/listener attempts");
        } finally {
            if (Display.isCreated()) Display.destroy();
            require(!Display.isCreated(), "actual context cleanup completed");
        }
        System.out.println(selectedPrefix < 0 ?
            "Material sequence: frozen scene32 draw then material query; cleanup-complete; network=0" :
            "Material sequence: prefix" + selectedPrefix + " end material query; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
