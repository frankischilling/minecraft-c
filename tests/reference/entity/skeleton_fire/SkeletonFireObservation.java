import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** PRIVATE, UNCOMPILED preparation: actual unchanged FR calls and real pm callbacks. */
@SuppressWarnings({"unchecked","rawtypes"})
public final class SkeletonFireObservation {
    static volatile int active=-1;
    static fr actor;
    static sn target;
    static gi wolf;
    static sl current;
    static fd worldA,worldB;
    static lm chunkA,chunkB;
    static int[] profile;
    static final List<Object> callbacks=new ArrayList<>();
    static final Constructor<fr> ACTOR_CONSTRUCTOR;
    static {try{ACTOR_CONSTRUCTOR=fr.class.getDeclaredConstructor(fd.class);}catch(Exception e){throw new ExceptionInInitializerError(e);}}
    static void need(boolean value,String message){ArrowConstructionObservation.need(value,message);}
    static Object get(Object o,Class<?> c,String n)throws Exception{return ArrowConstructionObservation.get(o,c,n);}
    static void put(Object o,Class<?> c,String n,Object v)throws Exception{ArrowConstructionObservation.put(o,c,n,v);}
    static Map<String,Object> map(Object... a){return ConstructorJson.map(a);}
    static String d(double v){return ArrowConstructionObservation.d(v);}
    static String f(float v){return ArrowConstructionObservation.f(v);}
    static String world(fd value){need(value==null||value==worldA||value==worldB,"declared actual World");return value==null?null:value==worldA?"world-a":"world-b";}
    static String identity(Object value){if(value==null)return null;if(value==actor)return "skeleton";if(value==target)return "target";if(value==wolf)return "wolf";if(value==current)return "arrow";throw new AssertionError("undeclared actual Entity identity");}
    static Object snapshot(sn value)throws Exception{
        if(value==null)return null;
        ls old=ArrowConstructionObservation.shooter;
        try{ArrowConstructionObservation.shooter=value instanceof sl?(ls)get(value,sl.class,"c"):actor;
            Map<String,Object> data=(Map<String,Object>)ArrowConstructionObservation.snapshot(value);
            if(value instanceof sl)((Map)data.get("arrow")).put("c",identity(get(value,sl.class,"c")));
            return data;
        }finally{ArrowConstructionObservation.shooter=old;}
    }
    static Object actor()throws Exception{return map("entity",snapshot(actor),"cooldown",actor.ae,"attacking",get(actor,ii.class,"e"));}
    static List<Object> entities(List list){List<Object> values=new ArrayList<>();for(Object v:list)values.add(identity(v));return values;}
    static Object chunk(lm c){if(c==null)return null;List<Object> slices=new ArrayList<>();for(List list:c.m)slices.add(entities(list));return map("x",c.j,"z",c.k,"flags",List.of(c.n,c.o,c.p,c.q),"slices",slices);}
    static Object world(fd w,lm c)throws Exception{if(w==null)return null;List<Object> access=new ArrayList<>();for(Object v:(List)get(w,fd.class,"u")){need(v==null||v instanceof Probe,"actual declared WorldAccess");access.add(v==null?null:"probe");}
        return map("identity",world(w),"remote",w.B,"entities",entities(w.b),"players",entities(w.d),"accesses",access,"random",ArrowConstructionObservation.random(w.r),"chunk",chunk(c));}
    static Object context()throws Exception{return map("counter",get(null,sn.class,"a"),"actor",actor(),"target",snapshot(target),"wolf",snapshot(wolf),
        "world_a",world(worldA,chunkA),"world_b",world(worldB,chunkB),"math_random",ArrowConstructionObservation.random(ArrowConstructionObservation.shared));}
    static final class Probe implements pm {
        final fd owner;
        Probe(fd owner){this.owner=owner;}
        public void a(String name,double x,double y,double z,float volume,float pitch){try{
            callbacks.add(map("kind","sound","world",world(owner),"name",name,"position",List.of(d(x),d(y),d(z)),"volume",f(volume),"pitch",f(pitch),"actor",actor()));
            if(profile[7]==1)throw new IllegalStateException("authored sound callback failure");
            if(profile[7]==6)actor.aI=worldB;if(profile[7]==7)actor.aI=null;
            callbacks.add(map("kind","sound_return","world",world(owner),"actor_world",world(actor.aI)));
        }catch(RuntimeException|Error e){throw e;}catch(Exception e){throw new AssertionError(e);}}
        public void a(sn entity){try{
            need(entity instanceof sl,"genuine publication argument is Arrow");current=(sl)entity;ArrowConstructionObservation.current=current;
            callbacks.add(map("kind","added","world",world(owner),"arrow",snapshot(current),"world_state",world(owner,owner==worldA?chunkA:chunkB)));
            if(profile[7]==2)throw new IllegalStateException("authored publication callback failure");
            if(profile[7]==3)current.c=null;
            if(profile[7]==4)put(current,sn.class,"bs",null);
            if(profile[7]==5)current.c=wolf;
            if(profile[7]==8)((Random)get(current,sn.class,"bs")).setSeed(profile[9]);
            callbacks.add(map("kind","added_return","world",world(owner),"arrow",snapshot(current)));
        }catch(RuntimeException|Error e){throw e;}catch(Exception e){throw new AssertionError(e);}}
        static void unexpected(){throw new AssertionError("unselected actual WorldAccess slot");}
        public void a(int x,int y,int z){unexpected();}
        public void b(int a,int b,int c,int d,int e,int f){unexpected();}
        public void a(String n,double x,double y,double z,double a,double b,double c){unexpected();}
        public void b(sn e){unexpected();}
        public void e(){unexpected();}
        public void a(String n,int x,int y,int z){unexpected();}
        public void a(int x,int y,int z,ow t){unexpected();}
        public void a(gs p,int a,int b,int c,int d,int e){unexpected();}
    }
    static void prepare(int[] p)throws Exception{
        current=null;ArrowConstructionObservation.current=null;callbacks.clear();chunkA=chunkB=null;
        worldA=new fd(new wy(),"SkeletonFireA"+p[0],xa.a(0),5143L+2L*p[0]);worldA.a(new br(8,64,8));
        worldB=p[7]==6?new fd(new wy(),"SkeletonFireB"+p[0],xa.a(0),5144L+2L*p[0]):null;if(worldB!=null)worldB.a(new br(8,64,8));
        ArrowConstructionObservation.worldA=worldA;ArrowConstructionObservation.worldB=worldB;
        need(ACTOR_CONSTRUCTOR.getDeclaringClass()==fr.class&&ACTOR_CONSTRUCTOR.getParameterTypes()[0]==fd.class,"genuine reflected actor constructor");
        actor=ACTOR_CONSTRUCTOR.newInstance(worldA);wolf=new gi(worldA);ArrowConstructionObservation.shooter=actor;
        double x=p[0]==12?32008.5:8.5;actor.c(x,64,8.5,30f,10f);actor.ae=p[2];put(actor,ii.class,"e",false);
        switch(p[3]){case 0:target=null;break;case 1:target=new dc(ArrowConstructionObservation.app,worldA,ArrowConstructionObservation.app.k,0);break;
            case 2:target=wolf;break;case 3:target=new sl(worldA);break;case 4:target=new bp(worldA);break;case 5:target=new fr(worldA);break;default:throw new AssertionError("target selector");}
        if(target!=null)target.c(x+7,66.25,11.25,0f,0f);
        if(p[5]!=0){chunkA=worldA.c(0,0);need(chunkA.getClass()==lm.class&&worldA.w().getClass()==ok.class&&worldA.w().a(0,0),"actual normal loaded Chunk");
            if(worldB!=null){chunkB=worldB.c(0,0);need(chunkB.getClass()==lm.class,"actual distinct World loaded Chunk");}}
        if(p[0]==12)need(!worldA.w().a(2000,0),"actual far source Chunk is unrequested");
        worldA.B=p[4]==2;if(p[4]==0)actor.aI=null;
        if(p[6]==1)worldA.a((pm)null);worldA.a(new Probe(worldA));if(worldB!=null)worldB.a(new Probe(worldB));
        if(p[8]==1)put(actor,sn.class,"bs",null);
        ArrowConstructionObservation.selectedWorld=actor.aI;
    }
    @SuppressWarnings("removal")public static void main(String[] args)throws Exception{
        need(args.length==4,"archive work profiles classes");ArrowConstructionObservation.archive=Path.of(args[0]).toRealPath();ArrowConstructionObservation.directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new ArrowConstructionObservation.Guard());ArrowConstructionObservation.bind(Path.of(args[3]));
        ArrowConstructionObservation.shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");ArrowConstructionObservation.bootstrap();
        System.out.println("FR_CONSTRUCTOR declaring=fr descriptor=(Lfd;)V modifiers="+ACTOR_CONSTRUCTOR.getModifiers());int count=0;
        try{for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            String[] words=line.trim().split("\\s+");need(words.length==10,"full literal profile");profile=new int[10];for(int i=0;i<10;i++)profile[i]=i==1?Integer.parseUnsignedInt(words[i],16):Integer.parseInt(words[i]);need(profile[0]==count,"profile order");prepare(profile);
            Object before=context();boolean returned=false;String exception="NONE",message=null;List<String> frames=new ArrayList<>();
            active=count;try{actor.a(target,Float.intBitsToFloat(profile[1]));returned=true;}
            catch(Throwable failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();for(StackTraceElement frame:failure.getStackTrace()){if(frame.getClassName().equals("SkeletonFireObservation"))break;frames.add(frame.getClassName()+"."+frame.getMethodName());}}
            finally{active=-1;}
            List<Object> input=new ArrayList<>();for(int i=0;i<10;i++)input.add(i==1?String.format(Locale.ROOT,"%08x",profile[i]):profile[i]);
            System.out.println("SKELETON_FIRE "+ConstructorJson.json(map("schema","SKELETON_FIRE_RAW_V1","case",count,"input",input,"returned",returned,"exception",exception,
                "before",before,"after",context(),"arrow_after",snapshot(current),"callbacks",new ArrayList<>(callbacks),"exception_message",message,"exception_frames",frames,
                "source_raw",map("actor",ArrowConstructionObservation.raw(actor),"arrow",ArrowConstructionObservation.raw(current),"target",ArrowConstructionObservation.raw(target)))));++count;
        }need(count==24&&ArrowConstructionObservation.network==0&&ArrowConstructionObservation.nativeLinks==0&&ArrowConstructionObservation.rejectedWrites==0&&ArrowConstructionObservation.execs==0,"finite source/effect guards");
        System.out.println("SKELETON_FIRE_COMPLETE cases=24 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
        }finally{ArrowConstructionObservation.workers();ArrowConstructionObservation.app.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker retirement");}}
    }
}
