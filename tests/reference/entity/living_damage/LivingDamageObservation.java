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

/** Authored operands and observers; all damage/knockback/sound/death methods are original. */
public final class LivingDamageObservation {
    static int network,nativeLinks,writes;
    static fd world;static dl actor;static gi source;static Random shared;
    static String sourceBefore;static eq sourceBox;static Random sourceRandom;
    static Observer observer;
    static final String[] BOX={"a","b","c","d","e","f"};
    static final double[][] POINTS={{3,4},{-2.8,3.3},{-3,-4},{0,0},{.005,.005},{.01,0}};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static String seed(Random r)throws Exception{return r==null?null:String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(r)).get());}
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
    static String sourceState()throws Exception{return json(map("position",position(source),"velocity",velocity(source),"bounds",bounds(source.aW),"random",seed(source.bs),"yaw",bits(source.aS),"ground",source.aX,"dead",source.be,"attacked",source.bb,"health",source.Y));}
    static Map<String,Object> state()throws Exception{
        List<Integer> health=new ArrayList<>();for(String n:new String[]{"Y","Z","E"})health.add(field(ls.class,n).getInt(actor));health.add(actor.by);for(String n:new String[]{"au","av","aa","ab"})health.add(field(ls.class,n).getInt(actor));
        return map("health",health,"animation",List.of(bits(actor.ac),bits(actor.al)),"velocity",velocity(actor),"attacked",actor.bb,"dying",field(ls.class,"ah").getBoolean(actor),"position",position(actor),"yaw",bits(actor.aS),"bounds",bounds(actor.aW),"random",seed(actor.bs),"math_random",seed(shared),"world_random",seed(world.r),"source_random",seed(source.bs),"source_cooldown",source.ae,"source_unchanged",sourceBefore.equals(sourceState())&&source.aW==sourceBox&&source.bs==sourceRandom&&source.aI==world,"sounds",new ArrayList<>(observer.sounds));
    }
    static final class Observer implements pm {
        final List<Object> sounds=new ArrayList<>();final int profile;final boolean throwing;Map<String,Object> checkpoint;
        Observer(int p,boolean t){profile=p;throwing=t;}
        public void a(String n,double x,double y,double z,float v,float p){
            sounds.add(map("name",n,"position",List.of(bits(x),bits(y),bits(z)),"volume",bits(v),"pitch",bits(p)));
            if(profile==19){try{checkpoint=state();}catch(Exception e){throw new AssertionError(e);}}
            if(throwing)throw new IllegalStateException("authored reached World sound observer failure");
        }
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void a(sn e){throw new AssertionError("unexpected entity publication");}
        public void b(sn e){throw new AssertionError("unexpected entity removal");}
        public void a(int x,int y,int z){throw new AssertionError("unexpected block notification");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("unexpected range notification");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String s,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("unexpected tile");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static void original(Class<?> c,Path jar)throws Exception{require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive origin: "+c.getName());}
    @SuppressWarnings({"removal","unchecked"})
    public static void main(String[] args)throws Exception{
        require(args.length==2,"original JAR and profile table");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p){++writes;throw new SecurityException("write denied");}
            @Override public void checkDelete(String p){++writes;throw new SecurityException("delete denied");}
        });
        for(Class<?> c:new Class<?>[]{dl.class,bg.class,ii.class,ls.class,sn.class,gi.class,fd.class,wy.class,xa.class,gr.class,eq.class,in.class})original(c,jar);
        new gr("LivingDamageReference","");world=new fd(new wy(),"LivingDamageReference",xa.a(0),23L);
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);shared=(Random)field(holder,"randomNumberGenerator").get(null);
        Method bite=gi.class.getDeclaredMethod("a",sn.class,float.class);bite.setAccessible(true);
        int row=0;
        for(int profile=0;profile<profiles.size();++profile){
            String[] c=profiles.get(profile).split(" ");require(c.length==16,"profile width");int[] n=new int[16];for(int i=0;i<16;++i)if(i!=10&&i!=11)n[i]=Integer.parseInt(c[i]);
            world.B=n[5]==1;((List<pm>)field(fd.class,"u").get(world)).clear();
            actor=new dl(world);source=new gi(world);actor.e(8.5,64.5,8.5);source.e(8.5+POINTS[n[9]][0],64.5,8.5+POINTS[n[9]][1]);
            actor.aS=Float.intBitsToFloat((int)Long.parseLong(c[10],16));actor.aP=.25;actor.aQ=Double.longBitsToDouble(Long.parseUnsignedLong(c[11],16));actor.aR=-.125;actor.bf=.125f;
            source.aS=15;source.aP=-.5;source.aQ=.75;source.aR=1.25;source.ae=123;source.aX=true;source.bb=false;source.be=false;source.d(n[14]!=0);
            actor.Y=n[2];actor.Z=17;actor.E=20;actor.by=n[3];field(ls.class,"au").setInt(actor,n[4]);field(ls.class,"av").setInt(actor,17);actor.aa=3;actor.ab=4;actor.ac=99;actor.al=.25f;actor.bb=false;field(ls.class,"ah").setBoolean(actor,false);actor.a(true);
            bounds(actor.aW,new double[]{8,64,8,9,65,9});bounds(source.aW,new double[]{source.aM-.5,64,source.aO-.5,source.aM+.5,65,source.aO+.5});
            if(n[8]==1)field(sn.class,"aW").set(actor,null);
            actor.bs.setSeed(n[13]);source.bs.setSeed(12345);world.r.setSeed(2468);shared.setSeed(n[12]);
            if(n[7]==1)field(sn.class,"bs").set(actor,null);if(n[5]==2)actor.aI=null;
            observer=new Observer(profile,n[15]!=0);world.a(observer);sourceBox=source.aW;sourceRandom=source.bs;sourceBefore=sourceState();
            List<Object> input=new ArrayList<>();for(int i=0;i<16;++i)input.add(i==10||i==11?c[i]:n[i]);
            int[] amounts=n[0]==2?new int[]{4,3,7}:new int[]{n[1]};
            for(int step=0;step<amounts.length;++step){
                Object result=null;String exception="NONE";
                try{if(n[0]==1){bite.invoke(source,actor,1f);}else result=actor.a(n[6]==1?null:source,amounts[step]);}
                catch(Throwable caught){Throwable t=caught instanceof InvocationTargetException?((InvocationTargetException)caught).getCause():caught;if(t instanceof NullPointerException)exception="NPE";else if(t instanceof IllegalStateException&&n[15]==1)exception="CALLBACK";else throw caught;}
                Map<String,Object> after=state();Object completion=null;String scope=exception.equals("NONE")?"RETURN":"EXCEPTION";
                if(profile==19){require(observer.checkpoint!=null&&exception.equals("NONE")&&Boolean.TRUE.equals(result),"actual complete death plus callback checkpoint");completion=map("result",result,"exception",exception,"state",after);after=observer.checkpoint;scope="DEATH_ENTRY";}
                System.out.println("LIVING_DAMAGE "+json(map("case",row++,"profile",profile,"step",step,"input",input,"amount",amounts[step],"scope",scope,"result",result,"exception",exception,"state",after,"completion",completion)));
                require(Boolean.TRUE.equals(after.get("source_unchanged")),"source geometry/Random identity preserved");
            }
        }
        require(profiles.size()==25&&row==27&&network==0&&nativeLinks==0&&writes==0,"bounded observations and external-effect guards");
        System.out.println("Original Living damage: 27 rows; network=0; native=0; write=0; genuine_world=1");
    }
}
