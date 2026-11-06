import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import net.minecraft.client.Minecraft;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;

/** Real N constructor lists and unchanged sky draw; prepared World/cache operands. */
public final class SkyDrawObservation {
    static int network;
    static final List<String> links=new ArrayList<>();
    static void require(boolean b,String m){if(!b)throw new AssertionError(m);}
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static Object raw(Class<?> type)throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance",Class.class)
            .invoke(field(unsafe,"theUnsafe").get(null),type);
    }
    static void set(Object o,Class<?> type,String name,Object value)throws Exception {field(type,name).set(o,value);}
    static int flag(int capability){return GL11.glIsEnabled(capability)?1:0;}
    @SuppressWarnings({"removal","unchecked"})
    public static void main(String[] args)throws Exception {
        require(args.length==2,"arguments");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),natives=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkWrite(String f){throw new SecurityException();}
            @Override public void checkLink(String library){
                require(links.size()<16,"link bound");links.add(library);
                boolean jdk=List.of("jawt","awt","awt_headless").contains(library);
                boolean lwjgl=Path.of(library).isAbsolute()&&Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll"));
                if(!jdk&&!lwjgl)throw new SecurityException("unexpected native "+library);
            }
        });
        for(Class<?> type:new Class<?>[]{n.class,kq.class,fd.class,dc.class,kv.class,ei.class,
                rh.class,wd.class,ay.class,xv.class,nw.class,ji.class,ik.class,cx.class,bt.class,u.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar),"foreign original class");
        Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Original source sky observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR "+GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER "+GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION "+GL11.glGetString(GL11.GL_VERSION));
            Minecraft app=(Minecraft)raw(kq.class);kv settings=(kv)raw(kv.class);
            dc camera=(dc)raw(dc.class);camera.aM=camera.aO=8.5;camera.aN=64.5;
            app.i=camera;app.z=settings;set(app,Minecraft.class,"S",new cx());
            // Real cached ji.b(String) path. This prepared actual b Map contains
            // actual device texture names; no engine method or returned ID is replaced.
            ji engine=(ji)raw(ji.class);HashMap<String,Integer> cache=new HashMap<>();
            set(engine,ji.class,"b",cache);
            set(engine,ji.class,"l",raw(ik.class)); // Actual selected-Pack field is read before cache lookup.
            int sun=GL11.glGenTextures(),moon=GL11.glGenTextures();
            ByteBuffer pixel=BufferUtils.createByteBuffer(4);pixel.put(new byte[]{-1,-1,-1,-1}).flip();
            for(int id:new int[]{sun,moon}) {
                GL11.glBindTexture(GL11.GL_TEXTURE_2D,id);
                GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MIN_FILTER,GL11.GL_NEAREST);
                GL11.glTexParameteri(GL11.GL_TEXTURE_2D,GL11.GL_TEXTURE_MAG_FILTER,GL11.GL_NEAREST);
                GL11.glTexImage2D(GL11.GL_TEXTURE_2D,0,GL11.GL_RGBA,1,1,0,GL11.GL_RGBA,GL11.GL_UNSIGNED_BYTE,pixel);
            }
            cache.put("/terrain/sun.png",sun);cache.put("/terrain/moon.png",moon);
            n renderer=new n(app,engine); // Actual complete constructor, stars/top/bottom programs.
            int top=field(n.class,"z").getInt(renderer),stars=field(n.class,"y").getInt(renderer),bottom=field(n.class,"A").getInt(renderer);
            require(GL11.glIsList(top)&&GL11.glIsList(stars)&&GL11.glIsList(bottom),"actual source lists");
            FloatBuffer color=BufferUtils.createFloatBuffer(16);
            long[] times={0,6000,12000,18000};
            for(int dim=0;dim<3;++dim) {
                fd world=(fd)raw(fd.class);ei info=new ei(23L,"Sky");
                xa dimension=xa.a(dim==0?0:dim==1?-1:1);
                set(world,fd.class,"x",info);set(world,fd.class,"t",dimension);dimension.a(world);
                app.f=world;set(renderer,n.class,"k",world);
                for(int t=0;t<times.length;++t)for(int ana=0;ana<2;++ana)for(int rain=0;rain<2;++rain) {
                    info.a(times[t]);settings.g=ana!=0;
                    set(world,fd.class,"i",(float)rain);set(world,fd.class,"j",(float)rain);
                    set(world,fd.class,"k",0f);set(world,fd.class,"l",0f);
                    GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glOrtho(-500,500,-500,500,-500,500);
                    GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
                    GL11.glColor4f(.125f,.25f,.5f,.75f);
                    GL11.glEnable(GL11.GL_TEXTURE_2D);GL11.glEnable(GL11.GL_ALPHA_TEST);GL11.glDisable(GL11.GL_BLEND);
                    GL11.glDisable(GL11.GL_LIGHTING);GL11.glDisable(GL11.GL_LIGHT0);GL11.glDisable(GL11.GL_LIGHT1);
                    GL11.glDisable(GL11.GL_COLOR_MATERIAL);GL11.glDisable(GL11.GL_FOG);GL11.glDepthMask(true);
                    GL11.glShadeModel(GL11.GL_SMOOTH);GL11.glFogi(GL11.GL_FOG_MODE,GL11.GL_LINEAR);
                    GL11.glFogf(GL11.GL_FOG_START,0);GL11.glFogf(GL11.GL_FOG_END,1000);
                    bt.b();renderer.a(.375f);
                    GL11.glGetFloat(GL11.GL_CURRENT_COLOR,color);
                    System.out.printf("SKY_DRAW %d %d %d %d",dim,t,ana,rain);
                    for(int c=0;c<4;++c)System.out.printf(" %08x",Float.floatToRawIntBits(color.get(c)));
                    System.out.printf(" %d %d %d %d %d %d %d%n",flag(GL11.GL_FOG),flag(GL11.GL_ALPHA_TEST),
                        flag(GL11.GL_BLEND),flag(GL11.GL_TEXTURE_2D),GL11.glGetBoolean(GL11.GL_DEPTH_WRITEMASK)?1:0,
                        GL11.glGetInteger(GL11.GL_SHADE_MODEL),field(bt.class,"e").getInt(null));
                    require(GL11.glGetError()==GL11.GL_NO_ERROR,"actual sky GL error");
                    require(cache.get("/terrain/sun.png")==sun&&cache.get("/terrain/moon.png")==moon,"actual cache stable");
                }
            }
            require(network==0,"network attempt");
            System.out.println("Original source sky: 48 rows; network=0; display=1; real-lists=3");
            for(String link:links)System.out.println("NATIVE_LINK "+link);
        } finally {Display.destroy();}
    }
}
