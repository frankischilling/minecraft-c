import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Genuine unchanged fd/ei/provider/Random owners; authored scenario inputs. */
public final class WeatherObservation {
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static fd create(Path root,int slot,int dimension,boolean rain,boolean thunder)throws Exception {
        mx handler=new mx(root.toFile(),"world"+slot,false);
        ei info=new ei(23L,"Weather");info.a(0,64,0);info.d(19132);info.e(30);info.f(20);info.a(thunder);info.b(rain);
        handler.a(info);
        fd world=new fd(handler,"Weather",-99L,xa.a(dimension));
        if(slot<4)System.out.printf("INIT %d %d %d %08x %08x %08x %08x%n",slot,rain?1:0,thunder?1:0,Float.floatToRawIntBits(world.i),Float.floatToRawIntBits(world.j),Float.floatToRawIntBits(world.k),Float.floatToRawIntBits(world.l));
        return world;
    }
    static void input(fd w,int scene)throws Exception {
        if(scene>=45 && scene!=45)return;
        w.m=scene%3==0?3:scene%3==1?0:Integer.MAX_VALUE;w.n=123;w.B=(scene&1)!=0;
        w.i=-11;w.k=-13;w.j=.25f;w.l=.75f;
        boolean rain=(scene&1)!=0,thunder=(scene&2)!=0;int rainTime=20,thunderTime=20;
        if(scene<16)switch(scene/4){case 0:rainTime=thunderTime=0;break;case 1:rainTime=thunderTime=1;break;case 2:rainTime=thunderTime=2;break;default:rainTime=Integer.MIN_VALUE;thunderTime=Integer.MAX_VALUE;break;}
        int[] edges={0,0x80000000,1,0x80000001,0x00800000,0x3f7fffff,0x3f800000,0x3fc00000,0xbf000000,0x7f800000,0xff800000,0x7fc12345,0xffc54321,0x7f812345,0xff854321,0x7f7fffff};
        if(scene>=16 && scene<32){int i=scene-16;w.j=Float.intBitsToFloat(edges[i]);w.l=Float.intBitsToFloat(edges[(i+5)%edges.length]);w.m=Integer.MIN_VALUE;}
        if(scene>=32 && scene<36){w.j=Float.intBitsToFloat(scene<34?0x7fc12345:0x7f812345);w.l=Float.intBitsToFloat(scene<34?0xffc54321:0xff854321);}
        if(scene==32){w.j=Float.intBitsToFloat(0x3a0003e5);w.l=Float.intBitsToFloat(0x3c300000);rain=true;thunder=false;}
        if(scene==33){w.j=Float.intBitsToFloat(0x3c300000);w.l=Float.intBitsToFloat(0x3a0003e5);rain=false;thunder=true;}
        if(scene>=39 && scene<=44){rainTime=0;thunderTime=scene>=43?2:0;rain=scene==44;thunder=scene==42;w.m=3;}
        if(scene==45){rainTime=thunderTime=2;rain=thunder=false;w.m=2;}
        w.x.f(rainTime);w.x.e(thunderTime);w.x.b(rain);w.x.a(thunder);w.r.setSeed(38);
        if(scene==35)w.r.nextGaussian();
    }
    static void row(fd w,int scene,boolean complete)throws Exception {
        long random=((AtomicLong)field(Random.class,"seed").get(w.r)).get();int continuation=w.r.nextInt();
        System.out.printf("WEATHER %d %d %d %d %d %d %d %d %08x %08x %08x %08x %012x %d %016x %d%n",scene,complete?1:0,w.m,w.n,w.x.n(),w.x.p(),w.x.m()?1:0,w.x.o()?1:0,Float.floatToRawIntBits(w.i),Float.floatToRawIntBits(w.j),Float.floatToRawIntBits(w.k),Float.floatToRawIntBits(w.l),random,field(Random.class,"haveNextNextGaussian").getBoolean(w.r)?1:0,Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(w.r)),continuation);
    }
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> c:new Class<?>[]{fd.class,ei.class,xa.class,rh.class,wd.class,ay.class,mx.class,fm.class,wt.class}) {
            Path origin=Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign original class "+c.getName());
            System.out.println("ORIGIN "+c.getName()+" "+origin);
        }
        fd[] worlds=new fd[5];for(int i=0;i<5;++i)worlds[i]=create(root,i,i==4?-1:0,(i&1)!=0,(i&2)!=0);
        for(int scene=0;scene<64;++scene){
            fd w=worlds[scene>=36&&scene<=38?4:0];input(w,scene);
            xa provider=w.t;ei info=w.x;Random random=w.r;
            if(scene==36||scene==38||scene==40)w.x=null;
            if(scene==37||scene==38||(scene>=41&&scene<=44))w.r=null;
            if(scene==39)field(fd.class,"t").set(w,null);
            boolean complete=true;
            try{w.m();}catch(NullPointerException expected){if(scene<39||scene>=45)throw expected;complete=false;}
            finally{field(fd.class,"t").set(w,provider);w.x=info;w.r=random;}
            if(complete!=(scene<39||scene>=45))throw new AssertionError("weather completion "+scene);
            row(w,scene,complete);
        }
        System.out.println("Original World weather: 64 states and 4 constructor states");
    }
}
