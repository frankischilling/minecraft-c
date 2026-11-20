import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Path;
import java.nio.file.Files;
import java.security.Permission;
import java.util.*;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.Pbuffer;
import org.lwjgl.opengl.PixelFormat;
import net.minecraft.client.Minecraft;

/** Authored preparation and read-only receipts; unchanged original BB/NW execute. */
public final class DurabilityOverlayTarget {
    public static int active=-1;
    static int network;
    static final List<String> links=new ArrayList<>();
    static Field field(Class<?> type,String name)throws Exception {Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static void require(boolean v,String text){if(!v)throw new AssertionError(text);}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static Object value(Object v){return v instanceof Float ? bits((Float)v) : v instanceof Double ? String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)v)) : v;}
    static Map<String,Object> primitive(Class<?> type,Object receiver,boolean statics)throws Exception {
        Map<String,Object> out=new TreeMap<>();
        for(Field f:type.getDeclaredFields())if(f.getType().isPrimitive()&&Modifier.isStatic(f.getModifiers())==statics){f.setAccessible(true);out.put(f.getName(),value(f.get(receiver)));}
        return out;
    }
    static List<String> words(int n)throws Exception {int[] raw=(int[])field(nw.class,"g").get(nw.a);List<String> result=new ArrayList<>();for(int i=0;i<n;++i)result.add(String.format(Locale.ROOT,"%08x",raw[i]));return result;}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toAbsolutePath(),natives=Path.of(args[2]).toRealPath();
        Files.createDirectories(directory);
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
            public void checkListen(int p){++network;throw new SecurityException("listener");}
            public void checkLink(String path){
                links.add(path);
                boolean lwjgl=Path.of(path).isAbsolute()&&Path.of(path).normalize().equals(natives.resolve("lwjgl64.dll"));
                boolean jdk=Set.of("awt","awt_headless","fontmanager","freetype","lcms","zip","nio","net").contains(path);
                require(lwjgl||jdk,"unexpected native link "+path);
            }
        });
        for(Class<?> type:new Class<?>[]{kq.class,Minecraft.class,kv.class,ik.class,ji.class,sj.class,bb.class,bw.class,cv.class,nw.class,iz.class,gm.class,qd.class,dq.class,wc.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original JAR "+type.getName());
        kq app=new kq(null,null,null,854,480,false,null);Pbuffer context=null;
        try {
            require(field(Minecraft.class,"a").get(null)==app,"actual constructor canonical App");
            field(Minecraft.class,"Z").set(app,directory.toFile());
            context=new Pbuffer(32,32,new PixelFormat(),null,null);context.makeCurrent();
            System.out.println("DURABILITY_DEVICE "+ConstructorJson.json(ConstructorJson.map("vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"hidden_pbuffer",true)));
            app.z=new kv();ik packs=new ik(app,directory.toFile());ji engine=new ji(packs,app.z);app.p=engine;
            app.q=new sj(app.z,"/font/default.png",engine);bb renderer=new bb();
            System.out.println("DURABILITY_OWNERS "+ConstructorJson.json(ConstructorJson.map("app",app.getClass().getName(),"settings",app.z.getClass().getName(),"packs",packs.getClass().getName(),"engine",engine.getClass().getName(),"font",app.q.getClass().getName(),"renderer",renderer.getClass().getName(),"nw",nw.a.getClass().getName(),"canonical_app",true,"raw_allocations",0)));
            System.out.println("DURABILITY_INITIAL "+ConstructorJson.json(ConstructorJson.map("static",primitive(nw.class,null,true),"instance",primitive(nw.class,nw.a,false),"words",words(48))));
            int[] ids={267,267,267,267,267,267,276,256,256};
            int[] damage={0,1,125,249,250,251,1561,1,250};
            String[] names={"iron-pristine","iron-one","iron-half","iron-last","iron-maximum","iron-over","diamond-maximum","shovel-one","shovel-maximum"};
            for(int i=0;i<ids.length;++i){
                iz stack=new iz(ids[i],1,damage[i]);int maximum=stack.j();
                System.out.println("DURABILITY_INPUT "+ConstructorJson.json(ConstructorJson.map("profile",names[i],"index",i,"id",ids[i],"damage",stack.i(),"max",maximum,"count",stack.a,"x",7,"y",11,"before",words(48),"nw_before",primitive(nw.class,nw.a,false))));
                active=i;renderer.b(app.q,engine,stack,7,11);active=-1;
                int error=GL11.glGetError();require(error==0,"actual GL returned "+error);
                System.out.println("DURABILITY_RETURN "+ConstructorJson.json(ConstructorJson.map("index",i,"id",stack.c,"count",stack.a,"damage",stack.i(),"nw_after",primitive(nw.class,nw.a,false),"words",words(48),"gl_error",error)));
            }
            require(network==0,"no target network attempts");
            System.out.println("DURABILITY_COMPLETE "+ConstructorJson.json(ConstructorJson.map("profiles",9,"network",network,"links",links,"game_substitution",false,"random_rebinding",false,"raw_allocations",0,"source_backing_cleared",false,"source_flags_repaired",false)));
        } finally {
            app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);require(!t.isAlive(),"Timer retired");}
            if(context!=null)context.destroy();
        }
    }
}
