import java.lang.invoke.*;
import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Source inputs on unchanged actual kq/xi/World/Chunk/dc owners. No override. */
public final class TravelObservation {
    static fd world;static lm chunk;static kq client;static dc p;static int rows;
    static final String[] FIELDS={"l","m","o","p","q","u"};
    static final vr[] STATS={jl.l,jl.m,jl.o,jl.p,jl.q,jl.u};
    static Field field(Class<?> c,String n)throws Exception {Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static MethodHandle special(Class<?> owner,String name,MethodType type)throws Exception {
        return MethodHandles.privateLookupIn(owner,MethodHandles.lookup()).findSpecial(owner,name,type,owner);
    }
    static void require(boolean condition,String label){if(!condition)throw new AssertionError(label);}
    static int flag(boolean v){return v?1:0;}
    static void cell(int x,int y,int z,int id){chunk.b[(x<<11)|(z<<7)|y]=(byte)id;}
    static void floor(int id){for(int x=6;x<=12;++x)for(int z=6;z<=12;++z)cell(x,7,z,id);}
    static void fluid(int id){for(int x=6;x<=12;++x)for(int z=6;z<=12;++z)for(int y=6;y<=10;++y)cell(x,y,z,id);}
    static void reset()throws Exception {
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
        p.bf=p.bo=p.bp=0;field(sn.class,"bk").setFloat(p,0);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=false;
        p.bq=false;field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.bj=0;field(sn.class,"b").setInt(p,0);
        p.u=false;p.a=new uo();p.aS=37;p.ak=.125f;p.al=.25f;p.am=.5f;
        p.b(1f,1f);p.e(8.5,8,8.5);p.aP=.2;p.aQ=-.1;p.aR=-.3;
        p.aJ=8.25;p.aK=7.5;p.aL=8.75;eq.b();bt.b();
        uu.m[1].bB=.6f;
    }
    static void prepare(int c)throws Exception {
        reset();if(c==1||c==2||c==3){floor(c==2?79:1);p.aX=true;}
        if(c==4||c==6||(c>=13&&c<=15))field(sn.class,"bx").setBoolean(p,true);
        if(c==5||c==6||c==16)fluid(10);if(c==7)fluid(8);
        if(c>=8&&c<=10){cell(8,8,8,65);p.aQ=-1;field(sn.class,"bk").setFloat(p,7);}
        if(c==9)p.a.e=true;
        if(c==10){cell(8,7,8,65);for(int y=7;y<=10;++y)for(int z=7;z<=9;++z)cell(9,y,z,1);p.aP=1;}
        if(c==11)p.aP=p.aQ=p.aR=0;if(c==12)p.aS=90;
        if(c==5||c==6||c==13||c==14||c==15||c==16)p.bq=true;
        if(c==14){p.aJ=-4;p.aL=3;}if(c==15||c==16)p.aY=true;
        if(c>=17&&c<=19){floor(c==18?79:1);p.aX=true;p.bq=true;if(c==19)uu.m[1].bB=.3f;}
        if(c>=20){p.bq=true;field(sn.class,"bx").setBoolean(p,true);
            p.aP=Double.longBitsToDouble(c==20?0x7ff8000000000123L:c==21?0xfff8000000000456L:0x7ff0000000000000L);}
    }
    static int[] totals(){int[] result=new int[STATS.length];for(int i=0;i<result.length;++i)result[i]=client.I.a(STATS[i]);return result;}
    static String[] delta(int[] before){String name="-";int amount=0;for(int i=0;i<before.length;++i){
        int change=client.I.a(STATS[i])-before[i];if(change!=0){require(name.equals("-"),"multiple actual statistic writes");name=FIELDS[i];amount=change;}}
        return new String[]{name,Integer.toString(amount)};
    }
    static void row(String tag,int c,String[] statistic)throws Exception {
        System.out.printf(Locale.ROOT,"CMP %s %d %016x %016x %016x %016x %016x %016x %08x %08x %08x %08x %d %d %d %d %s %s%n",
            tag,c,Double.doubleToRawLongBits(p.aM),Double.doubleToRawLongBits(p.aN),Double.doubleToRawLongBits(p.aO),
            Double.doubleToRawLongBits(p.aP),Double.doubleToRawLongBits(p.aQ),Double.doubleToRawLongBits(p.aR),
            Float.floatToRawIntBits(p.ak),Float.floatToRawIntBits(p.al),Float.floatToRawIntBits(p.am),
            Float.floatToRawIntBits(field(sn.class,"bk").getFloat(p)),flag(p.aX),flag(p.aY),flag(p.aZ),flag(p.ba),statistic[0],statistic[1]);++rows;
    }
    static void waitWorker(xi writer)throws Exception {
        Object child=field(xi.class,"d").get(writer);long end=System.nanoTime()+5_000_000_000L;
        while(field(cj.class,"a").getBoolean(child)){require(System.nanoTime()<end,"real Stats worker deadline");Thread.sleep(1);}
    }
    static void rounds(){
        int[] floats={0,0x80000000,1,0x80000001,0x3effffff,0x3f000000,0x3f000001,0xbeffffff,
            0xbf000000,0xbf000001,0x4affffff,0x4b000000,0x4effffff,0x4f000000,0xcf000000,0x7f7fffff,
            0x7f800000,0xff800000,0x7fc00123,0x7f800789};
        long[] doubles={0,0x8000000000000000L,1,0x3fdfffffffffffffL,0x3fe0000000000000L,0x3fe0000000000001L,
            0xbfdfffffffffffffL,0xbfe0000000000000L,0xbfe0000000000001L,0x432fffffffffffffL,
            0x4330000000000000L,0x43dfffffffffffffL,0x43e0000000000000L,0xc3e0000000000000L,
            0x7ff0000000000000L,0xfff0000000000000L,0x7ff8000000000123L};
        for(int bits:floats)System.out.printf("JDK F %08x %d%n",bits,Math.round(Float.intBitsToFloat(bits)));
        for(long bits:doubles)System.out.printf("JDK D %016x %d%n",bits,Math.round(Double.longBitsToDouble(bits)));
        long state=0x0123456789abcdefL;
        for(int i=0;i<2048;++i){state^=state<<13;state^=state>>>7;state^=state<<17;
            System.out.printf("JDK F %08x %d%n",(int)state,Math.round(Float.intBitsToFloat((int)state)));
            System.out.printf("JDK D %016x %d%n",state,Math.round(Double.longBitsToDouble(state)));}
    }
    public static void main(String[] args)throws Throwable {
        Path root=Path.of(args[0]);Files.createDirectories(root);
        try(java.io.InputStream stream=Math.class.getResourceAsStream("Math.class")){
            require(stream!=null,"actual JDK Math class resource");Files.copy(stream,root.resolve("jdk-Math.class"));}
        try {
            client=new kq(null,null,null,854,480,false,null);client.k=new gr("TravelPlayer","-");
            Path stats=root.resolve("stats-owner");Files.createDirectories(stats);client.I=new xi(client.k,stats.toFile());waitWorker(client.I);
            world=new fd(new mx(root.toFile(),"Travel",false),"Travel",xa.a(0),23L);chunk=world.c(0,0);
            p=new dc(client,world,client.k,0);p.a=new uo();
            Path jar=Path.of(args[1]).toRealPath();
            for(Class<?> type:new Class<?>[]{kq.class,xi.class,fd.class,lm.class,dc.class,gs.class,ls.class,sn.class,uu.class,uo.class,jl.class,in.class})
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class "+type);
            MethodHandle travel=special(ls.class,"a_",MethodType.methodType(void.class,float.class,float.class));
            for(int c=0;c<23;++c){prepare(c);travel.invoke(p,c==12?.00001f:.7f,c==12?-.00001f:-.4f);row("LIVING",c,new String[]{"-","0"});}
            for(int c=0;c<3;++c){prepare(c==0?11:c==1?0:4);int[] before=totals();p.a_(0,0);row("PLAYER",c,delta(before));}
            for(int c=0;c<3;++c){reset();if(c!=0)cell(8,8,8,c==1?65:1);System.out.printf("CMP LADDER %d %d%n",c,flag(p.p()));++rows;}
            reset();special(ls.class,"R",MethodType.methodType(void.class)).invoke(p);row("JUMP",0,new String[]{"-","0"});
            p.aQ=-9;int[] before=totals();special(gs.class,"R",MethodType.methodType(void.class)).invoke(p);row("JUMP",1,delta(before));
            Method statistic=gs.class.getDeclaredMethod("i",double.class,double.class,double.class);statistic.setAccessible(true);
            for(int c=0;c<11;++c){reset();if(c==0)fluid(8);if(c==1)field(sn.class,"bx").setBoolean(p,true);
                if(c==2||c==3||c==9)cell(8,8,8,65);if(c==4||c==7||c==8)p.aX=true;
                double x=c==5?.25:c==6?.26:c==7?.005:c==8?.004999999:1,y=c==9?Double.POSITIVE_INFINITY:c==3?-.01:c==2?.005:2;
                if(c==10)p.aH=new ju(world,8,8,8,1);
                before=totals();statistic.invoke(p,x,y,0.0);p.aH=null;String[] change=delta(before);
                System.out.printf("CMP STAT %d %s %s%n",c,change[0],change[1]);++rows;
            }
            reset();p.a((fd)null);field(sn.class,"bx").setBoolean(p,true);p.bq=true;
            travel.invoke(p,0f,0f);row("NULLWATER",0,new String[]{"-","0"});p.a(world);
            rounds();System.out.println("Original Player travel: "+rows+" rows");
        } finally {
            if(client!=null)client.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){thread.interrupt();thread.join(1000);}
        }
    }
}
