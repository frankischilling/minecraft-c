import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Prepared fields and passive observations around the complete unchanged CQ body. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class NightmareObservation {
    static volatile int active=-1;
    static fd world;
    static dc[] players;
    static List passed;
    static Random shared;
    static Path archive;
    static int callbackMode, additions, network, nativeLinks, writes, execs;
    static final List<Object> notifications=new ArrayList<>();
    static final IdentityHashMap<Object,String> identities=new IdentityHashMap<>();
    static Class<?>[] defaultClasses;
    static int fresh;
    static final int[][] FACING={{0,1},{-1,0},{0,-1},{1,0}};

    static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object get(Object value,Class<?> owner,String name)throws Exception{return field(owner,name).get(value);}
    static void put(Object value,Class<?> owner,String name,Object replacement)throws Exception{field(owner,name).set(value,replacement);}
    static Map<String,Object> map(Object... values){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<values.length;i+=2)result.put((String)values[i],values[i+1]);return result;}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String hash(byte[] data)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data));}
    static void label(Object value,String name){if(value!=null)identities.put(value,name);}
    static Object identity(Object value){return value==null?null:identities.computeIfAbsent(value,key->key.getClass().getName()+"-"+fresh++);}
    static Object scalar(Object value){
        if(value==null||value instanceof Boolean||value instanceof Integer||value instanceof Byte||value instanceof Short)return value;
        if(value instanceof Character)return(int)(Character)value;
        if(value instanceof Float)return map("F",f((Float)value));
        if(value instanceof Double)return map("D",d((Double)value));
        if(value instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)value));
        if(value instanceof String){List<Integer> units=new ArrayList<>();for(char c:((String)value).toCharArray())units.add((int)c);return map("utf16",units);}
        return map("class",value.getClass().getName(),"identity",identity(value));
    }
    static Object fields(Object value)throws Exception{
        if(value==null)return null;
        List<Field> fields=new ArrayList<>();for(Class<?> owner=value.getClass();owner!=Object.class;owner=owner.getSuperclass())for(Field f:owner.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers()))fields.add(f);
        fields.sort(Comparator.comparing(f->f.getDeclaringClass().getName()+"."+f.getName()));
        Map<String,Object> result=new LinkedHashMap<>();for(Field f:fields){f.setAccessible(true);result.put(f.getDeclaringClass().getName()+"."+f.getName(),scalar(f.get(value)));}return result;
    }
    static Object random(Random value)throws Exception{return value==null?null:List.of(String.format(Locale.ROOT,"%016x",((AtomicLong)get(value,Random.class,"seed")).get()),get(value,Random.class,"haveNextNextGaussian"),d((Double)get(value,Random.class,"nextNextGaussian")));}
    static Object xyz(br value){return value==null?null:List.of(value.a,value.b,value.c);}
    static Object box(eq value){return value==null?null:List.of(d(value.a),d(value.b),d(value.c),d(value.d),d(value.e),d(value.f));}
    static Object entity(sn value)throws Exception{
        Map<String,Object> result=map("class",value.getClass().getName(),"identity",identity(value),"fields",fields(value),"bounds",box(value.aW),"random",random(value.bs),"rider",identity(value.aG),"mount",identity(value.aH));
        if(value instanceof gs){gs p=(gs)value;result.put("bed",xyz(p.v));result.put("spawn",xyz(p.Q()));result.put("private_coordinate",xyz((br)get(p,gs.class,"bN")));result.put("timer",get(p,gs.class,"a"));}
        return result;
    }
    static Object members(List values){if(values==null)return null;List<Object> result=new ArrayList<>();for(Object value:values)result.add(scalar(value));return result;}
    static Object list(List values)throws Exception{return values==null?null:map("identity",identity(values),"class",values.getClass().getName(),"size",values.size(),"mod_count",values instanceof ArrayList?get(values,AbstractList.class,"modCount"):null,"members",members(values));}
    static Map chunks()throws Exception{return(Map)get(get(world,fd.class,"v"),ok.class,"e");}
    static Object chunkPlanes()throws Exception{
        List<lm> values=new ArrayList<>();for(Object value:chunks().values())values.add((lm)value);values.sort(Comparator.comparingInt((lm c)->c.j).thenComparingInt(c->c.k));
        List<Object> result=new ArrayList<>();for(lm chunk:values)result.add(map("x",chunk.j,"z",chunk.k,"blocks",hash(chunk.b),"metadata",hash(chunk.e.a),"sky",hash(chunk.f.a),"block_light",hash(chunk.g.a),"height",hash(chunk.h)));
        return result;
    }
    static Object state()throws Exception{
        List<Object> ps=new ArrayList<>();for(dc p:players)ps.add(entity(p));
        List<Object> entities=new ArrayList<>();for(Object value:world.b)entities.add(entity((sn)value));
        Object info=get(world,fd.class,"x");
        return map("players",ps,"entities",entities,"world_fields",fields(world),"info_fields",fields(info),"world_random",random(world.r),"math_random",random(shared),"world_players",list(world.d),"world_entities",list(world.b),"passed",list(passed),"passed_is_world_players",passed==world.d,"world_sleeping",get(world,fd.class,"J"),"entity_counter",get(null,sn.class,"a"),"class_array",classArray(),"chunks",chunkPlanes(),"notifications",new ArrayList<>(notifications));
    }
    static Object classArray()throws Exception{Class<?>[] array=(Class<?>[])get(null,cq.class,"a");if(array==null)return null;List<Object> result=new ArrayList<>();for(Class<?> value:array)result.add(value==null?null:value.getName());return result;}
    static lm chunk(int x,int z)throws Exception{lm value=(lm)chunks().get(yy.a(x>>4,z>>4));need(value!=null,"authored normal Chunk covers query");return value;}
    static void cell(int x,int y,int z,int id,int metadata)throws Exception{lm value=chunk(x,z);int at=((x&15)*16+(z&15))*128+y;value.b[at]=(byte)id;int shift=(at&1)*4;value.e.a[at>>1]=(byte)((value.e.a[at>>1]&~(15<<shift))|((metadata&15)<<shift));}
    static void barrier(int x,int z)throws Exception{for(int xx=x-3;xx<=x+3;++xx)for(int zz=z-3;zz<=z+3;++zz)for(int y=64;y<=67;++y)if(!((xx==x&&zz==z)||(xx==x&&zz==z-1)))cell(xx,y,zz,1,0);}
    static dc player(int x,int z,int direction)throws Exception{
        int dx=FACING[direction][0],dz=FACING[direction][1];cell(x,64,z,26,direction|8);cell(x-dx,64,z-dz,26,direction);
        dc p=new dc(null,world,new gr("NightmareComponent","-"),0);p.e(x+.5,64.0+(double)1.62f,z+.5);need(world.b(p),"actual Player insertion");
        p.a(new br(42,66,44));put(p,gs.class,"bN",new br(-11,65,13));
        need(p.b(x,64,z)==cw.a&&p.u,"actual successful sleep entry");put(p,gs.class,"a",100);cell(x,64,z,26,direction|12);return p;
    }
    static void prepare(long[] p)throws Exception{
        identities.clear();fresh=0;notifications.clear();additions=0;callbackMode=(int)p[7];
        Class<?>[] classes=(Class<?>[])get(null,cq.class,"a");System.arraycopy(defaultClasses,0,classes,0,classes.length);
        world=new fd(new wy(),"NightmareReference",xa.a(0),5143L);world.a(new br(8,64,8));world.B=false;world.q=2;world.f=13;
        Map map=chunks();map.clear();List loaded=(List)get(get(world,fd.class,"v"),ok.class,"f");loaded.clear();
        for(int x=-4;x<=4;++x)for(int z=-4;z<=4;++z){lm c=new lm(world,new byte[32768],x,z);c.n=true;for(int xx=0;xx<16;++xx)for(int zz=0;zz<16;++zz)c.b[(xx*16+zz)*128+63]=1;Arrays.fill(c.h,(byte)64);c.i=64;map.put(yy.a(x,z),c);loaded.add(c);}
        put(null,sn.class,"a",0);players=new dc[(int)p[4]];for(int i=0;i<players.length;++i)players[i]=player(i==0?8:-8,i==0?8:-8,(int)p[3]);
        int scene=(int)p[1];
        if(scene==0||scene==1)for(Object value:map.values()){lm c=(lm)value;Arrays.fill(c.b,(byte)(scene==0?0:1));Arrays.fill(c.e.a,(byte)0);}
        if(scene==3)for(Object value:map.values()){lm c=(lm)value;Arrays.fill(c.f.a,(byte)255);Arrays.fill(c.g.a,(byte)255);}
        if(scene==4||scene==6)barrier(8,8);
        if(scene==7)barrier(-8,-8);
        if(scene==5)for(int x=5;x<=11;++x)for(int z=5;z<=11;++z)if(!((x==8&&z==8)||(x==8&&z==7)))cell(x,64,z,50,5);
        if(scene==8){cell(6,64,8,1,0);cell(7,64,9,1,0);cell(7,65,8,1,0);}
        if(p[5]==1)Arrays.fill(classes,null);
        if(p[5]==2)Arrays.fill(classes,fd.class);
        if(p[5]==5)Arrays.fill(classes,uz.class);
        if(p[5]==6)Arrays.fill(classes,cn.class);
        passed=world.d;if(p[6]==1)passed=new ArrayList();if(p[6]==2)passed=null;
        label(world,"world");label(world.d,"entry-world-players");label(world.b,"world-entities");label(passed,"passed-list");
        for(int i=0;i<players.length;++i){label(players[i],"player"+i);label(players[i].aW,"player"+i+"-bounds");label(players[i].bs,"player"+i+"-random");label(players[i].v,"player"+i+"-bed");label(players[i].Q(),"player"+i+"-spawn");}
        world.r.setSeed(p[2]);shared.setSeed(0);put(null,sn.class,"a",1000);eq.b();bt.b();world.a(new Observer());
    }
    static final class Observer implements pm{
        public void a(sn value){try{++additions;label(value,"published"+additions);notifications.add(map("event","entity_added","class",value.getClass().getName(),"id",value.aD,"world_entities",world.b.size(),"world_players",world.d.size()));if(additions==1&&callbackMode==1&&players.length>1)world.d.remove(players[1]);if(additions==1&&callbackMode==2)world.d.add(players[0]);if(additions==1&&callbackMode==3)put(world,fd.class,"d",new ArrayList(world.d));if(additions==1&&callbackMode==4)throw new IllegalStateException("authored nightmare publication callback");}catch(RuntimeException e){throw e;}catch(Exception e){throw new AssertionError(e);}}
        public void a(String name,double x,double y,double z,float volume,float pitch){notifications.add(map("event","sound","name",name,"position",List.of(d(x),d(y),d(z)),"volume",f(volume),"pitch",f(pitch)));}
        public void a(int x,int y,int z){notifications.add(map("event","cell","position",List.of(x,y,z)));}
        public void b(int x,int y,int z,int a,int b,int c){notifications.add(map("event","range","bounds",List.of(x,y,z,a,b,c)));}
        public void b(sn value){throw new AssertionError("unexpected removal");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow value){throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected event");}
    }
    @SuppressWarnings("removal")static final class Guard extends SecurityManager{
        public void checkPermission(Permission permission){}
        public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
        public void checkListen(int port){++network;throw new SecurityException("listen");}
        public void checkLink(String name){++nativeLinks;throw new SecurityException("native link");}
        public void checkWrite(String path){++writes;throw new SecurityException("write");}
        public void checkDelete(String path){++writes;throw new SecurityException("delete");}
        public void checkExec(String command){++execs;throw new SecurityException("exec");}
    }
    static void bind(Path names)throws Exception{for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){Class<?> type=Class.forName(name.replace('/','.'),false,NightmareObservation.class.getClassLoader());need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"actual archive origin "+name);byte[] data;try(InputStream in=type.getResourceAsStream("/"+name+".class")){need(in!=null,"class resource");data=in.readAllBytes();}System.out.println("CLASS "+name+" "+data.length+" "+hash(data));}}
    @SuppressWarnings("removal")public static void main(String[] args)throws Exception{
        need(args.length==3,"archive profiles classes");archive=Path.of(args[0]).toRealPath();System.setSecurityManager(new Guard());
        new gr("NightmareSetup","-");Class.forName("cq");defaultClasses=((Class<?>[])get(null,cq.class,"a")).clone();shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");bind(Path.of(args[2]));int count=0;
        for(String line:Files.readAllLines(Path.of(args[1]),StandardCharsets.US_ASCII)){
            long[] p=Arrays.stream(line.split(" ")).mapToLong(Long::parseLong).toArray();need(p.length==8&&p[0]==count,"ordered declared profile");prepare(p);Object before=state();Object result=null;String exception="NONE";List<String> frames=new ArrayList<>();
            System.err.println("NIGHTMARE_STDERR_BEGIN "+count);
            active=count;try{result=cq.a(p[6]==3||p[6]==4?null:world,p[6]==4?null:passed);}catch(Throwable e){exception=e.getClass().getName().replace('.','/');for(StackTraceElement at:e.getStackTrace()){if(at.getClassName().equals("NightmareObservation"))break;frames.add(at.getClassName()+"."+at.getMethodName());}}finally{active=-1;}
            System.err.println("NIGHTMARE_STDERR_END "+count);
            Object after=state();List<Long> input=new ArrayList<>();for(long word:p)input.add(word);
            System.out.println("NIGHTMARE "+json(map("schema","NIGHTMARE_RAW_V1","case",count,"input",input,"result",result,"exception",exception,"exception_frames",frames,"before",before,"after",after)));
            ++count;players=null;world=null;passed=null;identities.clear();System.gc();
        }
        need(network==0&&nativeLinks==0&&writes==0&&execs==0,"external effect guards");System.out.println("NIGHTMARE_COMPLETE cases="+count+" network=0 native=0 writes=0 exec=0 ticks=0 GUI=0");
    }
    static String json(Object value){if(value==null)return"null";if(value instanceof Boolean||value instanceof Number)return value.toString();if(value instanceof String){StringBuilder out=new StringBuilder("\"");for(char c:((String)value).toCharArray())if(c=='"'||c=='\\')out.append('\\').append(c);else if(c<32||c>126)out.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else out.append(c);return out.append('"').toString();}if(value instanceof Map){StringJoiner out=new StringJoiner(",","{","}");for(Map.Entry<?,?> item:((Map<?,?>)value).entrySet())out.add(json(item.getKey())+":"+json(item.getValue()));return out.toString();}if(value instanceof Iterable){StringJoiner out=new StringJoiner(",","[","]");for(Object item:(Iterable<?>)value)out.add(json(item));return out.toString();}throw new IllegalArgumentException("JSON value");}
}
