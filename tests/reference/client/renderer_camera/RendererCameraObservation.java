import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
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

/** Actual PX camera bodies and GL matrices; only block/meta transport authored. */
public final class RendererCameraObservation {
    public static final class CameraWorld extends fd {
        int block, reads, metadataReads, originReads;
        CameraWorld() { super((wt)null,"unreached",0L); }
        @Override public int a(int x,int y,int z) {
            ++reads;
            if(x==8&&y==64&&z==8)++originReads;
            return block;
        }
        @Override public int e(int x,int y,int z) { ++metadataReads;return 0; }
    }
    static int network;
    static final List<String> links=new ArrayList<>();
    static void require(boolean b,String m) { if(!b)throw new AssertionError(m); }
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static void set(Object receiver,Class<?> type,String name,Object value)throws Exception {
        field(type,name).set(receiver,value);
    }
    static Object raw(Class<?> type)throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance",Class.class)
            .invoke(field(unsafe,"theUnsafe").get(null),type);
    }
    static Object call(px renderer,String method,Class<?>[] types,Object...args)throws Exception {
        Method m=px.class.getDeclaredMethod(method,types);m.setAccessible(true);
        try{return m.invoke(renderer,args);}catch(InvocationTargetException e){throw (Exception)e.getCause();}
    }
    static void matrix(int mode,FloatBuffer buffer) {
        GL11.glGetFloat(mode,buffer);
        for(int i=0;i<16;++i)System.out.printf(" %08x",Float.floatToRawIntBits(buffer.get(i)));
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==2,"arguments");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),natives=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            @Override public void checkPermission(Permission p) {}
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
        for(Class<?> type:new Class<?>[]{px.class,kq.class,fd.class,dc.class,kv.class,ei.class,
                rh.class,xv.class,n.class,bt.class,in.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI())
                .toRealPath().equals(jar),"foreign original class");
        Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Original renderer camera observation");
        try {
            Display.create();
            System.out.println("GL_VENDOR "+GL11.glGetString(GL11.GL_VENDOR));
            System.out.println("GL_RENDERER "+GL11.glGetString(GL11.GL_RENDERER));
            System.out.println("GL_VERSION "+GL11.glGetString(GL11.GL_VERSION));
            CameraWorld world=(CameraWorld)raw(CameraWorld.class);
            set(world,fd.class,"x",new ei(23L,"Camera"));
            xa dimension=new rh();set(world,fd.class,"t",dimension);dimension.a(world);
            FloatBuffer buffer=BufferUtils.createFloatBuffer(16);
            for(int mode=0;mode<12;++mode)for(int step=0;step<3;++step) {
                float partial=step*.5f;
                px renderer=(px)raw(px.class);
                Minecraft app=(Minecraft)raw(kq.class);
                dc player=(dc)raw(dc.class);
                kv settings=(kv)raw(kv.class);
                app.f=world;app.h=player;app.i=player;app.z=settings;app.g=(n)raw(n.class);
                app.d=854;app.e=480;set(renderer,px.class,"j",app);
                settings.e=2;settings.f=mode==4;settings.g=mode==5||mode==6;
                settings.A=mode==9||mode==10;settings.F=mode==9;
                player.aI=world;player.aM=8.5;player.aN=64.5;player.aO=8.5;
                player.aJ=7.5;player.aK=63.5;player.aL=9.5;
                player.aS=17;player.aT=-8;player.aU=10;player.aV=-4;player.bf=1.62f;
                player.Y=mode==2?0:20;player.aa=mode==3?6:0;player.ab=10;player.ac=27;player.ad=17;
                player.bi=mode==4?.75f:0;player.bj=mode==4?1.25f:0;
                player.h=mode==4?.2f:0;player.i=mode==4?.4f:0;
                player.af=mode==4?5:0;player.ag=mode==4?10:0;
                player.C=mode==8?.2f:0;player.B=mode==8?.35f:0;
                set(player,gs.class,"u",mode==11);
                set(renderer,px.class,"z",2f);set(renderer,px.class,"A",1f);
                set(renderer,px.class,"B",-2f);set(renderer,px.class,"C",1f);
                set(renderer,px.class,"u",4.25f);set(renderer,px.class,"t",3.5f);
                set(renderer,px.class,"l",37);
                set(renderer,px.class,"E",mode==7?2.0:1.0);
                set(renderer,px.class,"F",mode==7?.125:0.0);
                set(renderer,px.class,"G",mode==7?-.25:0.0);
                set(renderer,px.class,"w",15f);set(renderer,px.class,"v",30f);
                set(renderer,px.class,"y",-15f);set(renderer,px.class,"x",-5f);
                set(renderer,px.class,"D",true);
                world.block=mode==1?9:0;world.reads=world.metadataReads=world.originReads=0;
                bt.b(); // Between-call pool reset, never inside a camera invocation.
                call(renderer,"a",new Class<?>[]{float.class,int.class},partial,mode==6?1:0);
                int reads=world.reads,meta=world.metadataReads,origin=world.originReads;
                require(!field(px.class,"D").getBoolean(renderer),"real n cloud predicate result");
                System.out.printf("CAMERA %d %d %08x 0",mode,step,
                    Float.floatToRawIntBits(field(px.class,"k").getFloat(renderer)));
                matrix(GL11.GL_PROJECTION_MATRIX,buffer);matrix(GL11.GL_MODELVIEW_MATRIX,buffer);
                System.out.println();
                float fov=(Float)call(renderer,"d",new Class<?>[]{float.class},partial);
                System.out.printf("CAMERA_FIELDS %d %d %08x %016x %016x %016x %d %d %d%n",
                    mode,step,Float.floatToRawIntBits(fov),
                    Double.doubleToRawLongBits(field(px.class,"E").getDouble(renderer)),
                    Double.doubleToRawLongBits(field(px.class,"F").getDouble(renderer)),
                    Double.doubleToRawLongBits(field(px.class,"G").getDouble(renderer)),reads,meta,origin);
                require(GL11.glGetError()==GL11.GL_NO_ERROR,"original GL error");
            }
            require(network==0,"network attempt");
            System.out.println("Original renderer camera: 36 rows; network=0; display=1; matrices=1");
            for(String link:links)System.out.println("NATIVE_LINK "+link);
        } finally {Display.destroy();}
    }
}
