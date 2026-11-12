import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Actual unchanged App, Session, xi/kh/vu, World, Chunk and published dc. */
public final class ActiveTickObservation {
    static kq app;static fd world;static lm chunk;static dc p;static lr input;static kv settings;static xi writer;static vu notice;
    static vr[] STATS;
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static void require(boolean v,String s){if(!v)throw new AssertionError(s);}
    static int flag(boolean v){return v?1:0;}
    static void reset()throws Exception{
        Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
        for(int x=6;x<=12;++x)for(int z=6;z<=12;++z)chunk.b[(x<<11)|(z<<7)|7]=1;
        p.bf=p.bo=p.bp=0;field(sn.class,"bk").setFloat(p,0);
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;p.bq=false;
        field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);
        p.bu=5;p.bv=0;p.aG=p.aH=null;p.b(1f,1f);p.e(8.5,8,8.5);p.aP=p.aQ=p.aR=0;p.aX=true;
        p.bl=-9;p.bm=-10;p.bn=-11;p.aS=37;p.aT=-17;p.aU=-11;p.aV=-13;field(sn.class,"c").setBoolean(p,true);p.bt=17;p.bj=0;field(sn.class,"b").setInt(p,1);p.by=3;p.bz=300;
        p.E=p.Y=20;p.V=false;p.an=0;p.aw=p.ax=p.ay=0;p.az=false;p.ag=.5f;p.X=p.W=-1;
        p.H=p.I=0;p.J=-1;p.K=.25f;p.L=.5f;p.M=-1;field(ls.class,"a").setInt(p,0);p.ae=p.aa=p.ad=0;p.af=0;p.av=20;
        p.ak=.125f;p.al=.25f;p.am=.5f;p.ao=10.5;p.ap=8;p.aq=8.5;p.ar=10;p.as=30;
        p.u=p.j=false;p.k=0;p.h=-1;p.i=.125f;field(gs.class,"a").setInt(p,0);p.A=false;p.B=0;p.C=-1;p.z=0;
        p.o=p.p=p.q=p.r=p.s=p.t=0;p.e=p.d;p.v=null;world.B=false;world.q=1;
        p.a=input;input.a();input.a=input.b=0;input.c=input.d=input.e=false;eq.b();bt.b();
        ((Random)field(sn.class,"bs").get(p)).setSeed(38);world.r.setSeed(38);
    }
    static void resetStats(){for(vr stat:STATS)writer.a(stat,-writer.a(stat));}
    static void row(int scenario,boolean complete,int animation)throws Exception{
        System.out.printf("TICK %d %d",scenario,flag(complete));
        double[] doubles={p.aM,p.aN,p.aO,p.aP,p.aQ,p.aR,p.aJ,p.aK,p.aL,p.bl,p.bm,p.bn,p.o,p.p,p.q,p.r,p.s,p.t,p.aW.a,p.aW.b,p.aW.c,p.aW.d,p.aW.e,p.aW.f};
        for(double value:doubles)System.out.printf(" %016x",Double.doubleToRawLongBits(value));
        float[] floats={p.aS,p.aT,p.aU,p.aV,p.bo,p.bj,field(sn.class,"bk").getFloat(p),p.H,p.I,p.J,p.K,p.L,p.M,p.W,p.X,p.aw,p.ax,p.ay,p.ak,p.al,p.am,p.ag,p.h,p.i,p.B,p.C};
        for(float value:floats)System.out.printf(" %08x",Float.floatToRawIntBits(value));
        int[] ints={p.bt,p.by,p.bz,p.bv,p.Y,p.an,field(ls.class,"a").getInt(p),p.ae,p.aa,p.ad,p.k,field(gs.class,"a").getInt(p),p.z,animation};
        for(int value:ints)System.out.print(" "+value);
        boolean[] flags={p.aX,p.aY,p.aZ,field(sn.class,"bx").getBoolean(p),p.be,field(sn.class,"c").getBoolean(p),p.u,p.j,p.az,p.e==p.d};
        for(boolean value:flags)System.out.print(" "+flag(value));
        for(vr stat:STATS)System.out.print(" "+writer.a(stat));
        ny shown=(ny)field(vu.class,"f").get(notice);
        Random random=(Random)field(sn.class,"bs").get(p);long seed=((AtomicLong)field(Random.class,"seed").get(random)).get();
        System.out.printf(" %d %d %d %d %012x%n",shown==null?-1:shown.e,flag(field(vu.class,"i").getBoolean(notice)),
            flag(field(vu.class,"e").get(notice)!=null),flag(field(vu.class,"g").getLong(notice)!=0),seed);
    }
    static void pushout()throws Exception{
        Method method=dc.class.getDeclaredMethod("c",double.class,double.class,double.class);method.setAccessible(true);
        int[] nx={7,9,8,8},nz={8,8,7,9};
        for(int scenario=0;scenario<10;++scenario){
            reset();for(int y=8;y<=9;++y){chunk.b[(8<<11)|(8<<7)|y]=1;for(int i=0;i<4;++i)chunk.b[(nx[i]<<11)|(nz[i]<<7)|y]=1;}
            if(scenario==0)for(int y=8;y<=9;++y)chunk.b[(8<<11)|(8<<7)|y]=0;
            if(scenario>=1&&scenario<=4)for(int y=8;y<=9;++y)chunk.b[(nx[scenario-1]<<11)|(nz[scenario-1]<<7)|y]=0;
            if(scenario==6){chunk.b[(7<<11)|(8<<7)|8]=0;for(int y=8;y<=9;++y)chunk.b[(9<<11)|(8<<7)|y]=0;}
            if(scenario==7||scenario==8)for(int i=0;i<4;++i)for(int y=8;y<=9;++y)chunk.b[(nx[i]<<11)|(nz[i]<<7)|y]=0;
            if(scenario==9){chunk.b[(8<<11)|(8<<7)|8]=0;for(int y=8;y<=9;++y)chunk.b[(7<<11)|(8<<7)|y]=0;}
            p.aP=.125;p.aQ=-.25;p.aR=.375;
            require(Boolean.FALSE.equals(method.invoke(p,scenario==8?8.75:8.25,8.5,scenario==8?8.25:8.75)),"actual push-out false return");
            System.out.printf("PUSH %d %016x %016x %016x%n",scenario,Double.doubleToRawLongBits(p.aP),Double.doubleToRawLongBits(p.aQ),Double.doubleToRawLongBits(p.aR));
        }
    }
    static void workers()throws Exception{
        Object sync=field(xi.class,"d").get(writer);long deadline=System.nanoTime()+10_000_000_000L;
        for(;;){boolean alive=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getClass().getName().equals("vk")||thread.getClass().getName().equals("vj")){alive=true;thread.join(100);}
            if(!alive&&!field(cj.class,"a").getBoolean(sync))return;require(System.nanoTime()<deadline,"actual worker deadline");Thread.sleep(1);
        }
    }
    public static void main(String[] args)throws Exception{
        Locale.setDefault(Locale.ROOT);Path directory=Path.of(args[0]);Files.createDirectories(directory);
        app=new kq(null,null,null,854,480,false,null);
        STATS=new vr[]{jl.k,jl.l,jl.m,jl.o,jl.p,jl.q,jl.u};
        try{
            app.k=new gr("ActivePlayer","-");Path application=directory.resolve("application");Files.createDirectories(application);
            field(net.minecraft.client.Minecraft.class,"Z").set(app,application.toFile());writer=new xi(app.k,application.toFile());app.I=writer;ep.f.a(new kh(app));workers();
            settings=new kv();app.z=settings;notice=app.u;
            world=new fd(new mx(directory.toFile(),"Active",false),"Active",xa.a(0),23L);chunk=world.c(0,0);
            p=new dc(app,world,app.k,0);input=new lr(settings);reset();require(world.b(p)&&p.bF,"actual published Player");
            Path jar=Path.of(args[1]).toRealPath();
            for(Class<?> type:new Class<?>[]{kq.class,dc.class,gs.class,ls.class,sn.class,lr.class,uo.class,kv.class,ix.class,iz.class,aa.class,it.class,fd.class,lm.class,xi.class,cj.class,vu.class,kh.class,ep.class,jl.class,in.class}){
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign actual class "+type);System.out.println("ORIGIN "+type.getName());
            }
            for(int scenario=0;scenario<24;++scenario){
                if(scenario==22)continue; // Native-only unimplemented GUI-close descendant.
                reset();resetStats();iz stack=null;it bench=null;
                if(scenario==0)field(dc.class,"b").set(p,null);
                if(scenario==1)app.I=null;if(scenario==2)app.u=null;
                if(scenario==4){writer.a((vr)ep.f,0);stack=new iz(262,1,0);stack.b=5;p.c.a[0]=stack;}
                if(scenario==5)p.a(17,true);if(scenario==6)p.a(57,true);if(scenario==7||scenario==18)p.a(42,true);
                if(scenario==8){p.j=true;p.k=6;}if(scenario==9)field(gs.class,"a").setInt(p,109);if(scenario==10)field(gs.class,"a").setInt(p,Integer.MAX_VALUE);
                if(scenario==11||scenario==12){p.u=true;field(gs.class,"a").setInt(p,99);if(scenario==11)world.B=true;}
                if(scenario==13)p.Y=0;
                if(scenario==14){p.b(.6f,1.8f);p.bf=1.62f;p.e(8.5,9.62,8.5);}
                if(scenario==15)p.e(Double.longBitsToDouble(0x7ff8123456789abcL),8,Double.longBitsToDouble(0xfff8456789abcdefL));
                if(scenario==16){p.B=.025f;p.z=1;}if(scenario==17){p.B=-0.0f;p.z=Integer.MAX_VALUE;}if(scenario==18)p.bo=-.1f;
                if(scenario==19)p.a=null;if(scenario==20){p.an=2;p.aS=350;p.aT=10;}
                if(scenario==21){chunk.b[(8<<11)|(8<<7)|7]=58;bench=new it(p.c,world,8,7,8);p.e=bench;}
                if(scenario==23)app.z=null;
                boolean expected=!(scenario==0||scenario==1||scenario==2||scenario==12||scenario==19),complete=false;
                long before=System.currentTimeMillis();
                try{p.w_();complete=true;}catch(NullPointerException failure){
                    if(expected)throw failure;StackTraceElement top=failure.getStackTrace()[0];
                    require(top.getClassName().equals(scenario==12?"gs":"dc")&&top.getMethodName().equals(scenario==12?"am":"o"),"actual failure frame "+scenario);
                }
                long after=System.currentTimeMillis();field(dc.class,"b").set(p,app);app.I=writer;app.u=notice;app.z=settings;
                require(complete==expected,"completion differs "+scenario);
                if(scenario==3){long shown=field(vu.class,"g").getLong(notice);require(shown>=before-2500&&shown<=after-2500,"actual reminder clock range");}
                row(scenario,complete,stack==null?-1:stack.b);p.c.a[0]=null;if(bench!=null)p.e=p.d;
            }
            reset();resetStats();p.a(17,true);
            for(int tick=0;tick<16;++tick){eq.b();bt.b();p.w_();row(100+tick,true,-1);}
            pushout();
            System.out.println("Original Player active tick: 39 states");
        }finally{
            if(writer!=null)workers();app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){thread.interrupt();thread.join(2000);require(!thread.isAlive(),"timer retired");}
        }
    }
}
