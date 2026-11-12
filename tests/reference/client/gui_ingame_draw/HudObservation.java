import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Unchanged UQ draw over actual constructor-created World/Player/controller/UQ. */
public final class HudObservation {
    static int network;
    static final List<String> links = new ArrayList<>();
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
    static int texture(int variant) {
        int texture = GL11.glGenTextures();
        ByteBuffer pixels = BufferUtils.createByteBuffer(256*256*4);
        for (int y=0;y<256;++y) for (int x=0;x<256;++x) {
            pixels.put((byte)(variant==0 ? 160+x/4 : 64+x/2));
            pixels.put((byte)(variant==0 ? 32+y/8 : 64+y/2));
            pixels.put((byte)(variant==0 ? 192 : 240)); pixels.put((byte)255);
        }
        pixels.flip(); GL11.glBindTexture(GL11.GL_TEXTURE_2D, texture);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
        GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,256,256,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixels);
        return texture;
    }
    static String hex(byte[] data) {
        StringBuilder result = new StringBuilder();
        for (byte value:data) result.append(String.format("%02x", value&255));
        return result.toString();
    }
    static String digest(byte[] data) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(data));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(), natives=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission permission) {}
            @Override public void checkConnect(String host,int port) { ++network; throw new SecurityException("network denied"); }
            @Override public void checkListen(int port) { ++network; throw new SecurityException("listeners denied"); }
            @Override public void checkWrite(String path) { throw new SecurityException("file writes denied"); }
            @Override public void checkLink(String library) {
                require(links.size()<16,"native link bound"); links.add(library);
                if (!Path.of(library).isAbsolute() || !Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native "+library);
            }
        });
        for (Class<?> type:new Class<?>[]{uq.class,ub.class,qq.class,px.class,ob.class,fd.class,
                wy.class,dc.class,gs.class,ls.class,ix.class,ji.class,nw.class,u.class,bt.class,
                kv.class,kq.class,bb.class,in.class,sn.class,eq.class,ln.class,xa.class,ei.class,
                gr.class,ik.class,ge.class,gc.class,sw.class,sj.class,Minecraft.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");
        require(Random.class.getClassLoader()==null && Random.class.getModule().getName().equals("java.base"),"foreign Random class");
        Display.setDisplayMode(new DisplayMode(320,240));
        Display.setTitle("Original GuiIngame observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR "+GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER "+GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION "+GL11.glGetString(GL11.GL_VERSION));
            Minecraft app=(Minecraft)raw(kq.class);
            field(Minecraft.class,"a").set(null,app);
            kv settings=new kv(); app.z=settings;
            settings.j=false; settings.A=false; settings.B=false;
            fd world=new fd(new wy(),"GuiIngameReference",xa.a(0),23L); world.B=false; app.f=world;
            ei info=(ei)field(fd.class,"x").get(world); info.a(6000L); world.k();
            dc player=new dc(app,world,new gr("HudReference","-"),0); player.e(8.5,200.5,8.5);
            app.h=player; app.i=player; app.c=new ob(app);
            px renderer=(px)raw(px.class); field(px.class,"j").set(renderer,app); app.t=renderer;
            ji engine=(ji)raw(ji.class); app.p=engine;
            HashMap<String,Integer> cache=new HashMap<>(); field(ji.class,"b").set(engine,cache);
            field(ji.class,"h").set(engine,new HashMap<>()); field(ji.class,"l").set(engine,raw(ik.class));
            cache.put("/gui/gui.png",texture(0)); cache.put("/gui/icons.png",texture(1));
            cache.put("%blur%/misc/vignette.png",texture(1));
            lm chunk=world.c(0,0);chunk.b[8*2048+8*128+127]=0;
            chunk.f.a(8,127,8,15);chunk.g.a(8,127,8,0);
            uq hud=new uq(app); app.v=hud;
            require(player.s()==0 && player.P()==0 && !player.a(ln.g),"default armor/sleep/water");
            for (iz stack:player.c.a) require(stack==null,"default inventory is not empty");
            for (int slot=0;slot<4;++slot) require(player.c.d(slot)==null,"default armor is not empty");
            int[] actual=(int[])field(nw.class,"g").get(nw.a);
            IntBuffer integers=(IntBuffer)field(nw.class,"e").get(nw.a);
            Random random=(Random)field(uq.class,"f").get(hud);
            AtomicLong seed=(AtomicLong)field(Random.class,"seed").get(random);
            FloatBuffer modelview=BufferUtils.createFloatBuffer(16),projection=BufferUtils.createFloatBuffer(16),color=BufferUtils.createFloatBuffer(16);
            int index=0;
            for (int profile=0;profile<26;++profile) {
                int fancy=profile<24?profile/12:1,size=profile<24?(profile/6)%2:profile-24;
                int health=profile<24?(profile/2)%3:0,partial=profile<24?profile%2:0;
                settings.j=fancy!=0;hud.c=fancy==0?1f:.75f;
                info.a(fancy==0?6000L:18000L);world.k();
                player.e(8.5,profile<24?200.5:120.5,8.5);
                if(profile>=24) {
                    chunk.b[8*2048+8*128+120]=0;chunk.f.a(8,120,8,15);chunk.g.a(8,120,8,0);
                    int sampleX=in.b(player.aM),sampleY=in.b(player.aN-player.bf+(player.aW.e-player.aW.b)*.66),sampleZ=in.b(player.aO);
                    boolean bounds=world.a(in.b(player.aW.a),in.b(player.aW.b),in.b(player.aW.c),in.b(player.aW.d),in.b(player.aW.e),in.b(player.aW.f));
                    require(bounds && sampleX==8 && sampleY==120 && sampleZ==8,"actual in-height brightness bounds/sample");
                    float light=world.c(sampleX,sampleY,sampleZ);
                    require(player.a(.375f)==light && light>0f && light<1f,"actual in-height brightness light path");
                    System.out.printf("HUD_LIGHT %d %d %d %d 1 %08x%n",index,sampleX,sampleY,sampleZ,Float.floatToRawIntBits(light));
                }
                app.d=size==0?320:641; app.e=size==0?240:481; settings.I=size==0?0:2;
                player.Y=new int[]{20,3,11}[health]; player.Z=new int[]{20,7,13}[health]; player.by=health==0?0:15;
                player.c.c=health==0?0:health==1?8:4;
                field(uq.class,"h").setInt(hud,size==0?17:-2147483647);
                System.out.printf("HUD_INPUTS %d %d %08x %d%n",index,fancy,
                    Float.floatToRawIntBits(player.a(partial==0?.375f:1f)),field(fd.class,"f").getInt(world));
                Arrays.fill(actual,0); integers.position(0); bt.b();
                GL11.glViewport(0,0,320,240);
                GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glScalef(1.25f,.75f,1.5f);
                GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();GL11.glTranslatef(5,6,7);
                GL11.glColor4f(.125f,.25f,.5f,.75f);
                GL11.glEnable(GL11.GL_TEXTURE_2D); GL11.glEnable(GL11.GL_DEPTH_TEST); GL11.glDepthMask(true);
                GL11.glEnable(GL11.GL_ALPHA_TEST); GL11.glAlphaFunc(GL11.GL_GREATER,.1f);
                for(int cap:new int[]{GL11.GL_BLEND,GL11.GL_CULL_FACE,GL11.GL_FOG,GL11.GL_LIGHTING,GL11.GL_LIGHT0,GL11.GL_LIGHT1,GL11.GL_COLOR_MATERIAL,32826}) GL11.glDisable(cap);
                GL11.glColorMask(true,true,true,true); GL11.glShadeModel(GL11.GL_SMOOTH);
                GL11.glClearColor(0,0,0,1);GL11.glClearDepth(1);GL11.glClear(GL11.GL_COLOR_BUFFER_BIT|GL11.GL_DEPTH_BUFFER_BIT);
                hud.a(partial==0?.375f:1f,false,17,23);
                GL11.glGetFloat(GL11.GL_CURRENT_COLOR,color);
                GL11.glGetFloat(GL11.GL_MODELVIEW_MATRIX,modelview); GL11.glGetFloat(GL11.GL_PROJECTION_MATRIX,projection);
                System.out.printf("HUD_STATE %d %d %d %d %08x %08x %d %d %d %d %d",index,size,health,partial,
                    Float.floatToRawIntBits(field(ub.class,"k").getFloat(hud)),Float.floatToRawIntBits(hud.c),
                    seed.get(),field(bt.class,"e").getInt(null),GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),GL11.glGetInteger(GL11.GL_PROJECTION_STACK_DEPTH),GL11.glGetInteger(GL11.GL_MATRIX_MODE));
                for(int i=0;i<16;++i)System.out.printf(" %08x",Float.floatToRawIntBits(modelview.get(i)));
                for(int i=0;i<16;++i)System.out.printf(" %08x",Float.floatToRawIntBits(projection.get(i)));
                for(int i=0;i<4;++i)System.out.printf(" %08x",Float.floatToRawIntBits(color.get(i)));
                for(int cap:new int[]{GL11.GL_TEXTURE_2D,GL11.GL_DEPTH_TEST,GL11.GL_ALPHA_TEST,GL11.GL_BLEND,GL11.GL_CULL_FACE,GL11.GL_FOG,GL11.GL_LIGHTING,GL11.GL_LIGHT0,GL11.GL_LIGHT1,GL11.GL_COLOR_MATERIAL,32826}) System.out.print(" "+(GL11.glIsEnabled(cap)?1:0));
                System.out.println(" "+(GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK)?1:0)+" "+GL11.glGetInteger(GL11.GL_BLEND_SRC)+" "+GL11.glGetInteger(GL11.GL_BLEND_DST)+" "+integers.position());
                ByteBuffer words=ByteBuffer.allocate(6*5*4);
                for(int vertex=0;vertex<6;++vertex)for(int word=0;word<5;++word)words.putInt(actual[vertex*8+word]);
                System.out.println("HUD_WORDS "+index+" 5 6 "+hex(words.array()));
                ByteBuffer pixels=BufferUtils.createByteBuffer(320*240*4);GL11.glReadPixels(0,0,320,240,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixels);
                byte[] payload=new byte[pixels.capacity()];pixels.get(payload);int visible=0;
                for(int i=0;i<payload.length;i+=4)if(payload[i]!=0||payload[i+1]!=0||payload[i+2]!=0)++visible;
                System.out.println("HUD_PIXELS "+index+" 320 240 "+visible+" "+digest(payload));
                require(GL11.glGetError()==GL11.GL_NO_ERROR,"actual HUD GL error");++index;
            }
            require(network==0,"network attempt");
            System.out.println("Original HUD: 26 rows; network=0; display=1");
            for(String library:links)System.out.println("NATIVE_LINK "+library);
        } finally { Display.destroy(); }
    }
}
