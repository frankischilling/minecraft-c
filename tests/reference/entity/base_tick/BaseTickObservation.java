import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Actual unchanged World, Chunk, Session, LocalPlayer and WorldAccess interface. */
public final class BaseTickObservation {
    static Field field(Class<?> owner,String name)throws Exception {Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static int flag(boolean value){return value?1:0;}
    static final class Observer implements pm {
        dc player;int scenario,event,sounds,particles;boolean failSound,shrink;int failParticle=-1;
        public void a(String name,double x,double y,double z,float volume,float pitch) {
            System.out.printf("EVENT %d %d S %s %016x %016x %016x %08x %08x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),Float.floatToRawIntBits(volume),Float.floatToRawIntBits(pitch));
            ++sounds;
            if(shrink){player.b(.25f,1.0f);player.aP=1.25;player.aQ=-.5;player.aR=2.0;}
            if(failSound)throw new IllegalStateException("authored reached splash sound failure");
        }
        public void a(String name,double x,double y,double z,double vx,double vy,double vz) {
            System.out.printf("EVENT %d %d P %s %016x %016x %016x %016x %016x %016x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),Double.doubleToRawLongBits(vx),Double.doubleToRawLongBits(vy),Double.doubleToRawLongBits(vz));
            if(particles++==failParticle)throw new IllegalStateException("authored reached particle failure");
        }
        public void a(int x,int y,int z){throw new AssertionError("unexpected cell callback");}
        public void b(int a,int b,int c,int d,int e,int f){throw new AssertionError("unexpected range callback");}
        public void a(sn e){throw new AssertionError("unexpected entity add");}
        public void b(sn e){throw new AssertionError("unexpected entity remove");}
        public void e(){throw new AssertionError("unexpected renderer callback");}
        public void a(String s,int x,int y,int z){throw new AssertionError("unexpected music callback");}
        public void a(int x,int y,int z,ow e){throw new AssertionError("unexpected tile callback");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static ma record(dc p)throws Exception{return (ma)((Map<?,?>)field(ud.class,"b").get(p.bD)).get(0);}
    static void reset(fd world,lm chunk,dc p,Observer o,int scenario)throws Exception {
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
        p.bf=p.bo=p.bp=0;field(sn.class,"bk").setFloat(p,1.25f);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;p.bq=false;
        field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.b(1.0f,1.0f);p.e(8.5,8,8.5);p.aP=2;p.aQ=3;p.aR=4;
        field(sn.class,"c").setBoolean(p,true);p.bt=17;p.bj=.75f;p.bi=-77;
        p.aJ=-9;p.aK=-10;p.aL=-11;p.aU=-13;p.aV=-14;p.aS=25;p.aT=-17;
        field(sn.class,"bw").setInt(p,300);p.bz=123;p.by=3;
        p.W=-31;p.X=.25f;p.H=.375f;p.I=-32;p.L=.5f;p.M=-33;p.af=-34;p.ag=.75f;p.ae=3;p.aa=4;p.ad=0;p.Y=20;p.av=19;
        field(ls.class,"a").setInt(p,0);p.u=false;
        ma record=record(p);record.a(Byte.valueOf((byte)0));record.a(false);field(ud.class,"c").setBoolean(p.bD,false);
        world.B=false;((Random)field(sn.class,"bs").get(p)).setSeed(38);eq.b();bt.b();
        o.player=p;o.scenario=scenario;o.event=o.sounds=o.particles=0;o.failSound=o.shrink=false;o.failParticle=-1;
    }
    static void fluid(lm chunk,int id) {for(int x=7;x<=10;++x)for(int z=7;z<=10;++z)for(int y=7;y<=10;++y)chunk.b[(x<<11)|(z<<7)|y]=(byte)id;}
    static void row(dc p,Observer o,boolean complete)throws Exception {
        ma record=record(p);Random random=(Random)field(sn.class,"bs").get(p);
        long seed=((AtomicLong)field(Random.class,"seed").get(random)).get();
        System.out.printf("STATE %d %d %d %08x %016x %016x %016x %08x %08x",o.scenario,flag(complete),p.bt,Float.floatToRawIntBits(p.bi),
            Double.doubleToRawLongBits(p.aJ),Double.doubleToRawLongBits(p.aK),Double.doubleToRawLongBits(p.aL),Float.floatToRawIntBits(p.aU),Float.floatToRawIntBits(p.aV));
        System.out.printf(" %d %d %d %d %d %d %d %d",flag(field(sn.class,"bx").getBoolean(p)),p.bv,p.bz,flag(field(sn.class,"c").getBoolean(p)),((Byte)record.b()).intValue(),
            flag(field(ma.class,"d").getBoolean(record)),flag(field(ud.class,"c").getBoolean(p.bD)),flag(p.be));
        System.out.printf(" %08x %d %08x %08x %d %d %d %d %d %d",Float.floatToRawIntBits(p.W),field(ls.class,"a").getInt(p),Float.floatToRawIntBits(p.af),Float.floatToRawIntBits(p.M),p.ae,p.aa,p.by,p.ad,p.av,p.Y);
        System.out.printf(" %08x %08x %012x %016x %d %d %d",Float.floatToRawIntBits(p.I),Float.floatToRawIntBits(field(sn.class,"bk").getFloat(p)),seed,
            Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(random)),flag(field(Random.class,"haveNextNextGaussian").getBoolean(random)),o.sounds,o.particles);
        System.out.printf(" %016x %016x %016x %08x %d %d%n",Double.doubleToRawLongBits(p.aP),Double.doubleToRawLongBits(p.aQ),Double.doubleToRawLongBits(p.aR),Float.floatToRawIntBits(p.bg),field(eq.class,"h").getInt(null),field(bt.class,"e").getInt(null));
    }
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        fd world=new fd(new mx(root.toFile(),"BaseTick",false),"BaseTick",xa.a(0),23L);lm chunk=world.c(0,0);
        if(jl.x==null)throw new AssertionError("actual prepared statistics class");
        dc p=new dc(null,world,new gr(null,null),0);Observer o=new Observer();world.a(o);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> c:new Class<?>[]{dc.class,gs.class,ls.class,sn.class,fd.class,lm.class,ud.class,ma.class,pm.class}) {
            Path origin=Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign game class "+c.getName());
            System.out.println("ORIGIN "+c.getName()+" "+origin);
        }
        for(int scenario=0;scenario<31;++scenario) {
            reset(world,chunk,p,o,scenario);
            if(scenario==1)field(sn.class,"c").setBoolean(p,false);
            if(scenario==2||scenario==3||scenario==4||scenario==15||scenario==16||scenario==17||scenario==18||scenario==19) {
                fluid(chunk,8);field(sn.class,"c").setBoolean(p,scenario==2);field(sn.class,"bx").setBoolean(p,scenario==4||scenario==15||scenario==16);
            }
            if(scenario==5){field(sn.class,"bx").setBoolean(p,true);field(sn.class,"c").setBoolean(p,false);}
            if(scenario==6)p.bv=7;
            if(scenario==7)p.bv=20;
            if(scenario==8){p.bv=20;p.Y=0;}
            if(scenario==9){p.bv=3;field(sn.class,"bC").setBoolean(p,true);}
            if(scenario==10){p.bv=20;world.B=true;}
            if(scenario==11||scenario==12){fluid(chunk,10);field(sn.class,"bC").setBoolean(p,scenario==12);}
            if(scenario==13)p.e(8.5,-65,8.5);
            if(scenario==14)chunk.b[(8<<11)|(8<<7)|8]=1;
            if(scenario==15||scenario==16){p.bz=-19;if(scenario==16)o.failParticle=0;}
            if(scenario==17)o.failSound=true;
            if(scenario==18)o.failParticle=0;
            if(scenario==19)o.shrink=true;
            if(scenario==20)field(ls.class,"a").setInt(p,2000);
            if(scenario==21)field(ls.class,"a").setInt(p,Integer.MAX_VALUE);
            if(scenario==22)field(ls.class,"a").setInt(p,Integer.MIN_VALUE);
            if(scenario==23){p.Y=0;p.ad=19;}
            if(scenario>=24&&scenario<=28){fluid(chunk,8);field(sn.class,"c").setBoolean(p,false);}
            if(scenario==24){p.aP=Double.longBitsToDouble(0x7ff8123456789abcL);p.aQ=Double.longBitsToDouble(0xfff8456789abcdefL);p.aR=Double.longBitsToDouble(0x7ff876543210abcdL);}
            if(scenario==25){p.aP=Double.POSITIVE_INFINITY;p.aQ=Double.NEGATIVE_INFINITY;p.aR=Double.POSITIVE_INFINITY;}
            if(scenario==26)p.bg=Float.intBitsToFloat(0x7fc12345);
            if(scenario==27)p.bg=-.5f;
            if(scenario==28)p.aP=p.aQ=p.aR=-0.0;
            if(scenario==29){chunk.b[(8<<11)|(8<<7)|8]=8;chunk.b[(9<<11)|(8<<7)|7]=8;field(sn.class,"c").setBoolean(p,false);field(sn.class,"bx").setBoolean(p,true);}
            boolean expected=!(scenario==7||scenario==11||scenario==13||scenario==14||scenario==15||scenario==16||scenario==17||scenario==18||scenario==30),complete=false;
            fd savedWorld=p.aI;if(scenario==30)p.aI=null;
            try{p.U();complete=true;}catch(NullPointerException|IllegalStateException failure){if(expected)throw failure;}
            finally{p.aI=savedWorld;}
            if(complete!=expected)throw new AssertionError("completion differs "+scenario);
            row(p,o,complete);
        }
        System.out.println("Original Player base tick: 31 states");
    }
}
