import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Unchanged genuine hl, World, Chunk and ItemStack; authored pm observer. */
public final class ItemTickObservation {
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static int flag(boolean b){return b?1:0;}
    static final class Observer implements pm {
        hl item;int scenario,event,sounds,particles;boolean failSound,failParticle,mutate;
        public void a(String name,double x,double y,double z,float volume,float pitch){
            System.out.printf("EVENT %d %d S %s %016x %016x %016x %08x %08x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),
                Float.floatToRawIntBits(volume),Float.floatToRawIntBits(pitch));sounds++;
            if(mutate){item.e(8.25,8.5,8.75);item.aP=1.25;item.aQ=-.5;item.aR=2;}
            if(failSound)throw new IllegalStateException("authored reached Item sound failure");
        }
        public void a(String name,double x,double y,double z,double vx,double vy,double vz){
            System.out.printf("EVENT %d %d P %s %016x %016x %016x %016x %016x %016x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),
                Double.doubleToRawLongBits(vx),Double.doubleToRawLongBits(vy),Double.doubleToRawLongBits(vz));particles++;
            if(failParticle)throw new IllegalStateException("authored reached Item particle failure");
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
    static ma record(hl p)throws Exception{return (ma)((Map<?,?>)field(ud.class,"b").get(p.bD)).get(0);}
    static void reset(fd w,lm c,hl p,Observer o,int scenario)throws Exception{
        Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;p.bq=false;
        field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bp=p.bo=0;field(sn.class,"bk").setFloat(p,0);p.bf=.125f;
        field(sn.class,"c").setBoolean(p,true);p.bt=17;p.bj=.75f;p.bi=-77;
        p.aJ=-9;p.aK=-10;p.aL=-11;p.aU=-13;p.aV=-14;p.aS=25;p.aT=-17;
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.b=p.c=0;field(hl.class,"e").setInt(p,0);field(hl.class,"f").setInt(p,5);p.d=.25f;
        p.b(.25f,.25f);p.e(8.5,8.5,8.5);p.aP=.25;p.aQ=0;p.aR=-.125;
        ma r=record(p);r.a(Byte.valueOf((byte)0));r.a(false);field(ud.class,"c").setBoolean(p.bD,false);
        w.B=false;((Random)field(sn.class,"bs").get(p)).setSeed(38);eq.b();bt.b();
        o.item=p;o.scenario=scenario;o.event=o.sounds=o.particles=0;o.failSound=o.failParticle=o.mutate=false;
    }
    static void fluid(lm c,int id){for(int x=7;x<=10;x++)for(int z=7;z<=10;z++)for(int y=7;y<=10;y++)c.b[(x<<11)|(z<<7)|y]=(byte)id;}
    static void row(hl p,Observer o,boolean complete)throws Exception{
        ma r=record(p);Random random=(Random)field(sn.class,"bs").get(p);
        System.out.printf("STATE %d %d %d %d %d %d %d %08x",o.scenario,flag(complete),p.bt,p.b,field(hl.class,"e").getInt(p),p.c,field(hl.class,"f").getInt(p),Float.floatToRawIntBits(p.d));
        for(double d:new double[]{p.aM,p.aN,p.aO,p.aP,p.aQ,p.aR,p.aJ,p.aK,p.aL})System.out.printf(" %016x",Double.doubleToRawLongBits(d));
        for(float f:new float[]{p.aU,p.aV,p.aS,p.aT,p.bf,p.bg,p.bh,p.bi,p.bj,field(sn.class,"bk").getFloat(p),p.bo})System.out.printf(" %08x",Float.floatToRawIntBits(f));
        for(double d:new double[]{p.aW.a,p.aW.b,p.aW.c,p.aW.d,p.aW.e,p.aW.f})System.out.printf(" %016x",Double.doubleToRawLongBits(d));
        System.out.printf(" %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",flag(p.aX),flag(p.aY),flag(p.aZ),flag(p.ba),flag(p.bb),flag(p.bc),flag(p.bd),flag(p.be),flag(field(sn.class,"bx").getBoolean(p)),p.bv,flag(field(sn.class,"c").getBoolean(p)),((Byte)r.b()).intValue(),flag(field(ma.class,"d").getBoolean(r)),flag(field(ud.class,"c").getBoolean(p.bD)),field(eq.class,"h").getInt(null),field(bt.class,"e").getInt(null));
        System.out.printf(" %012x %016x %d %d %d%n",((AtomicLong)field(Random.class,"seed").get(random)).get(),Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(random)),flag(field(Random.class,"haveNextNextGaussian").getBoolean(random)),o.sounds,o.particles);
    }
    public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        fd world=new fd(new mx(root.toFile(),"ItemTick",false),"ItemTick",xa.a(0),23L);lm chunk=world.c(0,0);
        hl item=new hl(world,8.5,8.5,8.5,new iz(1,1,0)),nullWorld=new hl(null,8.5,8.5,8.5,new iz(1,1,0));
        Observer o=new Observer();world.a(o);Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> c:new Class<?>[]{hl.class,sn.class,fd.class,lm.class,iz.class,ud.class,ma.class,pm.class,uu.class,in.class,eq.class,bt.class}){
            Path origin=Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign game class "+c.getName());
            System.out.println("ORIGIN "+c.getName()+" "+origin);
        }
        for(int scenario=0;scenario<37;scenario++){
            hl p=scenario==30?nullWorld:item;reset(world,chunk,p,o,scenario);
            if(scenario==1||scenario==2){for(int x=6;x<=12;x++)for(int z=6;z<=12;z++)chunk.b[(x<<11)|(z<<7)|7]=(byte)(scenario==1?1:79);p.e(8.5,8.125,8.5);}
            if(scenario==3)p.c=3;if(scenario==4)p.c=-1;if(scenario==5)p.b=5999;
            if(scenario==6)p.b=Integer.MAX_VALUE;if(scenario==7)field(hl.class,"e").setInt(p,Integer.MAX_VALUE);
            if(scenario==8)p.be=true;if(scenario==9){field(sn.class,"bx").setBoolean(p,true);field(sn.class,"c").setBoolean(p,false);}
            if(scenario==10){p.bv=20;field(hl.class,"f").setInt(p,1);}
            if(scenario==11){p.bv=3;field(sn.class,"bC").setBoolean(p,true);}
            if(scenario==12||scenario==13||scenario==19||scenario==20){fluid(chunk,10);field(sn.class,"bC").setBoolean(p,scenario==13);}
            if(scenario>=14&&scenario<=18){fluid(chunk,8);field(sn.class,"c").setBoolean(p,scenario==14);field(sn.class,"bx").setBoolean(p,scenario==16);}
            if(scenario==17||scenario==19)o.failSound=true;if(scenario==18)o.failParticle=true;if(scenario==20)o.mutate=true;
            if(scenario>=21&&scenario<=28){
                chunk.b[(8<<11)|(8<<7)|8]=1;int[] dx={-1,1,0,0,0,0},dy={0,0,-1,1,0,0},dz={0,0,0,0,-1,1};
                for(int i=0;i<6;i++)if(scenario!=21&&!(scenario>=23&&i==scenario-23))chunk.b[((8+dx[i])<<11)|((8+dz[i])<<7)|(8+dy[i])]=1;
            }
            if(scenario==29)p.e(8.5,-65,8.5);if(scenario==31)p.bt=Integer.MAX_VALUE;
            if(scenario>=32&&scenario<=35){p.bq=true;
                if(scenario==32){p.aP=Double.longBitsToDouble(0x7ff8123456789abcL);p.aQ=Double.longBitsToDouble(0xfff8456789abcdefL);p.aR=Double.longBitsToDouble(0x7ff876543210abcdL);}
                if(scenario==33){p.aP=Double.longBitsToDouble(0x7ff0000000000001L);p.aQ=Double.longBitsToDouble(0xfff0000000000001L);p.aR=Double.longBitsToDouble(0x7ff0000000000002L);}
                if(scenario==34){p.aP=Double.POSITIVE_INFINITY;p.aQ=Double.NEGATIVE_INFINITY;p.aR=Double.POSITIVE_INFINITY;}
                if(scenario==35){p.aP=-0.0;p.aQ=0.0;p.aR=-0.0;}
            }
            if(scenario==36){fluid(chunk,8);field(sn.class,"c").setBoolean(p,false);o.mutate=true;}
            boolean complete=true;
            try{p.w_();}catch(IllegalStateException expected){if(!(o.failSound||o.failParticle))throw expected;complete=false;}
            catch(NullPointerException expected){if(scenario!=30)throw expected;complete=false;}
            row(p,o,complete);
        }
        System.out.println("Original Item tick: 37 states");
    }
}
