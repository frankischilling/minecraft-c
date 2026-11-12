import java.io.*;
import java.lang.reflect.*;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Real original Worlds, Players, Chunks and FD.n; only declared data/Random inputs are prepared. */
@SuppressWarnings({"unchecked","rawtypes"})
public final class RandomTickTarget {
    public static int activeCase=-1;
    public static fd world;
    public static Random parameter;
    public static int mode;
    static PrintWriter output;
    static final Method BODY;
    static {try{BODY=fd.class.getDeclaredMethod("n");BODY.setAccessible(true);}catch(Exception e){throw new ExceptionInInitializerError(e);}}
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static int integer(Object o,Class<?> c,String n)throws Exception{return field(c,n).getInt(o);}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static long seed(Random r)throws Exception{return ((AtomicLong)get(r,Random.class,"seed")).get();}
    static String seedHex(Random r)throws Exception{return r==null?"-":String.format(Locale.ROOT,"%012x",seed(r));}
    static void need(boolean ok,String text){if(!ok)throw new AssertionError(text);}
    static void emit(Map<String,Object> row){output.println(ConstructorJson.json(row));output.flush();}
    static Map<Integer,lm> cache(fd w)throws Exception{return (Map<Integer,lm>)get(get(w,fd.class,"v"),ok.class,"e");}
    static List<yy> order(){Set<yy>s=new HashSet<>();for(int x=-9;x<=9;++x)for(int z=-9;z<=9;++z)s.add(new yy(x,z));return new ArrayList<>(s);}
    static int step(int g){return g*3+1013904223;}
    static final class Observer implements pm {
        final List<String> events=new ArrayList<>();fd owner;boolean failSound,clearP,failCell,failAdd;int sounds,cells,ranges,adds,removes,particles;
        public void a(String name,double x,double y,double z,float volume,float pitch){
            ++sounds;events.add("SOUND "+name+" "+bits(x)+" "+bits(y)+" "+bits(z)+" "+bits(volume)+" "+bits(pitch));
            if(failSound)throw new IllegalStateException("authored reached sound observer failure");
            if(clearP)try{((Set)get(owner,fd.class,"P")).clear();}catch(ReflectiveOperationException e){throw new AssertionError(e);}catch(Exception e){throw new AssertionError(e);}
        }
        public void a(int x,int y,int z){++cells;events.add("CELL "+x+" "+y+" "+z);if(failCell)throw new IllegalStateException("authored reached cell observer failure");}
        public void b(int x,int y,int z,int a,int b,int c){++ranges;events.add("RANGE "+x+" "+y+" "+z+" "+a+" "+b+" "+c);}
        public void a(String n,double x,double y,double z,double a,double b,double c){++particles;events.add("PARTICLE "+n+" "+bits(x)+" "+bits(y)+" "+bits(z));}
        public void a(sn e){++adds;events.add("ADD "+e.getClass().getName()+" "+bits(e.aM)+" "+bits(e.aN)+" "+bits(e.aO));if(failAdd)throw new IllegalStateException("authored reached Entity observer failure");}
        public void b(sn e){++removes;events.add("REMOVE "+e.getClass().getName());}
        public void e(){events.add("REFRESH");}
        public void a(String s,int x,int y,int z){events.add("RECORD "+s+" "+x+" "+y+" "+z);}
        public void a(int x,int y,int z,ow t){events.add("TILE "+x+" "+y+" "+z+" "+(t==null?"null":t.getClass().getName()));}
        public void a(gs p,int a,int b,int c,int d,int e){events.add("LEVEL "+a+" "+b+" "+c+" "+d+" "+e);}
    }
    static fd make()throws Exception{
        fd w=new fd(new wy(),"RandomTickReference",xa.a(0),23L);w.B=false;w.q=1;w.f=0;field(fd.class,"m").setInt(w,0);
        cache(w).clear();((List)get(get(w,fd.class,"v"),ok.class,"f")).clear();return w;
    }
    static void fill(fd w,int radius,int profile)throws Exception{
        Map<Integer,lm> map=cache(w);List loaded=(List)get(get(w,fd.class,"v"),ok.class,"f");
        lm shared=profile==6?(lm)get(get(w,fd.class,"v"),ok.class,"b"):profile==12?new lm(w,new byte[32768],0,0):null;
        if(shared!=null){Arrays.fill(shared.b,(byte)(profile==6?2:1));if(profile==12){Arrays.fill(shared.h,(byte)128);shared.i=128;}shared.n=true;}
        for(int x=-radius;x<=radius;++x)for(int z=-radius;z<=radius;++z){
            lm c=shared!=null?shared:new lm(w,new byte[32768],x,z);c.n=true;
            if(shared==null&&(profile==2||profile==8)){Arrays.fill(c.b,(byte)1);Arrays.fill(c.h,(byte)128);c.i=128;}
            if(profile>=9&&profile<=11){for(int lx=0;lx<16;++lx)for(int lz=0;lz<16;++lz)c.b[(lx<<11)|(lz<<7)|63]=1;Arrays.fill(c.h,(byte)64);c.i=64;}
            map.put(yy.a(x,z),c);loaded.add(c);
        }
    }
    static void weather(fd w,float rain,float thunder)throws Exception{
        field(fd.class,"i").setFloat(w,rain);field(fd.class,"j").setFloat(w,rain);field(fd.class,"k").setFloat(w,thunder);field(fd.class,"l").setFloat(w,thunder);
    }
    static void isolateWater(fd w)throws Exception{
        Random r=new Random(0);int g=17;
        for(yy p:order()){
            r.nextInt(100000);if(r.nextInt(16)==0){g=step(g);int v=g>>2;lm c=cache(w).get(yy.a(p.a,p.b));c.b[((v&15)<<11)|(((v>>8)&15)<<7)|63]=9;}
            for(int n=0;n<80;++n)g=step(g);
        }
    }
    static void update(MessageDigest d,int value){d.update(ByteBuffer.allocate(4).putInt(value).array());}
    static void bytes(MessageDigest d,byte[] value){update(d,value==null?-1:value.length);if(value!=null)d.update(value);}
    static Map<String,Object> hashes(fd w)throws Exception{
        List<int[]> keys=new ArrayList<>();Map<Integer,lm> map=cache(w);
        // The fixture admits only the authored finite square, including actual key aliases.
        int radius=map.size()==441?10:9;
        if(!map.isEmpty())for(int x=-radius;x<=radius;++x)for(int z=-radius;z<=radius;++z)if(map.containsKey(yy.a(x,z)))keys.add(new int[]{x,z});
        MessageDigest raw=MessageDigest.getInstance("SHA-256"),nibbles=MessageDigest.getInstance("SHA-256"),height=MessageDigest.getInstance("SHA-256"),membership=MessageDigest.getInstance("SHA-256"),setHash=MessageDigest.getInstance("SHA-256");
        IdentityHashMap<lm,Integer> ids=new IdentityHashMap<>();
        for(int[] key:keys){lm c=map.get(yy.a(key[0],key[1]));Integer id=ids.get(c);
            if(id==null){id=ids.size()+1;ids.put(c,id);bytes(raw,c.b);bytes(nibbles,c.e.a);bytes(nibbles,c.f.a);bytes(nibbles,c.g.a);bytes(height,c.h);}
            for(int v:new int[]{key[0],key[1],c instanceof li?1:0,c.j,c.k,id})update(membership,v);
        }
        for(Object value:(Set)get(w,fd.class,"P")){yy p=(yy)value;update(setHash,p.a);update(setHash,p.b);}
        return ConstructorJson.map("unique_chunks",ids.size(),"raw",HexFormat.of().formatHex(raw.digest()),"nibbles",HexFormat.of().formatHex(nibbles.digest()),"height",HexFormat.of().formatHex(height.digest()),"membership",HexFormat.of().formatHex(membership.digest()),"P",HexFormat.of().formatHex(setHash.digest()));
    }
    static void frame(int profile)throws Exception{
        mode=0;fd w=make();world=w;String[] names={"empty_players","air_qhigh","stone_q0","air_ambient","sound_failure","P_mutation","blank_grass_remote","raw_null","tick_entry_null","snow_HD","ice_HD","lightning_HD","normal_alias","random_null"};
        if(profile!=0){fill(w,profile>=9&&profile<=11?10:9,profile);dc p=new dc(null,w,new gr("RandomTickPlayer","-"),0);p.e(.5,64.5,4.5);w.d.add(p);}
        if(profile==9||profile==10)w.t.b=new hd(kd.k,.5,.5);if(profile==11)w.t.b=new hd(kd.i,.5,.5);
        weather(w,profile==9||profile==11?1f:0f,profile==11?1f:0f);
        if(profile==10)isolateWater(w);
        int g=profile>=3&&profile<=5?10:17,q=profile==0||profile>=3&&profile<=5?1:profile==2||profile==12?0:6001;
        field(fd.class,"g").setInt(w,g);field(fd.class,"Q").setInt(w,q);w.r.setSeed(profile==11?107038380838084L:0L);w.B=profile==6;
        Observer observer=new Observer();observer.owner=w;observer.failSound=profile==4;observer.clearP=profile==5;w.a(observer);
        if(profile==7){yy first=order().get(0);cache(w).get(yy.a(first.a,first.b)).b=null;}
        uu stone=uu.m[1];boolean tick=uu.n[1];if(profile==8){uu.n[1]=true;uu.m[1]=null;}
        Random owned=w.r;if(profile==13)w.r=null;
        eq.b();bt.b();int complete=1;String failure=null,top=null;activeCase=profile;
        try{BODY.invoke(w);}catch(InvocationTargetException e){complete=-1;Throwable cause=e.getCause();failure=cause.getClass().getName();top=cause.getStackTrace()[0].toString();}
        activeCase=-1;uu.m[1]=stone;uu.n[1]=tick;
        Map<String,Object> data=hashes(w);
        String row=String.format(Locale.ROOT,"RANDOM_FRAME %d %s %d %08x %d %s %d %d %d %d %d %d %d %d %d",profile,names[profile],complete,integer(w,fd.class,"g"),integer(w,fd.class,"Q"),seedHex(w.r),integer(w,fd.class,"m"),((Set)get(w,fd.class,"P")).size(),cache(w).size(),data.get("unique_chunks"),w.b.size(),w.e.size(),observer.sounds,observer.cells,observer.ranges);
        String hashRow="RANDOM_HASH "+profile+" "+data.get("raw")+" "+data.get("nibbles")+" "+data.get("height")+" "+data.get("membership")+" "+data.get("P");
        emit(ConstructorJson.map("event","frame","index",profile,"scenario",names[profile],"row",row,"hash_row",hashRow,"events",observer.events,"failure",failure,"top",top,
            "g_input",g,"Q_input",q,"r_seed_input",profile==11?107038380838084L:0L,"h_unchanged",integer(w,fd.class,"h"),"manager_class",w.a().getClass().getName(),"declared_HD_input",profile>=9&&profile<=11,
            "source_only_failure",profile==4||profile==5,"box_cursor",integer(null,eq.class,"h"),"vector_cursor",integer(null,bt.class,"e")));
        if(profile==13)w.r=owned;
        System.out.println("FRAME "+profile+" "+complete);
    }
    static fd leafWorld()throws Exception{
        fd w=make();lm c=new lm(w,new byte[32768],0,0);c.n=true;cache(w).put(yy.a(0,0),c);((List)get(get(w,fd.class,"v"),ok.class,"f")).add(c);
        w.r.setSeed(89);weather(w,0,0);field(fd.class,"g").setInt(w,17);field(fd.class,"Q").setInt(w,6001);return w;
    }
    static void cell(lm c,int x,int y,int z,int id,int metadata){c.b[(x<<11)|(z<<7)|y]=(byte)id;c.e.a(x,y,z,metadata);if(id!=0){c.h[(z<<4)|x]=(byte)Math.max(c.h[(z<<4)|x]&255,y+1);}}
    static String scratchHash(int[] a)throws Exception{if(a==null)return "-";MessageDigest d=MessageDigest.getInstance("SHA-256");for(int value:a)update(d,value);return HexFormat.of().formatHex(d.digest());}
    static final List<String> LEAF_NAMES=List.of("grass_no_decay","grass_decay","grass_dark_air","grass_spread","grass_light4","grass_callback_failure",
        "leaves_flag0","leaves_fresh_absent","leaves_log_distance4","leaves_isolated","leaves_reused_absent","ice8","ice9","ice_remote9","snow11","snow_remote12","snow_drop4","snow_add_failure");
    static int[] firstScratch;
    static void leaves(int profile)throws Exception{
        mode=1;world=leafWorld();fd w=world;lm c=cache(w).get(yy.a(0,0));int block=profile<6?2:profile<11?18:profile<14?79:80;
        int x=profile==7||profile==10?12:8,y=64,z=8;
        int metadata=profile>=7&&profile<=10?8:0;cell(c,x,y,z,block,metadata);
        if(profile<6){
            if(profile==0||profile==1||profile==4)cell(c,8,65,8,1,0);
            if(profile==3||profile==5){c.f.a(8,65,8,15);c.f.a(7,65,8,15);cell(c,7,64,8,3,0);}
            if(profile==4)c.f.a(8,65,8,4);
        }
        if(profile==8){for(int xx=9;xx<12;++xx)cell(c,xx,64,8,18,8);cell(c,12,64,8,17,0);}
        if(profile>=11&&profile<=13){c.g.a(x,y,z,profile==11?8:9);cell(c,x,y-1,z,1,0);}
        if(profile>=14){c.g.a(x,y,z,profile==14?11:12);cell(c,x,y-1,z,1,0);}
        w.B=profile==13||profile==15;parameter=new Random(profile==1?4096:0);
        Observer observer=new Observer();observer.owner=w;observer.failCell=profile==5;observer.failAdd=profile==17;w.a(observer);
        eq.b();bt.b();int complete=1;String failure=null,top=null;activeCase=100+profile;
        try{uu.m[block].a(w,x,y,z,parameter);}catch(RuntimeException e){complete=-1;failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
        activeCase=-1;
        int[] scratch=(int[])get(uu.m[18],bk.class,"a");if(scratch!=null&&firstScratch==null)firstScratch=scratch;
        need(scratch==null||scratch==firstScratch,"actual persistent Leaves scratch identity");
        int center=c.a(x,y,z),meta=c.b(x,y,z),target=c.a(7,64,8),targetMeta=c.b(7,64,8),scratchCenter=scratch==null?0:scratch[16*1024+16*32+16];
        String row=String.format(Locale.ROOT,"BLOCK_TICK %d %s %d %s %d %s %s %d %d %d %d %d %d %d",profile,uu.m[block].getClass().getName(),block,LEAF_NAMES.get(profile),complete,seedHex(parameter),seedHex(w.r),center,meta,target,targetMeta,w.b.size(),scratch==null?0:1,scratchCenter);
        List<String> items=new ArrayList<>();int ordinal=0;
        for(Object e:w.b)if(e instanceof hl){hl item=(hl)e;items.add("DROPPED "+profile+" "+ordinal+++" "+item.a.c+" "+item.a.a+" "+integer(item.a,iz.class,"d")+" "+item.c+" "+bits(item.aM)+" "+bits(item.aN)+" "+bits(item.aO));}
        Map<String,Object> data=hashes(w);
        emit(ConstructorJson.map("event","block","index",profile,"scenario",LEAF_NAMES.get(profile),"row",row,"events",observer.events,"items",items,"failure",failure,"top",top,
            "scratch_hash",scratchHash(scratch),"scratch_same",scratch==null||scratch==firstScratch,"raw_hash",data.get("raw"),"nibble_hash",data.get("nibbles"),"height_hash",data.get("height"),
            "source_only_failure",profile==5||profile==17,"parameter_is_world_random",parameter==w.r,"ice_opacity",uu.q[79],"cells",observer.cells,"ranges",observer.ranges));
        System.out.println("BLOCK "+profile+" "+complete);
    }
    static void bindings(Path jar)throws Exception{
        for(String name:Files.readAllLines(Path.of(System.getProperty("random.tick.classes")))){
            Class<?> type=Class.forName(name,false,RandomTickTarget.class.getClassLoader());need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign source class "+name);
            byte[] data;try(InputStream in=type.getResourceAsStream("/"+name.replace('.','/')+".class")){data=in.readAllBytes();}
            System.out.println("BIND "+name+" "+data.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data)));
        }
    }
    public static void main(String[] args)throws Exception{
        new gr("RandomTickSetup","");
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))){output=out;
            for(int profile=0;profile<14;++profile)frame(profile);
            for(int profile=0;profile<18;++profile)leaves(profile);
            if(out.checkError())throw new IOException("observation output");
        }
        bindings(Path.of(args[1]).toRealPath());
    }
}
