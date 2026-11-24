import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/* Prepared inputs and passive observations surround unchanged original calls.
 * The listener implements the actual pm interface; it replaces no game class
 * or method. Every original invocation uses the verified supplied JAR. */
public final class BlockHarvestObservation {
    static int network, nativeLinks;
    static final int X=3,Y=64,Z=5;
    static Field field(Class<?> owner,String name) throws Exception {
        Field f=owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Random mathOwner() throws Exception {
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder");
        return (Random)field(holder,"randomNumberGenerator").get(null);
    }
    static long state(Random value) throws Exception {
        return ((AtomicLong)field(Random.class,"seed").get(value)).get();
    }
    static String d(double value) {return String.format(java.util.Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value) {return String.format(java.util.Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static int statistic(xi writer,vr value) {return value==null ? 0 : writer.a(value);}

    static final class Profile {
        final int index,mode,block,metadata,remote,held,count,damage,below,fault;
        final long seed,mathSeed;
        final String line;
        Profile(String line) {
            this.line=line;
            String[] p=line.split("\t",-1);
            if(p.length!=13 || !p[0].equals("PROFILE")) throw new IllegalArgumentException("profile shape");
            index=Integer.parseInt(p[1]); mode=Integer.parseInt(p[2]); block=Integer.parseInt(p[3]);
            metadata=Integer.parseInt(p[4]); remote=Integer.parseInt(p[5]); held=Integer.parseInt(p[6]);
            count=Integer.parseInt(p[7]); damage=Integer.parseInt(p[8]); below=Integer.parseInt(p[9]);
            seed=Long.parseLong(p[10]); mathSeed=Long.parseLong(p[11]); fault=Integer.parseInt(p[12]);
        }
    }

    @SuppressWarnings({"rawtypes","unchecked"})
    static fd world(Profile p) throws Exception {
        fd value=new fd(new wy(),"BlockHarvestReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);
        Map cache=(Map)field(ok.class,"e").get(provider);
        List loaded=(List)field(ok.class,"f").get(provider);
        cache.clear(); loaded.clear();
        byte[] blocks=new byte[32768];
        blocks[(X*16+Z)*128+Y]=(byte)p.block;
        blocks[(X*16+Z)*128+Y-1]=(byte)p.below;
        if(p.below==78) blocks[(X*16+Z)*128+Y-2]=1;
        lm chunk=new lm(value,blocks,0,0); chunk.n=true;
        cache.put(yy.a(0,0),chunk); loaded.add(chunk);
        /* Actual quiet metadata setter, declared preparation before observer
         * registration/RNG seeding. Source array packing keeps its own mask. */
        value.e(X,Y,Z,p.metadata);
        value.B=p.remote!=0;
        return value;
    }

    static final class Observer implements InvocationHandler {
        final Profile p;
        final fd world;
        final lm chunk;
        final xi writer;
        final Random math;
        final vr mined,used,broken;
        final int startMined,startUsed,startBroken;
        final List<String> events=new ArrayList<>();
        Observer(Profile p,fd world,xi writer,Random math) throws Exception {
            this.p=p; this.world=world; this.writer=writer; this.math=math;
            chunk=world.c(0,0);
            mined=jl.C[p.block]; used=p.held<0 ? null : jl.E[p.held];
            broken=p.held<0 ? null : jl.F[p.held];
            startMined=statistic(writer,mined); startUsed=statistic(writer,used);
            startBroken=statistic(writer,broken);
        }
        int itemCount() {int n=0;for(Object e:world.b) if(e instanceof hl) ++n;return n;}
        boolean inChunk(sn entity) {
            for(Object list:chunk.m) if(((List<?>)list).contains(entity)) return true;
            return false;
        }
        void event(String kind,hl item) throws Exception {
            int id=item==null ? -1 : item.a.c,delay=item==null ? -1 : item.c;
            events.add("EVENT\t"+p.index+"\t"+events.size()+"\t"+kind+"\t"+
                world.a(X,Y,Z)+"\t"+world.e(X,Y,Z)+"\t"+
                (statistic(writer,mined)-startMined)+"\t"+
                (statistic(writer,used)-startUsed)+"\t"+
                (statistic(writer,broken)-startBroken)+"\t"+itemCount()+"\t"+id+"\t"+delay+"\t"+
                (item!=null && world.b.contains(item) ? 1 : 0)+"\t"+
                (item!=null && inChunk(item) ? 1 : 0)+"\t"+state(world.r)+"\t"+state(math));
        }
        public Object invoke(Object proxy,Method method,Object[] args) throws Throwable {
            if(method.getDeclaringClass()==Object.class) {
                if(method.getName().equals("hashCode")) return System.identityHashCode(proxy);
                if(method.getName().equals("equals")) return proxy==args[0];
                return "BlockHarvestObserver";
            }
            Class<?>[] parameters=method.getParameterTypes();
            if(method.getName().equals("a") && parameters.length==1 && parameters[0]==sn.class && args[0] instanceof hl)
                event("INSERT",(hl)args[0]);
            else if(method.getName().equals("a") && parameters.length==3 && parameters[0]==int.class &&
                    (Integer)args[0]==X && (Integer)args[1]==Y && (Integer)args[2]==Z)
                event("CHANGE",null);
            return null;
        }
    }

    @SuppressWarnings("unchecked")
    static void observe(Profile p,kq app,xi writer,Random math) throws Exception {
        fd world=world(p);
        dc player=new dc(app,world,app.k,0); app.f=world; app.h=player;
        player.e(.5,64,.5);
        iz captured=p.held<0 ? null : new iz(p.held,p.count,p.damage);
        if(captured!=null) player.c.a(0,captured);
        Observer observer=new Observer(p,world,writer,math);
        Object proxy=Proxy.newProxyInstance(pm.class.getClassLoader(),new Class<?>[]{pm.class},observer);
        ((List<Object>)field(fd.class,"u").get(world)).add(proxy);
        world.r.setSeed(p.seed); math.setSeed(p.mathSeed);
        long beforeWorld=state(world.r),beforeMath=state(math);
        vr saved=jl.C[p.block];
        if(p.fault==1) jl.C[p.block]=null;
        if(p.fault==2) app.I=null;
        String status="OK"; int removed=-1;
        System.out.println(p.line);
        try {
            if(p.mode==1) removed=new os(app).b(X,Y,Z,1) ? 1 : 0;
            else uu.m[p.block].a(world,p.fault==3 ? null : player,X,Y,Z,p.metadata);
        } catch(NullPointerException expected) {
            status="NULL";
        } finally {
            jl.C[p.block]=saved; app.I=writer;
        }
        long afterWorld=state(world.r),afterMath=state(math);
        iz selected=player.c.b();
        System.out.println("RESULT\t"+p.index+"\t"+status+"\t"+removed+"\t"+
            (selected==null ? 0 : 1)+"\t"+(selected==null ? -1 : selected.c)+"\t"+
            (selected==null ? 0 : selected.a)+"\t"+(selected==null ? 0 : selected.i())+"\t"+
            (captured==null ? 0 : 1)+"\t"+(captured==null ? -1 : captured.c)+"\t"+
            (captured==null ? 0 : captured.a)+"\t"+(captured==null ? 0 : captured.i())+"\t"+
            world.a(X,Y,Z)+"\t"+world.e(X,Y,Z)+"\t"+
            (statistic(writer,observer.mined)-observer.startMined)+"\t"+
            (statistic(writer,observer.used)-observer.startUsed)+"\t"+
            (statistic(writer,observer.broken)-observer.startBroken)+"\t"+
            beforeWorld+"\t"+afterWorld+"\t"+beforeMath+"\t"+afterMath+"\t"+
            observer.itemCount()+"\t"+observer.events.size());
        int index=0;
        for(Object entity:world.b) if(entity instanceof hl) {
            hl item=(hl)entity;
            System.out.println("ITEM\t"+p.index+"\t"+index+++"\t"+item.a.c+"\t"+item.a.a+"\t"+
                item.a.i()+"\t"+item.c+"\t"+item.b+"\t"+d(item.aM)+"\t"+d(item.aN)+"\t"+
                d(item.aO)+"\t"+d(item.aP)+"\t"+d(item.aQ)+"\t"+d(item.aR)+"\t"+
                f(item.aS)+"\t"+f(item.d)+"\t1\t"+(observer.inChunk(item) ? 1 : 0));
        }
        for(String event:observer.events) System.out.println(event);
        ((List<Object>)field(fd.class,"u").get(world)).remove(proxy);
    }

    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        /* Bootstrap inert JDK networking owners first; the ordinary connect,
         * listen and native-link operations are actually denied after source
         * observations. No socket is bound or connected before the guard. */
        Socket deniedConnect=new Socket(); ServerSocket deniedListen=new ServerSocket();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(java.security.Permission p) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listener");}
            public void checkLink(String library) {++nativeLinks;throw new SecurityException("native library");}
        });
        Path directory=Path.of(args[0]); Files.createDirectories(directory);
        kq app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("BlockHarvestPlayer","-");
            field(net.minecraft.client.Minecraft.class,"Z").set(app,directory.toFile());
            xi writer=new xi(app.k,directory.toFile()); app.I=writer;
            Random math=mathOwner();
            List<String> lines=Files.readAllLines(Path.of(args[1]));
            for(String line:lines) if(!line.isEmpty()) observe(new Profile(line),app,writer,math);
            if(network!=0 || nativeLinks!=0) throw new AssertionError("source network/native attempt");
            System.out.println("GUARD\tsource_network_attempts\t0");
            System.out.println("GUARD\tsource_native_attempts\t0");
            try {deniedConnect.connect(new InetSocketAddress("127.0.0.1",9));throw new AssertionError("connect allowed");}
            catch(SecurityException expected) {if(network!=1) throw new AssertionError("connect guard");}
            System.out.println("GUARD\tconnect_denied\t1");
            try {deniedListen.bind(new InetSocketAddress("127.0.0.1",0));throw new AssertionError("listen allowed");}
            catch(SecurityException expected) {if(network!=2) throw new AssertionError("listen guard");}
            System.out.println("GUARD\tlisten_denied\t1");
            try {System.loadLibrary("minecraft_c_reference_denial_probe");throw new AssertionError("link allowed");}
            catch(SecurityException expected) {if(nativeLinks!=1) throw new AssertionError("link guard");}
            System.out.println("GUARD\tnative_link_denied\t1");
        } finally {
            deniedConnect.close(); deniedListen.close(); app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet()) if(t.getName().equals("Timer hack thread")) {
                t.interrupt(); t.join(2000);
                if(t.isAlive()) throw new AssertionError("Timer worker remained live");
            }
        }
    }
}
