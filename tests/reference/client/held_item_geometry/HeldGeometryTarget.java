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

/** Authored preparation and read-only receipts; unchanged original RA/NW execute. */
public final class HeldGeometryTarget {
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
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd world()throws Exception {
        fd w=new fd(new wy(),"HeldGeometry",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(w);Map map=(Map)field(ok.class,"e").get(provider);List list=(List)field(ok.class,"f").get(provider);
        map.clear();list.clear();lm chunk=new lm(w,new byte[32768],0,0);chunk.n=true;
        map.put(yy.a(0,0),chunk);list.add(chunk);return w;
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(), directory=Path.of(args[1]).toAbsolutePath(), natives=Path.of(args[2]).toRealPath();
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
        for(Class<?> type:new Class<?>[]{kq.class,Minecraft.class,kv.class,ik.class,ji.class,sj.class,ra.class,iy.class,nw.class,fd.class,lm.class,dc.class,gs.class,ls.class,iz.class,gm.class,qd.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original JAR "+type.getName());
        kq app=new kq(null,null,null,854,480,false,null);
        Pbuffer context=null;
        try {
            require(field(Minecraft.class,"a").get(null)==app,"actual constructor canonical App");
            field(Minecraft.class,"Z").set(app,directory.toFile());
            context=new Pbuffer(32,32,new PixelFormat(),null,null);context.makeCurrent();
            System.out.println("HELD_GEOMETRY_DEVICE "+ConstructorJson.json(ConstructorJson.map("vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION),"hidden_pbuffer",true)));
            app.z=new kv();app.k=new gr("HeldGeometry","-");
            ik packs=new ik(app,directory.toFile());ji engine=new ji(packs,app.z);app.p=engine;
            app.q=new sj(app.z,"/font/default.png",engine);
            fd w=world();app.f=w;dc player=new dc(app,w,app.k,0);app.h=player;app.i=player;
            player.e(.5,64,.5);
            ra hand=new ra(app);
            require(field(ra.class,"a").get(hand)==app,"genuine RA App identity");
            Object image=field(ra.class,"f").get(hand);
            require(field(iy.class,"c").get(image)==app.z&&field(iy.class,"d").get(image)==app.q,"genuine IY settings/font identities");
            require(ji.class.getResource("/gui/items.png")!=null,"original atlas resource");
            System.out.println("HELD_GEOMETRY_OWNERS "+ConstructorJson.json(ConstructorJson.map("app",app.getClass().getName(),"world",w.getClass().getName(),"player",player.getClass().getName(),"engine",engine.getClass().getName(),"packs",packs.getClass().getName(),"font",app.q.getClass().getName(),"hand",hand.getClass().getName(),"image",image.getClass().getName(),"nw",nw.a.getClass().getName(),"atlas",ji.class.getResource("/gui/items.png").toString(),"canonical_app",true,"raw_allocations",0)));
            List<String> initial=words(768);
            System.out.println("HELD_GEOMETRY_INITIAL "+ConstructorJson.json(ConstructorJson.map("static",primitive(nw.class,null,true),"instance",primitive(nw.class,nw.a,false),"all_zero",initial.stream().allMatch(s->s.equals("00000000")),"words",initial)));
            GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
            int[] ids={268,272,267,276,283,261};
            for(int i=0;i<ids.length;++i){
                iz stack=new iz(ids[i],1,0);player.c.a(0,stack);
                require(player.c.b()==stack,"genuine selected Stack");
                int icon=player.c(stack);
                System.out.println("HELD_GEOMETRY_INPUT "+ConstructorJson.json(ConstructorJson.map("profile",i,"id",ids[i],"icon",icon,"item_class",gm.c[ids[i]].getClass().getName(),"count",stack.a,"damage",stack.i(),"nw_before",primitive(nw.class,nw.a,false))));
                active=i;
                hand.a(player,stack);
                active=-1;
                int error=GL11.glGetError();require(error==0,"actual GL returned "+error);
                System.out.println("HELD_GEOMETRY_RETURN "+ConstructorJson.json(ConstructorJson.map("profile",i,"icon",icon,"nw_after",primitive(nw.class,nw.a,false),"last_words",words(768),"gl_error",error)));
            }
            require(network==0,"no target network attempts");
            System.out.println("HELD_GEOMETRY_COMPLETE "+ConstructorJson.json(ConstructorJson.map("profiles",6,"network",network,"links",links,"game_substitution",false,"random_rebinding",false,"raw_allocations",0)));
        } finally {
            app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);require(!t.isAlive(),"Timer retired");}
            if(context!=null)context.destroy();
        }
    }
}
