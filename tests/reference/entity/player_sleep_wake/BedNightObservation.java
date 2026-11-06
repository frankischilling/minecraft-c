import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Declared saved-field/plane component inputs; genuine original receivers and bodies. */
public final class BedNightObservation {
    static volatile int active=-1;
    static fd world;static lm chunk;static dc actor,other;static eq originalBounds;
    static br beforeV,beforeQ,beforeBN;
    static Path archive,directory;
    static int network,nativeLinks,writes,execs;
    static final int[][] FACING={{0,1},{-1,0},{0,-1},{1,0}};
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static void need(boolean ok,String message){if(!ok)throw new AssertionError(message);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object v,Class<?> c,String n)throws Exception{return field(c,n).get(v);}
    static void put(Object v,Class<?> c,String n,Object x)throws Exception{field(c,n).set(v,x);}
    static String hex(byte[] b){return HexFormat.of().formatHex(b);}
    static String d(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String f(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static Object identity(Object v){return v==null?null:identities.computeIfAbsent(v,k->identities.size()+1);}
    static Object scalar(Object v){
        if(v==null||v instanceof Boolean||v instanceof Integer||v instanceof Byte||v instanceof Short)return v;
        if(v instanceof Character)return (int)(Character)v;
        if(v instanceof Float)return map("F",f((Float)v));if(v instanceof Double)return map("D",d((Double)v));
        if(v instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)v));
        return map("identity",identity(v),"class",v.getClass().getName());
    }
    static Object fields(Object v,Class<?>... types)throws Exception{
        if(v==null)return null;Map<String,Object> r=new TreeMap<>();List<Field> fs=new ArrayList<>();
        for(Class<?> c:types)for(Field p:c.getDeclaredFields())if(!Modifier.isStatic(p.getModifiers()))fs.add(p);
        fs.sort(Comparator.comparing(p->p.getDeclaringClass().getName()+"."+p.getName()));
        for(Field p:fs){p.setAccessible(true);r.put(p.getDeclaringClass().getName()+"."+p.getName(),scalar(p.get(v)));}return r;
    }
    static Object random(Random r)throws Exception{return Arrays.asList(String.format(Locale.ROOT,"%016x",((AtomicLong)get(r,Random.class,"seed")).get()),get(r,Random.class,"haveNextNextGaussian"),d((Double)get(r,Random.class,"nextNextGaussian")));}
    static Object xyz(br p){return p==null?null:List.of(p.a,p.b,p.c);}
    static Object bounds(eq b){return b==null?null:List.of(d(b.a),d(b.b),d(b.c),d(b.d),d(b.e),d(b.f));}
    static String hash(byte[] b)throws Exception{return hex(MessageDigest.getInstance("SHA-256").digest(b));}
    static void cell(int x,int y,int z,int id,int metadata){int at=((x&15)*16+(z&15))*128+y;chunk.b[at]=(byte)id;int shift=(at&1)*4;chunk.e.a[at>>1]=(byte)((chunk.e.a[at>>1]&~(15<<shift))|((metadata&15)<<shift));}
    static Object planes()throws Exception{return map("blocks",hash(chunk.b),"metadata",hash(chunk.e.a),"sky",hash(chunk.f.a),"block_light",hash(chunk.g.a),"height",hash(chunk.h));}
    static Object state()throws Exception{
        br q=actor.Q();br bn=(br)get(actor,gs.class,"bN");
        Object selected=map("position",List.of(d(actor.aM),d(actor.aN),d(actor.aO)),"motion",List.of(d(actor.aP),d(actor.aQ),d(actor.aR)),
            "dimensions",List.of(f(actor.bg),f(actor.bh),f(actor.bf)),"sleep_offsets",List.of(f(actor.w),f(actor.y)),
            "sleeping",actor.u,"timer",get(actor,gs.class,"a"),"v",xyz(actor.v),"Q",xyz(q),"private_bN",xyz(bn),
            "bounds",bounds(actor.aW),"old_bounds",bounds(originalBounds),"world_J",get(world,fd.class,"J"),"remote",world.B,
            "world_players",world.d.size(),"world_entities",world.b.size(),"other_sleeping",other==null?null:other.u,
            "world_same",actor.aI==world,"bounds_same",actor.aW==originalBounds,"v_same",actor.v==beforeV,
            "Q_same",q==beforeQ,"Q_v_same",q==actor.v,"Q_before_v_same",q==beforeV,"private_bN_same",bn==beforeBN,
            "head",List.of(chunk.b[(8*16+8)*128+64]&255,(chunk.e.a[((8*16+8)*128+64)>>1]&15)),"planes",planes());
        return map("selected",selected,"actor_fields",fields(actor,sn.class,ls.class,gs.class,dc.class),
            "world_fields",fields(world,fd.class),"provider_fields",fields(world.t,xa.class),
            "bounds_fields",fields(actor.aW,eq.class),"old_bounds_fields",fields(originalBounds,eq.class),
            "coordinate_fields",Arrays.asList(fields(actor.v,br.class),fields(q,br.class),fields(bn,br.class)),
            "random",Arrays.asList(random(actor.bs),random(world.r)));
    }
    static dc player()throws Exception{dc p=new dc(null,world,new gr("BedNightComponent","-"),0);p.e(8.5,64.0+(double)1.62f,8.5);need(p.Y==20&&!p.u&&world.b(p),"actual healthy published DC");return p;}
    static void setup(int[] p)throws Exception{
        world=new fd(new wy(),"BedNightReference",xa.a(0),5143L);world.a(new br(8,64,8));chunk=world.c(0,0);
        need(chunk.getClass()==lm.class&&world.w().getClass()==ok.class,"actual normal loaded Chunk/Map provider");
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);Arrays.fill(chunk.h,(byte)64);
        for(int x=0;x<16;++x)for(int z=0;z<16;++z){cell(x,63,z,2,0);for(int y=64;y<128;++y){int index=(x*16+z)*128+y;chunk.f.a[index>>1]|=(byte)(15<<((index&1)*4));}}
        actor=player();other=null;actor.a(new br(42,66,44));put(actor,gs.class,"bN",new br(-11,65,13));
        put(actor,gs.class,"a",17);actor.w=.375f;actor.y=-.625f;actor.aP=.25;actor.aQ=-.375;actor.aR=.125;
        actor.bs.setSeed(38);world.r.setSeed(2468);world.f=5;
        int direction=p[2],dx=FACING[direction][0],dz=FACING[direction][1];
        cell(8,64,8,26,direction|8);cell(8-dx,64,8-dz,26,direction);
        if(p[1]==0){
            actor.v=new br(21,67,-23);
            if(p[4]==1)other=player();
            if(p[4]==2)world.B=true;
            if(p[4]==3)cell(8,64,8,0,direction|8);
        }else{
            need(actor.b(8,64,8)==cw.a&&actor.u&&(Boolean)get(world,fd.class,"J"),"genuine sleep-entry preparation and all-sleeping J");
            // Declared component inputs after the genuine entry, not elapsed ticks.
            put(actor,gs.class,"a",37);cell(8,64,8,26,direction|12);
            if(p[4]==1){cell(8,64,8,0,0);cell(8-dx,64,8-dz,0,0);}
            if(p[4]==2||p[4]==6)actor.v=null;
            if(p[4]==3){for(int x=6;x<=10;++x)for(int z=6;z<=10;++z)cell(x,65,z,1,0);}
            if(p[4]==4)world.B=true;
            if(p[4]==5||p[4]==6)actor.aI=null;
        }
        originalBounds=actor.aW;
        if((p[1]==0&&p[4]==4)||(p[1]==1&&p[4]==7))put(actor,sn.class,"aW",null);
        beforeV=actor.v;beforeQ=actor.Q();beforeBN=(br)get(actor,gs.class,"bN");identities.clear();
        need(world.d.contains(actor)&&world.b.contains(actor)&&world.d.size()==(other==null?1:2),"genuine actual World memberships");
    }
    @SuppressWarnings("removal") static final class Guard extends SecurityManager{
        public void checkPermission(Permission p){}
        public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
        public void checkListen(int p){++network;throw new SecurityException("listen");}
        public void checkLink(String n){++nativeLinks;throw new SecurityException("native link");}
        public void checkExec(String n){++execs;throw new SecurityException("exec");}
        public void checkWrite(String n){++writes;throw new SecurityException("write");}
        public void checkDelete(String n){++writes;throw new SecurityException("delete");}
    }
    static void bind(Path names)throws Exception{for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){Class<?> c=Class.forName(name,false,BedNightObservation.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"class origin "+name);byte[] b;try(InputStream in=c.getResourceAsStream("/"+name+".class")){need(in!=null,"class resource");b=in.readAllBytes();}System.out.println("CLASS "+name+" "+b.length+" "+hash(b));}}
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception{
        need(args.length==3,"jar profiles class names");archive=Path.of(args[0]).toRealPath();System.setSecurityManager(new Guard());bind(Path.of(args[2]));need(cw.class.getDeclaredFields().length==6,"passive enum preparation");int number=0;
        for(String line:Files.readAllLines(Path.of(args[1]),StandardCharsets.US_ASCII)){
            int[] p=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();need(p.length==5&&p[0]==number,"authored profile order");setup(p);
            Object before=state();cw result=null;String exception="NONE";
            active=number;try{if(p[1]==0)result=actor.b(8,64,8);else actor.a((p[3]&4)!=0,(p[3]&2)!=0,(p[3]&1)!=0);}
            catch(NullPointerException failure){exception="java/lang/NullPointerException";}
            finally{active=-1;}
            Object after=state();List<Integer> input=new ArrayList<>();for(int x:p)input.add(x);
            System.out.println("BED_NIGHT "+json(map("schema","BED_NIGHT_RAW_V1","case",number,"input",input,"result",result==null?null:map("name",result.name(),"ordinal",result.ordinal(),"current_a",result==cw.a),"exception",exception,"before",before,"after",after)));
            ++number;actor=null;other=null;world=null;chunk=null;originalBounds=null;beforeV=null;beforeQ=null;beforeBN=null;identities.clear();if(number%4==0)System.gc();
        }
        need(network==0&&nativeLinks==0&&writes==0&&execs==0,"source effect guards");System.out.println("BED_NIGHT_COMPLETE cases="+number+" network=0 native=0 writes=0 exec=0 ticks=0 GUI=0");
    }
    static Map<String,Object> map(Object... values){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<values.length;i+=2)r.put((String)values[i],values[i+1]);return r;}
    static String json(Object v){if(v==null)return "null";if(v instanceof Number||v instanceof Boolean)return v.toString();if(v instanceof String){StringBuilder r=new StringBuilder("\"");for(char c:((String)v).toCharArray())if(c=='"'||c=='\\')r.append('\\').append(c);else if(c<32||c>126)r.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else r.append(c);return r.append('"').toString();}if(v instanceof Map){StringJoiner r=new StringJoiner(",","{","}");for(Map.Entry<?,?> e:((Map<?,?>)v).entrySet())r.add(json(e.getKey())+":"+json(e.getValue()));return r.toString();}if(v instanceof Iterable){StringJoiner r=new StringJoiner(",","[","]");for(Object e:(Iterable<?>)v)r.add(json(e));return r.toString();}throw new IllegalArgumentException("JSON transport");}
}
