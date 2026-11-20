import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;
/** Actual unchanged World/Item/Chunk methods on genuine constructed owners. */
public final class WorldTickObservation {
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static final class Observer implements pm {
        hl item;int scenario,event,sounds,particles;boolean tileMode;int tileScene,tileEvent;boolean failSound,failParticle,mutate;
        public void a(String name,double x,double y,double z,float volume,float pitch){
            System.out.printf("EVENT %d %d S %s %016x %016x %016x %08x %08x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),
                Float.floatToRawIntBits(volume),Float.floatToRawIntBits(pitch));sounds++;
            if(mutate){item.e(8.25,8.5,8.75);item.aP=1.25;item.aQ=-.5;item.aR=2;}
            if(failSound)throw new IllegalStateException("authored reached Item sound failure");
        }
        public void a(String name,double x,double y,double z,double vx,double vy,double vz){
            if(tileMode){System.out.printf("TP %d %d %s %016x %016x %016x%n",tileScene,tileEvent++,name,Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z));return;}
            System.out.printf("EVENT %d %d P %s %016x %016x %016x %016x %016x %016x%n",scenario,event++,name,
                Double.doubleToRawLongBits(x),Double.doubleToRawLongBits(y),Double.doubleToRawLongBits(z),
                Double.doubleToRawLongBits(vx),Double.doubleToRawLongBits(vy),Double.doubleToRawLongBits(vz));particles++;
            if(failParticle)throw new IllegalStateException("authored reached Item particle failure");
        }
        public void a(int x,int y,int z){throw new AssertionError("unexpected cell callback");}
        public void b(int a,int b,int c,int d,int e,int f){throw new AssertionError("unexpected range callback");}
        fd world;hl second;boolean failRemoved,appendRemoved;int removed;
        public void a(sn e){}
        public void b(sn e){
            System.out.printf("EVENT %d %d R %d%n",scenario,removed++,e==second?1:0);
            if(appendRemoved&&removed==1)world.b(Collections.singletonList(second));
            if(failRemoved)throw new IllegalStateException("authored removal failure");
        }
        public void e(){throw new AssertionError("unexpected renderer callback");}
        public void a(String s,int x,int y,int z){throw new AssertionError("unexpected music callback");}
        public void a(int x,int y,int z,ow e){throw new AssertionError("unexpected tile callback");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }

    static void clear(fd w,lm c)throws Exception {
        w.b.clear();((List<?>)field(fd.class,"D").get(w)).clear();w.e.clear();w.c.clear();
        ((List<?>)field(fd.class,"G").get(w)).clear();
        for(List<?> list:c.m)list.clear();
    }
    static void reset(hl p)throws Exception {
        p.bf=0;p.bo=0;field(sn.class,"bk").setFloat(p,0);p.bp=0;
        p.aX=p.aY=p.aZ=p.ba=p.bb=p.bc=p.be=false;p.bd=true;field(sn.class,"bx").setBoolean(p,false);field(sn.class,"bC").setBoolean(p,false);p.bu=5;p.bv=0;
        p.aG=p.aH=null;p.b(1f,1f);p.e(8.5,8.5,8.5);p.bf=.125f;
        p.bl=-11;p.bm=-12;p.bn=-13;p.aU=-14;p.aV=-15;p.aS=25;p.aT=-17;
        p.bt=17;p.b=0;field(hl.class,"e").setInt(p,0);p.c=0;p.bF=false;p.bG=p.bH=p.bI=0;
        p.aP=p.aQ=p.aR=0;p.bq=true;((Random)field(sn.class,"bs").get(p)).setSeed(38);
    }
    static void row(fd w,lm c,hl p,Observer o,boolean complete)throws Exception {
        System.out.printf("STATE %d %d %d %d %d %d %d %d %d %d %d",o.scenario,complete?1:0,w.b.size(),((List<?>)field(fd.class,"D").get(w)).size(),w.e.size(),field(fd.class,"L").getBoolean(w)?1:0,p.be?1:0,p.bF?1:0,p.bG,p.bH,p.bI);
        for(List<?> list:c.m)System.out.printf(" %d",list.size());
        for(double v:new double[]{p.aM,p.aN,p.aO,p.bl,p.bm,p.bn})System.out.printf(" %016x",Double.doubleToRawLongBits(v));
        for(float v:new float[]{p.aS,p.aT,p.aU,p.aV})System.out.printf(" %08x",Float.floatToRawIntBits(v));
        System.out.printf(" %d %d %d%n",p.bt,p.b,o.removed);
    }
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);Path root=Path.of(args[0]);Files.createDirectories(root);
        fd w=new fd(new mx(root.toFile(),"WorldTick",false),"WorldTick",xa.a(0),23L);
        lm c=w.c(0,0);Arrays.fill(c.b,(byte)0);Arrays.fill(c.e.a,(byte)0);
        hl p=new hl(w,8.5,8.5,8.5,new iz(1,1,0)),q=new hl(w,8.5,8.5,8.5,new iz(1,1,0));
        Observer o=new Observer();o.world=w;o.second=q;w.a(o);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> k:new Class<?>[]{hl.class,sn.class,fd.class,lm.class,li.class,iz.class,pm.class,ow.class,js.class,cy.class,uk.class,ju.class,yl.class,dc.class,gs.class,ls.class,gr.class}){
            Path origin=Path.of(k.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
            if(!origin.equals(jar))throw new AssertionError("foreign original class "+k.getName());
            System.out.println("ORIGIN "+k.getName()+" "+origin);
        }
        for(int scene=0;scene<21;++scene) {
            if(scene==7)continue; // Native reached pre-existing Chunk diagnostic dependency.
            clear(w,c);reset(p);o.scenario=scene;o.removed=0;o.failRemoved=o.appendRemoved=false;
            q.be=false;q.bF=false;q.bG=q.bH=q.bI=0;
            boolean complete=true;
            if(scene<13) {
                if(scene==1)p.aN=-.25;if(scene==2)p.aN=128;if(scene==3)p.aN=-32;
                if(scene==4)p.aM=48;if(scene==5)p.aM=16;if(scene==6)p.aM=-16;
                if(scene==8){p.aM=Double.NaN;p.aN=Double.POSITIVE_INFINITY;p.aO=Double.NaN;p.aS=Float.NaN;p.aT=Float.POSITIVE_INFINITY;}
                if(scene==9){p.bF=true;p.bH=7;c.a(p);p.aN=33;}
                if(scene==10){c.a(p);p.be=true;}
                if(scene==11){c.a(p);p.aM=48;}
                if(scene==12){p.bF=true;p.bG=20;p.bH=-1;p.bI=20;}
                w.a(p,false);
            }else if(scene==13)w.f(p);
            else {
                if(scene==14){w.a(p);p.be=true;}
                if(scene>=15){if(!w.b(p))throw new AssertionError("Item publication");if(scene!=20)p.be=true;}
                if(scene>=16&&scene<=19)w.b(Collections.singletonList(p));
                if(scene==17)w.b(Collections.singletonList(p));
                if(scene==18)o.failRemoved=true;
                if(scene==19){o.appendRemoved=true;q.e(8.5,64.5,8.5);c.a(q);}
                try{w.g();}catch(IllegalStateException expected){if(scene!=18)throw expected;complete=false;}
            }
            row(w,c,p,o,complete);
        }
        clear(w,c);
        w.b(o); // Fixture terrain acquisition is outside the compared update.
        for(int x=-2;x<=2;++x)for(int z=-2;z<=2;++z){lm loaded=w.c(x,z);Arrays.fill(loaded.b,(byte)0);Arrays.fill(loaded.e.a,(byte)0);}
        w.a(o);
        for(int scene=21;scene<24;++scene) {
            reset(p);o.scenario=scene;o.removed=0;p.bF=false;c.a(p);
            if(scene==22)p.be=true;if(scene==23)p.b=5999;
            w.f(p);row(w,c,p,o,true);clear(w,c);
        }
        ju falling=new ju(w,8.5,8.5,8.5,1);yl cart=new yl(w,8.5,8.5,8.5,0);
        for(int scene=0;scene<7;++scene){
            clear(w,c);reset(p);reset(q);
            field(sn.class,"e").setDouble(p,0);field(sn.class,"d").setDouble(p,0);
            field(sn.class,"e").setDouble(q,370);field(sn.class,"d").setDouble(q,-370);
            sn mount=scene==4?falling:scene==5?cart:p;
            if(scene>=4){mount.b(1f,1f);mount.e(8.5,8.5,8.5);mount.aU=mount.aS;mount.aV=mount.aT;}
            mount.aG=q;q.aH=mount;c.a(p);c.a(q);
            if(scene==6){mount.aS=Float.NaN;mount.aT=Float.NaN;}
            if(scene==1)p.b=5999;if(scene==2)q.be=true;if(scene==3)q.aH=null;
            w.f(scene>=4?q:p);
            System.out.printf("RIDE %d %d %d %d %d %d %d %d %d",scene,mount.be?1:0,q.be?1:0,mount.aG==q?1:0,q.aH==mount?1:0,mount.bt,q.bt,scene>=4?0:p.b,q.b);
            for(double v:new double[]{mount.aN,q.aM,q.aN,q.aO})System.out.printf(" %016x",Double.doubleToRawLongBits(v));
            System.out.printf(" %08x %08x %016x %016x%n",Float.floatToRawIntBits(q.aS),Float.floatToRawIntBits(q.aT),Double.doubleToRawLongBits(field(sn.class,"e").getDouble(q)),Double.doubleToRawLongBits(field(sn.class,"d").getDouble(q)));
            mount.aG=null;q.aH=null;
        }
        clear(w,c);
        w.b(o);
        dc actualPlayer=new dc(null,w,new gr("WorldTick","-"),0);actualPlayer.e(8.5,8.5,8.5);actualPlayer.be=true;
        if(!w.b(actualPlayer))throw new AssertionError("real Player publication");w.b.clear();
        cy spawner=new cy();c.b[(6<<11)|(6<<7)|8]=52;w.a(6,8,6,spawner);
        w.a(o);
        List savedPlayers=w.d;o.tileMode=true;
        for(int scene=0;scene<7;++scene){
            o.tileScene=scene;o.tileEvent=0;spawner.b=scene==4||scene==5?Double.NaN:7;spawner.c=-5;
            spawner.a=scene==1||scene==5?0:scene==3?-1:20;w.B=scene==1||scene==5;w.r.setSeed(38);
            if(scene==0)w.d=(List)field(fd.class,"D").get(w);if(scene==6)spawner.d=null;
            boolean complete=true;
            try{w.g();}catch(NullPointerException expected){if(scene!=6)throw expected;complete=false;}
            int playerCount=w.d.size();w.d=savedPlayers;
            long state=((AtomicLong)field(Random.class,"seed").get(w.r)).get();int continuation=w.r.nextInt();
            System.out.printf("TILE %d %d %d %d %016x %016x %012x %d %d %d%n",scene,complete?1:0,field(fd.class,"L").getBoolean(w)?1:0,spawner.a,Double.doubleToRawLongBits(spawner.b),Double.doubleToRawLongBits(spawner.c),state,continuation,w.c.size(),playerCount);
            if(scene==6)spawner.d=w;
        }
        System.out.println("Original World Entity tick: 23 states, 7 riding states and 7 TileEntity states");
    }
}
