import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Unchanged original spawn predicates and World/Chunk leaves over authored real data. */
public final class SpawningTarget {
    public static int activeCase=-1;
    public static ls actor;
    public static fd world;
    static PrintWriter output;
    static int index;
    static final Set<Random> freshRandoms=Collections.newSetFromMap(new IdentityHashMap<>());
    static final String[] KINDS={"cn","uz","fr","gb"};
    static Field field(Class<?> owner,String name)throws Exception {Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object get(Object object,Class<?> owner,String name)throws Exception{return field(owner,name).get(object);}
    static long seed(Random random)throws Exception{return ((AtomicLong)get(random,Random.class,"seed")).get();}
    static String seedHex(Random random)throws Exception{return random==null?"-":String.format(Locale.ROOT,"%012x",seed(random));}
    static int cursor(Class<?> owner,String name)throws Exception{return field(owner,name).getInt(null);}
    static void emit(Map<String,Object> row){output.println(ConstructorJson.json(row));output.flush();}
    static final class FailureWorld extends fd {
        int calls,failAt;
        FailureWorld(long seed){super(new wy(),"ControlledSecondLight",xa.a(0),seed);}
        @Override public int n(int x,int y,int z){if(++calls==failAt)throw new IllegalStateException("authored reached block-light callee failure");return super.n(x,y,z);}
    }
    static fd make(long seed){return new fd(new wy(),"MobSpawningReference",xa.a(0),seed);}
    @SuppressWarnings("unchecked")
    static Map<Integer,lm> cache(fd w)throws Exception{return (Map<Integer,lm>)get(get(w,fd.class,"v"),ok.class,"e");}
    static lm install(fd w,int x,int z)throws Exception {
        lm chunk=new lm(w,new byte[32768],x,z);chunk.n=true;cache(w).put(yy.a(x,z),chunk);return chunk;
    }
    static void clean(fd w,lm chunk)throws Exception {
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);
        for(List<?> list:chunk.m)list.clear();
        ((List<?>)get(w,fd.class,"R")).clear();((List<?>)get(w,fd.class,"K")).clear();
        w.B=false;w.q=1;w.f=0;w.r.setSeed(89);
        for(String name:new String[]{"i","j","k","l"})field(fd.class,name).setFloat(w,0);
    }
    static ls mob(String kind,fd w)throws Exception{return (ls)Class.forName(kind).getDeclaredConstructor(fd.class).newInstance(w);}
    static void run(String kind,int scenario,long ownSeed,boolean controlled)throws Exception {
        actor.bs.setSeed(ownSeed);eq.b();bt.b();
        int id=index++,value=-1;String failure=null,top=null;
        activeCase=id;
        try{value=actor.d()?1:0;}catch(RuntimeException e){failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
        activeCase=-1;
        String row=String.format(Locale.ROOT,"ELIG_STATE %d %s %d %d %s %s %d %d %d %d %d %d",id,kind,scenario,value,
            seedHex(actor.bs),seedHex(world.r),world.f,cursor(eq.class,"h"),cursor(bt.class,"e"),
            ((List<?>)get(world,fd.class,"R")).size(),((List<?>)get(world,fd.class,"K")).size(),cache(world).size());
        List<String> bounds=new ArrayList<>();for(double coordinate:new double[]{actor.aW.a,actor.aW.b,actor.aW.c,actor.aW.d,actor.aW.e,actor.aW.f})bounds.add(String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(coordinate)));
        emit(ConstructorJson.map("event","eligibility","index",id,"kind",kind,"scenario",scenario,"controlled_virtual_callee",controlled,
            "own_seed_input",ownSeed,"state_row",row,"failure",failure,"top",top,
            "bounds",bounds,
            "source_world_class",world.getClass().getName(),"light_callee_calls",controlled?((FailureWorld)world).calls:null));
        System.out.println("CASE "+id+" "+kind+" "+scenario);
    }
    static void gzProfiles(fd w,lm chunk)throws Exception {
        for(String kind:KINDS)for(int scene=0;scene<11;++scene){
            clean(w,chunk);world=w;actor=mob(kind,w);actor.e(8.5,8.5,8.5);
            long own=scene==0?4096:0;
            if(scene==0||scene==8||scene==9)chunk.f.a(8,8,8,15);
            if(scene==1)chunk.g.a(8,8,8,15);
            if(scene==3)chunk.b[8*2048+8*128+8]=1;
            if(scene==4)chunk.b[8*2048+8*128+8]=9;
            if(scene>=5&&scene<=7){ls other=mob("cn",w);other.e(8.5,8.5,8.5);other.be=scene==6;other.aF=scene!=7;other.bq=scene==7;chunk.a(other);if(scene==7)chunk.a(other);}
            if(scene==8||scene==9){w.f=scene==8?11:0;for(String name:new String[]{"i","j","k","l"})field(fd.class,name).setFloat(w,1);}
            if(scene==10)chunk.a(actor);
            run(kind,scene,own,false);
        }
    }
    static void slimeProfiles(fd reject,fd allow,lm rejectChunk,lm allowChunk)throws Exception {
        for(int scene=0;scene<12;++scene){
            fd w=scene==2?reject:allow;lm chunk=scene==2?rejectChunk:allowChunk;
            clean(w,chunk);world=w;actor=new uw(w);((uw)actor).e(scene==0?2:scene==7?-1:scene==4||scene==5||scene==6?2:1);
            w.q=scene==0||scene==1||scene==2||scene==3?0:1;
            actor.e(scene==9?1048584.5:scene==8?40.5:8.5,scene==5?16.:scene==6?Double.NaN:8.5,scene==8?-23.5:8.5);
            Object oldProvider=get(w,fd.class,"v");
            if(scene==8)field(fd.class,"v").set(w,new ok(w,null,null));
            if(scene==10)actor.aI=null;
            Object oldInfo=scene==11?get(w,fd.class,"x"):null;
            if(scene==11)field(fd.class,"x").set(w,null);
            run("uw",scene,scene==1?1:0,false);
            if(scene==8)field(fd.class,"v").set(w,oldProvider);
            if(scene==11)field(fd.class,"x").set(w,oldInfo);
        }
    }
    static void controlledProfiles()throws Exception {
        for(int failAt:new int[]{2,3}){
            FailureWorld w=new FailureWorld(23);lm c=install(w,0,0);clean(w,c);world=w;actor=new cn(w);actor.e(8.5,8.5,8.5);
            c.f.a(8,8,8,15);w.f=11;for(String name:new String[]{"i","j","k","l"})field(fd.class,name).setFloat(w,1);
            w.calls=0;w.failAt=failAt;run("controlled_cn",failAt,0,true);
        }
    }
    static void rejectionProfile(fd w,lm chunk)throws Exception {
        clean(w,chunk);world=w;actor=new uw(w);((uw)actor).e(2);actor.e(8.5,8.5,8.5);run("uw",12,0,false);
    }
    static void chunkProfiles(fd w,fd rejection)throws Exception {
        int[][] coords={{0,0},{1,-1},{-2049,4097},{46341,-46341},{Integer.MAX_VALUE,Integer.MIN_VALUE},{65536,65536},{-1,-1}};
        int id=0;
        for(int blank=0;blank<2;++blank)for(int[] point:coords)for(long salt:new long[]{0,987234911L}){
            lm chunk=blank==0?new lm(w,new byte[32768],point[0],point[1]):new li(w,new byte[32768],point[0],point[1]);
            actor=null;world=w;activeCase=1000+id;Random random=chunk.a(salt);if(!freshRandoms.add(random))throw new AssertionError("fresh actual Chunk Random identity");String before=seedHex(random);int draw=random.nextInt(10);String after=seedHex(random);activeCase=-1;
            String row=String.format(Locale.ROOT,"CHUNK_RANDOM %d %d %d %d %016x %016x %s %d %s",id,blank,point[0],point[1],salt,w.s(),before,draw,after);
            emit(ConstructorJson.map("event","chunk_random","index",id++,"row",row));
        }
        for(int blank=0;blank<2;++blank){
            lm chunk=blank==0?new lm(rejection,new byte[32768],0,0):new li(rejection,new byte[32768],0,0);
            actor=null;world=rejection;activeCase=1000+id;Random random=chunk.a(0L);if(!freshRandoms.add(random))throw new AssertionError("fresh actual Chunk Random identity");String before=seedHex(random);int draw=random.nextInt(10);String after=seedHex(random);activeCase=-1;
            String row=String.format(Locale.ROOT,"CHUNK_RANDOM %d %d 0 0 0000000000000000 %016x %s %d %s",id,blank,rejection.s(),before,draw,after);
            emit(ConstructorJson.map("event","chunk_random","index",id++,"row",row));
        }
        int failureId=0;
        for(int blank=0;blank<2;++blank)for(int mode=0;mode<2;++mode){
            Object info=get(w,fd.class,"x");if(mode==1)field(fd.class,"x").set(w,null);
            lm chunk=blank==0?new lm(mode==0?null:w,new byte[32768],0,0):new li(mode==0?null:w,new byte[32768],0,0);
            actor=null;world=w;String failure=null,top=null;activeCase=2000+failureId;
            try{chunk.a(987234911L);}catch(RuntimeException e){failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
            activeCase=-1;field(fd.class,"x").set(w,info);
            emit(ConstructorJson.map("event","chunk_failure","index",failureId++,"blank",blank,"mode",mode,"failure",failure,"top",top));
        }
    }
    static void thunderProfiles(fd w)throws Exception {
        for(int id=0;id<3;++id){
            actor=null;world=w;float strength=id==0?Float.intBitsToFloat(0x3f666666):id==1?Float.intBitsToFloat(0x3f666667):Float.NaN;
            for(String name:new String[]{"i","j"})field(fd.class,name).setFloat(w,1);
            for(String name:new String[]{"k","l"})field(fd.class,name).setFloat(w,strength);
            activeCase=3000+id;boolean value=w.B();activeCase=-1;
            emit(ConstructorJson.map("event","thunder","index",id,"row",String.format(Locale.ROOT,"THUNDER %d %08x %d",id,Float.floatToRawIntBits(strength),value?1:0)));
        }
    }
    static void passiveRun(String event,String prefix,int id,int scene,int checkpoint)throws Exception {
        actor.bs.setSeed(0);eq.b();bt.b();int value=-1;String failure=null,top=null;activeCase=checkpoint;
        try{value=actor.d()?1:0;}catch(RuntimeException e){failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
        activeCase=-1;
        String kind=actor.getClass().getName();
        String row=String.format(Locale.ROOT,"%s %d %s %d %d %s %s %d %d %d %d %d %d",prefix,id,kind,scene,value,
            seedHex(actor.bs),seedHex(world.r),world.f,cursor(eq.class,"h"),cursor(bt.class,"e"),
            ((List<?>)get(world,fd.class,"R")).size(),((List<?>)get(world,fd.class,"K")).size(),cache(world).size());
        emit(ConstructorJson.map("event",event,"index",id,"kind",kind,"scenario",scene,"checkpoint_case",checkpoint,
            "state_row",row,"failure",failure,"top",top,"genuine_world",world.getClass()==fd.class));
        System.out.println("PASSIVE "+event+" "+id+" "+kind+" "+scene);
    }
    static void animalProfiles(fd w,lm chunk)throws Exception {
        int id=0;
        for(String kind:new String[]{"wh","dl","bx","ww","gi"})for(int scene=0;scene<10;++scene){
            clean(w,chunk);world=w;actor=mob(kind,w);actor.e(8.5,8.5,8.5);
            if(scene!=0)chunk.b[8*2048+8*128+7]=2;
            chunk.f.a(8,8,8,scene==0||scene==8?0:scene==1?8:scene==2||scene==9?9:15);
            if(scene==3)chunk.b[8*2048+8*128+8]=1;
            if(scene==4)chunk.b[8*2048+8*128+8]=9;
            if(scene>=5&&scene<=7){ls other=mob("cn",w);other.e(8.5,8.5,8.5);other.be=scene==6;other.aF=scene!=7;other.bq=scene==7;chunk.a(other);if(scene==7)chunk.a(other);}
            if(scene==8)chunk.g.a(8,8,8,9);
            if(scene==9)w.f=15;
            passiveRun("animal","ANIMAL_STATE",id,scene,4000+id);++id;
        }
    }
    static void waterProfiles(fd w,lm chunk)throws Exception {
        for(int scene=0;scene<9;++scene){
            clean(w,chunk);world=w;actor=new xt(w);actor.e(8.5,scene==8?200.5:8.5,8.5);
            if(scene>=1&&scene<=3)chunk.b[8*2048+8*128+8]=(byte)(scene==1?1:scene==2?9:11);
            if(scene>=4&&scene<=6){ls other=mob("cn",w);other.e(8.5,8.5,8.5);other.be=scene==5;other.aF=scene!=6;other.bq=scene==6;chunk.a(other);if(scene==6)chunk.a(other);}
            if(scene==7)chunk.a(actor);
            passiveRun("water","WATER_STATE",scene,scene,5000+scene);
        }
    }
    static void pathProfiles(fd w,lm chunk)throws Exception {
        Method method=bg.class.getDeclaredMethod("a",int.class,int.class,int.class);method.setAccessible(true);
        for(int scene=0;scene<6;++scene){
            clean(w,chunk);world=w;actor=new wh(w);actor.e(8.5,8.5,8.5);actor.bs.setSeed(0);
            if(scene==0)chunk.b[8*2048+8*128+7]=2;
            if(scene==2||scene==5)chunk.f.a(8,8,8,15);
            if(scene==3||scene==4)chunk.g.a(8,8,8,scene==3?11:12);
            if(scene==5)w.f=15;
            eq.b();bt.b();activeCase=6000+scene;float value=(Float)method.invoke(actor,8,8,8);activeCase=-1;
            String row=String.format(Locale.ROOT,"PATH_WEIGHT %d wh %d %08x",scene,scene,Float.floatToRawIntBits(value));
            emit(ConstructorJson.map("event","path_weight","index",scene,"kind","wh","scenario",scene,"row",row,
                "brightness_bits",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(w.c(8,8,8))),
                "entry_scope","Direct original protected BG.a component; not a BG.d bypass result"));
        }
    }
    static void bindings(Path jar)throws Exception {
        for(String name:Files.readAllLines(Path.of(System.getProperty("spawn.probe.classes")))){
            Class<?> type=Class.forName(name,false,SpawningTarget.class.getClassLoader());
            if(!Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new AssertionError("foreign source class "+name);
            byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+name.replace('.','/')+".class")){bytes=in.readAllBytes();}
            System.out.println("BIND "+name+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        }
    }
    public static void main(String[] args)throws Exception {
        new gr("SpawningSetup","");fd reject=make(23),allow=make(987234911L),rejection=make(186643064605892L),uwRejection=make(186643064605892L^987234911L);lm rc=install(reject,0,0),ac=install(allow,0,0),jc=install(uwRejection,0,0);
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))){output=out;
            gzProfiles(reject,rc);slimeProfiles(reject,allow,rc,ac);controlledProfiles();rejectionProfile(uwRejection,jc);chunkProfiles(reject,rejection);thunderProfiles(reject);
            animalProfiles(reject,rc);waterProfiles(reject,rc);pathProfiles(reject,rc);
            if(out.checkError())throw new IOException("observation output");
        }
        bindings(Path.of(args[1]).toRealPath());
    }
}
