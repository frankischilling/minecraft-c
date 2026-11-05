import java.lang.reflect.Field;
import java.nio.file.Path;
import java.nio.file.Files;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;
import net.minecraft.client.Minecraft;

/** Unchanged scene Lever/Dust rendering, original translation before final narrowing. */
public final class LeverSceneObservation {
    static int network, links, execs;
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static Object raw(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        return unsafe.getMethod("allocateInstance", Class.class)
            .invoke(field(unsafe, "theUnsafe").get(null), type);
    }
    static void require(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
    static String d(double value) { return String.format(Locale.ROOT, "%016x", Double.doubleToRawLongBits(value)); }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        require(args.length == 3, "jar, private world and synthetic planes");
        Path jar = Path.of(args[0]).toRealPath(), directory = Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission permission) {}
            public void checkConnect(String host, int port) { ++network; throw new SecurityException(); }
            public void checkListen(int port) { ++network; throw new SecurityException(); }
            public void checkLink(String name) { ++links; throw new SecurityException(); }
            public void checkExec(String name) { ++execs; throw new SecurityException(); }
            public void checkWrite(String name) { require(Path.of(name).toAbsolutePath().normalize().startsWith(directory), "private World writes"); }
            public void checkDelete(String name) { checkWrite(name); }
        });
        for (Class<?> type : new Class<?>[]{cv.class, ew.class, fd.class, lm.class, nw.class,
                uu.class, xr.class, sm.class, bt.class, in.class, Minecraft.class, kq.class, kv.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar), "unchanged class origin");
        fd world = new fd(new mx(directory.getParent().toFile(), directory.getFileName().toString(), false), "", 23L);
        lm[] chunks = {world.c(0,0), world.c(0,1), world.c(1,0), world.c(1,1)};
        ew cache = new ew(world, 0, 0, 0, 31, 127, 31);
        
        Minecraft app = (Minecraft)raw(kq.class);
        kv settings = (kv)raw(kv.class); settings.k = true; app.z = settings;
        field(Minecraft.class, "a").set(null, app);
        nw tess = nw.a; tess.b(); tess.b(0, -64, 0);
        byte[] planes=Files.readAllBytes(Path.of(args[2]));require(planes.length==82176,"all five synthetic planes");
        byte[][] arrays={chunks[0].b,chunks[0].e.a,chunks[0].f.a,chunks[0].g.a,chunks[0].h};
        byte[][] declared=new byte[arrays.length][];
        int offset=0;
        for(int i=0;i<arrays.length;++i){declared[i]=Arrays.copyOfRange(planes,offset,offset+arrays[i].length);require(Arrays.equals(arrays[i],declared[i]),"genuine loaded plane matches authored input"+i);offset+=arrays[i].length;}
        int calls=0;
        for (int light=0;light<16;++light) for (int ao=0;ao<2;++ao) for (int powered=-1;powered<2;++powered) {
            settings.k = ao != 0;
            field(fd.class,"f").setInt(world,light);
            lm chunk=chunks[0];
            for(int i=0;i<arrays.length;++i)System.arraycopy(declared[i],0,arrays[i],0,arrays[i].length);
            chunk.b[(3<<11)|(2<<7)|65]=(byte)(powered<0?0:69);
            chunk.e.a(3,65,2,powered<0?0:powered==0?4:12);chunk.e.a(4,65,2,powered<1?0:15);
            byte[][] operands=new byte[arrays.length][];for(int i=0;i<arrays.length;++i)operands[i]=arrays[i].clone();
            for(int id:new int[]{69,55}) {
                if(id==69&&powered<0)continue;
                int x=id==69?3:4;
                uu block=uu.m[id];
                cv renderer=new cv(cache);
                require(!field(cv.class,"f").getBoolean(renderer),"ordinary face visibility");
                int before=field(nw.class,"o").getInt(tess),cursor=field(bt.class,"e").getInt(null);
                boolean returned=renderer.b(block,x,65,2);
                int after=field(nw.class,"o").getInt(tess);
                StringBuilder row=new StringBuilder("SCENE\t"+light+"\t"+ao+"\t"+powered+"\t"+id+"\t"+returned
                    +"\t"+field(cv.class,"d").getInt(renderer)+"\t"+field(cv.class,"m").getBoolean(renderer)
                    +"\t"+(field(bt.class,"e").getInt(null)-cursor)+"\t"+d(block.bs)+","+d(block.bt)+","+d(block.bu)+","+d(block.bv)+","+d(block.bw)+","+d(block.bx)+"\t");
                int[] words=(int[])field(nw.class,"g").get(tess);
                for(int i=before;i<after;++i){if(i!=before)row.append(',');row.append(String.format(Locale.ROOT,"%08x",words[i]));}
                System.out.println(row);++calls;
                for(int i=0;i<arrays.length;++i)require(Arrays.equals(arrays[i],operands[i]),"render leaves source arrays intact");
            }
        }
        require(calls==160,"complete authored operand grid");
        require(network == 0 && links == 0 && execs == 0, "no network/native/exec");
        System.out.println("SCENE_COMPLETE calls=160 network=0 native=0 exec=0");
    }
}
