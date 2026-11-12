import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
import sun.reflect.ReflectionFactory;

/** Prepared original World/Bed/DC/NH/UQ bodies. No startup, drawing or sleeping. */
public final class BedInteractionObservation {
    static int network, nativeLinks, writes, execs, number;
    static volatile int active = -1;
    static Path archive;
    static final int[][] FACING = {{0,1},{-1,0},{0,-1},{1,0}};
    static Field field(Class<?> type, String name) throws Exception {
        Field result=type.getDeclaredField(name);result.setAccessible(true);return result;
    }
    static Object prepared(Class<?> type) throws Exception {
        Constructor<?> ctor=ReflectionFactory.getReflectionFactory()
            .newConstructorForSerialization(type,Object.class.getDeclaredConstructor());
        ctor.setAccessible(true);return ctor.newInstance();
    }
    static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    static String d(double v){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(v));}
    static String f(float v){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(v));}
    static List<Object> random(Random r)throws Exception {
        return Arrays.asList(String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(r)).get()),
            field(Random.class,"haveNextNextGaussian").getBoolean(r),d(field(Random.class,"nextNextGaussian").getDouble(r)));
    }
    static String planes(lm c)throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");
        hash.update(c.b);hash.update(c.e.a);return HexFormat.of().formatHex(hash.digest());
    }
    static void cell(lm c,int x,int y,int z,int id,int meta) {
        int at=((x&15)*16+(z&15))*128+y;c.b[at]=(byte)id;
        int shift=(at&1)*4;c.e.a[at>>1]=(byte)((c.e.a[at>>1]&~(15<<shift))|((meta&15)<<shift));
    }
    static fd world()throws Exception {
        fd w=new fd(new wy(),"BedInteractionReference",xa.a(0),5143L);w.a(new br(0,64,0));
        lm c=w.c(0,0);need(c.getClass()==lm.class&&w.w().getClass()==ok.class,"normal World/provider/Chunk");
        Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);w.f=0;
        need(w.b.size()==0&&w.d.size()==0&&w.t.f()&&!w.t.c,"genuine fresh Overworld");return w;
    }
    static List<Object> actor(dc p)throws Exception {
        br v=p.v,q=p.Q();eq b=p.aW;
        return Arrays.asList(p.Y,p.u,field(gs.class,"a").getInt(p),
            v==null?null:List.of(v.a,v.b,v.c),q==null?null:List.of(q.a,q.b,q.c),
            List.of(d(p.aM),d(p.aN),d(p.aO)),List.of(d(p.aP),d(p.aQ),d(p.aR)),
            List.of(f(p.bg),f(p.bh),f(p.bf)),List.of(f(p.aS),f(p.aT)),
            List.of(d(b.a),d(b.b),d(b.c),d(b.d),d(b.e),d(b.f)));
    }
    @SuppressWarnings("unchecked")
    static List<sw> messages(uq hud)throws Exception{return (List<sw>)field(uq.class,"e").get(hud);}
    static String units(String text){StringBuilder out=new StringBuilder();for(int i=0;i<text.length();++i)out.append(String.format(Locale.ROOT,"%04x",(int)text.charAt(i)));return out.toString();}
    static List<Object> items(fd w)throws Exception {
        List<Object> result=new ArrayList<>();
        for(Object value:w.b){
            if(!(value instanceof hl))continue;hl item=(hl)value;
            need(item.aI==w&&item.a!=null,"actual World Item/Stack");
            lm c=w.c(item.bG,item.bI);int members=0;
            for(List<?> slice:c.m)for(Object member:slice)if(member==item)++members;
            need(members==1,"same actual Item in normal Chunk slice");
            result.add(map("class",item.getClass().getName(),"stack",List.of(item.a.c,item.a.a,item.a.i(),item.a.b),
                "position",List.of(d(item.aM),d(item.aN),d(item.aO)),"pickup_delay",item.c,
                "world_member",true,"chunk_members",members));
        }return result;
    }
    static void neighbor(int meta,boolean partner,boolean remote)throws Exception {
        fd w=world();lm c=w.c(0,0);int direction=meta&3;
        boolean head=(meta&8)!=0;int dx=FACING[direction][0],dz=FACING[direction][1];
        int px=8+(head?-dx:dx),pz=8+(head?-dz:dz);
        cell(c,8,64,8,26,meta);if(partner)cell(c,px,64,pz,26,head?direction:direction|8);
        w.B=remote;w.r.setSeed(2468);List<Object> before=random(w.r);String input=planes(c);
        active=number;try{uu.T.b(w,8,64,8,81);}finally{active=-1;}
        int id=w.a(8,64,8),afterMeta=w.e(8,64,8);
        List<Object> drops=items(w);need(drops.size()==(!partner&&!head&&!remote?1:0),"exact Bed drop count");
        need(id==(partner?26:0),"actual partner removal result");
        row(map("kind","neighbor","metadata",meta,"partner",partner,"remote",remote,"changed_id",81,
            "partner_xyz",List.of(px,64,pz),"input_planes",input,"output_planes",planes(c),
            "id",id,"metadata_after",afterMeta,"partner_id",w.a(px,64,pz),
            "random_before",before,"random_after",random(w.r),"items",drops));
    }
    static void activation(int meta,boolean partner,boolean remote)throws Exception {
        fd w=world();lm c=w.c(0,0);int direction=meta&3,dx=FACING[direction][0],dz=FACING[direction][1];
        boolean head=(meta&8)!=0;int hx=head?8:8+dx,hz=head?8:8+dz;
        cell(c,8,64,8,26,meta);if(!head&&partner)cell(c,hx,64,hz,26,meta|8);
        if(head&&partner)cell(c,8-dx,64,8-dz,26,meta&7);
        kq app=null;uq hud=null;dc p=null;List<Object> prior=null;Object inventory=null;
        if(!remote&&(head||partner)) {
            app=(kq)prepared(kq.class);sj font=(sj)prepared(sj.class);
            int[] widths=new int[256];Arrays.fill(widths,7);widths[32]=4;field(sj.class,"b").set(font,widths);app.q=font;
            hud=new uq(app);app.v=hud;p=new dc(app,w,new gr("BedInteractionPlayer","-"),0);
            need(p.Y==20&&!p.N()&&p.W()&&w.b(p),"genuine awake healthy published DC");
            prior=actor(p);inventory=p.c;
        }
        w.B=remote;w.r.setSeed(2468);List<Object> before=random(w.r);String input=planes(c);
        boolean returned;active=number;try{returned=uu.T.a(w,8,64,8,p);}finally{active=-1;}
        List<Object> notice=new ArrayList<>();
        if(hud!=null)for(sw message:messages(hud))notice.add(List.of(units(message.a),message.b));
        boolean reachesDay=!remote&&(head||partner);
        need(returned&&notice.size()==(reachesDay?1:0),"actual day/short-return notice count");
        if(p!=null)need(prior.equals(actor(p))&&p.c==inventory,"day branch leaves actual Player/Inventory unchanged");
        need(before.equals(random(w.r))&&items(w).isEmpty(),"day/remote/absent-partner Random and drops");
        row(map("kind","activate","metadata",meta,"partner",partner,"remote",remote,
            "resolved_xyz",List.of(hx,64,hz),"returned",returned,"input_planes",input,"output_planes",planes(c),
            "origin_id",w.a(8,64,8),"resolved_id",w.a(hx,64,hz),"resolved_metadata",w.e(hx,64,hz),
            "daytime",w.f(),"provider_allows",w.t.f(),"notice",notice,
            "player_unchanged",p==null?null:true,"player_before",prior,"random_before",before,"random_after",random(w.r)));
    }
    static void row(Map<String,Object> value){value.put("case",number++);System.out.println("BED_INTERACTION "+json(value));}
    static Map<String,Object> map(Object... pairs){Map<String,Object> out=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)out.put((String)pairs[i],pairs[i+1]);return out;}
    static String json(Object value){
        if(value==null)return "null";if(value instanceof Number||value instanceof Boolean)return value.toString();
        if(value instanceof String){StringBuilder out=new StringBuilder("\"");for(char c:((String)value).toCharArray())if(c=='"'||c=='\\')out.append('\\').append(c);else if(c<32||c>126)out.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else out.append(c);return out.append('"').toString();}
        if(value instanceof Map){StringJoiner out=new StringJoiner(",","{","}");for(Map.Entry<?,?> p:((Map<?,?>)value).entrySet())out.add(json(p.getKey())+":"+json(p.getValue()));return out.toString();}
        if(value instanceof Iterable){StringJoiner out=new StringJoiner(",","[","]");for(Object p:(Iterable<?>)value)out.add(json(p));return out.toString();}throw new IllegalArgumentException("transport");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        need(args.length==1,"unchanged JAR path");archive=Path.of(args[0]).toRealPath();Locale.setDefault(Locale.ROOT);
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
            public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String n){++nativeLinks;throw new SecurityException("native");}
            public void checkExec(String n){++execs;throw new SecurityException("exec");}
            public void checkWrite(String n){++writes;throw new SecurityException("write");}
            public void checkDelete(String n){++writes;throw new SecurityException("delete");}
        });
        for(Class<?> type:new Class<?>[]{ve.class,uu.class,fd.class,lm.class,ok.class,wy.class,xa.class,
                hl.class,iz.class,sn.class,ls.class,gs.class,dc.class,gr.class,br.class,cw.class,rh.class,wd.class,kq.class,
                net.minecraft.client.Minecraft.class,sj.class,uq.class,sw.class,nh.class}) {
            need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"original class origin "+type.getName());
            byte[] bytes;try(InputStream in=type.getResourceAsStream("/"+type.getName().replace('.','/')+".class")){bytes=in.readAllBytes();}
            System.out.println("CLASS "+type.getName()+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        }
        xa overworld=xa.a(0),nether=xa.a(-1);need(overworld.f()&&!nether.f(),"genuine provider predicates");
        System.out.println("PROVIDER_FRONTIER Overworld=true Nether=false next=VE95..256 source-explosion-uninvoked=1");
        for(int meta=0;meta<16;++meta)for(boolean partner:new boolean[]{false,true})for(boolean remote:new boolean[]{false,true})neighbor(meta,partner,remote);
        for(int meta=0;meta<16;++meta)activation(meta,true,false);
        for(int meta=0;meta<4;++meta)activation(meta,false,false);
        activation(0,false,true);
        need(number==85&&network==0&&nativeLinks==0&&writes==0&&execs==0,"complete external-effect-free scope");
        System.out.println("BED_INTERACTION_COMPLETE cases=85 network=0 native=0 writes=0 exec=0 prepared-app-font=1 genuine-DC-UQ=1 sleep=0 explosion=0 draw=0");
    }
}
