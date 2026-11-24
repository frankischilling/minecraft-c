import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.PixelFormat;

/** Authored legacy-GL material controls; no game body is replaced or executed. */
public final class MaterialObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String words(FloatBuffer values, int count) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < count; ++i) {
            if (i != 0) text.append(',');
            text.append(String.format("%08x", Float.floatToRawIntBits(values.get(i))));
        }
        return text.toString();
    }
    static FloatBuffer vector(float... values) {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(16); buffer.put(values).flip(); return buffer;
    }
    static void material(int face, int parameter, float... values) {
        GL11.glMaterial(face, parameter, vector(values));
    }
    static String query(int face, int parameter, int count) {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(16);
        GL11.glGetMaterial(face, parameter, buffer); return words(buffer, count);
    }
    static void state(String name) {
        System.out.print("MATERIAL " + name);
        for (int face : new int[]{GL11.GL_FRONT, GL11.GL_BACK})
            for (int parameter : new int[]{GL11.GL_AMBIENT, GL11.GL_DIFFUSE, GL11.GL_SPECULAR, GL11.GL_EMISSION})
                System.out.print(" " + query(face, parameter, 4));
        System.out.print(" " + query(GL11.GL_FRONT, GL11.GL_SHININESS, 1));
        System.out.print(" " + query(GL11.GL_BACK, GL11.GL_SHININESS, 1));
        FloatBuffer color = BufferUtils.createFloatBuffer(16); GL11.glGetFloat(GL11.GL_CURRENT_COLOR, color);
        System.out.println(" " + words(color, 4) + " " + (GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0) + " " +
            GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE) + " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER));
        require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual material state query");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 1, "native directory argument");
        Locale.setDefault(Locale.ROOT); Path natives = Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }
        });
        Display.setDisplayMode(new DisplayMode(64, 64)); Display.setTitle("Legacy material tracking controls");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("MATERIAL_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            state("initial");
            GL11.glColor4f(.25f, .5f, .75f, .375f);
            GL11.glColorMaterial(GL11.GL_FRONT, GL11.GL_AMBIENT);
            GL11.glEnable(GL11.GL_COLOR_MATERIAL); state("ambient_enabled");
            material(GL11.GL_FRONT, GL11.GL_AMBIENT, .875f, .125f, .25f, .625f);
            state("tracked_ambient_direct_write");
            material(GL11.GL_FRONT, GL11.GL_AMBIENT_AND_DIFFUSE, .5f, .625f, .125f, .75f);
            state("ambient_tracked_ambient_and_diffuse_write");
            GL11.glColor4f(.125f, .25f, .5f, .875f); state("next_color");
            GL11.glColorMaterial(GL11.GL_FRONT, GL11.GL_DIFFUSE); state("diffuse_selected");
            GL11.glColorMaterial(GL11.GL_BACK, GL11.GL_EMISSION); state("back_emission_selected");
            material(GL11.GL_BACK, GL11.GL_EMISSION, .875f, .125f, .25f, .625f);
            state("tracked_emission_direct_write");
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK, GL11.GL_SPECULAR); state("both_specular_selected");
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK, GL11.GL_AMBIENT_AND_DIFFUSE); state("both_ambient_diffuse_selected");
            GL11.glDisable(GL11.GL_COLOR_MATERIAL); state("tracking_disabled");
            GL11.glColor4f(.75f, .5f, .25f, .125f); state("disabled_next_color");
            material(GL11.GL_FRONT, GL11.GL_AMBIENT_AND_DIFFUSE, .5f, .625f, .125f, .75f);
            state("disabled_direct_write");
            GL11.glEnable(GL11.GL_COLOR_MATERIAL); state("tracking_reenabled");
            GL11.glMaterialf(GL11.GL_BACK, GL11.GL_SHININESS, 32); state("untracked_shininess_write");
            material(GL11.GL_FRONT_AND_BACK, GL11.GL_EMISSION, 1.25f, -.125f, .375f, .5f);
            state("untracked_out_of_range_emission");
            require(network == 0, "no network/listener attempts");
        } finally { Display.destroy(); }
        System.out.println("Original material controls: 16 rows; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
