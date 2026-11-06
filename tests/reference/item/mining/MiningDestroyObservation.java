import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;

/* Genuine original Item/IZ calls. Preparation and observations are outside
 * each invocation; no original method or class is changed or substituted. */
public final class MiningDestroyObservation {
    static int network, nativeLinks;
    static Field field(Class<?> owner, String name) throws Exception {
        Field f=owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    @SuppressWarnings({"rawtypes", "unchecked"})
    static fd world(int blockId) throws Exception {
        fd value=new fd(new wy(),"MiningDestroyReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);
        Map cache=(Map)field(ok.class,"e").get(provider);
        List loaded=(List)field(ok.class,"f").get(provider);
        cache.clear(); loaded.clear();
        byte[] blocks=new byte[32768]; blocks[(3*16+5)*128+64]=(byte)blockId;
        lm chunk=new lm(value,blocks,0,0); chunk.n=true;
        cache.put(yy.a(0,0),chunk); loaded.add(chunk);
        return value;
    }
    static void direct(int id,int block,int count,int damage) {
        gm item=gm.c[id]; iz stack=new iz(id,count,damage); stack.b=5;
        boolean result=item.a(stack,block,-3,64,7,(ls)null);
        System.out.println("DESTROY\t"+id+"\t"+block+"\t"+count+"\t"+damage+"\t"+
            (result ? 1 : 0)+"\t"+stack.a+"\t"+stack.i()+"\t"+stack.b);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(java.security.Permission p) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listener");}
            public void checkLink(String library) {++nativeLinks;throw new SecurityException("native library");}
        });
        uu stone=uu.u;
        for(int id=0;id<gm.c.length;++id) if(gm.c[id]!=null)
            for(int block:new int[]{1,18,30,35,0,-1}) direct(id,block,2,0);
        for(int id=0;id<gm.c.length;++id) if(gm.c[id]!=null) {
            String owner=gm.c[id].getClass().getMethod("a",iz.class,int.class,int.class,int.class,
                int.class,ls.class).getDeclaringClass().getName();
            if(!owner.equals("dq") && !owner.equals("qd") && !owner.equals("bl")) continue;
            int maximum=gm.c[id].f(), step=owner.equals("qd") ? 2 : 1;
            for(int damage:new int[]{maximum-step,maximum-step+1,maximum,maximum+1,-1,
                                     Integer.MAX_VALUE,Integer.MIN_VALUE})
                for(int count:new int[]{1,0,-3,2,Integer.MIN_VALUE})
                    direct(id,owner.equals("bl") ? 18 : 1,count,damage);
        }
        Path directory=Path.of(args[0]); Files.createDirectories(directory);
        kq app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("MiningDestroyPlayer","-");
            field(net.minecraft.client.Minecraft.class,"Z").set(app,directory.toFile());
            xi writer=new xi(app.k,directory.toFile()); app.I=writer;
            fd world=world(0); world.B=false;
            dc player=new dc(app,world,app.k,0); app.f=world; app.h=player;
            for(int id:new int[]{270,274,278,268,359,3})
                for(int block:new int[]{1,18,30,35})
                    for(int damage:new int[]{0,gm.c[id].f(),gm.c[id].f()-1,Integer.MAX_VALUE})
                        for(int count:new int[]{1,0,-3,2}) {
                            iz stack=new iz(id,count,damage); stack.b=5;
                            int used=writer.a(jl.E[id]);
                            int broken=jl.F[id]==null ? -1 : writer.a(jl.F[id]);
                            stack.a(block,-3,64,7,player);
                            System.out.println("STACK_DESTROY\t"+id+"\t"+block+"\t"+count+"\t"+
                                damage+"\t"+stack.a+"\t"+stack.i()+"\t"+stack.b+"\t"+
                                (writer.a(jl.E[id])-used)+"\t"+
                                (broken<0 ? -1 : writer.a(jl.F[id])-broken));
                        }
            int[][] profiles={{270,1,0},{270,1,59},{270,2,59},{270,0,0},{270,-3,0},{268,1,59}};
            for(int profile=0;profile<profiles.length;++profile) {
                int id=profiles[profile][0],count=profiles[profile][1],damage=profiles[profile][2];
                fd target=world(1); target.B=false;
                dc miner=new dc(app,target,app.k,0); app.f=target; app.h=miner;
                miner.e(.5,64,.5);
                iz held=new iz(id,count,damage); miner.c.a(0,held);
                int used=writer.a(jl.E[id]),broken=writer.a(jl.F[id]),mined=writer.a(jl.C[1]);
                if(!new os(app).b(3,64,5,1) || target.a(3,64,5)!=0)
                    throw new AssertionError("actual controller did not remove Stone");
                int items=0;
                for(Object entity:target.b) if(entity instanceof hl) {
                    hl item=(hl)entity;
                    if(item.a.c!=4 || item.a.a!=1 || item.a.i()!=0)
                        throw new AssertionError("actual Stone drop differs");
                    ++items;
                }
                System.out.println("CONTROLLER_DESTROY\t"+profile+"\t"+id+"\t"+count+"\t"+
                    damage+"\t"+held.a+"\t"+held.i()+"\t"+(miner.c.b()==null ? 1 : 0)+"\t"+
                    (writer.a(jl.E[id])-used)+"\t"+(writer.a(jl.F[id])-broken)+"\t"+
                    (writer.a(jl.C[1])-mined)+"\t"+items);
            }
            if(network!=0 || nativeLinks!=0) throw new AssertionError("unexpected network/native access");
        } finally {
            app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet()) if(t.getName().equals("Timer hack thread")) {
                t.interrupt(); t.join(2000);
                if(t.isAlive()) throw new AssertionError("Timer worker remained live");
            }
        }
    }
}
