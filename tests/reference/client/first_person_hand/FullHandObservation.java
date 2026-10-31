import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import net.minecraft.client.Minecraft;
import java.util.List;
import java.util.Locale;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Real World/Player and unchanged empty-arm/overlay bodies with prepared RA/cache operands. */
public final class FullHandObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static Field field(Class<?> type, String name) throws Exception {
        Field field = type.getDeclaredField(name);
        field.setAccessible(true);
        return field;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance",Class.class).invoke(field(unsafe,"theUnsafe").get(null),type);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar = Path.of(args[0]).toRealPath(), natives = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host, int port) {
                ++network; throw new SecurityException("network denied");
            }
            @Override public void checkListen(int port) {
                ++network; throw new SecurityException("listeners denied");
            }
            @Override public void checkWrite(String path) {
                throw new SecurityException("file writes denied");
            }
            @Override public void checkLink(String library) {
                require(links.size() < 16, "native link bound"); links.add(library);
                if (!Path.of(library).isAbsolute() || !Path.of(library).normalize()
                    .equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native " + library);
            }
        });
        for (Class<?> type : new Class<?>[]{ra.class, ds.class, fh.class, ps.class, tz.class, ib.class,
                    bt.class, nw.class, ge.class, in.class, fd.class, dc.class, wy.class, ji.class,
                    kq.class, kv.class, th.class, u.class, gr.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar), "foreign original class");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Original full hand observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR " + GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER " + GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION " + GL11.glGetString(GL11.GL_VERSION));
            Minecraft app = (Minecraft)raw(kq.class);
            kv settings = new kv(); app.z = settings;
            fd world = new fd(new wy(), "FirstPersonHandReference", xa.a(0), 23L);
            world.B = false; app.f = world;
            ei info = (ei)field(fd.class, "x").get(world); info.a(6000L); world.k();
            lm chunk = world.c(0,0);
            chunk.b[8*2048+8*128+127] = 0;
            chunk.f.a(8,127,8,15); chunk.g.a(8,127,8,0);
            require(world.c(8,200,8)==1.0f,"actual full hand brightness");
            dc player = new dc(app, world, new gr("HandReference", "-"), 0);
            player.e(8.5, 200.5, 8.5); app.h = player; app.i = player;
            player.aS = 33; player.aU = 12; player.aT = -17; player.aV = 3;
            player.W = .1f; player.X = .5f;
            ji engine = (ji)raw(ji.class); app.p = engine;
            HashMap<String,Integer> cache = new HashMap<>();
            field(ji.class, "b").set(engine, cache);
            field(ji.class, "h").set(engine, new HashMap<>());
            field(ji.class, "l").set(engine, raw(ik.class));
            int texture = GL11.glGenTextures();
            ByteBuffer image = BufferUtils.createByteBuffer(64*32*4);
            for(int i=0;i<64*32*4;++i)image.put((byte)255);image.flip();
            GL11.glBindTexture(GL11.GL_TEXTURE_2D,texture);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
            GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
            GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,64,32,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,image);
            cache.put("/mob/char.png",texture);
            ra renderer = (ra)raw(ra.class);
            field(ra.class,"a").set(renderer,app);
            field(ra.class,"c").setFloat(renderer,.75f);
            field(ra.class,"d").setFloat(renderer,.25f);
            int[] actual = (int[])field(nw.class,"g").get(nw.a);
            IntBuffer integers=(IntBuffer)field(nw.class,"e").get(nw.a);
            FloatBuffer matrix=BufferUtils.createFloatBuffer(16);
            FloatBuffer color=BufferUtils.createFloatBuffer(16);
            for(int entry=0;entry<2;++entry)for(int partial=0;partial<2;++partial){
                Arrays.fill(actual,0);integers.position(0);bt.b();
                GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
                GL11.glColor4f(.125f,.25f,.5f,.75f);
                GL11.glEnable(GL11.GL_ALPHA_TEST);GL11.glDisable(32826);
                GL11.glDisable(GL11.GL_LIGHTING);GL11.glDisable(GL11.GL_LIGHT0);GL11.glDisable(GL11.GL_LIGHT1);
                GL11.glDisable(GL11.GL_COLOR_MATERIAL);GL11.glDisable(GL11.GL_FOG);
                if(entry==0)renderer.a(partial==0?.375f:1f);else renderer.b(partial==0?.375f:1f);
                GL11.glGetFloat(GL11.GL_CURRENT_COLOR,color);GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,matrix);
                System.out.printf("HAND_DRAW %d %d %d %d",entry,partial,GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),field(bt.class,"e").getInt(null));
                for(int i=0;i<16;++i)System.out.printf(" %08x",Float.floatToRawIntBits(matrix.get(i)));
                for(int i=0;i<4;++i)System.out.printf(" %08x",Float.floatToRawIntBits(color.get(i)));
                for(int cap:new int[]{GL11.GL_ALPHA_TEST,32826,GL11.GL_LIGHTING,GL11.GL_LIGHT0,GL11.GL_LIGHT1,GL11.GL_COLOR_MATERIAL})System.out.print(" "+(GL11.glIsEnabled(cap)?1:0));
                System.out.println(" "+integers.position());
                require(GL11.glGetError()==GL11.GL_NO_ERROR,"actual full hand GL error");
            }
            require(network == 0, "network attempt");
            System.out.println("Original full hand: 4 rows; network=0; display=1");
            for (String library : links) System.out.println("NATIVE_LINK " + library);
        } finally { Display.destroy(); }
    }
}
