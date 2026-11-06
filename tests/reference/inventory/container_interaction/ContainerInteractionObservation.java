import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Declared pre-call inputs; unchanged original constructors and controller bodies. */
public final class ContainerInteractionObservation {
    static kq app;
    static Path verifiedJar;
    static fd world;
    static dc player;
    static aa container;
    static xi writer;
    static int network, nativeLinks, rows;
    static IdentityHashMap<iz,String> identities;
    static LinkedHashMap<String,iz> objects;
    static List<Object> events, drops;
    static int nextIdentity;
    static long auditSeed;
    static float auditFloat(){auditSeed=(auditSeed*0x5deece66dL+11)&((1L<<48)-1);return (auditSeed>>>24)/(float)(1<<24);}
    static Field field(Class<?> owner,String name) throws Exception {
        Field value=owner.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value,String text) {if(!value) throw new AssertionError(text);}
    static String d(double value) {return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value) {return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static String seed(Random random) throws Exception {return String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(random)).get());}
    static Map<String,Object> map(Object... fields) {
        Map<String,Object> result=new LinkedHashMap<>();
        for(int i=0;i<fields.length;i+=2) result.put((String)fields[i],fields[i+1]);
        return result;
    }
    static String json(Object value) {
        if(value==null) return "null";
        if(value instanceof String) return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r")+"\"";
        if(value instanceof Number || value instanceof Boolean) return value.toString();
        List<String> entries=new ArrayList<>();
        if(value instanceof Map) {for(Object entry:((Map<?,?>)value).entrySet()) {
            Map.Entry<?,?> item=(Map.Entry<?,?>)entry; entries.add(json(item.getKey())+":"+json(item.getValue()));
        } return "{"+String.join(",",entries)+"}";}
        for(Object item:(Iterable<?>)value) entries.add(json(item));
        return "["+String.join(",",entries)+"]";
    }
    static List<Integer> values(iz value) {return Arrays.asList(value.c,value.a,value.i(),value.b);}
    static String ref(iz value) {
        if(value==null) return null;
        String name=identities.get(value);
        if(name==null) {name="N"+(++nextIdentity);identities.put(value,name);objects.put(name,value);}
        return name;
    }
    static List<Object> refs(iz[] values) {List<Object> result=new ArrayList<>();for(iz value:values)result.add(ref(value));return result;}
    static List<Object> inventory(lw value) {List<Object> result=new ArrayList<>();for(int i=0;i<value.a();++i)result.add(ref(value.f_(i)));return result;}
    static Map<String,Object> stats() throws Exception {
        Map<String,Object> result=new TreeMap<>();
        for(Object item:((Map<?,?>)field(xi.class,"a").get(writer)).entrySet()) {
            Map.Entry<?,?> entry=(Map.Entry<?,?>)item;
            result.put(Integer.toString(((vr)entry.getKey()).e),entry.getValue());
        }
        return result;
    }
    static Map<String,Object> snapshot() throws Exception {
        List<Object> cached=new ArrayList<>();for(Object value:container.d) cached.add(ref((iz)value));
        Map<String,Object> result=map("main",refs(player.c.a),"armor",refs(player.c.b),"craft",inventory(container.a),
            "result",inventory(container.b),"cursor",ref(player.c.i()),"cached",cached,
            "selected",player.c.c,"changed",player.c.e,"default-same",player.d==container,
            "active-same",player.e==container,"inventory-owner-same",player.c.d==player,"stats",stats());
        Map<String,Object> stacks=new LinkedHashMap<>();
        for(Map.Entry<String,iz> entry:objects.entrySet()) stacks.put(entry.getKey(),values(entry.getValue()));
        result.put("stacks",stacks);return result;
    }
    @SuppressWarnings({"unchecked","rawtypes"})
    static fd makeWorld() throws Exception {
        fd value=new fd(new wy(),"ContainerInteractionReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);
        Map cache=(Map)field(ok.class,"e").get(provider);List loaded=(List)field(ok.class,"f").get(provider);
        require(cache.isEmpty() && loaded.isEmpty(),"genuine empty initial cache");
        lm chunk=new lm(value,new byte[32768],0,0);chunk.n=true;
        cache.put(yy.a(0,0),chunk);loaded.add(chunk);
        return value;
    }
    static final class Listener implements ec {
        public void a(dw receiver,int slot,iz value) {require(receiver==container,"actual callback container");events.add(map("event","slot","slot",slot,"ref",ref(value),"value",value==null ? null : values(value)));}
        public void a(dw receiver,int key,int value) {throw new AssertionError("unexpected property callback");}
    }
    static final class Observer implements pm {
        public void a(sn value) {
            try {
                require(value instanceof hl && value.aI==world && world.b.contains(value),"real World Item publication");
                hl item=(hl)value;int members=0;
                Object provider=field(fd.class,"v").get(world);
                for(Object chunk:((List<?>)field(ok.class,"f").get(provider))) for(List<?> slice:((lm)chunk).m)
                    for(Object member:slice) if(member==item)++members;
                require(members==1,"one actual Chunk membership");
                float angle=(auditFloat()*(float)Math.PI)*2f;
                auditFloat();auditFloat();auditFloat();
                Map<String,Object> record=map("trig",map("angle",f(angle),"cos",d(Math.cos((double)angle)),"sin",d(Math.sin((double)angle))),"ref",ref(item.a),"value",values(item.a),"pickup-delay",item.c,
                    "world-member",true,"chunk-members",members,"position",Arrays.asList(d(item.aM),d(item.aN),d(item.aO)),
                    "velocity",Arrays.asList(d(item.aP),d(item.aQ),d(item.aR)),"private-random",seed(item.bs));
                drops.add(record);events.add(map("event","drop","index",drops.size()-1,"ref",ref(item.a)));
            }catch(Exception failure){throw new AssertionError(failure);}
        }
        public void a(String n,double x,double y,double z,float volume,float pitch) {throw new AssertionError("unexpected sound "+n);}
        public void a(String n,double x,double y,double z,double a,double b,double c) {throw new AssertionError("unexpected particle");}
        public void b(sn v) {throw new AssertionError("unexpected removal");}
        public void a(int x,int y,int z) {throw new AssertionError("unexpected block");}
        public void b(int x,int y,int z,int a,int b,int c) {throw new AssertionError("unexpected range");}
        public void e() {throw new AssertionError("unexpected refresh");}
        public void a(String n,int x,int y,int z) {throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow v) {throw new AssertionError("unexpected TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e) {throw new AssertionError("unexpected event");}
    }
    static void set(String text,lw target) {
        if(text.equals("-"))return;
        for(String token:text.split(",")) {String[] parts=token.split("=");target.a(Integer.parseInt(parts[0]),objects.get(parts[1]));}
    }
    static void begin(int index) {}
    static void end() {}
    @SuppressWarnings("unchecked")
    static void run(String line,Path directory) throws Exception {
        String[] p=line.split("\\t",-1);String profile=p[0];
        Files.createDirectories(directory.resolve(profile));writer=new xi(app.k,directory.resolve(profile).toFile());app.I=writer;
        world=makeWorld();world.B=false;player=new dc(app,world,app.k,0);app.f=world;app.h=player;
        player.e(8.5,64,8.5);player.aS=0;player.aT=0;
        require(player.d==player.e && player.d instanceof aa,"actual Player constructor default and active Container");
        container=(aa)player.d;
        if(rows==0) {names();graph();}
        identities=new IdentityHashMap<>();objects=new LinkedHashMap<>();nextIdentity=0;
        events=new ArrayList<>();drops=new ArrayList<>();
        if(!p[6].equals("-")) for(String token:p[6].split(";")) {
            String[] v=token.split(",");iz stack=new iz(Integer.parseInt(v[1]),Integer.parseInt(v[2]),Integer.parseInt(v[3]));stack.b=Integer.parseInt(v[4]);
            objects.put(v[0],stack);identities.put(stack,v[0]);
        }
        set(p[7],player.c); // main indices 0..35
        if(!p[8].equals("-"))for(String token:p[8].split(",")) {String[] v=token.split("=");player.c.a(36+Integer.parseInt(v[0]),objects.get(v[1]));}
        set(p[9],container.a);if(!p[10].equals("-"))player.c.b(objects.get(p[10]));
        require(player.c.c==0 && !player.c.e,"constructor selected index and dirty state retained");
        ((List<ec>)field(dw.class,"g").get(container)).add(new Listener());world.a(new Observer());
        Map<String,Object> before=snapshot();String randomBefore=seed(player.bs),worldBefore=seed(world.r);
        iz returned=null;String exception="none";
        auditSeed=Long.parseLong(randomBefore,16);
        begin(rows);
        // No game field, Random, class or method is replaced during this invocation.
        try {
            if(p[1].equals("close")) app.c.a(Integer.parseInt(p[2]),player);
            else returned=app.c.a(Integer.parseInt(p[2]),Integer.parseInt(p[3]),Integer.parseInt(p[4]),Boolean.parseBoolean(p[5]),player);
        }catch(RuntimeException failure){exception=failure.getClass().getName();}
        end();
        String returnedRef=ref(returned);Map<String,Object> after=snapshot();
        System.out.println("CONTAINER_INTERACTION "+json(map("profile",profile,"operation",p[1],"exception",exception,
            "before",before,"after",after,"return",returnedRef,"events",events,"drops",drops,
            "player-random-before",randomBefore,"player-random-after",seed(player.bs),
            "world-random-before",worldBefore,"world-random-after",seed(world.r))));++rows;
    }
    static void graph() throws Exception {
        List<Object> slots=new ArrayList<>();
        for(Object raw:container.e) {
            gp slot=(gp)raw;Object backing=field(gp.class,"e").get(slot);
            String owner=backing==player.c ? "inventory" : backing==container.a ? "craft" : backing==container.b ? "result" : "foreign";
            require(!owner.equals("foreign"),"actual source Slot backing owners");
            slots.add(map("class",slot.getClass().getName(),"slot",slot.a,"backing",owner,"index",field(gp.class,"d").getInt(slot),"x",slot.b,"y",slot.c));
        }
        System.out.println("CONTAINER_GRAPH "+json(map("slots",slots,"cached-size",container.d.size(),"window",container.f,"server",container.c)));
    }
    static void screen(Path directory) throws Exception {
        Path profile=directory.resolve("screen");Files.createDirectories(profile);writer=new xi(app.k,profile.toFile());app.I=writer;
        world=makeWorld();player=new dc(app,world,app.k,0);app.f=world;app.h=player;container=(aa)player.d;
        app.u=new vu(app);app.z=new kv(app,profile.toFile());
        ue value=new ue(player);
        bb staticRenderer=(bb)field(id.class,"l").get(null);
        List<?> buttons=(List<?>)field(da.class,"e").get(value);
        Map<String,Object> before=map("class",value.getClass().getName(),"container-same",value.j==player.d,"default-active-same",player.d==player.e,
            "input",value.f,"mouse-x",f(field(ue.class,"l").getFloat(value)),"mouse-y",f(field(ue.class,"m").getFloat(value)),"width",field(id.class,"a").getInt(value),"height",field(id.class,"i").getInt(value),"buttons",buttons.size(),
            "pauses",value.c(),"open-stat",writer.a((vr)ep.f),"open-granted",writer.a(ep.f),"static-renderer-class",staticRenderer.getClass().getName(),
            "static-renderer-distinct-notice",staticRenderer!=field(vu.class,"h").get(app.u));
        value.a(app,427,240);value.a('x',30);value.a();
        require(app.r==null && player.d==player.e,"CPU initialize/unrelated key/live tick preserve App screen and Container");
        Map<String,Object> after=map("app-same",field(da.class,"b").get(value)==app,"font-same",field(da.class,"g").get(value)==app.q,"font-null",app.q==null,
            "scaled-width",value.c,"scaled-height",value.d,"buttons",buttons.size(),"default-active-same",player.d==player.e,
            "pauses",value.c(),"open-stat",writer.a((vr)ep.f),"open-granted",writer.a(ep.f));
        value.h();
        System.out.println("INVENTORY_SCREEN "+json(map("before",before,"after",after,"closed-default-active-same",player.d==player.e,
            "close-cursor-empty",player.c.i()==null,"close-grid-empty",container.a.f_(0)==null&&container.a.f_(1)==null&&container.a.f_(2)==null&&container.a.f_(3)==null)));
    }
    static void names() throws Exception {
        for(int index=0;index<gm.c.length;++index)if(gm.c[index]!=null) {
            gm item=gm.c[index];
            require(Path.of(item.getClass().getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(verifiedJar),"actual Item name class archive");
            List<Object> samples=new ArrayList<>();
            for(int damage:new int[]{0,15}) {
                String value=null,exception="none",translated=null,trimmed=null;Boolean sameRepeat=null,sameNoarg=null;
                try {
                    iz input=new iz(index,1,damage);value=item.a(input);
                    sameRepeat=value==item.a(input);sameNoarg=value==item.a();
                    translated=nh.a().b(value);trimmed=translated.trim();
                }catch(RuntimeException failure){exception=failure.getClass().getName();}
                samples.add(map("damage",damage,"value",value,"exception",exception,"same-repeat",sameRepeat,
                    "same-noarg",sameNoarg,"translated",translated,"trimmed",trimmed));
            }
            System.out.println("ITEM_NAME "+json(map("id",index,"class",item.getClass().getName(),
                "owner",item.getClass().getMethod("a",iz.class).getDeclaringClass().getName(),"samples",samples)));
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toAbsolutePath();Files.createDirectories(directory);
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission p) {}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
            public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String l){++nativeLinks;throw new SecurityException("native link");}
        });
        for(String name:new String[]{"kq","fd","dc","ix","aa","dw","gp","pv","yv","mq","wl","iz","ob","ue","id","da","hl","gs","sn"}) {
            Class<?> type=Class.forName(name,false,ContainerInteractionObservation.class.getClassLoader());
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"genuine archive "+name);
        }
        verifiedJar=jar;
        app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("ContainerInteractionPlayer","-");field(net.minecraft.client.Minecraft.class,"Z").set(app,directory.toFile());app.c=new ob(app);
            for(String line:Files.readAllLines(Path.of(args[2])))if(!line.isEmpty())run(line,directory);
            screen(directory);
            require(network==0 && nativeLinks==0,"bounded CPU calls without native or network");
            System.out.println("Original container interaction: rows="+rows+"; genuine_owners=1; random_rebinding=0; network=0; native=0");
        }finally{
            app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())if(thread.getName().equals("Timer hack thread")){thread.interrupt();thread.join(2000);require(!thread.isAlive(),"Timer worker retired");}
        }
    }
}
