import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** New authored inputs and passive snapshots around ordinary unchanged calls. */
public final class PlayerNbtSleepMatrixObservation {
    static volatile int active=-1;
    static fd world; static lm chunk; static dc actor; static nu input;
    static eq originalBounds; static br beforeV,beforeQ,beforeBN;
    static Object beforeMain,beforeArmor,beforeInfo,beforeList;
    static SourceWorker worker;
    static Path archive;
    static int network,nativeLinks,writes,execs;
    static final int[][] FACING={{0,1},{-1,0},{0,-1},{1,0}};
    static int[] profile; static String profileName;
    static final IdentityHashMap<Object,Integer> identities=new IdentityHashMap<>();
    static void need(boolean b,String s){if(!b)throw new AssertionError(s);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static void put(Object o,Class<?> c,String n,Object v)throws Exception{field(c,n).set(o,v);}
    static String d(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String f(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static String hash(byte[] b)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b));}
    static Object identity(Object v){return v==null?null:identities.computeIfAbsent(v,k->identities.size()+1);}
    static Object scalar(Object v){
        if(v==null||v instanceof Boolean||v instanceof Integer||v instanceof Byte||v instanceof Short)return v;
        if(v instanceof Float)return map("F",f((Float)v));
        if(v instanceof Double)return map("D",d((Double)v));
        if(v instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)v));
        if(v instanceof Character)return (int)(Character)v;
        if(v instanceof String){List<Integer> a=new ArrayList<>();for(char c:((String)v).toCharArray())a.add((int)c);return map("utf16",a);}
        return map("identity",identity(v),"class",v.getClass().getName());
    }
    static Object fields(Object o,Class<?>... types)throws Exception{
        if(o==null)return null;
        Map<String,Object> r=new TreeMap<>();List<Field> fs=new ArrayList<>();
        for(Class<?> c:types)for(Field p:c.getDeclaredFields())if(!Modifier.isStatic(p.getModifiers()))fs.add(p);
        fs.sort(Comparator.comparing(p->p.getDeclaringClass().getName()+"."+p.getName()));
        for(Field p:fs){p.setAccessible(true);r.put(p.getDeclaringClass().getName()+"."+p.getName(),scalar(p.get(o)));}
        return r;
    }
    static Object random(Random r)throws Exception{return Arrays.asList(
        String.format(Locale.ROOT,"%016x",((AtomicLong)get(r,Random.class,"seed")).get()),
        get(r,Random.class,"haveNextNextGaussian"),d((Double)get(r,Random.class,"nextNextGaussian")));}
    static Object xyz(br p){return p==null?null:List.of(p.a,p.b,p.c);}
    static Object bounds(eq b){return b==null?null:List.of(d(b.a),d(b.b),d(b.c),d(b.d),d(b.e),d(b.f));}
    @SuppressWarnings("unchecked") static Map<String,ij> compound(nu n)throws Exception{return (Map<String,ij>)get(n,nu.class,"a");}
    static Object tag(ij t)throws Exception{
        if(t==null)return null;
        if(t instanceof nu){Map<String,Object> values=new TreeMap<>();for(Map.Entry<String,ij> e:compound((nu)t).entrySet())values.put(e.getKey(),tag(e.getValue()));return map("class",t.getClass().getName(),"kind",10,"values",values);}
        if(t instanceof sp){List<Object> values=new ArrayList<>();for(Object e:(List<?>)get(t,sp.class,"a"))values.add(tag((ij)e));return map("class",t.getClass().getName(),"kind",9,"element_type",get(t,sp.class,"b"),"values",values);}
        Object value=get(t,t.getClass(),"a");int kind=value instanceof Byte?1:value instanceof Short?2:value instanceof Integer?3:value instanceof Long?4:value instanceof Float?5:value instanceof Double?6:value instanceof String?8:-1;
        need(kind>=1,"prepared scalar tag kind");return map("class",t.getClass().getName(),"kind",kind,"value",scalar(value));
    }
    static Object stacks(Object array)throws Exception{
        List<Object> r=new ArrayList<>();for(Object s:(Object[])array)r.add(s==null?null:map("identity",identity(s),"fields",fields(s,iz.class)));return r;
    }
    static Object infoPlayer()throws Exception{
        Field selected=null;for(Field p:ei.class.getDeclaredFields())if(!Modifier.isStatic(p.getModifiers())&&p.getType()==nu.class){need(selected==null,"unique Info.Player field");selected=p;}
        need(selected!=null,"actual Info.Player field");selected.setAccessible(true);return selected.get(world.x);
    }
    static Object planes()throws Exception{return map("blocks",hash(chunk.b),"metadata",hash(chunk.e.a),"sky",hash(chunk.f.a),"block_light",hash(chunk.g.a),"height",hash(chunk.h));}
    static Object snapshot()throws Exception{
        br q=(br)get(actor,gs.class,"b"),bn=(br)get(actor,gs.class,"bN");
        Object main=get(actor.c,ix.class,"a"),armor=get(actor.c,ix.class,"b");
        List<Object> players=new ArrayList<>();for(Object p:world.d)players.add(map("class",p.getClass().getName(),"actor",p==actor));
        return map("selected",map(
            "position",List.of(d(actor.aM),d(actor.aN),d(actor.aO)),"motion",List.of(d(actor.aP),d(actor.aQ),d(actor.aR)),
            "history",Arrays.asList(scalar(get(actor,sn.class,"aJ")),scalar(get(actor,sn.class,"aK")),scalar(get(actor,sn.class,"aL")),scalar(get(actor,sn.class,"bl")),scalar(get(actor,sn.class,"bm")),scalar(get(actor,sn.class,"bn"))),
            "base_flags",map("bk",scalar(get(actor,sn.class,"bk")),"bv",scalar(get(actor,sn.class,"bv")),"bz",scalar(get(actor,sn.class,"bz")),"aX",scalar(get(actor,sn.class,"aX"))),
            "dimensions",List.of(f(actor.bg),f(actor.bh),f(actor.bf)),"angles",List.of(f(actor.aS),f(actor.aT),f(actor.aU),f(actor.aV)),
            "sleep_offsets",List.of(f(actor.w),f(actor.y)),"living",List.of(actor.Y,actor.aa,actor.ad,actor.ae),
            "dimension",actor.m,"sleeping",actor.u,"timer",get(actor,gs.class,"a"),"score",actor.g,
            "v",xyz(actor.v),"Q",xyz(q),"private_bN",xyz(bn),"bounds",bounds(actor.aW),"old_bounds",bounds(originalBounds),
            "world_J",get(world,fd.class,"J"),"world_same",actor.aI==world,"bounds_same",actor.aW==originalBounds,
            "v_same",actor.v==beforeV,"Q_same",q==beforeQ,"Q_v_same",q==actor.v,"private_bN_same",bn==beforeBN,
            "world_players",world.d.size(),"world_entities",world.b.size(),"player_list_same",world.d==beforeList,"player_list_members",players,
            "client_null",get(actor,dc.class,"b")==null,"main_same",main==beforeMain,"armor_same",armor==beforeArmor,
            "inventory_selected",actor.c.c,"inventory_dirty",actor.c.e,
            "info_same",world.x==beforeInfo,"info_player_present",infoPlayer()!=null,"info_player_input",infoPlayer()==input,
            "main",stacks(main),"armor",stacks(armor),"head",List.of(chunk.b[(8*16+8)*128+64]&255,chunk.e.a[((8*16+8)*128+64)>>1]&15),"planes",planes()),
            "actor_fields",fields(actor,sn.class,ls.class,gs.class,dc.class),"world_fields",fields(world,fd.class),"provider_fields",fields(world.t,xa.class),
            "info_fields",fields(world.x,ei.class),"bounds_fields",fields(actor.aW,eq.class),"old_bounds_fields",fields(originalBounds,eq.class),
            "coordinate_fields",Arrays.asList(fields(actor.v,br.class),fields(q,br.class),fields(bn,br.class)),
            "inventory_fields",fields(actor.c,ix.class),"old_main",stacks(beforeMain),"old_armor",stacks(beforeArmor),"input_tree",tag(input),"random",Arrays.asList(random(actor.bs),random(world.r)));
    }
    static void cell(int x,int y,int z,int id,int meta){int n=((x&15)*16+(z&15))*128+y;chunk.b[n]=(byte)id;int s=(n&1)*4;chunk.e.a[n>>1]=(byte)((chunk.e.a[n>>1]&~(15<<s))|((meta&15)<<s));}
    static sp doubles(double... values)throws Exception{sp a=new sp();for(double v:values){nu n=new nu();n.a("v",v);a.a(compound(n).get("v"));}return a;}
    static sp floats(float... values)throws Exception{sp a=new sp();for(float v:values){nu n=new nu();n.a("v",v);a.a(compound(n).get("v"));}return a;}
    static void typed(nu n,String name,int kind,int value){if(kind==1)n.a(name,(byte)value);else if(kind==2)n.a(name,(short)value);else if(kind==3)n.a(name,value);else need(kind==0,"declared absent scalar kind");}
    static nu input(int[] p)throws Exception{
        nu n=new nu();n.a("Pos",doubles(8.5,64.9375,8.5));n.a("Motion",doubles(.1,.2,.3));n.a("Rotation",floats(450,-450));
        n.a("FallDistance",3.5f);n.a("Fire",(short)-19);n.a("Air",(short)298);n.a("OnGround",true);
        n.a("Health",(short)p[9]);n.a("HurtTime",(short)2);n.a("DeathTime",(short)3);n.a("AttackTime",(short)4);
        nu s=new nu();s.a("Slot",(byte)0);s.a("id",(short)1);s.a("Count",(byte)3);s.a("Damage",(short)4);sp list=new sp();list.a(s);
        if(p[23]==9)n.a("Inventory",list);else typed(n,"Inventory",p[23],7);
        n.a("Dimension",-1);typed(n,"Sleeping",p[5],p[6]);typed(n,"SleepTimer",p[7],p[8]);
        if((p[10]&1)!=0)typed(n,"SpawnX",p[11],p[12]);
        if((p[10]&2)!=0)typed(n,"SpawnY",p[13],p[14]);
        if((p[10]&4)!=0)typed(n,"SpawnZ",p[15],p[16]);
        typed(n,"Score",p[17],p[18]);
        return n;
    }
    static void setup(int[] p)throws Exception{
        world=new fd(new wy(),"NbtSleepMatrix",xa.a(0),5143L);world.a(new br(8,64,8));chunk=world.c(0,0);
        need(chunk.getClass()==lm.class&&world.w().getClass()==ok.class,"actual normal Chunk/Map provider");
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);Arrays.fill(chunk.h,(byte)64);
        for(int x=0;x<16;x++)for(int z=0;z<16;z++){cell(x,63,z,2,0);for(int y=64;y<128;y++){int n=(x*16+z)*128+y;chunk.f.a[n>>1]|=(byte)(15<<((n&1)*4));}}
        int direction=p[3],dx=FACING[direction][0],dz=FACING[direction][1];
        cell(8,64,8,p[4]==1?0:p[4]==2?1:26,p[4]==1?0:p[4]==2?direction:direction|12);
        cell(8-dx,64,8-dz,p[4]==1||p[4]==2?0:26,p[4]==1||p[4]==2?0:direction);
        if(p[4]==3)for(int x=5;x<=11;x++)for(int z=5;z<=11;z++)cell(x,65,z,1,0);
        actor=new dc(null,world,new gr("NbtSleepMatrix","-"),0);actor.e(8.5,64.0+(double)1.62f,8.5);
        actor.a(p[19]==0?null:new br(42,66,44));put(actor,gs.class,"bN",new br(-11,65,13));actor.g=900;
        iz common=new iz(2,5,9);actor.c.a(0,common);actor.c.a(36,common);actor.c.c=7;actor.c.e=true;
        input=input(p);if(p[24]!=0)world.x.a(input);
        world.B=p[22]!=0;
        if(p[25]!=0){
            need(infoPlayer()==null&&world.d.isEmpty()&&world.b.isEmpty(),"genuine sleeper starts unpublished with absent Info.Player");
            world.a(actor);world.f=5;
            need(actor.b(8,64,8)==cw.a&&actor.u&&(Boolean)get(world,fd.class,"J"),"genuine GS.b and actual World.y preparation");
        }
        if(p[20]!=0)actor.aI=null;
        originalBounds=actor.aW;if(p[21]!=0)put(actor,sn.class,"aW",null);
        beforeV=actor.v;beforeQ=(br)get(actor,gs.class,"b");beforeBN=(br)get(actor,gs.class,"bN");
        beforeMain=get(actor.c,ix.class,"a");beforeArmor=get(actor.c,ix.class,"b");beforeInfo=world.x;beforeList=world.d;
        identities.clear();need(world.d.size()==p[25]&&world.b.size()==p[25]&&(p[25]==0||world.d.get(0)==actor&&world.b.get(0)==actor),"actual declared receiver membership");
    }
    static final class SourceWorker extends Thread {
        final int number;final boolean attachment;final dc receiver;final fd receiverWorld;final nu tag;
        volatile boolean returned;volatile Throwable failure;
        SourceWorker(int n){super("original-nbt-matrix-"+n);number=n;attachment=profile[2]!=0;receiver=actor;receiverWorld=world;tag=input;setDaemon(true);}
        public void run(){try{if(attachment)receiverWorld.a(receiver);else receiver.e(tag);returned=true;}catch(Throwable e){failure=e;}}
    }
    @SuppressWarnings("removal") static final class Guard extends SecurityManager {
        public void checkPermission(Permission p){}
        public void checkConnect(String h,int p){network++;throw new SecurityException("network");}
        public void checkListen(int p){network++;throw new SecurityException("listen");}
        public void checkLink(String n){nativeLinks++;throw new SecurityException("native link");}
        public void checkExec(String n){execs++;throw new SecurityException("exec");}
        public void checkWrite(String n){writes++;throw new SecurityException("write");}
        public void checkDelete(String n){writes++;throw new SecurityException("delete");}
    }
    static void bind(Path names)throws Exception{for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){
        Class<?> c=Class.forName(name,false,PlayerNbtSleepMatrixObservation.class.getClassLoader());
        need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"actual archive class "+name);
        byte[] b;try(InputStream in=c.getResourceAsStream("/"+name+".class")){need(in!=null,"class bytes");b=in.readAllBytes();}
        System.out.println("CLASS "+name+" "+b.length+" "+hash(b));}}
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception {
        need(args.length==3,"archive classnames profiles");archive=Path.of(args[0]).toRealPath();System.setSecurityManager(new Guard());bind(Path.of(args[1]));
        long deadline=System.nanoTime()+180_000_000_000L;int returned=0,failed=0,number=0;
        for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            String[] words=line.split(" ");need(words.length==26,"full concrete profile columns");
            profile=new int[26];profile[0]=Integer.parseInt(words[0]);profileName=words[1];
            for(int i=2;i<26;i++)profile[i]=Integer.parseInt(words[i]);need(profile[0]==number,"declared profile order");
            setup(profile);Object before=snapshot();worker=new SourceWorker(number);active=number;worker.start();
            long left=(deadline-System.nanoTime())/1_000_000;need(left>0,"target deadline");worker.join(left);need(!worker.isAlive(),"source worker actually reaped");active=-1;
            Object after=snapshot();if(worker.returned)returned++;else failed++;
            need(worker.returned==(worker.failure==null),"actual source return or actual abrupt completion");
            System.out.println("NBT_SLEEP_MATRIX "+json(map("case",number,"name",profileName,"entry",profile[2]!=0?"FD_ATTACH":"SN_READ","before",before,"after",after,
                "worker",map("name",worker.getName(),"alive",worker.isAlive(),"returned",worker.returned,"failure",worker.failure==null?null:worker.failure.getClass().getName().replace('.','/')))));
            number++;actor=null;world=null;chunk=null;input=null;worker=null;originalBounds=null;beforeV=beforeQ=beforeBN=null;
            beforeMain=beforeArmor=beforeInfo=beforeList=null;identities.clear();if(number%4==0)System.gc();
        }
        need(number==40&&returned+failed==40&&network==0&&nativeLinks==0&&writes==0&&execs==0,"complete matrix/source effect guards");
        System.out.println("NBT_SLEEP_MATRIX_COMPLETE cases=40 workers_reaped=40 returned="+returned+" failed="+failed+" network=0 native=0 writes=0 exec=0 GUI=0");
    }
    static Map<String,Object> map(Object... a){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<a.length;i+=2)r.put((String)a[i],a[i+1]);return r;}
    static String json(Object v){if(v==null)return "null";if(v instanceof Number||v instanceof Boolean)return v.toString();if(v instanceof String){StringBuilder b=new StringBuilder("\"");for(char c:((String)v).toCharArray())if(c=='"'||c=='\\')b.append('\\').append(c);else if(c<32||c>126)b.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else b.append(c);return b.append('"').toString();}if(v instanceof Map){StringJoiner b=new StringJoiner(",","{","}");for(Map.Entry<?,?> e:((Map<?,?>)v).entrySet())b.add(json(e.getKey())+":"+json(e.getValue()));return b.toString();}if(v instanceof Iterable){StringJoiner b=new StringJoiner(",","[","]");for(Object e:(Iterable<?>)v)b.add(json(e));return b.toString();}throw new IllegalArgumentException("JSON transport");}
}
