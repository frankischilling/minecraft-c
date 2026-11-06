import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.Permission;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Declared inputs and observations. All game receivers/bodies come from the supplied archive. */
public final class PlayerHurtObservation {
    static kq app;
    static xi writer;
    static fd world;
    static dc actor;
    static ix inventory;
    static iz stack;
    static Random shared, actorRandom;
    static Object mainArray, armorArray, box;
    static Observer observer;
    static volatile int active = -1;
    static int currentProfile, network, nativeLinks, rejectedWrites;
    static Path directory, archive;
    static List<Object> sounds;
    static Map<String,Object> checkpoint;

    static void need(boolean ok,String message) { if(!ok)throw new AssertionError(message); }
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static Object get(Object owner,Class<?> type,String name)throws Exception {return field(type,name).get(owner);}
    static void put(Object owner,Class<?> type,String name,Object value)throws Exception {field(type,name).set(owner,value);}
    static String fword(float value) {return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String dword(double value) {return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static Map<String,Object> map(Object... values) {return ConstructorJson.map(values);}
    static Object scalar(Object value) {
        if(value==null)return null;
        if(value instanceof Float)return map("F",fword((Float)value));
        if(value instanceof Double)return map("D",dword((Double)value));
        if(value instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)value));
        if(value instanceof Boolean)return value;
        if(value instanceof Number)return ((Number)value).intValue();
        if(value instanceof String) {
            List<Integer> units=new ArrayList<>();for(char c:((String)value).toCharArray())units.add((int)c);
            return map("utf16",units);
        }
        return map("nonnull",true);
    }
    static Map<String,Object> fields(Object value,Class<?>... owners)throws Exception {
        Map<String,Object> out=new TreeMap<>();
        for(Class<?> owner:owners)for(Field f:owner.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())) {
            f.setAccessible(true);out.put(owner.getName()+"."+f.getName(),scalar(f.get(value)));
        }
        return out;
    }
    static Object random(Random value)throws Exception {
        if(value==null)return null;
        return Arrays.asList(String.format(Locale.ROOT,"%016x",((AtomicLong)get(value,Random.class,"seed")).get()),
            get(value,Random.class,"haveNextNextGaussian"),dword((Double)get(value,Random.class,"nextNextGaussian")));
    }
    static Object bounds()throws Exception {
        Object value=get(actor,sn.class,"aW");if(value==null)return null;
        List<Object> out=new ArrayList<>();for(String name:new String[]{"a","b","c","d","e","f"})
            out.add(scalar(get(value,eq.class,name)));
        return out;
    }
    static Object slots(Object array) {
        if(array==null)return null;
        List<Object> out=new ArrayList<>();for(Object value:(Object[])array) {
            need(value==null||value==stack,"only declared Stack identities in Inventory");
            out.add(value==null?null:"stack");
        }
        return out;
    }
    static Object stackState()throws Exception {
        if(stack==null)return null;
        int id=(Integer)get(stack,iz.class,"c");
        Object item=null;
        if(id>=0&&id<gm.c.length&&gm.c[id]!=null) {
            gm value=gm.c[id];
            item=map("class",value instanceof wa?"wa":"other","maximum",get(value,gm.class,"a"),
                "defense",value instanceof wa?get(value,wa.class,"bl"):null);
        }
        return map("id",id,"count",get(stack,iz.class,"a"),"damage",get(stack,iz.class,"d"),
            "animation",get(stack,iz.class,"b"),"item",item);
    }
    static Object statMap(String name)throws Exception {
        Map<?,?> value=(Map<?,?>)get(writer,xi.class,name);
        if(value==null)return null;
        TreeMap<Integer,Integer> sorted=new TreeMap<>();
        for(Map.Entry<?,?> entry:value.entrySet()) {
            int id=(Integer)get(entry.getKey(),vr.class,"e");
            need(sorted.put(id,((Number)entry.getValue()).intValue())==null,"unique statistic IDs");
        }
        List<Object> out=new ArrayList<>();for(Map.Entry<Integer,Integer> entry:sorted.entrySet())
            out.add(Arrays.asList(entry.getKey(),entry.getValue()));
        return out;
    }
    static Map<String,Object> state()throws Exception {
        List<Object> filters=new ArrayList<>();
        for(String name:new String[]{"bN","bO","bP"})filters.add(fields(get(actor,dc.class,name),cu.class));
        List<Integer> counts=new ArrayList<>();for(String name:new String[]{"b","d","e","D"})
            counts.add(((List<?>)get(world,fd.class,name)).size());
        return map("fields",fields(actor,sn.class,ls.class,gs.class,dc.class),"bounds",bounds(),
            "filters",filters,"inventory",map("visible",actor.c==inventory,"main",slots(get(inventory,ix.class,"a")),
                "armor",slots(get(inventory,ix.class,"b")),"selected",get(inventory,ix.class,"c"),
                "cursor",scalar(get(inventory,ix.class,"f")),"stack",stackState()),
            "random",Arrays.asList(random(actor.bs),random(world.r),random(shared)),
            "world",map("fields",fields(world,fd.class),"counts",counts),
            "writer",map("total",statMap("a"),"unsent",statMap("b"),"dirty",get(writer,xi.class,"c")),
            "aliases",map("world",actor.aI==world,"app",get(actor,dc.class,"b")==app,
                "writer",app.I==writer,"inventory",actor.c==inventory,
                "main_array",get(inventory,ix.class,"a")==mainArray,"armor_array",get(inventory,ix.class,"b")==armorArray,
                "bounds",get(actor,sn.class,"aW")==box,"actor_random",actor.bs==actorRandom,
                "default_active_container",actor.d==actor.e,
                "filters_distinct",get(actor,dc.class,"bN")!=get(actor,dc.class,"bO")&&
                    get(actor,dc.class,"bN")!=get(actor,dc.class,"bP")&&get(actor,dc.class,"bO")!=get(actor,dc.class,"bP")),
            "sounds",new ArrayList<>(sounds));
    }
    static final class Observer implements pm {
        public void a(String name,double x,double y,double z,float volume,float pitch) {
            try {
                sounds.add(map("name",name,"position",Arrays.asList(dword(x),dword(y),dword(z)),
                    "volume",fword(volume),"pitch",fword(pitch)));
                if(currentProfile==21)checkpoint=state();
                if(currentProfile==18)throw new IllegalStateException("authored Player sound observer failure");
                if(currentProfile==19)actor.aI=null;
            } catch(RuntimeException e) {throw e;} catch(Exception e) {throw new AssertionError(e);}
        }
        public void a(sn value) {throw new AssertionError("unexpected publication");}
        public void b(sn value) {throw new AssertionError("unexpected removal");}
        public void a(String n,double x,double y,double z,double a,double b,double c) {throw new AssertionError("unexpected particle");}
        public void a(int x,int y,int z) {throw new AssertionError("unexpected block notification");}
        public void b(int x,int y,int z,int a,int b,int c) {throw new AssertionError("unexpected range");}
        public void e() {throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z) {throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow value) {throw new AssertionError("unexpected tile");}
        public void a(gs p,int a,int b,int c,int d,int e) {throw new AssertionError("unexpected event");}
    }
    static void workers()throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        for(;;) {
            boolean found=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getClass().getName().equals("vk")||t.getClass().getName().equals("vj")) {
                found=true;t.join(100);
            }
            Object sync=get(writer,xi.class,"d");
            if(!found&&!(Boolean)get(sync,cj.class,"a"))return;
            need(System.nanoTime()<deadline,"statistics worker deadline");Thread.sleep(1);
        }
    }
    @SuppressWarnings("removal")
    static final class Guard extends SecurityManager {
        public void checkPermission(Permission p) {}
        public void checkConnect(String h,int p) {++network;throw new SecurityException("network");}
        public void checkListen(int p) {++network;throw new SecurityException("listener");}
        public void checkLink(String name) {++nativeLinks;throw new SecurityException("unexpected native link: "+name);}
        public void checkWrite(String name) {if(!Path.of(name).toAbsolutePath().normalize().startsWith(directory)){++rejectedWrites;throw new SecurityException("write outside work directory");}}
        public void checkDelete(String name) {checkWrite(name);}
    }
    static void bind(Path names,Path output)throws Exception {
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output,StandardCharsets.UTF_8))) {
            for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)) {
                Class<?> type=Class.forName(name.replace('/','.'),false,PlayerHurtObservation.class.getClassLoader());
                Path origin=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
                need(origin.equals(archive),"actual original origin "+name);
                byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name+".class")){need(in!=null,"class bytes");bytes=in.readAllBytes();}
                out.println(name+"\t"+bytes.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
            }
            need(!out.checkError(),"binding output");
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        need(args.length==5,"archive directory profiles classes binding");
        archive=Path.of(args[0]).toRealPath();directory=Path.of(args[1]).toAbsolutePath().normalize();Files.createDirectories(directory);
        System.setSecurityManager(new Guard());
        app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("Player123","-");put(app,net.minecraft.client.Minecraft.class,"Z",directory.toFile());
            writer=new xi(app.k,directory.toFile());app.I=writer;ep.f.a(new kh(app));workers();
            world=new fd(new wy(),"PlayerHurtReference",xa.a(0),5143L);world.a(new br(0,64,0));
            observer=new Observer();world.a(observer);int row=0;
            for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)) {
                String[] words=line.split(" ");int[] n=Arrays.stream(words).mapToInt(Integer::parseInt).toArray();
                need(n.length==9+n[8]&&n[0]==currentProfile,"fixed ordered profile");
                world.B=false;world.q=2;app.I=writer;
                actor=new dc(app,world,app.k,0);app.f=world;app.h=actor;app.i=actor;
                inventory=actor.c;mainArray=get(inventory,ix.class,"a");armorArray=get(inventory,ix.class,"b");
                actorRandom=actor.bs;stack=null;sounds=new ArrayList<>();checkpoint=null;
                actor.e(8.5,200.5,8.5);actor.aS=30;actor.aP=.25;actor.aQ=-.375;actor.aR=-.125;box=actor.aW;
                actor.E=20;actor.Y=n[1];actor.Z=17;actor.by=n[2];actor.aa=3;actor.ab=4;actor.ad=0;actor.ac=99;actor.al=.25f;actor.bb=false;actor.be=false;
                put(actor,ls.class,"au",n[3]);put(actor,ls.class,"av",17);put(actor,ls.class,"ah",false);put(actor,gs.class,"bO",n[4]);actor.u=false;
                if(n[5]!=0){stack=new iz(n[5],1,n[6]);((iz[])armorArray)[3]=stack;}
                if(shared==null)shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");
                System.out.println("PLAYER_HURT_CONSTRUCTOR "+ConstructorJson.json(map("profile",currentProfile,"state",state())));
                actor.bs.setSeed(38);world.r.setSeed(2468);shared.setSeed(0);
                if(n[7]==1)world.B=true;
                if(n[7]==2)actor.c=null;
                if(n[7]==3)put(inventory,ix.class,"b",null);
                if(n[7]==4)put(stack,iz.class,"c",-1);
                if(n[7]==5)put(actor,dc.class,"b",null);
                if(n[7]==6)app.I=null;
                if(n[7]==7)actor.aI=null;
                if(n[7]==8)put(actor,sn.class,"bs",null);
                for(int step=0;step<n[8];++step) {
                    Map<String,Object> before=state();Object result=null;String exception="NONE";
                    active=row;
                    try {result=actor.a((sn)null,n[9+step]);}
                    catch(NullPointerException e){exception="NPE";}
                    catch(ArrayIndexOutOfBoundsException e){exception="INDEX";}
                    catch(IllegalStateException e){need(currentProfile==18,"only declared callback");exception="CALLBACK";}
                    finally {active=-1;}
                    Map<String,Object> after=state();Object completion=null;
                    String scope=exception.equals("NONE")?"RETURN":"EXCEPTION";
                    if(currentProfile==21) {
                        need(checkpoint!=null&&Boolean.TRUE.equals(result)&&exception.equals("NONE"),"original actual death completion");
                        completion=map("result",result,"exception",exception,"state",after);after=checkpoint;scope="FATAL_PREFIX";
                    }
                    List<Integer> input=new ArrayList<>();for(int x:n)input.add(x);
                    System.out.println("PLAYER_HURT "+ConstructorJson.json(map("schema","PLAYER_HURT_V1","case",row++,"profile",currentProfile,"step",step,"input",input,"amount",n[9+step],"scope",scope,"result",result,"exception",exception,"before",before,"state",after,"completion",completion)));
                }
                ++currentProfile;
            }
            need(row==28&&currentProfile==22,"fixed packet inventory");bind(Path.of(args[3]),Path.of(args[4]));
            need(network==0&&nativeLinks==0&&rejectedWrites==0,"external-effect guards");
            System.out.println("Original Player hurt: 28 rows; 22 profiles; network=0; native=0; rejected_writes=0");
        } finally {
            if(writer!=null)workers();app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker retirement");}
        }
    }
}
