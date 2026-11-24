import java.nio.ByteBuffer;
import java.nio.ByteOrder;
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
import org.lwjgl.opengl.GL12;
import org.lwjgl.opengl.PixelFormat;

/** Authored legacy-GL scenes and two unchanged NW quad draws. */
public final class RasterObservation {
    static final String[] NAMES = {
        "byte_m128_negative", "byte_m127_negative", "byte_m1_negative", "byte_zero_positive",
        "byte_p1_positive", "byte_p64_positive", "byte_p126_positive", "byte_p127_positive",
        "byte_x_zero", "byte_mixed", "current_zero", "current_half", "current_unit",
        "current_controls_00", "current_controls_10", "current_controls_01", "current_controls_11",
        "byte_controls_00", "byte_controls_10", "byte_controls_01", "byte_controls_11",
        "triangle_flat", "triangle_smooth", "fan_flat", "fan_smooth",
        "rotate_after_upload", "rotate_before_upload", "mirrored_scale",
        "material_ambient", "material_diffuse", "material_emission", "material_specular",
        "material_switch_freeze", "list_replay", "texture_alpha", "texture_fog",
        "converted_quad_flat", "converted_quad_smooth"
    };
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static FloatBuffer vector(float... values) {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(16); buffer.put(values).flip(); return buffer;
    }
    static void material(int parameter, float... values) {
        GL11.glMaterial(GL11.GL_FRONT_AND_BACK, parameter, vector(values));
    }
    static void light(int index, int parameter, float... values) {
        GL11.glLight(GL11.GL_LIGHT0 + index, parameter, vector(values));
    }
    static void normalControls(int controls) {
        if ((controls & 1) != 0) GL11.glEnable(GL11.GL_NORMALIZE); else GL11.glDisable(GL11.GL_NORMALIZE);
        if ((controls & 2) != 0) GL11.glEnable(GL12.GL_RESCALE_NORMAL); else GL11.glDisable(GL12.GL_RESCALE_NORMAL);
    }
    static void setup(boolean simple) {
        GL11.glViewport(0, 0, 64, 64);
        GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
        GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
        for (int capability : new int[]{GL11.GL_DITHER, GL11.GL_BLEND, GL11.GL_ALPHA_TEST,
                GL11.GL_DEPTH_TEST, GL11.GL_CULL_FACE, GL11.GL_TEXTURE_2D, GL11.GL_FOG,
                GL11.GL_COLOR_MATERIAL}) GL11.glDisable(capability);
        GL11.glEnable(GL11.GL_LIGHTING); GL11.glEnable(GL11.GL_LIGHT0);
        if (simple) GL11.glDisable(GL11.GL_LIGHT1); else GL11.glEnable(GL11.GL_LIGHT1);
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_LOCAL_VIEWER, 0);
        GL11.glLightModeli(GL11.GL_LIGHT_MODEL_TWO_SIDE, 0);
        GL11.glLightModel(GL11.GL_LIGHT_MODEL_AMBIENT, simple ? vector(0,0,0,1) : vector(.125f,.25f,.125f,1));
        material(GL11.GL_AMBIENT, simple ? new float[]{0,0,0,1} : new float[]{.2f,.2f,.2f,1});
        material(GL11.GL_DIFFUSE, simple ? new float[]{1,1,1,1} : new float[]{.8f,.8f,.8f,1});
        material(GL11.GL_SPECULAR, 0,0,0,1); material(GL11.GL_EMISSION, 0,0,0,1);
        GL11.glMaterialf(GL11.GL_FRONT_AND_BACK, GL11.GL_SHININESS, 0);
        light(0, GL11.GL_POSITION, 0,0,1,0); light(1, GL11.GL_POSITION, 1,0,0,0);
        light(0, GL11.GL_DIFFUSE, simple ? new float[]{1,1,1,1} : new float[]{.5f,.25f,.125f,1});
        light(1, GL11.GL_DIFFUSE, .125f,.5f,.25f,1);
        for (int index = 0; index < 2; ++index) {
            light(index, GL11.GL_AMBIENT, 0,0,0,1); light(index, GL11.GL_SPECULAR, 0,0,0,1);
        }
        GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK, GL11.GL_AMBIENT_AND_DIFFUSE);
        GL11.glColor4f(1,1,1,1); GL11.glNormal3f(0,0,1); normalControls(0);
        GL11.glShadeModel(GL11.GL_SMOOTH); GL11.glAlphaFunc(GL11.GL_GREATER, .1f);
        GL11.glFogi(GL11.GL_FOG_MODE, GL11.GL_LINEAR); GL11.glFogf(GL11.GL_FOG_START, 0);
        GL11.glFogf(GL11.GL_FOG_END, 1); GL11.glFog(GL11.GL_FOG_COLOR, vector(.25f,.5f,.75f,1));
        GL11.glClearColor(0,0,0,0); GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);
    }
    static final int[][] COLORS = {
        {128,64,32,255}, {32,192,64,160}, {64,32,224,96},
        {224,128,64,224}, {64,224,128,128}, {128,64,32,255}
    };
    static ByteBuffer geometry(boolean fan, boolean scaled, boolean fog) {
        float[][] positions = fan ? new float[][]{{0,0,0},{-.75f,-.75f,0},{.75f,-.75f,0},
            {.75f,.75f,0},{-.75f,.75f,0},{-.75f,-.75f,0}} :
            new float[][]{{-.75f,-.75f,0},{.75f,-.75f,0},{-.75f,.75f,0}};
        ByteBuffer bytes = BufferUtils.createByteBuffer(positions.length * 32).order(ByteOrder.nativeOrder());
        int[][] normals = {{0,0,127},{64,0,64},{0,127,0},{-64,64,64},{0,-64,127},{0,0,127}};
        for (int i = 0; i < positions.length; ++i) {
            int offset = i * 32;
            bytes.putFloat(offset, positions[i][0] / (scaled ? 2 : 1));
            bytes.putFloat(offset + 4, positions[i][1] / (scaled ? 3 : 1));
            bytes.putFloat(offset + 8, fog ? new float[]{-.5f,-.25f,-.75f,-.5f,-.25f,-.75f}[i] : 0);
            bytes.putFloat(offset + 12, (positions[i][0] + .75f) / 1.5f);
            bytes.putFloat(offset + 16, (positions[i][1] + .75f) / 1.5f);
            for (int component = 0; component < 4; ++component) bytes.put(offset + 20 + component, (byte)COLORS[i][component]);
            for (int component = 0; component < 3; ++component) bytes.put(offset + 24 + component, (byte)normals[i][component]);
        }
        return bytes;
    }
    static void constantBytes(ByteBuffer bytes, int x, int y, int z) {
        for (int i = 0; i < bytes.capacity() / 32; ++i) {
            bytes.put(i * 32 + 24, (byte)x); bytes.put(i * 32 + 25, (byte)y); bytes.put(i * 32 + 26, (byte)z);
        }
    }
    static void arrays(ByteBuffer bytes, boolean normals, boolean colors, boolean texture) {
        GL11.glEnableClientState(GL11.GL_VERTEX_ARRAY); bytes.position(0); GL11.glVertexPointer(3, 32, bytes.asFloatBuffer());
        if (normals) {
            GL11.glEnableClientState(GL11.GL_NORMAL_ARRAY); bytes.position(24); GL11.glNormalPointer(32, bytes);
        } else GL11.glDisableClientState(GL11.GL_NORMAL_ARRAY);
        if (colors) {
            GL11.glEnableClientState(GL11.GL_COLOR_ARRAY); bytes.position(20); GL11.glColorPointer(4, true, 32, bytes);
        } else GL11.glDisableClientState(GL11.GL_COLOR_ARRAY);
        if (texture) {
            GL11.glEnableClientState(GL11.GL_TEXTURE_COORD_ARRAY); bytes.position(12); GL11.glTexCoordPointer(2, 32, bytes.asFloatBuffer());
        } else GL11.glDisableClientState(GL11.GL_TEXTURE_COORD_ARRAY);
        bytes.position(0);
    }
    static void state(String name, int frame) {
        StringBuilder flags = new StringBuilder();
        for (int capability : new int[]{GL11.GL_LIGHTING, GL11.GL_LIGHT0, GL11.GL_LIGHT1,
                GL11.GL_NORMALIZE, GL12.GL_RESCALE_NORMAL, GL11.GL_COLOR_MATERIAL,
                GL11.GL_TEXTURE_2D, GL11.GL_ALPHA_TEST, GL11.GL_FOG}) flags.append(GL11.glIsEnabled(capability) ? '1' : '0');
        System.out.println("RASTER_STATE " + name + " " + frame + " " + flags + " " +
            GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE) + " " + GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER) + " " +
            GL11.glGetInteger(GL11.GL_SHADE_MODEL) + " " + GL11.glGetInteger(GL11.GL_FOG_MODE));
    }
    static void pixels(String name, int frame) {
        ByteBuffer pixels = BufferUtils.createByteBuffer(64 * 64 * 4); GL11.glReadBuffer(GL11.GL_BACK);
        GL11.glPixelStorei(GL11.GL_PACK_ALIGNMENT, 1);
        GL11.glReadPixels(0,0,64,64,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixels);
        StringBuilder text = new StringBuilder(64 * 64 * 8);
        for (int y = 63; y >= 0; --y)
            for (int x = 0; x < 64 * 4; ++x) text.append(String.format("%02x", pixels.get(y * 64 * 4 + x) & 255));
        System.out.println("RASTER " + name + " " + frame + " " + text);
        state(name, frame);
        require(GL11.glGetError() == GL11.GL_NO_ERROR, "actual raster/readback completed");
    }
    static int texture() {
        int id = GL11.glGenTextures(); GL11.glBindTexture(GL11.GL_TEXTURE_2D, id);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MIN_FILTER, GL11.GL_NEAREST);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MAG_FILTER, GL11.GL_NEAREST);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_S, GL11.GL_REPEAT);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_T, GL11.GL_REPEAT);
        ByteBuffer bytes = BufferUtils.createByteBuffer(16);
        bytes.put(new byte[]{(byte)255,64,32,0, 32,(byte)255,64,(byte)128,
                            64,32,(byte)255,(byte)255, (byte)255,(byte)255,(byte)255,(byte)255}).flip();
        GL11.glTexImage2D(GL11.GL_TEXTURE_2D, 0, GL11.GL_RGBA, 2,2,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,bytes);
        GL11.glEnable(GL11.GL_TEXTURE_2D); GL11.glEnable(GL11.GL_ALPHA_TEST); return id;
    }
    static void converted(boolean flat) {
        GL11.glEnable(GL11.GL_COLOR_MATERIAL); GL11.glShadeModel(flat ? GL11.GL_FLAT : GL11.GL_SMOOTH);
        nw tessellator = nw.a; tessellator.b();
        float[][] positions = {{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}};
        float[][] normals = {{0,0,1},{.5f,0,.5f},{0,1,0},{0,0,-1}};
        for (int i = 0; i < 4; ++i) {
            tessellator.a(COLORS[i][0], COLORS[i][1], COLORS[i][2], COLORS[i][3]);
            tessellator.b(normals[i][0],normals[i][1],normals[i][2]);
            tessellator.a(positions[i][0],positions[i][1],positions[i][2]);
        }
        tessellator.a();
    }
    static void scene(int index) {
        String name = NAMES[index]; boolean simple = index <= 20 || index >= 25 && index <= 27;
        setup(simple);
        if (index >= 36) { converted(index == 36); pixels(name,0); return; }
        boolean fan = index == 23 || index == 24 || index == 34 || index == 35;
        boolean controls = index >= 13 && index <= 20;
        ByteBuffer bytes = geometry(fan, controls, index == 35);
        boolean normalArray = index < 10 || index >= 17 && index <= 24 || index == 33 || index >= 34;
        boolean colorArray = index >= 21 && index <= 24 || index == 33;
        int texture = 0;
        if (index < 8) {
            int value = new int[]{-128,-127,-1,0,1,64,126,127}[index]; constantBytes(bytes,0,0,value);
            if (value < 0) light(0,GL11.GL_POSITION,0,0,-1,0);
        } else if (index == 8) { constantBytes(bytes,0,0,0); light(0,GL11.GL_POSITION,1,0,0,0); }
        else if (index == 9) {
            constantBytes(bytes,127,-127,0); GL11.glEnable(GL11.GL_LIGHT1);
            light(0,GL11.GL_POSITION,1,0,0,0); light(0,GL11.GL_DIFFUSE,1,0,0,1);
            light(1,GL11.GL_POSITION,0,-1,0,0); light(1,GL11.GL_DIFFUSE,0,1,0,1);
        } else if (index <= 12) { GL11.glNormal3f(0,0,new float[]{0,.5f,1}[index-10]); }
        else if (controls) {
            GL11.glScaled(2,3,4); normalControls((index - (index < 17 ? 13 : 17)));
            GL11.glNormal3f(0,0,.5f); if (normalArray) constantBytes(bytes,16,64,64);
        } else if (index <= 24) {
            GL11.glEnable(GL11.GL_COLOR_MATERIAL); GL11.glShadeModel(index % 2 == 1 ? GL11.GL_FLAT : GL11.GL_SMOOTH);
        } else if (index == 25 || index == 26) {
            GL11.glNormal3f(1,0,0);
            if (index == 25) light(0,GL11.GL_POSITION,1,0,0,0);
            GL11.glRotatef(90,0,0,1);
            if (index == 26) light(0,GL11.GL_POSITION,1,0,0,0);
        } else if (index == 27) {
            light(0,GL11.GL_POSITION,1,0,0,0); GL11.glNormal3f(1,0,0); GL11.glScalef(-1,1,1);
        } else if (index >= 28 && index <= 31) {
            GL11.glColor4f(.75f,.5f,.25f,.625f);
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,new int[]{GL11.GL_AMBIENT,GL11.GL_DIFFUSE,GL11.GL_EMISSION,GL11.GL_SPECULAR}[index-28]);
            GL11.glEnable(GL11.GL_COLOR_MATERIAL); GL11.glNormal3f(0,0,1);
        } else if (index == 32) {
            GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_AMBIENT); GL11.glEnable(GL11.GL_COLOR_MATERIAL);
            GL11.glColor4f(.25f,.5f,.75f,.5f); GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_DIFFUSE);
            GL11.glColor4f(.75f,.25f,.5f,.75f); GL11.glColorMaterial(GL11.GL_FRONT_AND_BACK,GL11.GL_EMISSION);
            GL11.glColor4f(.125f,.25f,.125f,1); GL11.glDisable(GL11.GL_COLOR_MATERIAL);
            GL11.glColor4f(.875f,.5f,.25f,.125f);
        } else if (index >= 33) {
            GL11.glEnable(GL11.GL_COLOR_MATERIAL);
            if (index >= 34) { GL11.glColor4f(.75f,.5f,.25f,.625f); texture = texture(); }
            if (index == 35) GL11.glEnable(GL11.GL_FOG);
        }
        arrays(bytes,normalArray,colorArray,texture != 0);
        if (index == 33) {
            int id = GL11.glGenLists(1); require(id != 0, "actual list allocation");
            GL11.glNewList(id,GL11.GL_COMPILE); GL11.glColor4f(.75f,.5f,.25f,.625f);
            GL11.glPushMatrix(); GL11.glScaled(1,1,.5); GL11.glDrawArrays(GL11.GL_TRIANGLES,0,3);
            GL11.glPopMatrix(); GL11.glEndList();
            for (int i = 0; i < bytes.capacity(); ++i) bytes.put(i,(byte)0);
            GL11.glCallList(id); pixels(name,0);
            GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);
            GL11.glLoadIdentity(); GL11.glRotatef(90,0,0,1); GL11.glScaled(.5,.5,2);
            GL11.glCallList(id); pixels(name,1); GL11.glDeleteLists(id,1);
        } else {
            GL11.glDrawArrays(fan ? GL11.GL_TRIANGLE_FAN : GL11.GL_TRIANGLES,0,fan ? 6 : 3); pixels(name,0);
        }
        if (texture != 0) GL11.glDeleteTextures(texture);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 3, "JAR/native/profile arguments"); Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        int index = Integer.parseInt(args[2]); require(index >= 0 && index < NAMES.length, "declared profile index");
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
        require(Path.of(nw.class.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                "unchanged NW archive authority");
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Legacy lighting raster controls");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("RASTER_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            for (int channel : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8, "actual RGBA8 framebuffer");
            System.out.println("RASTER_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                GL11.glGetInteger(GL11.GL_STENCIL_BITS)); scene(index); require(network == 0, "no network/listener attempts");
        } finally { Display.destroy(); }
        System.out.println("Original raster: " + NAMES[index] + "; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
