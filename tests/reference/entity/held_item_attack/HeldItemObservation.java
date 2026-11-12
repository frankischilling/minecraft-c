import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored finite preparation; only original OB/GS execute each attack. */
public final class HeldItemObservation {
    static kq app;
    static xi writer;
    static fd world;
    static lm chunk;
    static int network, nativeLinks, drops, rows, cowRows, sounds, particles, breakSounds;
    static List<String> observedSounds=new ArrayList<>(), observedItems=new ArrayList<>(), observedEvents=new ArrayList<>();
    static List<hl> observedObjects=new ArrayList<>();
    static String bits(float value) {return String.format(java.util.Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String seed(Random random) throws Exception {return String.format(java.util.Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(random)).get());}
    static Field field(Class<?> owner, String name) throws Exception {
        Field value=owner.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value,String text) {
        if (!value) throw new AssertionError(text);
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd makeWorld() throws Exception {
        fd value=new fd(new wy(),"HeldItemReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);
        Map cache=(Map)field(ok.class,"e").get(provider);
        List loaded=(List)field(ok.class,"f").get(provider);
        cache.clear(); loaded.clear();
        chunk=new lm(value,new byte[32768],0,0); chunk.n=true;
        cache.put(yy.a(0,0),chunk); loaded.add(chunk);
        return value;
    }
    static final class Observer implements pm {
        public void a(String n,double x,double y,double z,float v,float p) {
            observedEvents.add("\"SOUND\"");
            observedSounds.add("{\"name\":\""+n+"\",\"volume\":\""+bits(v)+"\",\"pitch\":\""+bits(p)+"\"}");
            ++sounds; if(n.equals("random.break")) ++breakSounds;
        }
        public void a(sn value) {
            require(value instanceof hl && world.b.contains(value),"actual Item publication");
            hl item=(hl)value; int members=0;
            for(hl other:observedObjects) require(other!=item && other.a!=item.a,"distinct original Item/Stack owners");
            observedObjects.add(item);
            for(List<?> slice:chunk.m) for(Object member:slice) if(member==value) ++members;
            require(members==1,"actual Chunk publication");
            observedEvents.add("\"ADD\"");
            try {
                observedItems.add("{\"stack\":["+item.a.c+","+item.a.a+","+item.a.i()+","+item.a.b+"],\"pickup-delay\":"+item.c+
                    ",\"world-member\":true,\"chunk-members\":"+members+",\"distinct\":true,\"private-random\":\""+seed(item.bs)+"\"}");
            } catch(Exception failure) {throw new AssertionError(failure);}
            ++drops;
        }
        public void a(String n,double x,double y,double z,double a,double b,double c) {++particles;}
        public void b(sn v) {throw new AssertionError("unexpected removal");}
        public void a(int x,int y,int z) {throw new AssertionError("unexpected block");}
        public void b(int x,int y,int z,int a,int b,int c) {throw new AssertionError("unexpected range");}
        public void e() {throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z) {throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow v) {throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e) {throw new AssertionError("unexpected event");}
    }
    static void reset(vr stat,int value) {writer.a(stat,value-writer.a(stat));}
    static void run(String profile,int id,int metadata,int count,String mode) throws Exception {
        run(profile,id,metadata,count,mode,mode.equals("fatal") ? "Sheep" : "Cow",false);
    }
    static void run(String profile,int id,int metadata,int count,String mode,String targetKind,boolean saddled) throws Exception {
        world=makeWorld(); world.B=false;
        dc player=new dc(app,world,app.k,0); app.f=world; app.h=player;
        player.e(.5,64,.5); player.aP=player.aQ=player.aR=0; player.g=3;
        boolean fatal=mode.equals("fatal");
        ls target=targetKind.equals("Pig") ? new wh(world) : targetKind.equals("Sheep") ? new dl(world) : new bx(world);
        if(target instanceof wh) ((wh)target).a(saddled);
        target.e(1.5,64,.5); target.aS=0; target.aP=target.aQ=target.aR=0;
        target.E=20; target.Y=fatal ? 1 : profile.startsWith("durable-") ? 20 : 10; target.Z=10; target.by=0; target.aa=target.ab=0;
        field(ls.class,"au").setInt(target,0); field(ls.class,"av").setInt(target,0);
        field(ls.class,"ah").setBoolean(target,false); field(ls.class,"T").setInt(target,fatal ? 7 : 0);
        if (mode.equals("falling")) player.aQ=-.1;
        gm item=gm.c[id];
        int power=item.a(target);
        if (mode.contains("resisted")) {target.by=20; field(ls.class,"au").setInt(target,mode.equals("equal-resisted") ? power : mode.equals("higher-resisted") ? power+1 : 1);}
        iz held=new iz(id,count,metadata); held.b=5;
        player.c.a(0,held);
        require(player.c.c==0 && player.c.b()==held && !player.c.e,"prepared actual selected Stack");
        reset(jl.w,0); reset(jl.z,0); reset(jl.E[id],11);
        if(jl.F[id]!=null) reset(jl.F[id],17);
        final int beforeUsed=writer.a(jl.E[id]);
        final int beforeBroken=jl.F[id]==null ? -1 : writer.a(jl.F[id]);
        final int beforeSaddle=target instanceof wh ? (((wh)target).v() ? 1 : 0) : -1;
        world.a(new Observer()); drops=0; sounds=0; particles=0; breakSounds=0;
        observedSounds.clear(); observedItems.clear(); observedEvents.clear(); observedObjects.clear();
        final boolean cow=profile.startsWith("cow10-");
        String cowBefore=cow ? seed(target.bs) : "", playerBefore=cow ? seed(player.bs) : "", worldBefore=cow ? seed(world.r) : "";
        // No original state writes or entropy rebinding occur inside this invocation.
        new ob(app).b(player,target);
        iz current=player.c.b();
        String selected="{\"profile\":\""+profile+"\",\"mode\":\""+mode+"\",\"id-before\":"+id+
            ",\"count-before\":"+count+",\"damage-before\":"+metadata+",\"animation-before\":5"+
            ",\"id-after\":"+held.c+",\"count-after\":"+held.a+",\"damage-after\":"+held.i()+",\"animation-after\":"+held.b+
            ",\"selected-same\":"+(current==held)+",\"selected-cleared\":"+(current==null)+",\"inventory-changed\":"+player.c.e+
            ",\"health\":"+target.Y+",\"previous-health\":"+target.Z+",\"hurt\":"+target.by+
            ",\"hurt-time\":"+target.aa+",\"max-hurt\":"+target.ab+",\"last-damage\":"+field(ls.class,"au").getInt(target)+
            ",\"idle\":"+field(ls.class,"av").getInt(target)+",\"dying\":"+field(ls.class,"ah").getBoolean(target)+
            ",\"score\":"+player.g+",\"mob-kills\":"+writer.a(jl.z)+",\"damage-dealt\":"+writer.a(jl.w)+
            ",\"used-before\":"+beforeUsed+",\"used-after\":"+writer.a(jl.E[id])+",\"drops\":"+drops+
            ",\"item-class\":\""+item.getClass().getName()+"\",\"attack-owner\":\""+item.getClass().getMethod("a",sn.class).getDeclaringClass().getName()+
            "\",\"hit-owner\":\""+item.getClass().getMethod("a",iz.class,ls.class,ls.class).getDeclaringClass().getName()+
            "\",\"use-owner\":\""+item.getClass().getMethod("a",iz.class,ls.class).getDeclaringClass().getName()+
            "\",\"target-kind\":\""+targetKind+"\",\"max-damage\":"+item.f()+",\"attack-power\":"+power+
            ",\"saddled-before\":"+beforeSaddle+",\"saddled-after\":"+(target instanceof wh ? (((wh)target).v() ? 1 : 0) : -1)+
            ",\"broken-before\":"+beforeBroken+",\"broken-after\":"+(jl.F[id]==null ? -1 : writer.a(jl.F[id]))+
            ",\"sound-events\":"+sounds+",\"particle-events\":"+particles+",\"break-sounds\":"+breakSounds+"}";
        if(cow) {
            System.out.println("HELD_ITEM_COW {\"profile\":\""+profile+"\",\"selected\":"+selected+
                ",\"cow-before\":\""+cowBefore+"\",\"cow-after\":\""+seed(target.bs)+
                "\",\"player-before\":\""+playerBefore+"\",\"player-after\":\""+seed(player.bs)+
                "\",\"world-before\":\""+worldBefore+"\",\"world-after\":\""+seed(world.r)+
                "\",\"sounds\":["+String.join(",",observedSounds)+"],\"items\":["+String.join(",",observedItems)+
                "],\"events\":["+String.join(",",observedEvents)+"]}");
            ++cowRows;
        } else {System.out.println("HELD_ITEM "+selected); ++rows;}
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[0]).toRealPath(), directory=Path.of(args[1]).toAbsolutePath();
        Files.createDirectories(directory);
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission p) {}
            public void checkConnect(String h,int p) {++network;throw new SecurityException("network");}
            public void checkListen(int p) {++network;throw new SecurityException("listener");}
            public void checkLink(String l) {++nativeLinks;throw new SecurityException("native link");}
        });
        for (String n:new String[]{"kq","dc","gs","ix","ob","fd","ok","lm","ls","sn","bx","dl","wh","xi","jl","iz","gm","ck","bi","dq","qd","sa","bu","hl"}) {
            Class<?> type=Class.forName(n,false,HeldItemObservation.class.getClassLoader());
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive "+n);
        }
        require(ck.class.getMethod("a",sn.class).getDeclaringClass()==gm.class &&
                bi.class.getMethod("a",sn.class).getDeclaringClass()==gm.class &&
                ck.class.getMethod("a",iz.class,ls.class,ls.class).getDeclaringClass()==gm.class &&
                bi.class.getMethod("a",iz.class,ls.class,ls.class).getDeclaringClass()==gm.class,"actual combat method resolution");
        app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("HeldItemPlayer","-");
            field(net.minecraft.client.Minecraft.class,"Z").set(app,directory.toFile());
            writer=new xi(app.k,directory.toFile()); app.I=writer;
            for (int id:new int[]{3,35}) {
                String item=id==3 ? "dirt" : "wool";
                for (String mode:new String[]{"ordinary","falling","resisted","fatal"})
                    run(item+"-"+mode,id,id==3 ? 0 : 6,7,mode);
            }
            for (int metadata=0;metadata<16;++metadata)
                run("wool-meta-"+metadata,35,metadata,7,"ordinary");
            for (int id:new int[]{3,35}) for (int count:new int[]{0,-3})
                for (String mode:new String[]{"ordinary","fatal"})
                    run((id==3 ? "dirt" : "wool")+"-count"+count+"-"+mode,id,id==3 ? 0 : 6,count,mode);
            require(rows==32,"retained research prefix");
            Map<String,Integer> inherited=new TreeMap<>();
            int tools=0,inventory=0;
            for(int id=0;id<gm.c.length;++id) if(gm.c[id]!=null) {
                gm item=gm.c[id]; Class<?> type=item.getClass();
                String attack=type.getMethod("a",sn.class).getDeclaringClass().getName();
                String hit=type.getMethod("a",iz.class,ls.class,ls.class).getDeclaringClass().getName();
                String use=type.getMethod("a",iz.class,ls.class).getDeclaringClass().getName();
                String icon=type.getMethod("a",int.class).getDeclaringClass().getName();
                String color=type.getMethod("f",int.class).getDeclaringClass().getName();
                String full3d=type.getMethod("b").getDeclaringClass().getName();
                String rotate=type.getMethod("c").getDeclaringClass().getName();
                require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"genuine original Item class");
                System.out.println("HELD_ITEM_CLASS {\"id\":"+id+",\"class\":\""+type.getName()+"\",\"super-class\":\""+type.getSuperclass().getName()+"\",\"attack-owner\":\""+attack+"\",\"hit-owner\":\""+hit+"\",\"use-owner\":\""+use+"\",\"max-damage\":"+item.f()+
                    ",\"icon-owner\":\""+icon+"\",\"color-owner\":\""+color+"\",\"full3d-owner\":\""+full3d+"\",\"rotate-owner\":\""+rotate+
                    "\",\"full3d\":"+item.b()+",\"rotate\":"+item.c()+",\"icon0\":"+item.a(0)+",\"color0\":"+item.f(0)+"}");
                if(!full3d.equals("gm"))
                    System.out.println("HELD_ITEM_RENDER {\"id\":"+id+",\"metadata\":15,\"icon\":"+item.a(15)+",\"color\":"+item.f(15)+",\"full3d\":"+item.b()+",\"rotate\":"+item.c()+"}");
                ++inventory;
                if(attack.equals("gm") && hit.equals("gm")) {
                    inherited.putIfAbsent(type.getName(),id);
                    if(type==gm.class) run("base-id-"+id,id,0,7,"ordinary");
                }
            }
            for(Map.Entry<String,Integer> entry:inherited.entrySet()) {
                int id=entry.getValue();
                if(gm.c[id].getClass()!=gm.class) run("inherited-class-"+entry.getKey()+"-id-"+id,id,0,7,"ordinary");
            }
            for(int id=0;id<gm.c.length;++id) if(gm.c[id]!=null) {
                gm item=gm.c[id];
                String hit=item.getClass().getMethod("a",iz.class,ls.class,ls.class).getDeclaringClass().getName();
                if(!hit.equals("dq") && !hit.equals("qd")) continue;
                ++tools; int step=hit.equals("dq") ? 2 : 1, max=item.f();
                for(String mode:new String[]{"ordinary","falling","resisted","equal-resisted","higher-resisted","fatal"})
                    run("durable-"+id+"-"+mode,id,0,1,mode);
                int[] damage={max-step,max-step+1,max,max+1,-1,Integer.MAX_VALUE,Integer.MIN_VALUE};
                String[] names={"ends-at-max","crosses-max","from-max","past-max","negative-damage","wrap-max","wrap-min"};
                for(int i=0;i<damage.length;++i) run("durable-"+id+"-"+names[i],id,damage[i],1,"ordinary");
                for(int count:new int[]{0,-3,2}) run("durable-"+id+"-count"+count+"-break",id,max,count,"ordinary");
            }
            require(tools==20,"twenty actual DQ/QD tool/sword Items");
            for(String target:new String[]{"Pig","Cow","Sheep"}) for(int count:new int[]{1,0,-3,2})
                for(boolean saddled:new boolean[]{false,true}) {
                    if(!target.equals("Pig") && saddled) continue;
                    run("saddle-"+target.toLowerCase()+"-count"+count+"-"+(saddled ? "saddled" : "plain"),329,0,count,"ordinary",target,saddled);
                }
            run("cow10-diamond-ordinary",276,0,1,"ordinary","Cow",false);
            run("cow10-diamond-falling",276,0,1,"falling","Cow",false);
            run("cow10-diamond-break",276,gm.c[276].f(),1,"ordinary","Cow",false);
            require(cowRows==3,"complete independent-entropy Cow death observations");
            require(network==0 && nativeLinks==0,"complete bounded CPU observation");
            System.out.println("Original held Item attack: "+rows+" rows; cow_rows=3; inventory="+inventory+"; tools=20; network=0; native=0; genuine_owners=1; random_rebinding=0");
        } finally {
            app.J=false;
            for (Thread t:Thread.getAllStackTraces().keySet()) if (t.getName().equals("Timer hack thread")) {
                t.interrupt(); t.join(2000); require(!t.isAlive(),"Timer worker retired");
            }
        }
    }
}
