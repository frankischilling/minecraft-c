import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** No replacement game class: actual constructed dc/World/Inventory/Item. */
public final class PickupObservation {
    static fd world;
    static dc player;
    static Object get(Object object, Class<?> owner, String name) throws Exception {
        Field f=owner.getDeclaredField(name);f.setAccessible(true);return f.get(object);
    }
    static void clear() {Arrays.fill(player.c.a,null);}
    static void put(int slot,int id,int count,int damage) {player.c.a[slot]=new iz(id,count,damage);}
    static iz scene(int n) {
        clear();int id=262,count=5,damage=7;
        if(n==1){count=10;damage=9;put(0,262,60,0);}
        if(n==2||n==3||n==4||n==9){
            for(int i=0;i<36;++i)put(i,262,i==0&&n!=2&&n!=9?63:64,0);
            count=n==3?10:n==4?1:5;
        }
        if(n==5||n==6){id=35;count=3;damage=n==5?2:1;put(0,35,2,1);}
        if(n==7||n==8||n==9||n==22){id=270;count=n==22?0:2;damage=n==7?0:1;}
        if(n==10)count=0;if(n==11)count=-4;if(n==12)count=Integer.MAX_VALUE;if(n==13)count=Integer.MIN_VALUE;
        if(n==14||n==15)count=3;
        if(n==16)put(0,263,1,0);if(n==17)id=-1;if(n==18)id=0;
        if(n==19){id=35;count=3;damage=-2;put(0,35,2,-2);}
        if(n==20){count=70;put(0,262,-2,0);}if(n==21)put(0,262,Integer.MIN_VALUE,0);
        iz incoming=new iz(id,count,damage);
        if(n==14||n==15){player.c.a[0]=incoming;if(n==15)player.c.a[1]=incoming;}
        return incoming;
    }
    static void row(iz incoming,int n,String result) {
        System.out.printf("INSERT %d %s %d:%d:%d:%d",n,result,incoming.c,incoming.a,incoming.i(),incoming.b);
        for(int i=0;i<36;++i){
            iz s=player.c.a[i];if(s==null){System.out.print(" -");continue;}
            int alias=s==incoming?-1:i;
            for(int j=0;j<i&&alias!=-1;++j)if(player.c.a[j]==s){alias=j;break;}
            System.out.printf(" %d:%d:%d:%d:%d",alias,s.c,s.a,s.i(),s.b);
        }
        System.out.println(" "+(player.c.e?1:0));
    }
    static long randomState(hl item) throws Exception {
        return ((AtomicLong)get((Random)get(item,sn.class,"bs"),Random.class,"seed")).get();
    }
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        world=new fd(new mx(root.toFile(),"Pickup",false),"Pickup",xa.a(0),23L);
        if(jl.f==null)throw new AssertionError("actual prepared statistics");
        player=new dc(null,world,new gr(null,null),0);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> type:new Class<?>[]{fd.class,dc.class,gs.class,ix.class,iz.class,gm.class,hl.class,uu.class,ep.class}){
            Path origin=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign game class "+type);System.out.println("ORIGIN "+type.getName()+" "+origin);
        }
        for(int n=0;n<23;++n){
            iz incoming=scene(n);String result;
            try{result=player.c.a(incoming)?"TRUE":"FALSE";if(n==17||n==18)throw new AssertionError("missing source failure");}
            catch(ArrayIndexOutOfBoundsException failure){if(n!=17)throw failure;StackTraceElement top=failure.getStackTrace()[0];if(!top.getClassName().equals("iz")||!top.getMethodName().equals("e"))throw failure;result="INDEX";}
            catch(NullPointerException failure){if(n!=18)throw failure;StackTraceElement top=failure.getStackTrace()[0];if(!top.getClassName().equals("iz")||!top.getMethodName().equals("e"))throw failure;result="NULL";}
            row(incoming,n,result);
        }
        for(int n=0;n<7;++n){
            clear();iz stack=new iz(n==4?uu.K.bn:n==5?gm.aD.bf:262,5,0);
            hl item=new hl(world,8,200,8,stack);item.c=n==1?3:0;item.be=false;
            ((Random)get(item,sn.class,"bs")).setSeed(38);world.B=n==0;
            if(n==2||n==3)for(int i=0;i<36;++i)put(i,262,i==0&&n==3?63:64,0);
            boolean complete=false;
            try{item.b(player);complete=true;}
            catch(NullPointerException failure){
                if(n<=3)throw failure;
                StackTraceElement top=failure.getStackTrace()[0];String method=n==4||n==5?"a":"b";
                if(!top.getClassName().equals("dc")||!top.getMethodName().equals(method))throw failure;
            }
            if(complete!=(n<=3))throw new AssertionError("pickup completion "+n);
            System.out.printf("PICKUP %d %d %d %d %012x%n",n,complete?1:0,stack.a,item.be?1:0,randomState(item));
            row(stack,100+n,complete?"DONE":"NULL");world.B=false;
        }
        System.out.println("Original Item pickup: 37 states");
    }
}
