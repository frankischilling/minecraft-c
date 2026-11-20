import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.FloatBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import net.minecraft.client.Minecraft;

/** Declared genuine constructor graph and two unchanged original UE draws. */
public final class InventoryDrawingTarget {
    public static int active=-1;
    public static kq app;
    public static fd world;
    public static dc player;
    public static ue screen;
    public static th manager;
    public static ds renderer;
    public static fh primary;
    static int network;
    static final List<String> links=new ArrayList<>();
    static Field field(Class<?> owner,String name)throws Exception {
        for(Class<?> c=owner;c!=null;c=c.getSuperclass())try{Field value=c.getDeclaredField(name);value.setAccessible(true);return value;}catch(NoSuchFieldException absent){}
        throw new NoSuchFieldException(owner.getName()+"."+name);
    }
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static Object primitive(Object value){return value instanceof Float ? f((Float)value) : value instanceof Double ? d((Double)value) : value;}
    static Map<String,Object> primitives(Class<?> owner,Object object,boolean statics)throws Exception {
        Map<String,Object> result=new TreeMap<>();
        for(Field value:owner.getDeclaredFields())if(value.getType().isPrimitive()&&Modifier.isStatic(value.getModifiers())==statics){value.setAccessible(true);result.put(value.getName(),primitive(value.get(object)));}
        return result;
    }
    static List<String> words(int count)throws Exception {
        int[] raw=(int[])field(nw.class,"g").get(nw.a);List<String> result=new ArrayList<>();
        for(int i=0;i<count;++i)result.add(String.format(Locale.ROOT,"%08x",raw[i]));return result;
    }
    static List<String> matrix(int kind){FloatBuffer values=BufferUtils.createFloatBuffer(16);GL11.glGetFloat(kind,values);List<String> result=new ArrayList<>();for(int i=0;i<16;++i)result.add(f(values.get(i)));return result;}
    static Map<String,Object> pose()throws Exception {
        Map<String,Object> result=new LinkedHashMap<>();
        for(String name:new String[]{"H","aS","aT","bE"})result.put(name,f(field(player.getClass(),name).getFloat(player)));
        result.put("manager-i",f(manager.i));result.put("mouse-x",f(field(ue.class,"l").getFloat(screen)));result.put("mouse-y",f(field(ue.class,"m").getFloat(screen)));
        return result;
    }
    static List<Object> models()throws Exception {
        List<Object> result=new ArrayList<>();
        for(String name:new String[]{"a","g","h"}){fh model=(fh)field(ds.class,name).get(renderer);result.add(ConstructorJson.map("role",name,"class",model.getClass().getName(),"held",model.k,"sneak",model.l));}
        return result;
    }
    static Map<String,Object> gl(){return ConstructorJson.map("modelview",matrix(GL11.GL_MODELVIEW_MATRIX),"projection",matrix(GL11.GL_PROJECTION_MATRIX),
        "modelview-depth",GL11.glGetInteger(GL11.GL_MODELVIEW_STACK_DEPTH),"projection-depth",GL11.glGetInteger(GL11.GL_PROJECTION_STACK_DEPTH),
        "matrix-mode",GL11.glGetInteger(GL11.GL_MATRIX_MODE),"lighting",GL11.glIsEnabled(GL11.GL_LIGHTING),"light0",GL11.glIsEnabled(GL11.GL_LIGHT0),
        "light1",GL11.glIsEnabled(GL11.GL_LIGHT1),"color-material",GL11.glIsEnabled(GL11.GL_COLOR_MATERIAL),"rescale-normal",GL11.glIsEnabled(32826),
        "texture",GL11.glIsEnabled(GL11.GL_TEXTURE_2D),"depth",GL11.glIsEnabled(GL11.GL_DEPTH_TEST),"alpha",GL11.glIsEnabled(GL11.GL_ALPHA_TEST),
        "blend",GL11.glIsEnabled(GL11.GL_BLEND));}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==3,"jar game natives");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toAbsolutePath(),natives=Path.of(args[2]).toRealPath();Files.createDirectories(directory);
        Path jdk=Path.of(System.getProperty("java.home")).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
            public void checkListen(int port){++network;throw new SecurityException("listener");}
            public void checkLink(String path){
                links.add(path);Path candidate=Path.of(path);
                boolean lwjgl=candidate.isAbsolute()&&candidate.normalize().equals(natives.resolve("lwjgl64.dll"));
                boolean java=candidate.isAbsolute()&&candidate.normalize().startsWith(jdk.resolve("bin")) || !candidate.isAbsolute()&&Files.isRegularFile(jdk.resolve("bin").resolve(path+".dll"));
                require(lwjgl||java,"unexpected native link "+path);
            }
        });
        for(Class<?> type:new Class<?>[]{kq.class,Minecraft.class,fd.class,dc.class,gs.class,ix.class,aa.class,ue.class,id.class,da.class,ub.class,
            kv.class,gr.class,xi.class,vu.class,lr.class,uo.class,ik.class,ji.class,sj.class,th.class,ds.class,gv.class,bw.class,fh.class,ps.class,tz.class,nw.class,ra.class,iy.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"genuine original class "+type.getName());
        app=new kq(null,null,null,854,480,false,null);
        try {
            require(field(Minecraft.class,"a").get(null)==app,"canonical constructor App");field(Minecraft.class,"Z").set(app,directory.toFile());
            Display.setDisplayMode(new DisplayMode(854,480));Display.setTitle("Original Inventory drawing observation");Display.create();
            System.out.println("INVENTORY_DRAW_DEVICE "+ConstructorJson.json(ConstructorJson.map("display-created",Display.isCreated(),"pbuffer",false,
                "width",Display.getWidth(),"height",Display.getHeight(),"vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION))));
            app.z=new kv(app,directory.toFile());app.k=new gr("InventoryDrawing","-");
            ik packs=new ik(app,directory.toFile());ji engine=new ji(packs,app.z);app.p=engine;app.q=new sj(app.z,"/font/default.png",engine);
            app.I=new xi(app.k,directory.toFile());app.u=new vu(app);
            world=new fd(new wy(),"InventoryDrawing",xa.a(0),23L);app.f=world;
            player=new dc(app,world,app.k,0);app.h=player;app.i=player;
            // Minecraft.a(FD,String,GS): captures App.h191, NEW LR194,
            // captures actual App.z199, LR.<init>202, publishes DC.a205.
            require(app.h.a==null,"genuine DC constructor leaves Input unattached");
            app.h.a=new lr(app.z);
            require(app.h.a.getClass()==lr.class&&field(lr.class,"g").get(app.h.a)==app.z,"actual source Input/Settings owner edge");
            boolean[] keys=(boolean[])field(lr.class,"f").get(app.h.a);require(keys.length==10,"source LR ten key cells");
            for(boolean pressed:keys)require(!pressed,"actual untouched constructor key state");
            require(player.d==player.e && player.d instanceof aa && player.c.b()==null,"actual constructor Inventory and default Container");
            for(iz value:player.c.a)require(value==null,"actual empty main Inventory");for(iz value:player.c.b)require(value==null,"actual empty armor");
            manager=th.a;manager.f=new ra(app);manager.a(world,engine,app.q,player,app.z,.375f);
            renderer=(ds)manager.a(player);primary=(fh)field(ds.class,"a").get(renderer);
            require(renderer.getClass()==ds.class&&field(gv.class,"e").get(renderer)==primary&&manager.h==player,"canonical DS/primary model/same view Player");
            require(field(ra.class,"a").get(manager.f)==app,"genuine startup RA App owner");
            screen=new ue(player);screen.a(app,427,240);
            require(screen.j==player.d&&player.d==player.e&&screen.f&&!screen.c(),"actual UE/ID Player Container and nonpause input");
            System.out.println("INVENTORY_DRAW_OWNERS "+ConstructorJson.json(ConstructorJson.map("app",app.getClass().getName(),"world",world.getClass().getName(),
                "player",player.getClass().getName(),"inventory",player.c.getClass().getName(),"container",player.d.getClass().getName(),"screen",screen.getClass().getName(),
                "engine",engine.getClass().getName(),"packs",packs.getClass().getName(),"font",app.q.getClass().getName(),"manager",manager.getClass().getName(),"renderer",renderer.getClass().getName(),
                "model",primary.getClass().getName(),"view-player-same",manager.h==player,"world-member",world.b.contains(player),"open-stat",app.I.a((vr)ep.f),
                "input",player.a.getClass().getName(),"input-settings-same",field(lr.class,"g").get(player.a)==app.z,"input-key-count",keys.length,"source-sneaking-before",player.t(),"skin-url",player.bA,"cape-url",player.n,"downloads-before",((Map<?,?>)field(ji.class,"h").get(engine)).size(),"raw-allocations",0)));
            System.out.println("INVENTORY_DRAW_INITIAL "+ConstructorJson.json(ConstructorJson.map("nw-static",primitives(nw.class,null,true),"nw",primitives(nw.class,nw.a,false),"words",words(48),"pose",pose())));
            GL11.glViewport(0,0,854,480);GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();GL11.glOrtho(0,427,240,0,1000,3000);
            GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();GL11.glTranslatef(0,0,-2000);GL11.glEnable(GL11.GL_TEXTURE_2D);GL11.glColor4f(1,1,1,1);
            require(GL11.glGetError()==0,"declared GL setup success");
            int[][] mouse={{-30,-40},{-60,-70}};
            for(int i=0;i<2;++i){
                Map<String,Object> before=pose();Map<String,Object> glBefore=gl();
                System.out.println("INVENTORY_DRAW_INPUT "+ConstructorJson.json(ConstructorJson.map("profile",i,"mouse",List.of(mouse[i][0],mouse[i][1]),"partial",f(.375f),"pose",before,"models",models(),"gl",glBefore)));
                active=i;
                screen.a(mouse[i][0],mouse[i][1],.375f);
                active=-1;
                GL11.glFinish();int error=GL11.glGetError();require(error==0,"actual UE draw GL error "+error);
                Map<String,Object> after=pose();for(String name:new String[]{"H","aS","aT"})require(before.get(name).equals(after.get(name)),"actual successful pose restoration "+name);
                require(after.get("bE").equals(f(0))&&after.get("manager-i").equals(f(180))&&after.get("mouse-x").equals(f(mouse[i][0]))&&after.get("mouse-y").equals(f(mouse[i][1])),"actual returned UE continuation/mouse stores");
                System.out.println("INVENTORY_DRAW_RETURN "+ConstructorJson.json(ConstructorJson.map("profile",i,"pose",after,"models",models(),"gl",gl(),"gl-error",error,
                    "nw",primitives(nw.class,nw.a,false),"last-words",words(48),"default-active-same",player.d==player.e,"downloads-after",((Map<?,?>)field(ji.class,"h").get(engine)).size())));
                Display.update();require(!Display.isCloseRequested(),"owned Display stayed live through returned draw");
            }
            require(network==0,"no network attempts");
            System.out.println("INVENTORY_DRAW_COMPLETE "+ConstructorJson.json(ConstructorJson.map("profiles",2,"display-created",Display.isCreated(),"network",network,"links",links,
                "game-substitution",false,"random-rebinding",false,"raw-allocations",0,"nw-clears",0,"source-input-attachments",1,"player-field-repairs",0,"world-field-repairs",0)));
        }finally{
            active=-1;app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){thread.interrupt();thread.join(2000);require(!thread.isAlive(),"Timer retired");}
            if(Display.isCreated())Display.destroy();
        }
    }
}
