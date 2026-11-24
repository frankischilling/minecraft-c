import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored observations of actual unchanged World/Chunk/LocalPlayer owners. */
public final class MovementObservation {
    static Field field(Class<?> owner,String name)throws Exception {
        Field value=owner.getDeclaredField(name);value.setAccessible(true);return value;
    }
    static void reset(dc p)throws Exception {
        p.bf=p.bo=p.bp=0;field(sn.class,"bk").setFloat(p,0);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;
        p.bq=false;field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.b(1.0f,1.0f);p.e(8.5,8,8.5);p.aP=2;p.aQ=3;p.aR=4;
        p.bj=0;field(sn.class,"b").setInt(p,0);p.u=false;p.a.e=false;
    }
    static void cell(lm chunk,int x,int y,int z,int id){chunk.b[(x<<11)|(z<<7)|y]=(byte)id;}
    static int flag(boolean value){return value?1:0;}
    static long state(Random random)throws Exception{return ((AtomicLong)field(Random.class,"seed").get(random)).get();}
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path root=Path.of(args[0]);Files.createDirectories(root);
        fd world=new fd(new mx(root.toFile(),"Movement",false),"Movement",xa.a(0),23L);
        lm chunk=world.c(0,0);
        dc p=new dc(null,world,new gr(null,null),0);p.a=new uo();
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> actual:new Class<?>[]{fd.class,lm.class,dc.class,gs.class,ls.class,sn.class,ct.class,uo.class,uu.class}) {
            Path origin=Path.of(actual.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign runtime class "+actual.getName());
            System.out.println("ORIGIN "+actual.getName()+" "+origin);
        }
        ct[] sounds={uu.m[1].by,uu.m[20].by,uu.m[12].by};
        for(int i=0;i<sounds.length;++i) {
            ct sound=sounds[i];String a=sound.a(),aa=sound.a(),d=sound.d(),dd=sound.d();
            System.out.printf("CMP SOUND %d a %s %d %08x %08x%n",i,a,flag(a==aa),Float.floatToRawIntBits(sound.b()),Float.floatToRawIntBits(sound.c()));
            System.out.printf("CMP SOUND %d d %s %d %08x %08x%n",i,d,flag(d==dd),Float.floatToRawIntBits(sound.b()),Float.floatToRawIntBits(sound.c()));
        }
        for(int scenario=0;scenario<6;++scenario) {
            Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);reset(p);world.r.setSeed(38);
            int support=scenario==0?0:scenario==4?8:scenario==5?73:1;
            if(scenario==1||scenario==2||scenario==5)for(int x=6;x<=12;++x)for(int z=6;z<=12;++z)cell(chunk,x,7,z,1);
            if(scenario!=3)cell(chunk,9,7,8,support);
            if(scenario==2)cell(chunk,9,8,8,78);
            if(scenario==3)cell(chunk,9,6,8,85);
            p.b(scenario==0?.25:1.0,scenario==0?.5:-.25,scenario==0?-.25:0.0);
            System.out.printf("CMP MOVE %d %016x %016x %016x %08x %d %d %d %d %d %08x %d %012x %d%n",scenario,
                Double.doubleToRawLongBits(p.aM),Double.doubleToRawLongBits(p.aN),Double.doubleToRawLongBits(p.aO),
                Float.floatToRawIntBits(p.bj),field(sn.class,"b").getInt(p),flag(p.aX),flag(p.aY),flag(p.aZ),flag(p.ba),
                Float.floatToRawIntBits(field(sn.class,"bk").getFloat(p)),p.bv,state(world.r),chunk.b[(9<<11)|(8<<7)|7]&255);
        }
        reset(p);
        for(int scenario=0;scenario<4;++scenario) {
            p.z=scenario-1;p.A=false;p.S();
            System.out.printf("CMP PORTAL %d %d %d%n",scenario,p.z,flag(p.A));
        }
        for(int scenario=0;scenario<3;++scenario) {
            field(sn.class,"bk").setFloat(p,scenario*.75f);p.a(0.0,true);
            System.out.printf("CMP FALL %d %08x%n",scenario,Float.floatToRawIntBits(field(sn.class,"bk").getFloat(p)));
        }
        for(int scenario=0;scenario<2;++scenario) {
            p.av=19;p.Y=scenario==0?0:20;p.bb=false;
            boolean ordinary=p.a((sn)null,scenario==0?7:0);
            System.out.printf("CMP DAMAGE %d %d %d %d %d%n",scenario,flag(ordinary),p.av,p.Y,flag(p.bb));
        }
        int[] roundInputs={0x40000000,0x400051eb,0x400051ec,0x400051ed,0x4affffff,0x4b000000,0x4b000001,
            0x5bffffff,0x5c000000,0x5dffffff,0x7f7fffff,0x7f800000};
        for(int i=0;i<roundInputs.length;++i)
            System.out.printf("JDK ROUND %d %08x %d%n",i,roundInputs[i],(int)Math.round((double)Float.intBitsToFloat(roundInputs[i])*100.0));
        System.out.println("Original Player movement: 21 rows");
    }
}
