import java.nio.ByteBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL12;
import org.lwjgl.opengl.PixelFormat;

/** Successor authored scenes; the frozen 38-profile helper remains unchanged. */
public final class RasterExtendedObservation {
    static final String[] NAMES = {
        "specular_nonlocal", "specular_local", "specular_shininess_zero", "specular_shininess_32",
        "twoside_front", "twoside_back", "oneside_back", "back_tracking_color",
        "single_specular_texture", "separate_specular_texture", "separate_specular_texture_fog",
        "unlit_triangle_flat", "unlit_triangle_smooth", "unlit_fan_flat", "unlit_fan_smooth",
        "clamped_lighting_smooth", "clamped_lighting_flat", "positional_local_specular"
    };
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static void reverse(ByteBuffer bytes) {
        byte[] first = new byte[32], second = new byte[32];
        for (int i = 0; i < 32; ++i) { first[i] = bytes.get(32+i); second[i] = bytes.get(64+i); }
        for (int i = 0; i < 32; ++i) { bytes.put(32+i,second[i]); bytes.put(64+i,first[i]); }
    }
    static void scene(int index) {
        String name = NAMES[index]; RasterObservation.setup(false);
        GL11.glLightModel(GL11.GL_LIGHT_MODEL_AMBIENT,RasterObservation.vector(0,0,0,1));
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER,0);
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_TWO_SIDE,0);
        GL11.glLightModeli(GL12.GL_LIGHT_MODEL_COLOR_CONTROL,GL12.GL_SINGLE_COLOR);
        GL11.glDisable(GL11.GL_LIGHT1);
        boolean unlit = index >= 11 && index <= 14;
        boolean fan = index == 13 || index == 14 || index >= 8 && index <= 10;
        boolean colorArray = unlit || index == 7;
        boolean normalArray = index == 7 || index == 15 || index == 16;
        ByteBuffer bytes = RasterObservation.geometry(fan,false,!unlit);
        RasterObservation.material(GL11.GL_AMBIENT,0,0,0,1);
        RasterObservation.material(GL11.GL_DIFFUSE,0,0,0,.625f);
        RasterObservation.material(GL11.GL_SPECULAR,1,1,1,1);
        RasterObservation.material(GL11.GL_EMISSION,0,0,0,1);
        GL11.glMaterialf(GL11.GL_FRONT_AND_BACK,GL11.GL_SHININESS,8);
        RasterObservation.light(0,GL11.GL_DIFFUSE,0,0,0,1);
        RasterObservation.light(0,GL11.GL_SPECULAR,1,.5f,.25f,1);
        GL11.glNormal3f(0,.6f,.8f);
        int texture = 0;
        if (index <= 3) {
            if (index == 1) GL11.glLightModeli(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER,1);
            if (index == 2) GL11.glMaterialf(GL11.GL_FRONT_AND_BACK,GL11.GL_SHININESS,0);
            if (index == 3) GL11.glMaterialf(GL11.GL_FRONT_AND_BACK,GL11.GL_SHININESS,32);
        } else if (index >= 4 && index <= 7) {
            RasterObservation.material(GL11.GL_SPECULAR,0,0,0,1);
            GL11.glMaterial(GL11.GL_FRONT,GL11.GL_DIFFUSE,RasterObservation.vector(1,0,0,.25f));
            GL11.glMaterial(GL11.GL_BACK,GL11.GL_DIFFUSE,RasterObservation.vector(0,1,0,.75f));
            RasterObservation.light(0,GL11.GL_DIFFUSE,1,1,1,1);
            RasterObservation.light(0,GL11.GL_SPECULAR,0,0,0,1);
            if (index != 6) GL11.glLightModeli(GL11.GL_LIGHT_MODEL_TWO_SIDE,1);
            GL11.glNormal3f(0,0,index == 4 ? 1 : -1);
            if (index != 4) reverse(bytes);
            if (index == 7) {
                RasterObservation.constantBytes(bytes,0,0,-128);
                GL11.glColorMaterial(GL11.GL_BACK,GL11.GL_DIFFUSE);
                GL11.glColor4f(.25f,.75f,.5f,.625f); GL11.glEnable(GL11.GL_COLOR_MATERIAL);
            }
        } else if (index >= 8 && index <= 10) {
            RasterObservation.material(GL11.GL_DIFFUSE,.5f,.5f,.5f,.75f);
            RasterObservation.material(GL11.GL_SPECULAR,.5f,.75f,.25f,1);
            RasterObservation.light(0,GL11.GL_DIFFUSE,.5f,.25f,.125f,1);
            RasterObservation.light(0,GL11.GL_SPECULAR,.5f,.5f,.5f,1);
            GL11.glNormal3f(0,0,1); texture = RasterObservation.texture();
            if (index != 8) GL11.glLightModeli(GL12.GL_LIGHT_MODEL_COLOR_CONTROL,GL12.GL_SEPARATE_SPECULAR_COLOR);
            if (index == 10) GL11.glEnable(GL11.GL_FOG);
        } else if (unlit) {
            GL11.glDisable(GL11.GL_LIGHTING);
            GL11.glShadeModel(index == 11 || index == 13 ? GL11.GL_FLAT : GL11.GL_SMOOTH);
        } else if (index == 15 || index == 16) {
            GL11.glEnable(GL11.GL_LIGHT1);
            RasterObservation.material(GL11.GL_DIFFUSE,1,1,1,1);
            RasterObservation.material(GL11.GL_SPECULAR,0,0,0,1);
            for (int light = 0; light < 2; ++light) {
                RasterObservation.light(light,GL11.GL_POSITION,0,0,1,0);
                RasterObservation.light(light,GL11.GL_DIFFUSE,1,1,1,1);
                RasterObservation.light(light,GL11.GL_SPECULAR,0,0,0,1);
            }
            for (int i = 0; i < 3; ++i) {
                bytes.put(i*32+24,(byte)0); bytes.put(i*32+25,(byte)0);
                bytes.put(i*32+26,(byte)new int[]{127,64,0}[i]);
            }
            GL11.glShadeModel(index == 16 ? GL11.GL_FLAT : GL11.GL_SMOOTH);
        } else if (index == 17) {
            GL11.glLightModeli(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER,1);
            RasterObservation.light(0,GL11.GL_POSITION,.25f,.25f,.5f,1);
            RasterObservation.light(0,GL11.GL_DIFFUSE,.25f,.5f,.25f,1);
            RasterObservation.material(GL11.GL_DIFFUSE,.5f,.25f,.75f,.625f);
        }
        RasterObservation.arrays(bytes,normalArray,colorArray,texture != 0);
        GL11.glDrawArrays(fan ? GL11.GL_TRIANGLE_FAN : GL11.GL_TRIANGLES,0,fan ? 6 : 3);
        RasterObservation.pixels(name,0);
        System.out.println("RASTER_MODEL " + name + " 0 " + GL11.glGetInteger(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER) + " " +
            GL11.glGetInteger(GL11.GL_LIGHT_MODEL_TWO_SIDE) + " " + GL11.glGetInteger(GL12.GL_LIGHT_MODEL_COLOR_CONTROL));
        require(GL11.glGetError() == GL11.GL_NO_ERROR,"extended raster/state readback completed");
        if (texture != 0) GL11.glDeleteTextures(texture);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 2,"native/profile arguments");
        Locale.setDefault(Locale.ROOT); Path natives = Path.of(args[0]).toRealPath();
        int index = Integer.parseInt(args[1]); require(index >= 0 && index < NAMES.length,"declared extended profile");
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) { ++network; throw new SecurityException(); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException(); }
            @Override public void checkWrite(String file) { throw new SecurityException(); }
            @Override public void checkLink(String library) {
                links.add(library);
                require(Path.of(library).isAbsolute() && Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")),
                        "only pinned genuine LWJGL native");
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Extended legacy lighting raster controls");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("RASTER_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            for (int channel : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8,"actual RGBA8 framebuffer");
            System.out.println("RASTER_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                GL11.glGetInteger(GL11.GL_STENCIL_BITS)); scene(index); require(network == 0,"no networking");
        } finally { Display.destroy(); }
        System.out.println("Original extended raster: " + NAMES[index] + "; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
