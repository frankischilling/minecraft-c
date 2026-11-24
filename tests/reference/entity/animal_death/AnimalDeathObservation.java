import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored operands and read-only observers; original death/drop/publication bodies execute. */
public final class AnimalDeathObservation {
    public static int active=-1;
    static int network,nativeLinks,writes;
    static fd world;static lm chunk;static ls actor;static gi source;static Random shared;
    static Observer observer;static String sourceBefore;static eq sourceBox;static Random sourceRandom;
    static final String[] BOX={"a","b","c","d","e","f"};
    static final List<hl> items=new ArrayList<>();
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static String seed(Random r)throws Exception{return r==null?null:String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(r)).get());}
    static String uniquifier()throws Exception{return String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seedUniquifier").get(null)).get());}
    static List<String> position(sn e){return List.of(bits(e.aM),bits(e.aN),bits(e.aO));}
    static List<String> velocity(sn e){return List.of(bits(e.aP),bits(e.aQ),bits(e.aR));}
    static List<String> bounds(eq box)throws Exception{if(box==null)return null;List<String> r=new ArrayList<>();for(String n:BOX)r.add(bits(field(eq.class,n).getDouble(box)));return r;}
    static void bounds(eq box,double[] values)throws Exception{for(int i=0;i<6;++i)field(eq.class,BOX[i]).setDouble(box,values[i]);}
    static Map<String,Object> map(Object... pairs){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)r.put((String)pairs[i],pairs[i+1]);return r;}
    static String json(Object value){
        if(value==null)return "null";
        if(value instanceof String)return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"")+"\"";
        if(value instanceof Map){StringBuilder b=new StringBuilder("{");for(Object item:((Map<?,?>)value).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)item;if(b.length()>1)b.append(',');b.append(json(e.getKey())).append(':').append(json(e.getValue()));}return b.append('}').toString();}
        if(value instanceof List){StringBuilder b=new StringBuilder("[");for(Object x:(List<?>)value){if(b.length()>1)b.append(',');b.append(json(x));}return b.append(']').toString();}
        return value.toString();
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd make()throws Exception{
        fd w=new fd(new wy(),"AnimalDeathReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(w);Map cache=(Map)field(ok.class,"e").get(provider);List loaded=(List)field(ok.class,"f").get(provider);cache.clear();loaded.clear();
        chunk=new lm(w,new byte[32768],0,0);chunk.n=true;cache.put(yy.a(0,0),chunk);loaded.add(chunk);return w;
    }
    static int membership(sn e){int count=0;for(List<?> slice:chunk.m)for(Object member:slice)if(member==e)++count;return count;}
    static Map<String,Object> item(hl p)throws Exception{
        boolean unique=true;for(hl other:items)if(other!=p&&other.a==p.a)unique=false;
        return map("stack",List.of(p.a.c,p.a.a,field(iz.class,"d").getInt(p.a),p.a.b),"position",position(p),"previous_position",List.of(bits(p.aJ),bits(p.aK),bits(p.aL)),"velocity",velocity(p),"rotation",List.of(bits(p.aS),bits(p.aT),bits(p.aU),bits(p.aV),bits(p.d)),"size",List.of(bits(p.bf),bits(p.bg),bits(p.bh)),"bounds",bounds(p.aW),"body",List.of(p.b,p.c,field(hl.class,"e").getInt(p),field(hl.class,"f").getInt(p),p.bt,p.bv,p.by),"flags",List.of(p.aX,p.aY,p.aZ,p.ba,p.bb,p.bc,p.bd,p.be,p.bq,p.bF),"chunk",List.of(p.bG,p.bH,p.bI),"private_random",seed(p.bs),"world",p.aI==world?"PRIMARY":"FOREIGN","world_member",world.b.contains(p),"chunk_members",membership(p),"stack_unique",unique);
    }
    static String sourceState()throws Exception{return json(map("position",position(source),"velocity",velocity(source),"bounds",bounds(source.aW),"random",seed(source.bs),"yaw",bits(source.aS),"ground",source.aX,"dead",source.be,"attacked",source.bb,"health",source.Y));}
    static Map<String,Object> state()throws Exception{
        List<Integer> health=new ArrayList<>();for(String n:new String[]{"Y","Z","E"})health.add(field(ls.class,n).getInt(actor));health.add(actor.by);for(String n:new String[]{"au","av","aa","ab","ad","T"})health.add(field(ls.class,n).getInt(actor));
        Object watcher=null;if(actor instanceof dl&&actor.bD!=null)watcher=((Number)actor.bD.a(16)).intValue();
        List<Object> dropped=new ArrayList<>();for(hl p:items)dropped.add(item(p));
        return map("health",health,"animation",List.of(bits(actor.ac),bits(actor.al)),"velocity",velocity(actor),"attacked",actor.bb,"dying",field(ls.class,"ah").getBoolean(actor),"dead",actor.be,"position",position(actor),"yaw",bits(actor.aS),"bounds",bounds(actor.aW),"random",seed(actor.bs),"math_random",seed(shared),"world_random",seed(world.r),"source_random",seed(source.bs),"source_cooldown",source.ae,"source_unchanged",sourceBefore.equals(sourceState())&&source.aW==sourceBox&&source.bs==sourceRandom&&source.aI==world,"world",actor.aI==world?"PRIMARY":actor.aI==null?null:"FOREIGN","watcher",watcher,"sounds",new ArrayList<>(observer.sounds),"items",dropped,"world_loaded",world.b.size(),"chunk_count",chunk.m[4].size(),"events",new ArrayList<>(observer.events));
    }
    static final class Observer implements pm {
        final List<Object> sounds=new ArrayList<>(),events=new ArrayList<>();final int mode;
        Observer(int m){mode=m;}
        public void a(String n,double x,double y,double z,float v,float p){sounds.add(map("name",n,"position",List.of(bits(x),bits(y),bits(z)),"volume",bits(v),"pitch",bits(p)));events.add(map("kind","SOUND","index",sounds.size()-1));if(mode==4)actor.aI=null;}
        public void a(sn e){
            require(e instanceof hl,"actual Item publication");hl p=(hl)e;require(!items.contains(p),"distinct publication object");items.add(p);events.add(map("kind","ADD","index",items.size()-1,"world_member",world.b.contains(p),"chunk_members",membership(p)));
            if(mode==3&&items.size()==1)actor.aI=null;
            if(mode==1&&items.size()==1||mode==2&&items.size()==2)throw new IllegalStateException("authored reached Item publication observer failure");
        }
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void b(sn e){throw new AssertionError("unexpected entity removal");}
        public void a(int x,int y,int z){throw new AssertionError("unexpected block notification");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("unexpected range notification");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String s,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("unexpected tile");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static void original(Class<?> c,Path jar)throws Exception{require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive origin: "+c.getName());}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        require(args.length==2,"original JAR and profile table");Locale.setDefault(Locale.ROOT);Path jar=Path.of(args[0]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p){++writes;throw new SecurityException("write denied");}
            @Override public void checkDelete(String p){++writes;throw new SecurityException("delete denied");}
        });
        for(Class<?> c:new Class<?>[]{dl.class,wh.class,bx.class,ww.class,bg.class,ii.class,ls.class,sn.class,gi.class,hl.class,iz.class,fd.class,ok.class,lm.class,wy.class,xa.class,gr.class,eq.class,in.class,uu.class,gm.class,ud.class})original(c,jar);
        new gr("AnimalDeathReference","");Method bite=gi.class.getDeclaredMethod("a",sn.class,float.class);bite.setAccessible(true);
        int row=0;
        for(int profile=0;profile<profiles.size();++profile){
            String[] cells=profiles.get(profile).split(" ");require(cells.length==14,"profile width");int[] n=Arrays.stream(cells).mapToInt(Integer::parseInt).toArray();
            world=make();world.B=n[6]==1;actor=(ls)Class.forName(new String[]{"dl","wh","bx","ww","gi"}[n[0]]).getDeclaredConstructor(fd.class).newInstance(world);source=new gi(world);
            Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);shared=(Random)field(holder,"randomNumberGenerator").get(null);
            Map<String,Object> constructor=map("actor_private_random",seed(actor.bs),"source_private_random",seed(source.bs),"math_random",seed(shared));
            actor.e(8.5,64.5,8.5);source.e(11.5,64.5,12.5);actor.aS=30;actor.aP=.25;actor.aQ=-.375;actor.aR=-.125;actor.bf=.125f;
            source.aS=15;source.aP=-.5;source.aQ=.75;source.aR=1.25;source.ae=123;source.aX=true;source.bb=false;source.be=false;source.d(false);
            actor.Y=n[1]==2?2:4;actor.Z=17;actor.E=20;actor.by=0;field(ls.class,"au").setInt(actor,0);field(ls.class,"av").setInt(actor,17);actor.aa=3;actor.ab=4;actor.ad=7;actor.ac=99;actor.al=.25f;actor.bb=false;actor.be=false;actor.bv=n[4];field(ls.class,"ah").setBoolean(actor,false);field(ls.class,"T").setInt(actor,n[10]);
            if(actor instanceof dl){((dl)actor).e_(n[2]);((dl)actor).a(n[3]!=0);}
            bounds(actor.aW,new double[]{8,64,8,9,65,9});bounds(source.aW,new double[]{11,64,12,12,65,13});
            actor.bs.setSeed(n[5]);source.bs.setSeed(12345);world.r.setSeed(2468);
            if(n[11]!=0)field(sn.class,"bs").set(actor,null);if(n[12]!=0)field(sn.class,"bD").set(actor,null);if(n[6]==2)actor.aI=null;
            items.clear();observer=new Observer(n[8]);world.a(observer);sourceBefore=sourceState();sourceBox=source.aW;sourceRandom=source.bs;
            List<Object> input=new ArrayList<>();for(int v:n)input.add(v);
            for(int step=0;step<n[9];++step){
                String mathBefore=seed(shared),uniquifierBefore=uniquifier();Object result=null;String exception="NONE";active=row;
                try{if(n[1]==1)actor.b(n[7]==1?null:source);else if(n[1]==2)bite.invoke(source,actor,1f);else result=actor.a(n[7]==1?null:source,4);}
                catch(Throwable caught){Throwable t=caught instanceof InvocationTargetException?((InvocationTargetException)caught).getCause():caught;if(t instanceof NullPointerException)exception="NPE";else if(t instanceof IllegalStateException&&(n[8]==1||n[8]==2))exception="CALLBACK";else throw caught;}
                active=-1;
                String scope=exception.equals("NONE")?"FULL":exception.equals("NPE")?"SOURCE_FAILURE":"PUBLICATION_CALLBACK";
                Map<String,Object> after=state();System.out.println("ANIMAL_DEATH "+json(map("case",row++,"profile",profile,"step",step,"input",input,"result",result,"exception",exception,"scope",scope,"math_before",mathBefore,"uniquifier_before",uniquifierBefore,"uniquifier_after",uniquifier(),"constructor",constructor,"state",after)));
                require(Boolean.TRUE.equals(after.get("source_unchanged")),"source geometry/Random identity preserved");
            }
        }
        require(profiles.size()==42&&row==43&&network==0&&nativeLinks==0&&writes==0,"bounded observations/external-effect guards");
        System.out.println("Original animal death: 43 rows; network=0; native=0; write=0; genuine_world_chunk=1");
    }
}
