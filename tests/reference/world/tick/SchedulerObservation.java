import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Unchanged original methods; explicitly authored fields and nullable fixtures. */
public final class SchedulerObservation {
    static Field field(Class<?> owner,String name)throws Exception {
        Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static fd world(Path root,String name)throws Exception {
        mx handler=new mx(root.toFile(),name,false);
        ei info=new ei(23L,"Weather");info.a(0,64,0);info.d(19132);info.e(30);info.f(20);
        handler.a(info);
        return new fd(handler,"Weather",-99L,xa.a(0));
    }
    static void row(fd w,String name)throws Exception {
        System.out.printf("TICK %s %d %d %d %d %d %d%n",name,w.x.f(),w.f,
            field(fd.class,"J").getBoolean(w)?1:0,field(fd.class,"Q").getInt(w),
            w.x.p(),w.x.n());
    }
    @SuppressWarnings("unchecked")
    static TreeSet<qy> ordered(fd w)throws Exception {
        return (TreeSet<qy>)field(fd.class,"E").get(w);
    }
    @SuppressWarnings("unchecked")
    static Set<qy> membership(fd w)throws Exception {
        return (Set<qy>)field(fd.class,"F").get(w);
    }
    static qy add(fd w,int x,int y,long time)throws Exception {
        qy tick=new qy(x,y,0,1);tick.a(time);ordered(w).add(tick);membership(w).add(tick);return tick;
    }
    static void drain(fd w)throws Exception {
        add(w,1,-100,5);
        System.out.printf("DRAIN future %d %d %d%n",w.a(false)?1:0,ordered(w).size(),membership(w).size());
        System.out.printf("DRAIN force %d %d %d%n",w.a(true)?1:0,ordered(w).size(),membership(w).size());
        for(int i=0;i<1001;++i)add(w,i,-100,0);
        boolean pending=w.a(false);
        System.out.printf("DRAIN cap %d %d %d %d%n",pending?1:0,ordered(w).size(),membership(w).size(),ordered(w).first().a);
        System.out.printf("DRAIN rest %d %d %d%n",w.a(false)?1:0,ordered(w).size(),membership(w).size());
        qy tick=add(w,0,64,0);membership(w).remove(tick);
        try {w.a(false);throw new AssertionError("size mismatch returned");}
        catch(IllegalStateException expected) {
            if(!"TickNextTick list out of synch".equals(expected.getMessage()))throw expected;
            System.out.printf("DRAIN mismatch %d %d%n",ordered(w).size(),membership(w).size());
        }
        membership(w).add(tick);
        cl saved=w.v;w.v=null;
        try {w.a(false);throw new AssertionError("null provider returned");}
        catch(NullPointerException expected) {
            System.out.printf("DRAIN absent-provider %d %d%n",ordered(w).size(),membership(w).size());
        } finally {w.v=saved;}
    }
    static final class StopBiome extends RuntimeException {private static final long serialVersionUID=1L;}
    static final class ProbeManager extends xv {
        int x,z;
        ProbeManager(){super();}
        @Override public kd a(yy pair){x=pair.a;z=pair.b;throw new StopBiome();}
    }
    static Object raw(Class<?> type)throws Exception {
        Class<?> unsafe=Class.forName("sun.misc.Unsafe");
        Field f=field(unsafe,"theUnsafe");
        return unsafe.getMethod("allocateInstance",Class.class).invoke(f.get(null),type);
    }
    @SuppressWarnings("unchecked")
    static void spawn(fd w)throws Exception {
        dc player=(dc)raw(dc.class);player.aM=-.25;player.aO=32;
        w.d.add(player);w.a(false,true);
        xv saved=w.t.b;ProbeManager manager=new ProbeManager();w.t.b=manager;
        try {w.l();throw new AssertionError("biome probe returned");}
        catch(StopBiome expected) {
            Set<yy> candidates=(Set<yy>)field(cq.class,"b").get(null);
            yy first=candidates.iterator().next();
            if(first.a!=manager.x||first.b!=manager.z)throw new AssertionError("iterator/lookup");
            System.out.printf("SPAWN prefix %d %d %d%n",candidates.size(),manager.x,manager.z);
        } finally {w.t.b=saved;w.d.clear();}
    }
    static final class StopSpawn extends Error {private static final long serialVersionUID=1L;}
    static final class RecordingRandom extends Random {
        private static final long serialVersionUID=1L;
        int calls,prefixCalls;long prefixState;
        RecordingRandom(long seed){super(seed);}
        @Override public int nextInt(int bound){++calls;return super.nextInt(bound);}
        @Override public float nextFloat(){
            try {prefixState=((AtomicLong)field(Random.class,"seed").get(this)).get();}
            catch(Exception error){throw new AssertionError(error);}
            prefixCalls=calls;return super.nextFloat();
        }
    }
    /** Authored flat-world predicate inputs; original cq and actual wh constructor remain unchanged. */
    static final class FlatWorld extends fd {
        int initialX,initialY,initialZ,trialX,trialY,trialZ;boolean initial;
        String spawnedClass;
        private FlatWorld(){super((wt)null,"",0L,xa.a(0));}
        @Override public boolean h(int x,int y,int z){
            if(!initial){initial=true;initialX=x;initialY=y;initialZ=z;}
            if(y==63){trialX=x;trialY=y+1;trialZ=z;}
            return y==63;
        }
        @Override public ln f(int x,int y,int z){return ln.a;}
        @Override public int a(int x,int y,int z){return y==63?2:0;}
        @Override public int m(int x,int y,int z){return 15;}
        @Override public boolean a(eq box){return true;}
        @Override public List a(sn entity,eq box){return Collections.emptyList();}
        @Override public boolean b(eq box){return false;}
        @Override public boolean b(sn entity){spawnedClass=entity.getClass().getName();throw new StopSpawn();}
    }
    @SuppressWarnings("unchecked")
    static void constructorPrefix(fd base)throws Exception {
        kd biome=base.a().a(new yy(-3,-3));List<bj> entries=biome.a(lk.b);
        int sum=0;for(bj entry:entries)sum+=entry.b;
        int seed=-1,ticket=0;
        for(int candidate=0;candidate<1000000;++candidate){
            Random r=new Random(candidate);int pick=r.nextInt(sum);int left=pick;bj selected=null;
            for(bj entry:entries){selected=entry;if((left-=entry.b)<0)break;}
            if(selected.a!=wh.class)continue;
            int x=r.nextInt(16),y=r.nextInt(128),z=r.nextInt(16);
            if(y!=64)continue;
            x+=r.nextInt(6)-r.nextInt(6);r.nextInt(1);r.nextInt(1);z+=r.nextInt(6)-r.nextInt(6);
            if(x>=0&&x<16&&z>=0&&z<16){seed=candidate;ticket=pick;break;}
        }
        if(seed<0)throw new AssertionError("seed search");
        FlatWorld w=(FlatWorld)raw(FlatWorld.class);w.b=new ArrayList();w.d=new ArrayList();
        dc player=(dc)raw(dc.class);player.aM=-.25;player.aO=32;w.d.add(player);
        w.x=new ei(23L,"Weather");w.x.a(0,64,0);field(fd.class,"t").set(w,base.t);
        RecordingRandom random=new RecordingRandom(seed);w.r=random;
        try{cq.a(w,false,true);throw new AssertionError("constructor prefix returned");}
        catch(StopSpawn expected){
            if(!"wh".equals(w.spawnedClass)||random.prefixCalls!=10)throw new AssertionError("selected Mob/draw prefix");
            System.out.printf("SPAWN constructor %d %d %d %s %d %d %d %d %d %d %d %012x%n",
                seed,sum,ticket,w.spawnedClass,w.initialX,w.initialY,w.initialZ,
                w.trialX,w.trialY,w.trialZ,random.prefixCalls,random.prefixState);
        }
    }
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path root=Path.of(args[0]);Files.createDirectories(root);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> type:new Class<?>[]{fd.class,ei.class,cq.class,yy.class,lk.class,kd.class,
                xv.class,dc.class,qy.class,ok.class,xa.class,mx.class,wh.class,bg.class,ls.class,sn.class,bj.class}) {
            Path origin=Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign original "+type.getName());
            System.out.println("ORIGIN "+type.getName()+" "+origin);
        }
        fd w=world(root,"ordinary");w.r.setSeed(38);field(fd.class,"Q").setInt(w,7);w.a(false,false);
        w.l();row(w,"basic");
        w.x.a(-24001L);w.x.f(20);w.x.e(30);field(fd.class,"J").setBoolean(w,true);field(fd.class,"Q").setInt(w,7);
        w.l();row(w,"negative-sleep");
        field(fd.class,"p").setInt(w,0);long before=w.x.f();
        try {w.l();throw new AssertionError("p0 returned");}
        catch(ArithmeticException expected) {
            System.out.printf("TICK zero-period %d %d%n",before,w.x.f());
        }
        field(fd.class,"p").setInt(w,40);
        fd queue=world(root,"drain");drain(queue);
        spawn(w);
        constructorPrefix(w);
        System.out.println("Original World scheduler: 3 tick states, 6 drain states, 2 spawn prefixes");
    }
}
