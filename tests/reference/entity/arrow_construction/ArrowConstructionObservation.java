import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Calls complete methods in the unchanged original archive, without ticking. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class ArrowConstructionObservation {
    static volatile int active=-1;
    static sl current,repeat;
    static ls shooter;
    static fd worldA,worldB,selectedWorld;
    static kq app;
    static xi writer;
    static ls[] shooters;
    static Random shared;
    static Path archive,directory;
    static int network,nativeLinks,rejectedWrites,execs;
    static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object get(Object receiver,Class<?> owner,String name)throws Exception{return field(owner,name).get(receiver);}
    static void put(Object receiver,Class<?> owner,String name,Object value)throws Exception{field(owner,name).set(receiver,value);}
    static Map<String,Object> map(Object... words){return ConstructorJson.map(words);}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static Object random(Random value)throws Exception{return value==null?null:List.of(
        String.format(Locale.ROOT,"%016x",((AtomicLong)get(value,Random.class,"seed")).get()),
        get(value,Random.class,"haveNextNextGaussian"),d((Double)get(value,Random.class,"nextNextGaussian")));}
    static Object payload(Object value){
        if(value==null)return map("kind","Null","value",null);
        if(value instanceof Byte)return map("kind","Byte","value",((Byte)value).intValue());
        if(value instanceof Integer)return map("kind","Integer","value",value);
        if(value instanceof String){List<Integer> units=new ArrayList<>();for(char c:((String)value).toCharArray())units.add((int)c);return map("kind","String","value",units);}
        throw new AssertionError("unrepresented actual watcher payload "+value.getClass().getName());
    }
    static Object watcher(ud value)throws Exception{
        if(value==null)return null;Map values=(Map)get(value,ud.class,"b");List<Object> records=new ArrayList<>();
        if(values!=null)for(Object key:new TreeSet(values.keySet())){
            ma record=(ma)values.get(key);need(record!=null,"genuine watcher record");
            records.add(map("key",key,"type",get(record,ma.class,"a"),"id",get(record,ma.class,"b"),
                "value",payload(get(record,ma.class,"c")),"dirty",get(record,ma.class,"d")));}
        return map("map_null",values==null,"dirty",get(value,ud.class,"c"),"records",records);
    }
    static List<String> doubles(sn value,String... names)throws Exception{List<String> result=new ArrayList<>();for(String name:names)result.add(d((Double)get(value,sn.class,name)));return result;}
    static List<String> floats(sn value,String... names)throws Exception{List<String> result=new ArrayList<>();for(String name:names)result.add(f((Float)get(value,sn.class,name)));return result;}
    static Object snapshot(sn value)throws Exception{
        if(value==null)return null;Object own=null;
        if(value instanceof sl){Map<String,Object> fields=new LinkedHashMap<>();for(String name:new String[]{"a","b","c","d","e","f","g","h","i","j","k"}){
            Object word=get(value,sl.class,name);if(name.equals("c")){need(word==null||word==shooter,"actual typed shooter reference");word=word==null?null:"shooter";}fields.put(name,word);}own=fields;}
        need(value.aI==null||value.aI==worldA||value.aI==worldB,"actual declared World reference");
        eq box=value.aW;
        return map("class",value.getClass().getName(),"id",value.aD,"world",value.aI==null?null:value.aI==worldA?"world-a":"world-b",
            "arrow",own,"position",doubles(value,"aM","aN","aO"),"motion",doubles(value,"aP","aQ","aR"),
            "history",doubles(value,"aJ","aK","aL","bl","bm","bn"),"angles",floats(value,"aS","aT","aU","aV"),
            "dimensions",floats(value,"bg","bh","bf"),"bounds",box==null?null:List.of(d(box.a),d(box.b),d(box.c),d(box.d),d(box.e),d(box.f)),
            "dead",value.be,"random",random(value.bs),"watcher",watcher(value.bD));
    }
    static Object raw(sn value)throws Exception{
        if(value==null)return null;List<Field> list=new ArrayList<>();for(Class<?> type=value.getClass();type!=Object.class;type=type.getSuperclass())
            for(Field f:type.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers()))list.add(f);
        list.sort(Comparator.comparing(f->f.getDeclaringClass().getName()+"."+f.getName()));Map<String,Object> result=new LinkedHashMap<>();
        for(Field f:list){f.setAccessible(true);Object word=f.get(value);Object typed;
            if(word==null||word instanceof Boolean||word instanceof Byte||word instanceof Short||word instanceof Integer)typed=word;
            else if(word instanceof Float)typed=map("F",f((Float)word));else if(word instanceof Double)typed=map("D",d((Double)word));
            else if(word instanceof Long)typed=map("J",String.format(Locale.ROOT,"%016x",(Long)word));
            else if(word instanceof String)typed=payload(word);else typed=map("class",word.getClass().getName(),"same_arrow",word==current,
                "same_shooter",word==shooter,"same_world_a",word==worldA,"same_world_b",word==worldB);
            result.put(f.getDeclaringClass().getName()+"."+f.getName(),typed);}return result;
    }
    static Object context()throws Exception{return map("counter",get(null,sn.class,"a"),"world_random",selectedWorld==null?null:random(selectedWorld.r),
        "math_random",random(shared),"shooter",snapshot(shooter));}
    static double[] geometry(int mode){switch(mode){case 0:return new double[]{7,-3,11};case 1:return new double[]{-0.0,0.0,-0.0};
        case 2:return new double[]{Double.MIN_VALUE,-Double.MIN_VALUE,-17.25};case 3:return new double[]{Double.longBitsToDouble(0x7ff8000000001234L),Double.POSITIVE_INFINITY,Double.NEGATIVE_INFINITY};default:throw new AssertionError("geometry");}}
    static double[] direction(int mode){double nan=Double.longBitsToDouble(0x7ff8000000001234L);switch(mode){
        case 0:return new double[]{1,2,3};case 1:return new double[]{0,0,0};case 2:return new double[]{-0.0,0.0,-0.0};
        case 3:return new double[]{Double.MIN_VALUE,-Double.MIN_VALUE,Double.MIN_VALUE};case 4:return new double[]{1e300,-1e300,1e300};
        case 5:return new double[]{nan,2,3};case 6:return new double[]{1,nan,3};case 7:return new double[]{1,2,nan};
        case 8:return new double[]{Double.POSITIVE_INFINITY,2,3};case 9:return new double[]{1,Double.NEGATIVE_INFINITY,3};case 10:return new double[]{1,2,Double.POSITIVE_INFINITY};default:throw new AssertionError("direction");}}
    static float argument(int mode,boolean speed){switch(mode){case 0:return speed?1.5f:1f;case 1:return 0f;case 2:return speed?-1.5f:-1f;
        case 3:return Float.intBitsToFloat(0x7fc01234);case 4:return Float.POSITIVE_INFINITY;default:throw new AssertionError("float selector");}}
    static List<String> vector(double[] values){return List.of(d(values[0]),d(values[1]),d(values[2]));}
    static void workers()throws Exception{
        long deadline=System.nanoTime()+10_000_000_000L;for(;;){boolean found=false;for(Thread t:Thread.getAllStackTraces().keySet())
            if(t.getClass().getName().equals("vk")||t.getClass().getName().equals("vj")){found=true;t.join(100);}
            if(!found&&!(Boolean)get(get(writer,xi.class,"d"),cj.class,"a"))return;need(System.nanoTime()<deadline,"statistics workers deadline");Thread.sleep(1);}
    }
    static void bootstrap()throws Exception{
        app=new kq(null,null,null,854,480,false,null);app.k=new gr("ArrowConstructor","-");put(app,net.minecraft.client.Minecraft.class,"Z",directory.toFile());
        writer=new xi(app.k,directory.toFile());app.I=writer;ep.f.a(new kh(app));workers();
        worldA=new fd(new wy(),"ArrowWorldA",xa.a(0),5143L);worldA.a(new br(8,64,8));
        worldB=new fd(new wy(),"ArrowWorldB",xa.a(0),5144L);worldB.a(new br(-8,64,-8));
        dc player=new dc(app,worldA,app.k,0);app.f=worldA;app.h=player;app.i=player;
        shooters=new ls[]{null,new fr(worldA),player,new gi(worldA),new bp(worldA)};
    }
    @SuppressWarnings("removal")static final class Guard extends SecurityManager{
        public void checkPermission(Permission p){}
        public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
        public void checkListen(int port){++network;throw new SecurityException("listen");}
        public void checkLink(String name){++nativeLinks;throw new SecurityException("native link");}
        public void checkExec(String command){++execs;throw new SecurityException("exec");}
        public void checkWrite(String path){if(!Path.of(path).toAbsolutePath().normalize().startsWith(directory)){++rejectedWrites;throw new SecurityException("outside isolated fixture");}}
        public void checkDelete(String path){checkWrite(path);}
    }
    static void bind(Path names)throws Exception{for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){
        Class<?> type=Class.forName(name.replace('/','.'),false,ArrowConstructionObservation.class.getClassLoader());
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"original class origin "+name);
        byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name+".class")){need(in!=null,"original class resource");bytes=in.readAllBytes();}
        System.out.println("CLASS "+name+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));}}
    @SuppressWarnings("removal")public static void main(String[] args)throws Exception{
        need(args.length==4,"archive work profiles classes");archive=Path.of(args[0]).toRealPath();directory=Path.of(args[1]).toRealPath();System.setSecurityManager(new Guard());bind(Path.of(args[3]));
        shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");int count=0;
        try{for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            int[] p=Arrays.stream(line.trim().split("\\s+")).mapToInt(Integer::parseInt).toArray();need(p.length==12&&p[0]==count,"ordered profile");
            if(count==1)bootstrap();shooter=shooters==null?null:shooters[p[3]];if(worldA!=null)worldA.B=p[2]==2;
            if(shooter!=null){shooter.c(8.5,64,8.5,30f,10f);if(p[11]==2)shooter.Y=0;}
            fd world=p[2]==0?null:p[2]==3?worldB:worldA;selectedWorld=world;double[] position=geometry(p[4]),direction=direction(p[5]);float speed=argument(p[6],true),spread=argument(p[7],false);
            current=null;if(p[1]==3){if(p[10]>=0){need(p[10]==16&&repeat!=null,"actual Arrow reuse");current=repeat;}
                else{current=new sl(world);current.aP=.25;current.aQ=-.375;current.aR=.5;current.aS=11;current.aT=-12;current.aU=13;current.aV=-14;
                    put(current,sl.class,"j",37);put(current,sl.class,"k",75);current.bs.setSeed(p[8]);if(p[9]!=0)current.bs.nextGaussian();}
                if(p[11]==1)put(current,sn.class,"bs",null);if(p[11]==3)current.be=true;}
            Object before=context(),arrowBefore=snapshot(current),rawBefore=raw(current);boolean returned=false;String exception="NONE",message=null;List<String> frames=new ArrayList<>();
            active=count;try{switch(p[1]){case 0:current=new sl(world);break;case 1:current=new sl(world,position[0],position[1],position[2]);break;
                case 2:current=new sl(world,shooter);break;case 3:current.a(direction[0],direction[1],direction[2],speed,spread);break;default:throw new AssertionError("entry");}returned=true;}
            catch(Throwable failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();for(StackTraceElement frame:failure.getStackTrace()){
                if(frame.getClassName().equals("ArrowConstructionObservation"))break;frames.add(frame.getClassName()+"."+frame.getMethodName());}}
            finally{active=-1;}
            List<Integer> input=new ArrayList<>();for(int word:p)input.add(word);
            System.out.println("ARROW_CONSTRUCTION "+ConstructorJson.json(map("schema","ARROW_CONSTRUCTION_RAW_V1","case",count,"input",input,
                "arguments",map("position",vector(position),"direction",vector(direction),"speed",f(speed),"spread",f(spread)),"returned",returned,"exception",exception,
                "before",before,"after",context(),"arrow_before",arrowBefore,"arrow_after",snapshot(current),"exception_message",message,"exception_frames",frames,
                "source_raw",map("arrow_before",rawBefore,"arrow_after",raw(current),"shooter",raw(shooter)))));
            if(count==16)repeat=current;++count;
        }need(count==40&&network==0&&nativeLinks==0&&rejectedWrites==0&&execs==0,"finite source/effect guards");
        System.out.println("ARROW_CONSTRUCTION_COMPLETE cases=40 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
        }finally{if(writer!=null)workers();if(app!=null){app.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){
            t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker retirement");}}}
    }
}
