import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GLContext;

/** Unchanged PX.a(IF), declared prepared fields and genuine native GL state. */
public final class FogStateObservation {
    public static final class TransportWorld extends fd {
        int block;
        TransportWorld() { super((wt)null, "unreached", 0L); }
        @Override public int a(int x, int y, int z) { return block; }
        @Override public int e(int x, int y, int z) { return 0; }
    }
    static int network;
    static final List<String> nativeLinks = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field result = type.getDeclaredField(name); result.setAccessible(true); return result;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
            .invoke(field(unsafe, "theUnsafe").get(null), type);
    }
    static int floatState(int pname, int index) {
        FloatBuffer buffer = BufferUtils.createFloatBuffer(16);
        GL11.glGetFloat(pname, buffer);
        return Float.floatToRawIntBits(buffer.get(index));
    }
    static void observe(int dimension, int anaglyph, int pass, int branch,
                        boolean nv, FloatBuffer buffer) {
        System.out.printf("FOG_STATE %d %d %d %d %d", dimension, anaglyph, pass, branch,
                          GL11.glGetInteger(GL11.GL_FOG_MODE));
        for (int pname : new int[]{GL11.GL_FOG_DENSITY,GL11.GL_FOG_START,GL11.GL_FOG_END})
            System.out.printf(" %08x", floatState(pname, 0));
        for (int i = 0; i < 4; ++i) System.out.printf(" %08x", floatState(GL11.GL_FOG_COLOR, i));
        for (int i = 0; i < 3; ++i) System.out.printf(" %08x", floatState(GL11.GL_CURRENT_NORMAL, i));
        for (int i = 0; i < 4; ++i) System.out.printf(" %08x", floatState(GL11.GL_CURRENT_COLOR, i));
        System.out.printf(" %d %d %d %d %d %d %d %d%n",
            GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL) ? 1 : 0,
            GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_FACE),
            GL11.glGetInteger(GL11.GL_COLOR_MATERIAL_PARAMETER), nv ? 1 : 0,
            nv ? GL11.glGetInteger(34138) : -1,
            buffer.position(), buffer.limit(), buffer.capacity());
    }
    static void pixels(boolean nv) {
        GL11.glViewport(0,0,64,64);
        GL11.glMatrixMode(GL11.GL_PROJECTION); GL11.glLoadIdentity();
        GL11.glScalef(0.125f,0.125f,0.03125f);
        GL11.glMatrixMode(GL11.GL_MODELVIEW); GL11.glLoadIdentity();
        for (int feature : new int[]{GL11.GL_TEXTURE_2D,GL11.GL_LIGHTING,GL11.GL_COLOR_MATERIAL,
                GL11.GL_BLEND,GL11.GL_ALPHA_TEST,GL11.GL_DEPTH_TEST,GL11.GL_CULL_FACE,
                GL11.GL_DITHER,GL11.GL_SCISSOR_TEST,32925}) GL11.glDisable(feature);
        GL11.glEnable(GL11.GL_FOG);
        FloatBuffer color = BufferUtils.createFloatBuffer(16);
        color.put(new float[]{0,0,0,1}).flip();
        GL11.glFog(GL11.GL_FOG_COLOR,color);
        GL11.glFogi(GL11.GL_FOG_MODE,GL11.GL_LINEAR);
        GL11.glFogf(GL11.GL_FOG_START,0); GL11.glFogf(GL11.GL_FOG_END,24);
        GL11.glColor4f(1,1,1,1);
        GL11.glClearColor(0,0,0,0);
        System.out.printf("FOG_PIXEL_PROFILE %d %d%n", nv ? 1 : 0, GL11.glGetInteger(GL11.GL_FOG_HINT));
        int[][] points = {{16,16},{32,16},{32,32},{24,24},{40,24}};
        int[] modes = {GL11.GL_LINEAR,GL11.GL_EXP,GL11.GL_EXP2};
        String[] tags = {"FOG_PIXEL","FOG_PIXEL_EXP","FOG_PIXEL_EXP2"};
        for (int mode = 0; mode < modes.length; ++mode) {
          GL11.glFogi(GL11.GL_FOG_MODE,modes[mode]);
          if (mode != 0) GL11.glFogf(GL11.GL_FOG_DENSITY,0.1f);
          for (int group = 0; group < (nv ? 2 : 1); ++group) {
            if (nv) GL11.glFogi(34138, group == 0 ? 34139 : 34140);
            GL11.glClear(GL11.GL_COLOR_BUFFER_BIT | GL11.GL_DEPTH_BUFFER_BIT);
            GL11.glBegin(GL11.GL_TRIANGLES);
            GL11.glVertex3f(-6,-6,-2); GL11.glVertex3f(6,-6,-18); GL11.glVertex3f(0,6,-2);
            GL11.glEnd();
            GL11.glReadBuffer(GL11.GL_BACK);
            for (int[] point : points) {
                ByteBuffer pixel = BufferUtils.createByteBuffer(16);
                GL11.glReadPixels(point[0],point[1],1,1,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
                System.out.printf("%s %d %d %d %d %d %d %d%n",tags[mode],
                    nv ? GL11.glGetInteger(34138) : -1,point[0],point[1],
                    pixel.get(0)&255,pixel.get(1)&255,pixel.get(2)&255,pixel.get(3)&255);
            }
          }
        }
        require(GL11.glGetError() == GL11.GL_NO_ERROR,"controlled fog triangle GL error");
    }
    @SuppressWarnings("removal")
    public static void main(String[] arguments) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(arguments[0]).toRealPath();
        Path natives = Path.of(arguments[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) {
                ++network; throw new SecurityException("observer denies network");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("observer denies listeners");
            }
            @Override public void checkWrite(String file) {
                throw new SecurityException("observer denies writes");
            }
            @Override public void checkLink(String library) {
                nativeLinks.add(library);
                boolean lwjgl = Path.of(library).isAbsolute() &&
                    Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll"));
                if (!lwjgl) throw new SecurityException("unexpected native link " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{px.class,kq.class,fd.class,dc.class,kv.class,ge.class,xa.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar),"foreign original class");
        Display.setDisplayMode(new DisplayMode(64,64));
        Display.setTitle("Original PX fog-state observation");
        try {
            Display.create();
            System.out.println("FOG_DRIVER "+GL11.glGetString(GL11.GL_VENDOR)+" | "+
                GL11.glGetString(GL11.GL_RENDERER)+" | "+GL11.glGetString(GL11.GL_VERSION));
            boolean nv = GLContext.getCapabilities().GL_NV_fog_distance;
            System.out.printf("FOG_INITIAL %d %d%n",nv?1:0,nv?GL11.glGetInteger(34138):-1);
            px renderer = (px)raw(px.class);
            Minecraft app = (Minecraft)raw(kq.class);
            TransportWorld world = (TransportWorld)raw(TransportWorld.class);
            dc camera = (dc)raw(dc.class);
            kv settings = (kv)raw(kv.class);
            field(fd.class,"x").set(world,new ei(23L,"Fog"));
            xa[] dimensions = {new rh(),new wd()};
            for (xa dimension : dimensions) dimension.a(world);
            camera.aI=world; camera.aM=8.5; camera.aN=64.5; camera.aO=8.5;
            app.i=camera; app.f=world; app.z=settings;
            field(px.class,"j").set(renderer,app);
            FloatBuffer buffer = ge.e(16);
            field(px.class,"f").set(renderer,buffer);
            renderer.g=0.2f; renderer.h=0.4f; renderer.i=0.6f;
            field(px.class,"k").setFloat(renderer,256);
            Class.forName("in",true,FogStateObservation.class.getClassLoader());
            Method method = px.class.getDeclaredMethod("a",int.class,float.class);
            method.setAccessible(true);
            for (int dimension=0;dimension<2;++dimension)
                for (int anaglyph=0;anaglyph<2;++anaglyph)
                    for (int pass : new int[]{-1,0,1})
                        for (int branch=0;branch<4;++branch) {
                            field(fd.class,"t").set(world,dimensions[dimension]);
                            settings.g=anaglyph!=0;
                            field(px.class,"D").setBoolean(renderer,branch==0);
                            world.block=branch==1?9:branch==2?11:0;
                            float partial=pass<0?0.375f:pass==0?Float.NEGATIVE_INFINITY:
                                Float.intBitsToFloat(0x7fc12345);
                            method.invoke(renderer,pass,partial);
                            observe(dimension,anaglyph,pass,branch,nv,buffer);
                            require(renderer.g==0.2f && renderer.h==0.4f && renderer.i==0.6f,
                                    "source RGB fields mutated");
                            require(GL11.glGetError()==GL11.GL_NO_ERROR,"source fog-state GL error");
                        }
            pixels(nv);
            require(network==0,"external work attempted");
            System.out.println("Original fog state: 48 rows; network=0; display=1; buffer=1");
            for (String link : nativeLinks) System.out.println("NATIVE_LINK "+link);
        } finally { Display.destroy(); }
    }
}
