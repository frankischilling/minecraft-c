import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Unchanged protected GI attack over genuine GI/JU owners, both World=NULL. */
public final class WolfAttackObservation {
    static int network, nativeLinks, writes;
    static final double[][] POINTS = {{3,0,4},{3.3,0,-2.8},{0,3,0},{0,0,0},{0,2,0},
        {-5.25,-1,2.125},{16777215.75,0,-256.75}};
    static final String[] BOX = {"a","b","c","d","e","f"};
    static void require(boolean value,String message) { if(!value)throw new AssertionError(message); }
    static Field field(Class<?> type,String name)throws Exception {
        Field result=type.getDeclaredField(name);result.setAccessible(true);return result;
    }
    static long seed(Random random)throws Exception { return ((AtomicLong)field(Random.class,"seed").get(random)).get(); }
    static String bits(double value) { return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value)); }
    static String position(sn value) { return "[\""+bits(value.aM)+"\",\""+bits(value.aN)+"\",\""+bits(value.aO)+"\"]"; }
    static String velocity(sn value) { return "[\""+bits(value.aP)+"\",\""+bits(value.aQ)+"\",\""+bits(value.aR)+"\"]"; }
    static String bounds(eq value)throws Exception {
        if(value==null)return "null";
        StringBuilder result=new StringBuilder("[");
        for(int i=0;i<BOX.length;++i){if(i!=0)result.append(',');result.append('"').append(bits(field(eq.class,BOX[i]).getDouble(value))).append('"');}
        return result.append(']').toString();
    }
    static void bounds(eq value,double[] coordinates)throws Exception {
        for(int i=0;i<BOX.length;++i)field(eq.class,BOX[i]).setDouble(value,coordinates[i]);
    }
    static String target(ju value)throws Exception {
        return position(value)+velocity(value)+bounds(value.aW)+value.a+":"+value.b+":"+value.be+":"+seed(value.bs);
    }
    static void original(Class<?> type,Path jar)throws Exception {
        require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original class: "+type.getName());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==2,"original JAR and profiles");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath();
        java.util.List<String> profiles=Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission permission){}
            @Override public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String library){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String path){++writes;throw new SecurityException("write denied");}
            @Override public void checkDelete(String path){++writes;throw new SecurityException("delete denied");}
        });
        for(Class<?> type:new Class<?>[]{gi.class,ju.class,sn.class,ls.class,eq.class,in.class})original(type,jar);
        Method attack=gi.class.getDeclaredMethod("a",sn.class,float.class);attack.setAccessible(true);
        int index=0;
        for(String line:profiles){
            if(line.isBlank())continue;
            String[] cells=line.split(" ");require(cells.length==8,"profile width");
            long randomSeed=Long.parseLong(cells[0]);int distance=(int)Long.parseLong(cells[1],16);
            int grounded=Integer.parseInt(cells[2]),point=Integer.parseInt(cells[3]),tame=Integer.parseInt(cells[4]);
            int targetMode=Integer.parseInt(cells[5]),boxMode=Integer.parseInt(cells[6]),cooldown=Integer.parseInt(cells[7]);
            gi actor=new gi((fd)null);ju victim=new ju((fd)null);
            require(actor.aI==null&&victim.aI==null,"actual NULL World constructors");
            actor.e(8.5,64.5,8.5);double[] delta=POINTS[point];
            victim.e(8.5+delta[0],64.5+delta[1],8.5+delta[2]);
            actor.aP=.25;actor.aQ=-.375;actor.aR=-.125;
            victim.aP=-.5;victim.aQ=.75;victim.aR=1.25;
            actor.aX=grounded!=0;field(ls.class,"ae").setInt(actor,cooldown);
            actor.d(tame!=0);
            victim.a=12;victim.b=7;victim.be=false;victim.bb=false;
            bounds(actor.aW,new double[]{8,64,8,9,65,9});
            bounds(victim.aW,new double[]{victim.aM-.5,victim.aN-.5,victim.aO-.5,
                victim.aM+.5,victim.aN+.5,victim.aO+.5});
            if(boxMode==1)field(sn.class,"aW").set(victim,null);
            else if(boxMode==2)field(sn.class,"aW").set(actor,null);
            else if(boxMode==3)victim.aW.e=64;
            else if(boxMode==4)victim.aW.b=65;
            actor.bs.setSeed(randomSeed);victim.bs.setSeed(12345);
            Random actorRandom=actor.bs,targetRandom=victim.bs;
            eq ownBox=actor.aW,targetBox=victim.aW;
            String before=target(victim),ownBounds=bounds(ownBox),ownPosition=position(actor);
            String exception="NONE";
            try{attack.invoke(actor,targetMode==1?null:victim,Float.intBitsToFloat(distance));}
            catch(InvocationTargetException caught){if(caught.getCause() instanceof NullPointerException)exception="NPE";else throw caught;}
            boolean unchanged=before.equals(target(victim))&&victim.bs==targetRandom&&victim.aW==targetBox&&victim.aI==null;
            require(actor.bs==actorRandom&&actor.aW==ownBox&&actor.aI==null&&ownBounds.equals(bounds(actor.aW))&&ownPosition.equals(position(actor)),"unchanged attack actor geometry/Random identity");
            StringBuilder profile=new StringBuilder("[").append(randomSeed).append(",\"").append(cells[1]).append('"');
            for(int i=2;i<cells.length;++i)profile.append(',').append(Integer.parseInt(cells[i]));
            profile.append(']');
            System.out.println("WOLF_ATTACK {\"case\":"+(index++)+",\"profile\":"+profile+
                ",\"exception\":\""+exception+"\",\"velocity\":"+velocity(actor)+
                ",\"cooldown\":"+field(ls.class,"ae").getInt(actor)+",\"random\":\""+String.format(Locale.ROOT,"%016x",seed(actor.bs))+
                "\",\"target_unchanged\":"+unchanged+",\"target_attacked\":"+victim.bb+",\"actor_position\":"+position(actor)+
                ",\"target_position\":"+position(victim)+",\"actor_bounds\":"+bounds(actor.aW)+
                ",\"target_bounds\":"+bounds(victim.aW)+"}");
            require(unchanged,"actual FallingSand target unchanged");
        }
        require(index==24&&network==0&&nativeLinks==0&&writes==0,"bounded scope and external effects");
        System.out.println("Original Wolf attack: 24 rows; network=0; native=0; write=0; world=0");
    }
}
