import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Prepared genuine owners around unchanged complete damage and ally-alert calls. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class PlayerAllyAlertObservation {
    static volatile int active=-1;
    static kq app;
    static xi writer;
    static fd world;
    static dc actor;
    static ls attacker;
    static bx sentinel;
    static gi[] wolves;
    static Random shared;
    static Path archive,directory;
    static int network,nativeLinks,rejectedWrites,execs,fresh;
    static final List<Object> events=new ArrayList<>();
    static final IdentityHashMap<Object,String> identities=new IdentityHashMap<>();
    static final List<sn> entities=new ArrayList<>();
    static void need(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static Field field(Class<?> owner,String name)throws Exception{Field value=owner.getDeclaredField(name);value.setAccessible(true);return value;}
    static Object get(Object value,Class<?> owner,String name)throws Exception{return field(owner,name).get(value);}
    static void put(Object value,Class<?> owner,String name,Object replacement)throws Exception{field(owner,name).set(value,replacement);}
    static Map<String,Object> map(Object... words){return ConstructorJson.map(words);}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static Object identity(Object value){return value==null?null:identities.computeIfAbsent(value,key->"reference-"+fresh++);}
    static Object scalar(Object value){
        if(value==null||value instanceof Boolean||value instanceof Integer||value instanceof Byte||value instanceof Short)return value;
        if(value instanceof Float)return map("F",f((Float)value));if(value instanceof Double)return map("D",d((Double)value));
        if(value instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)value));
        if(value instanceof String){List<Integer> units=new ArrayList<>();for(char unit:((String)value).toCharArray())units.add((int)unit);return map("utf16",units);}
        return map("class",value.getClass().getName(),"identity",identity(value));
    }
    static Object fields(Object value)throws Exception{
        if(value==null)return null;List<Field> fields=new ArrayList<>();
        for(Class<?> owner=value.getClass();owner!=Object.class;owner=owner.getSuperclass())
            for(Field f:owner.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers()))fields.add(f);
        fields.sort(Comparator.comparing(f->f.getDeclaringClass().getName()+"."+f.getName()));
        Map<String,Object> result=new LinkedHashMap<>();for(Field f:fields){f.setAccessible(true);result.put(f.getDeclaringClass().getName()+"."+f.getName(),scalar(f.get(value)));}return result;
    }
    static Object random(Random value)throws Exception{return value==null?null:List.of(
        String.format(Locale.ROOT,"%016x",((AtomicLong)get(value,Random.class,"seed")).get()),
        get(value,Random.class,"haveNextNextGaussian"),d((Double)get(value,Random.class,"nextNextGaussian")));}
    static Object box(eq value){return value==null?null:List.of(d(value.a),d(value.b),d(value.c),d(value.d),d(value.e),d(value.f));}
    static Object id(sn value){return value==null?null:value.aD;}
    static Object ids(List values){List<Object> result=new ArrayList<>();for(Object value:values)result.add(id((sn)value));return result;}
    static Object watcher(ud value)throws Exception{
        if(value==null)return null;Map values=(Map)get(value,ud.class,"b");List<Object> records=new ArrayList<>();
        if(values!=null)for(Object raw:new TreeSet(values.keySet())){
            ma record=(ma)values.get(raw);records.add(record==null?map("key",raw,"record",null):map("key",raw,"type",record.c(),
                "id",record.a(),"value",scalar(record.b()),"dirty",get(record,ma.class,"d")));}
        return map("map_null",values==null,"dirty",get(value,ud.class,"c"),"records",records);
    }
    static Object entity(sn value)throws Exception{return map("class",value.getClass().getName(),"id",value.aD,
        "identity",identity(value),"fields",fields(value),"bounds",box(value.aW),"random",random(value.bs),
        "watcher",watcher(value.bD),"target",value instanceof ii?id((sn)get(value,ii.class,"d")):null);}
    static Object statistics()throws Exception{
        List<Object> tables=new ArrayList<>();for(String name:new String[]{"a","b"}){
            TreeMap<Integer,Integer> ordered=new TreeMap<>();for(Object raw:((Map)get(writer,xi.class,name)).entrySet()){
                Map.Entry entry=(Map.Entry)raw;need(ordered.put((Integer)get(entry.getKey(),vr.class,"e"),((Number)entry.getValue()).intValue())==null,"unique actual statistic IDs");}
            List<Object> values=new ArrayList<>();for(Map.Entry<Integer,Integer> entry:ordered.entrySet())values.add(List.of(entry.getKey(),entry.getValue()));tables.add(values);}
        return map("total",tables.get(0),"unsent",tables.get(1),"dirty",get(writer,xi.class,"c"));
    }
    static Map chunks()throws Exception{return(Map)get(get(world,fd.class,"v"),ok.class,"e");}
    static Object chunkState()throws Exception{
        List<lm> ordered=new ArrayList<>();for(Object value:chunks().values())ordered.add((lm)value);
        ordered.sort(Comparator.comparingInt((lm value)->value.j).thenComparingInt(value->value.k));
        List<Object> result=new ArrayList<>();for(lm chunk:ordered){List<Object> slices=new ArrayList<>();
            for(List slice:(List[])get(chunk,lm.class,"m"))slices.add(ids(slice));
            MessageDigest digest=MessageDigest.getInstance("SHA-256");for(byte[] bytes:new byte[][]{chunk.b,chunk.e.a,chunk.f.a,chunk.g.a,chunk.h})digest.update(bytes);
            result.add(map("x",chunk.j,"z",chunk.k,"slices",slices,"planes",HexFormat.of().formatHex(digest.digest())));}
        return result;
    }
    static Object state()throws Exception{
        List<Object> values=new ArrayList<>();for(sn value:entities)values.add(entity(value));
        return map("entities",values,"actor_name",scalar(actor.l),"actor_world_null",actor.aI==null,
            "actor_app_null",get(actor,dc.class,"b")==null,"actor_inventory_null",actor.c==null,
            "world_entities",ids(world.b),"world_players",ids(world.d),"chunks",chunkState(),
            "world_random",random(world.r),"math_random",random(shared),"statistics",statistics(),
            "box_pool_used",get(null,eq.class,"h"),"events",new ArrayList<>(events));
    }
    static void publish(sn value,String name)throws Exception{
        identities.put(value,name);entities.add(value);need(world.b(value),"actual World publication "+name);
    }
    static void pose(sn value,double x,double y,double z,float yaw){value.c(x,y,z,yaw,0);value.aP=value.aQ=value.aR=0;}
    static ma record(gi value,int key)throws Exception{return(ma)((Map)get(value.bD,ud.class,"b")).get(key);}
    static void prepare(int[] p)throws Exception{
        identities.clear();entities.clear();events.clear();fresh=0;world.b.clear();world.d.clear();world.B=false;world.q=p[6];
        Map cache=chunks();cache.clear();List loaded=(List)get(get(world,fd.class,"v"),ok.class,"f");loaded.clear();
        for(int x=-2;x<=2;++x)for(int z=-2;z<=2;++z){lm chunk=new lm(world,new byte[32768],x,z);chunk.n=true;
            for(int xx=0;xx<16;++xx)for(int zz=0;zz<16;++zz)chunk.b[(xx*16+zz)*128+63]=1;
            Arrays.fill(chunk.h,(byte)64);chunk.i=64;cache.put(yy.a(x,z),chunk);loaded.add(chunk);}
        put(null,sn.class,"a",0);actor=new dc(app,world,app.k,0);app.f=world;app.h=actor;app.i=actor;
        pose(actor,8.5,64,8.5,30);publish(actor,"actor");
        switch(p[2]){case 0:attacker=new uz(world);break;case 1:case 2:case 3:attacker=new gi(world);break;
            case 4:attacker=new gb(world);break;case 5:attacker=new bp(world);break;
            case 6:attacker=new dc(app,world,new gr("OtherPlayer","-"),0);break;case 7:attacker=null;break;default:throw new AssertionError("attacker kind");}
        if(attacker!=null){pose(attacker,10.5,64,8.5,0);publish(attacker,"attacker");}
        if(p[2]==2||p[2]==3){gi target=(gi)attacker;target.d(true);target.a(p[2]==2?actor.l:"ForeignPlayer");}
        sentinel=new bx(world);pose(sentinel,-10.5,64,-10.5,0);publish(sentinel,"sentinel");
        wolves=new gi[6];for(int i=0;i<wolves.length;++i){gi wolf=new gi(world);wolves[i]=wolf;
            pose(wolf,i==5?40.5:9.5+i,64,9.5,0);wolf.d(i!=3);wolf.b(i==1||i==4);
            wolf.a(i==2?"ForeignPlayer":actor.l);if(i==4)wolf.c(sentinel);publish(wolf,"wolf"+i);}
        actor.E=20;actor.Y=p[5];actor.Z=17;actor.by=0;actor.aa=3;actor.ab=4;actor.ad=0;actor.ac=99;actor.al=.25f;
        actor.aP=.25;actor.aQ=-.375;actor.aR=-.125;actor.bb=false;actor.be=false;actor.u=false;
        put(actor,ls.class,"au",0);put(actor,ls.class,"av",17);put(actor,ls.class,"ah",false);put(actor,gs.class,"bO",0);
        ((Map)get(writer,xi.class,"a")).clear();((Map)get(writer,xi.class,"b")).clear();put(writer,xi.class,"c",false);
        for(sn value:entities){if(value.bD!=null){put(value.bD,ud.class,"c",false);for(Object record:((Map)get(value.bD,ud.class,"b")).values())((ma)record).a(false);}}
        eq.b();events.clear();int mode=p[3];
        if(mode==1)actor.l=null;
        if(mode==2)attacker.bD=null;
        if(mode==3)put(attacker.bD,ud.class,"b",null);
        if(mode==4)((Map)get(attacker.bD,ud.class,"b")).remove(16);
        if(mode==5)record((gi)attacker,16).a((Object)null);
        if(mode==6)record((gi)attacker,16).a(Integer.valueOf(7));
        if(mode==7)record((gi)attacker,17).a((Object)null);
        if(mode==8)record((gi)attacker,17).a(Integer.valueOf(7));
        if(mode==9)wolves[0].bD=null;
        if(mode==10)wolves[1].bD=null;
        if(mode==11)put(wolves[0].bD,ud.class,"b",null);
        if(mode==12)((Map)get(wolves[0].bD,ud.class,"b")).remove(16);
        if(mode==13)record(wolves[0],16).a((Object)null);
        if(mode==14)record(wolves[0],16).a(Integer.valueOf(7));
        if(mode==15)record(wolves[0],17).a((Object)null);
        if(mode==16)record(wolves[0],17).a(Integer.valueOf(7));
        if(mode==17)((Map)get(wolves[0].bD,ud.class,"b")).remove(17);
        if(mode==18)actor.aI=null;
        if(mode==19)put(actor,dc.class,"b",null);
        if(mode==20)actor.c=null;
        world.B=p[7]!=0;
    }
    static final class Observer implements pm{
        public void a(String name,double x,double y,double z,float volume,float pitch){events.add(map("event","sound","name",name,
            "position",List.of(d(x),d(y),d(z)),"volume",f(volume),"pitch",f(pitch)));}
        public void a(sn value){events.add(map("event","entity_added","id",value.aD));}
        public void b(sn value){throw new AssertionError("unexpected removal");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void a(int x,int y,int z){throw new AssertionError("unexpected Block notification");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("unexpected range");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow value){throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected World event");}
    }
    static void workers()throws Exception{
        long deadline=System.nanoTime()+10_000_000_000L;for(;;){boolean found=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getClass().getName().equals("vk")||thread.getClass().getName().equals("vj")){found=true;thread.join(100);}
            if(!found&&!(Boolean)get(get(writer,xi.class,"d"),cj.class,"a"))return;
            need(System.nanoTime()<deadline,"statistics workers deadline");Thread.sleep(1);}
    }
    @SuppressWarnings("removal")static final class Guard extends SecurityManager{
        public void checkPermission(Permission permission){}
        public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
        public void checkListen(int port){++network;throw new SecurityException("listen");}
        public void checkLink(String name){++nativeLinks;throw new SecurityException("native link");}
        public void checkExec(String command){++execs;throw new SecurityException("exec");}
        public void checkWrite(String path){if(!Path.of(path).toAbsolutePath().normalize().startsWith(directory)){++rejectedWrites;throw new SecurityException("outside isolated statistics directory");}}
        public void checkDelete(String path){checkWrite(path);}
    }
    static void bind(Path names)throws Exception{for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){
        Class<?> type=Class.forName(name.replace('/','.'),false,PlayerAllyAlertObservation.class.getClassLoader());
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"original class origin "+name);
        byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name+".class")){need(in!=null,"class resource");bytes=in.readAllBytes();}
        System.out.println("CLASS "+name+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));}}
    @SuppressWarnings("removal")public static void main(String[] args)throws Exception{
        need(args.length==4,"archive work profiles classes");archive=Path.of(args[0]).toRealPath();directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new Guard());bind(Path.of(args[3]));app=new kq(null,null,null,854,480,false,null);
        try{app.k=new gr("AllyPlayer","-");put(app,net.minecraft.client.Minecraft.class,"Z",directory.toFile());
            writer=new xi(app.k,directory.toFile());app.I=writer;ep.f.a(new kh(app));workers();
            world=new fd(new wy(),"PlayerAllyAlertReference",xa.a(0),5143L);world.a(new br(8,64,8));world.a(new Observer());
            shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");
            Method helper=gs.class.getDeclaredMethod("a",ls.class,boolean.class);helper.setAccessible(true);int count=0;
            for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
                int[] input=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();need(input.length==8&&input[0]==count,"ordered profile");
                prepare(input);Object before=state(),result=null;String exception="NONE",message=null;List<String> frames=new ArrayList<>();
                active=count;try{if(input[1]==0)result=actor.a((sn)attacker,input[4]);else helper.invoke(actor,attacker,input[1]==2);}
                catch(Throwable failure){if(failure instanceof InvocationTargetException)failure=((InvocationTargetException)failure).getCause();
                    exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();
                    for(StackTraceElement frame:failure.getStackTrace()){if(frame.getClassName().equals("PlayerAllyAlertObservation"))break;frames.add(frame.getClassName()+"."+frame.getMethodName());}}
                finally{active=-1;}
                Object after=state();List<Integer> declared=new ArrayList<>();for(int word:input)declared.add(word);
                System.out.println("PLAYER_ALLY_ALERT "+ConstructorJson.json(map("schema","PLAYER_ALLY_ALERT_RAW_V1","case",count,"input",declared,
                    "result",result,"exception",exception,"exception_message",message,"exception_frames",frames,"before",before,"after",after)));
                ++count;actor=null;attacker=null;sentinel=null;wolves=null;entities.clear();identities.clear();if(count%4==0)System.gc();}
            need(network==0&&nativeLinks==0&&rejectedWrites==0&&execs==0,"external effect guards");
            System.out.println("PLAYER_ALLY_ALERT_COMPLETE cases="+count+" network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
        }finally{if(writer!=null)workers();app.J=false;for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){
            thread.interrupt();thread.join(2000);need(!thread.isAlive(),"Timer worker retirement");}}
    }
}
