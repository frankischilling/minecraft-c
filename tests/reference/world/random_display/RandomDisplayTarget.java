import java.io.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored fixture data and callback transport. Original World/Block methods stay in the JAR. */
@SuppressWarnings({"unchecked","rawtypes"})
public final class RandomDisplayTarget {
    public static int active=-1;
    public static fd world;
    public static Random argument,original,replacement;
    public static uu actor;
    static PrintWriter output;
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Object o,Class<?> c,String n)throws Exception{return field(c,n).get(o);}
    static int number(Object o,Class<?> c,String n)throws Exception{return field(c,n).getInt(o);}
    static String seed(Random r)throws Exception{return r==null?"-":String.format(Locale.ROOT,"%012x",((AtomicLong)get(r,Random.class,"seed")).get());}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static void need(boolean ok,String text){if(!ok)throw new AssertionError(text);}
    static void emit(Map<String,Object> row){output.println(ConstructorJson.json(row));output.flush();}
    static Map<Integer,lm> cache(fd w)throws Exception{return(Map<Integer,lm>)get(get(w,fd.class,"v"),ok.class,"e");}
    static fd make()throws Exception{fd w=new fd(new wy(),"RandomDisplayReference",xa.a(0),23L);w.B=false;w.q=1;w.f=0;cache(w).clear();((List)get(get(w,fd.class,"v"),ok.class,"f")).clear();field(fd.class,"g").setInt(w,17);field(fd.class,"Q").setInt(w,6001);return w;}
    static void fill(fd w,int id,boolean fallback)throws Exception{
        List loaded=(List)get(get(w,fd.class,"v"),ok.class,"f");lm blank=fallback?(lm)get(get(w,fd.class,"v"),ok.class,"b"):null;
        if(blank!=null)Arrays.fill(blank.b,(byte)id);
        for(int x=-1;x<=1;x++)for(int z=-1;z<=1;z++){lm c=blank==null?new lm(w,new byte[32768],x,z):blank;c.n=true;if(blank==null)Arrays.fill(c.b,(byte)id);if(id!=0){Arrays.fill(c.h,(byte)128);c.i=128;}cache(w).put(yy.a(x,z),c);loaded.add(c);}
    }
    static void cell(fd w,int x,int y,int z,int id,int metadata)throws Exception{
        if(y<0||y>=128||x< -32000000||x>=32000000||z< -32000000||z>=32000000)return;lm c=cache(w).get(yy.a(x>>4,z>>4));need(c!=null,"authored cached cell");c.b[((x&15)<<11)|((z&15)<<7)|y]=(byte)id;c.e.a(x&15,y,z&15,metadata);
        if(id!=0)c.h[((z&15)<<4)|(x&15)]=(byte)Math.max(c.h[((z&15)<<4)|(x&15)]&255,y+1);
    }
    static final class Observer implements pm {
        final List<String> events=new ArrayList<>();String action;int id,particles,sounds;boolean changed;
        Observer(int id,String action){this.id=id;this.action=action;}
        void callback(boolean particle)throws Exception{
            if(particle&&action.equals("swap_random")&&!changed){changed=true;world.r=replacement;}
            if(particle&&action.equals("clear_observers")&&!changed){changed=true;((List)get(world,fd.class,"u")).clear();}
            if(particle&&action.equals("null_provider")&&!changed){changed=true;field(fd.class,"v").set(world,null);}
            if(particle&&action.equals("fail_particle"))throw new IllegalStateException("authored reached particle callback failure");
            if(!particle&&action.equals("fail_sound"))throw new IllegalStateException("authored reached sound callback failure");
        }
        public void a(String n,double x,double y,double z,double a,double b,double c){int ordinal=events.size();particles++;events.add("DISPLAY_PARTICLE "+id+" "+ordinal+" "+n+" "+bits(x)+" "+bits(y)+" "+bits(z)+" "+bits(a)+" "+bits(b)+" "+bits(c));try{callback(true);}catch(RuntimeException e){throw e;}catch(Exception e){throw new AssertionError(e);}}
        public void a(String n,double x,double y,double z,float a,float b){int ordinal=events.size();sounds++;events.add("DISPLAY_SOUND "+id+" "+ordinal+" "+n+" "+bits(x)+" "+bits(y)+" "+bits(z)+" "+bits(a)+" "+bits(b));try{callback(false);}catch(RuntimeException e){throw e;}catch(Exception e){throw new AssertionError(e);}}
        public void a(int x,int y,int z){events.add("UNEXPECTED_CELL");}
        public void b(int x,int y,int z,int a,int b,int c){events.add("UNEXPECTED_RANGE");}
        public void a(sn e){events.add("UNEXPECTED_ADD");}
        public void b(sn e){events.add("UNEXPECTED_REMOVE");}
        public void e(){events.add("UNEXPECTED_REFRESH");}
        public void a(String s,int x,int y,int z){events.add("UNEXPECTED_RECORD");}
        public void a(int x,int y,int z,ow t){events.add("UNEXPECTED_TILE");}
        public void a(gs p,int a,int b,int c,int d,int e){events.add("UNEXPECTED_LEVEL");}
    }
    static List<String[]> table(Path p)throws Exception{List<String[]> out=new ArrayList<>();List<String> lines=Files.readAllLines(p);for(int i=1;i<lines.size();i++)out.add(lines.get(i).split("\t",-1));return out;}
    static void sampler(String[] p)throws Exception{
        int id=Integer.parseInt(p[0]),block=Integer.parseInt(p[2]),x=Integer.parseInt(p[3]),y=Integer.parseInt(p[4]),z=Integer.parseInt(p[5]);String scene=p[6],action=p[7];
        world=make();fill(world,block,scene.equals("fallback"));world.r.setSeed(0);original=world.r;replacement=new Random(123);argument=null;actor=null;
        Observer observer=new Observer(id,action);world.a(observer);Object provider=get(world,fd.class,"v");uu stone=uu.m[1];if(scene.equals("entry_null"))uu.m[1]=null;if(scene.equals("random_null"))world.r=null;
        int outcome=1;String failure=null,top=null;active=id;
        try{world.q(x,y,z);}catch(RuntimeException e){outcome=-1;failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
        active=-1;
        String row="SAMPLER_STATE "+id+" "+p[1]+" "+outcome+" "+seed(world.r)+" "+seed(original)+" "+number(world,fd.class,"g")+" "+number(world,fd.class,"Q")+" "+observer.particles+" "+observer.sounds+" "+observer.events.size();
        emit(ConstructorJson.map("event","sampler","case",id,"input",List.of(p),"row",row,"events",observer.events,"failure",failure,"top",top,"source_only_callback_failure",action.startsWith("fail"),"local_default_entropy_rebound",false));uu.m[1]=stone;if(scene.equals("random_null"))world.r=original;if(action.equals("null_provider"))field(fd.class,"v").set(world,provider);
    }
    static void direct(String[] p)throws Exception{
        int id=Integer.parseInt(p[0]),block=Integer.parseInt(p[1]),meta=Integer.parseInt(p[3]),x=Integer.parseInt(p[6]),y=Integer.parseInt(p[7]),z=Integer.parseInt(p[8]);String scene=p[9],action=p[10];
        world=make();fill(world,0,false);world.r.setSeed(Long.parseLong(p[5]));original=world.r;replacement=new Random(123);argument=p[4].equals("NULL")?null:new Random(Long.parseLong(p[4]));actor=uu.m[block];
        cell(world,x,y,z,block,meta);
        if(scene.equals("above_stone"))cell(world,x,y+1,z,1,0);
        if(scene.equals("below_stone"))cell(world,x,y-1,z,1,0);
        if(scene.equals("below_leaves"))cell(world,x,y-1,z,18,0);
        if(scene.equals("opaque_six"))for(int[] d:new int[][]{{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,0,0},{-1,0,0}})cell(world,x+d[0],y+d[1],z+d[2],1,0);
        if(scene.equals("portal_left"))cell(world,x-1,y,z,90,0);
        if(scene.equals("portal_right"))cell(world,x+1,y,z,90,0);
        if(scene.startsWith("fire_faces_")){int mask=Integer.parseInt(scene.substring(11));int[][] d={{-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,1,0}};for(int i=0;i<5;i++)if((mask&(1<<i))!=0)cell(world,x+d[i][0],y+d[i][1],z+d[i][2],5,0);}
        Object wrapped=null,fireArray=null;
        if(scene.startsWith("wrapper_")){wrapped=get(actor,ss.class,"a");field(ss.class,"a").set(actor,scene.equals("wrapper_null")?null:uu.m[50]);}
        if(scene.equals("fire_array_null")){fireArray=get(uu.as,yq.class,"a");field(yq.class,"a").set(uu.as,null);}
        if(scene.equals("world_random_null"))world.r=null;
        Observer observer=new Observer(id,action);world.a(observer);int outcome=1;String failure=null,top=null;active=1000+id;
        try{actor.b(world,x,y,z,argument);}catch(RuntimeException e){outcome=-1;failure=e.getClass().getName();top=e.getStackTrace()[0].toString();}
        active=-1;
        String row="DISPLAY_STATE "+id+" "+block+" "+actor.getClass().getName()+" "+p[2]+" "+outcome+" "+seed(argument)+" "+seed(world.r)+" "+seed(original)+" "+observer.particles+" "+observer.sounds+" "+observer.events.size();
        emit(ConstructorJson.map("event","display","case",id,"input",List.of(p),"row",row,"events",observer.events,"failure",failure,"top",top,"source_only_callback_failure",action.startsWith("fail"),"argument_is_world_random",argument!=null&&argument==world.r));
        if(scene.startsWith("wrapper_"))field(ss.class,"a").set(actor,wrapped);if(scene.equals("fire_array_null"))field(yq.class,"a").set(uu.as,fireArray);if(scene.equals("world_random_null"))world.r=original;
    }
    static void bindings(Path jar)throws Exception{
        for(String name:Files.readAllLines(Path.of(System.getProperty("display.classes")))){Class<?> c=Class.forName(name,false,RandomDisplayTarget.class.getClassLoader());need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");byte[] data;try(InputStream in=c.getResourceAsStream("/"+name.replace('.','/')+".class")){data=in.readAllBytes();}System.out.println("BIND "+name+" "+data.length+" "+HexFormat.of().formatHex(java.security.MessageDigest.getInstance("SHA-256").digest(data)));}
    }
    public static void main(String[] args)throws Exception{
        new gr("RandomDisplaySetup","");Path source=Path.of(args[2]);try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0])))){output=out;for(String[] p:table(source.resolve("samplers.tsv")))sampler(p);for(String[] p:table(source.resolve("profiles.tsv")))direct(p);if(out.checkError())throw new IOException("transport output");}bindings(Path.of(args[1]).toRealPath());
    }
}
