import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.PixelFormat;

/** Actual double-coordinate NW geometry, separate from the frozen empty controls. */
public final class ConvertedRasterObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static Field field(String name) throws Exception {
        Field value = nw.class.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        require(args.length == 3,"JAR/native/profile arguments"); Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        int index = Integer.parseInt(args[2]); require(index == 0 || index == 1,"declared actual quad profile");
        String name = index == 0 ? "actual_quad_flat" : "actual_quad_smooth";
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
        for (Class<?> type : new Class<?>[]{nw.class,ge.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
                    "unchanged original archive authority");
        Display.setDisplayMode(new DisplayMode(64,64)); Display.setTitle("Actual converted NW lighting");
        try {
            Display.create(new PixelFormat().withAlphaBits(8).withDepthBits(16));
            System.out.println("RASTER_DRIVER " + GL11.glGetString(GL11.GL_VENDOR) + " | " +
                GL11.glGetString(GL11.GL_RENDERER) + " | " + GL11.glGetString(GL11.GL_VERSION));
            for (int channel : new int[]{GL11.GL_RED_BITS,GL11.GL_GREEN_BITS,GL11.GL_BLUE_BITS,GL11.GL_ALPHA_BITS})
                require(GL11.glGetInteger(channel) == 8,"actual RGBA8 framebuffer");
            System.out.println("RASTER_FORMAT 8 8 8 8 " + GL11.glGetInteger(GL11.GL_DEPTH_BITS) + " " +
                GL11.glGetInteger(GL11.GL_STENCIL_BITS));
            RasterObservation.setup(false); GL11.glEnable(GL11.GL_COLOR_MATERIAL);
            GL11.glShadeModel(index == 0 ? GL11.GL_FLAT : GL11.GL_SMOOTH);
            nw value = nw.a; value.b();
            double[][] positions = {{-.75,-.75,0},{.75,-.75,0},{.75,.75,0},{-.75,.75,0}};
            float[][] normals = {{0,0,1},{.5f,0,.5f},{0,1,0},{0,0,-1}};
            for (int i = 0; i < 4; ++i) {
                int[] color = RasterObservation.COLORS[i]; value.a(color[0],color[1],color[2],color[3]);
                value.b(normals[i][0],normals[i][1],normals[i][2]);
                value.a(positions[i][0],positions[i][1],positions[i][2]);
            }
            require(field("h").getInt(value) == 6 && field("o").getInt(value) == 48 &&
                    field("n").getBoolean(value) && field("l").getBoolean(value) &&
                    !field("m").getBoolean(value) && field("w").getBoolean(value),
                    "actual four source vertices produce six normal/color vertices and 48 words");
            int[] array = (int[])field("g").get(value);
            require(array[3*8+6] == 0 && array[4*8+6] == 0,"fresh duplicate normal words remain untouched zeros");
            StringBuilder text = new StringBuilder("CONVERTED_WORDS " + name + " 6 48 ");
            for (int i = 0; i < 48; ++i) text.append(i == 0 ? "" : ",").append(String.format("%08x",array[i]));
            System.out.println(text); value.a(); RasterObservation.pixels(name,0);
            require(network == 0,"no network/listener attempts");
        } finally { Display.destroy(); }
        System.out.println("Original converted raster: " + name + "; cleanup-complete; network=0");
        for (String link : links) System.out.println("NATIVE_LINK " + link);
    }
}
