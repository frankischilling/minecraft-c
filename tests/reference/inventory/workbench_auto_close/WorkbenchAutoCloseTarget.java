import java.awt.Canvas;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import net.minecraft.client.Minecraft;

/** Genuine constructor scenes and actual eligibility-driven Player ticks. */
public final class WorkbenchAutoCloseTarget {
    static kq app;
    static fd world;
    static dc player;
    static oo screen;
    static it container;
    static xi writer;
    static int network, rows;
    static List<String> links=new ArrayList<>();
    static IdentityHashMap<iz,String> identities;
    static LinkedHashMap<String,iz> objects;
    static List<Object> events,drops;
    static int nextIdentity;
    static Field field(Class<?> owner,String name)throws Exception {Field result=owner.getDeclaredField(name);result.setAccessible(true);return result;}
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String seed(Random value)throws Exception{return String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(value)).get());}
    static Map<String,Object> map(Object... fields){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<fields.length;i+=2)result.put((String)fields[i],fields[i+1]);return result;}
    static String json(Object value){
        if(value==null)return "null";
        if(value instanceof String)return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r")+"\"";
        if(value instanceof Boolean||value instanceof Number)return value.toString();
        List<String> entries=new ArrayList<>();
        if(value instanceof Map){for(Map.Entry<?,?> entry:((Map<?,?>)value).entrySet())entries.add(json(entry.getKey())+":"+json(entry.getValue()));return "{"+String.join(",",entries)+"}";}
        for(Object entry:(Iterable<?>)value)entries.add(json(entry));return "["+String.join(",",entries)+"]";
    }
    static String ref(iz value){if(value==null)return null;String result=identities.get(value);if(result==null){result="N"+(++nextIdentity);identities.put(value,result);objects.put(result,value);}return result;}
    static List<Integer> values(iz value){return Arrays.asList(value.c,value.a,value.i(),value.b);}
    static List<Object> inventory(lw value){List<Object> result=new ArrayList<>();for(int i=0;i<value.a();++i)result.add(ref(value.f_(i)));return result;}
    static Map<String,Object> stats()throws Exception{Map<String,Object> result=new TreeMap<>();for(Map.Entry<?,?> entry:((Map<?,?>)field(xi.class,"a").get(writer)).entrySet())result.put(Integer.toString(((vr)entry.getKey()).e),entry.getValue());return result;}
    static Map<String,Object> selected()throws Exception{
        Map<String,Object> result=new LinkedHashMap<>();
        for(String name:new String[]{"aM","aN","aO","aP","aQ","aR"})result.put("sn."+name,d(field(sn.class,name).getDouble(player)));
        for(String name:new String[]{"o","p","q","r","s","t"})result.put("gs."+name,d(field(gs.class,name).getDouble(player)));
        result.put("sn.bt",player.bt);result.put("ls.Y",player.Y);result.put("sn.aS",f(player.aS));result.put("sn.aT",f(player.aT));return result;
    }
    static Map<String,Object> snapshot()throws Exception{
        List<Object> cache=new ArrayList<>();for(Object value:container.d)cache.add(ref((iz)value));
        Map<String,Object> result=map("inventory",inventory(player.c),"craft",inventory(container.a),"result",inventory(container.b),"cursor",ref(player.c.i()),"cached",cache,
            "active-it",player.e==container,"active-default",player.e==player.d,"default-aa",player.d instanceof aa,"app-screen-same",app.r==screen,"app-screen-null",app.r==null,
            "selected",player.c.c,"changed",player.c.e,"stats",stats(),"player",selected(),"world-items",world.b.size());
        Map<String,Object> stacks=new LinkedHashMap<>();for(Map.Entry<String,iz> entry:objects.entrySet())stacks.put(entry.getKey(),values(entry.getValue()));result.put("stacks",stacks);return result;
    }
    @SuppressWarnings({"unchecked","rawtypes"})
    static fd makeWorld()throws Exception{
        fd value=new fd(new wy(),"WorkbenchAutoClose",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);Map cache=(Map)field(ok.class,"e").get(provider);List loaded=(List)field(ok.class,"f").get(provider);
        require(cache.isEmpty()&&loaded.isEmpty(),"actual empty constructor chunk cache");
        for(int x=-1;x<=1;++x)for(int z=-1;z<=1;++z){byte[] blocks=new byte[32768];if(x==0&&z==0)blocks[(8<<11)|(8<<7)|64]=58;lm chunk=new lm(value,blocks,x,z);cache.put(yy.a(x,z),chunk);loaded.add(chunk);}
        return value;
    }
    static long next(long seed){return (seed*0x5deece66dL+11)&((1L<<48)-1);}
    static final class Observer implements pm {
        public void a(sn value){try{
            require(value instanceof hl&&value.aI==world&&world.b.contains(value),"actual same-World Item publication");hl item=(hl)value;int members=0;
            Object provider=field(fd.class,"v").get(world);for(Object raw:((List<?>)field(ok.class,"f").get(provider)))for(List<?> slice:((lm)raw).m)for(Object member:slice)if(member==item)++members;
            require(members==1,"actual one Chunk membership");
            long after=Long.parseLong(seed(player.bs),16),before=after;for(int i=0;i<4;++i)before=((before-11)*0xdfe05bcb1365L)&((1L<<48)-1);
            float u=(next(before)>>>24)/(float)(1<<24);float angle=(u*(float)Math.PI)*2f;
            drops.add(map("ref",ref(item.a),"value",values(item.a),"pickup-delay",item.c,"world-member",true,"chunk-members",members,
                "position",Arrays.asList(d(item.aM),d(item.aN),d(item.aO)),"velocity",Arrays.asList(d(item.aP),d(item.aQ),d(item.aR)),
                "private-random",seed(item.bs),"player-random-before",String.format(Locale.ROOT,"%016x",before),"player-random-after",String.format(Locale.ROOT,"%016x",after),
                "trig",map("angle",f(angle),"cos",d(Math.cos((double)angle)),"sin",d(Math.sin((double)angle)))));
            events.add(map("event","drop","index",drops.size()-1,"ref",ref(item.a)));
        }catch(Exception error){throw new AssertionError(error);}}
        public void a(String n,double x,double y,double z,float v,float p){throw new AssertionError("unexpected sound "+n);}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle "+n);}
        public void b(sn v){throw new AssertionError("unexpected removal");}
        public void a(int x,int y,int z){throw new AssertionError("unexpected Block callback");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("unexpected range");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow v){throw new AssertionError("unexpected Tile Entity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected World event");}
    }
    static void begin(int index){}
    static void end(){}
    static void run(String line,Path directory)throws Exception{
        String[] args=line.split("\\t");String profile=args[0],operation=args[1];
        Path scene=directory.resolve(profile);Files.createDirectories(scene);writer=new xi(app.k,scene.toFile());app.I=writer;
        world=makeWorld();app.f=world;player=new dc(app,world,app.k,0);app.h=player;app.i=player;
        require(player.a==null&&player.e==player.d&&player.d instanceof aa,"actual Player constructor owners/Input");
        player.a=new lr(app.z); // Declared genuine Minecraft world-load constructor/attachment protocol, before calls.
        player.b(Double.parseDouble(args[2]),Double.parseDouble(args[3]),Double.parseDouble(args[4]),0f,0f);
        player.a(8,64,8); // Actual DC opening invokes OO construction, display, and inherited ID.b publication.
        require(app.r instanceof oo,"source display publishes genuine OO");screen=(oo)app.r;container=(it)screen.j;
        require(player.e==container&&player.d!=container,"source initialize publishes IT with constructor AA intact");
        identities=new IdentityHashMap<>();objects=new LinkedHashMap<>();nextIdentity=0;events=new ArrayList<>();drops=new ArrayList<>();
        boolean alias=Boolean.parseBoolean(args[6]);iz cursor=new iz(alias?280:5,alias?2:7,0),first=alias?cursor:new iz(280,2,0),last=alias?cursor:null;
        for(Object[] pair:new Object[][]{{"cursor",cursor},{"grid0",first},{"grid8",last}})if(pair[1]!=null&&!identities.containsKey((iz)pair[1])){identities.put((iz)pair[1],(String)pair[0]);objects.put((String)pair[0],(iz)pair[1]);}
        player.c.b(cursor);container.a.a(0,first);container.a.a(8,last);
        if(Boolean.parseBoolean(args[5]))require(world.c(8,64,8,0),"declared actual World Block removal method");
        require(world.a(8,64,8)==(Boolean.parseBoolean(args[5])?0:58),"actual Table input");
        world.a(new Observer());
        Map<String,Object> before=snapshot();String playerBefore=seed(player.bs),worldBefore=seed(world.r);
        Map<String,Object> focusBefore=map("display-active",Display.isActive(),"N",app.N,"mouse-grabbed",org.lwjgl.input.Mouse.isGrabbed());
        Object returned=null;String exception="none";
        begin(rows);
        try{if(operation.equals("gs-tick"))player.w_();else if(operation.equals("predicate"))returned=container.b(player);else if(operation.equals("direct-dc"))player.r();else throw new AssertionError("declared operation");}
        catch(RuntimeException error){exception=error.getClass().getName();error.printStackTrace();}
        end();
        GL11.glFinish();int glError=GL11.glGetError();require(glError==0,"actual close GL continuation");
        System.out.println("WORKBENCH_AUTO_CLOSE "+json(map("profile",profile,"operation",operation,"exception",exception,"return",returned,"before",before,"after",snapshot(),
            "events",events,"drops",drops,"player-random-before",playerBefore,"player-random-after",seed(player.bs),"world-random-before",worldBefore,"world-random-after",seed(world.r),
            "focus-before",focusBefore,"focus-after",map("display-active",Display.isActive(),"N",app.N,"mouse-grabbed",org.lwjgl.input.Mouse.isGrabbed()),"gl-error",glError)));
        ++rows;
        // Teardown uses actual GS/DC close after the observed row. It is outside all selected boundaries.
        if(app.r!=null)player.r();require(app.r==null&&player.e==player.d,"source teardown closes retained scene");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        require(args.length==4,"jar game natives inputs");Locale.setDefault(Locale.ROOT);Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toAbsolutePath(),natives=Path.of(args[2]).toRealPath();Files.createDirectories(directory);
        Path jdk=Path.of(System.getProperty("java.home")).toRealPath();
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission value){}public void checkConnect(String h,int p){++network;throw new SecurityException("network");}public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String value){links.add(value);Path path=Path.of(value);boolean lwjgl=path.isAbsolute()&&path.normalize().equals(natives.resolve("lwjgl64.dll"));boolean java=path.isAbsolute()&&path.normalize().startsWith(jdk.resolve("bin"))||!path.isAbsolute()&&Files.isRegularFile(jdk.resolve("bin").resolve(value+".dll"));require(lwjgl||java,"declared native link "+value);}});
        for(Class<?> type:new Class<?>[]{kq.class,Minecraft.class,fd.class,dc.class,gs.class,ls.class,sn.class,ix.class,aa.class,oo.class,it.class,id.class,da.class,ob.class,lr.class,kv.class,xi.class,hl.class})require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"genuine archive class "+type.getName());
        app=new kq(null,null,null,854,480,false,null);
        try{
            require(field(Minecraft.class,"a").get(null)==app,"canonical App constructor identity");field(Minecraft.class,"Z").set(app,directory.toFile());
            Display.setDisplayMode(new DisplayMode(854,480));Display.setTitle("Original Workbench automatic close");Display.create();
            System.out.println("WORKBENCH_AUTO_CLOSE_DEVICE "+json(map("display-created",Display.isCreated(),"pbuffer",false,"vendor",GL11.glGetString(GL11.GL_VENDOR),"renderer",GL11.glGetString(GL11.GL_RENDERER),"version",GL11.glGetString(GL11.GL_VERSION))));
            Canvas component=new Canvas();component.setSize(854,480);app.C=new vy(component);
            app.z=new kv(app,directory.toFile());app.k=new gr("WorkbenchAutoClose","-");app.c=new ob(app);app.u=new vu(app);
            for(String line:Files.readAllLines(Path.of(args[3])))if(!line.isEmpty())run(line,directory);
            require(network==0,"no network attempts");
            System.out.println("WORKBENCH_AUTO_CLOSE_COMPLETE "+json(map("rows",rows,"network",network,"links",links,"display-created",Display.isCreated(),"raw-allocations",0,"game-substitution",false,"random-rebinding",false,"invocation-repairs",0,"source-input-attachments",rows)));
        }finally{app.J=false;for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){thread.interrupt();thread.join(2000);require(!thread.isAlive(),"Timer retired");}if(Display.isCreated())Display.destroy();}
    }
}
