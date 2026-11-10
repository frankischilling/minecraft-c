import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored direct-call inputs and passive observers, never a Slime/World replacement. */
public final class SlimeRemovalObservation {
    static fd primary, secondary;
    static uw parent;
    static Path outputRoot;
    static int network, nativeLinks, rejectedWrites, observerMode;
    static final Map<fd,List<lm>> chunks = new LinkedHashMap<>();
    static final List<uw> children = new ArrayList<>();
    static final List<Object> observed = new ArrayList<>(), entropy = new ArrayList<>();
    static void need(boolean ok,String message) { if(!ok) throw new AssertionError(message); }
    static Field field(Class<?> owner,String name) throws Exception {
        Field value=owner.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static String seed(Random random) throws Exception {
        return random==null?null:String.format(Locale.ROOT,"%016x",
            ((AtomicLong)field(Random.class,"seed").get(random)).get());
    }
    static String fword(float value) { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value)); }
    static String dword(double value) { return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value)); }
    static Map<String,Object> map(Object... cells) {
        Map<String,Object> value=new LinkedHashMap<>();
        for(int i=0;i<cells.length;i+=2)value.put((String)cells[i],cells[i+1]); return value;
    }
    static String json(Object value) {
        if(value==null)return "null";
        if(value instanceof String)return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"")+"\"";
        if(value instanceof Map){StringBuilder out=new StringBuilder("{");
            for(Map.Entry<?,?> e:((Map<?,?>)value).entrySet()){
                if(out.length()>1)out.append(',');out.append(json(e.getKey())).append(':').append(json(e.getValue()));}
            return out.append('}').toString();}
        if(value instanceof List){StringBuilder out=new StringBuilder("[");for(Object x:(List<?>)value){
            if(out.length()>1)out.append(',');out.append(json(x));}return out.append(']').toString();}
        return value.toString();
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd world(String name) throws Exception {
        fd value=new fd(new wy(),name,xa.a(0),23L);
        Object provider=field(fd.class,"v").get(value);
        Map cache=(Map)field(ok.class,"e").get(provider);
        List loaded=(List)field(ok.class,"f").get(provider); cache.clear();loaded.clear();
        List<lm> own=new ArrayList<>();
        for(int x=-1;x<=1;++x)for(int z=-1;z<=1;++z){
            lm chunk=new lm(value,new byte[32768],x,z);chunk.n=true;
            cache.put(yy.a(x,z),chunk);loaded.add(chunk);own.add(chunk);}
        chunks.put(value,own); return value;
    }
    static int membership(fd world,sn entity) {
        int count=0;for(lm chunk:chunks.get(world))for(List<?> slice:chunk.m)
            for(Object member:slice)if(member==entity)++count;return count;
    }
    static final class Observer implements pm {
        final fd world;
        Observer(fd value){world=value;}
        public void a(sn entity) {
            try {
                need(entity instanceof uw && entity!=parent,"actual Slime child");
                uw child=(uw)entity;need(!children.contains(child),"distinct child identity");
                for(uw old:children)need(old.aD!=child.aD,"distinct actual Entity IDs");
                need(world.b.contains(child)&&membership(world,child)==1,"real World/Chunk publication before callback");
                int index=children.size();children.add(child);
                observed.add(map("index",index,"size",child.v(),"health",child.Y,
                    "position",List.of(dword(child.aM),dword(child.aN),dword(child.aO)),
                    "yaw",fword(child.aS),"pitch",fword(child.aT),
                    "world",child.aI==primary?"PRIMARY":child.aI==secondary?"SECONDARY":null,
                    "world_member",world.b.contains(child),"chunk_members",membership(world,child),
                    "parent_dead",parent.be,"random_after",seed(parent.bs)));
                entropy.add(map("index",index,"private_random",seed(child.bs),
                    "animation",List.of(fword(child.al),fword(child.a)),"constructor_jump",field(uw.class,"c").getInt(child)));
                if(observerMode==1&&index==1)throw new IllegalStateException("authored second child callback failure");
                if(index==0){
                    if(observerMode==2)parent.e(10.5,65.,9.5);
                    if(observerMode==3)parent.e(4);
                    if(observerMode==4)parent.bs.setSeed(12345);
                    if(observerMode==5)parent.aI=null;
                    if(observerMode==6)parent.aI=secondary;
                }
            } catch(RuntimeException e){throw e;} catch(Exception e){throw new AssertionError(e);}
        }
        public void a(String n,double x,double y,double z,float v,float p){throw new AssertionError("sound");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("particle");}
        public void b(sn e){throw new AssertionError("entity removal callback");}
        public void a(int x,int y,int z){throw new AssertionError("block");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("range");}
        public void e(){throw new AssertionError("refresh");}
        public void a(String n,int x,int y,int z){throw new AssertionError("record");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("tile");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("event");}
    }
    static void origin(Class<?> type,Path jar) throws Exception {
        need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),
             "unchanged original class origin "+type.getName());
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        need(args.length==3,"original JAR, exact profile table, private output root");
        Locale.setDefault(Locale.ROOT);Path jar=Path.of(args[0]).toRealPath();
        List<String> profiles=Files.readAllLines(Path.of(args[1]));outputRoot=Path.of(args[2]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p){if(!Path.of(p).toAbsolutePath().normalize().startsWith(outputRoot)){
                ++rejectedWrites;throw new SecurityException("write outside private output denied");}}
            @Override public void checkDelete(String p){++rejectedWrites;throw new SecurityException("delete denied");}
        });
        for(Class<?> type:new Class<?>[]{uw.class,ls.class,sn.class,fd.class,ok.class,lm.class,
                wy.class,xa.class,gr.class,ud.class,pm.class,eq.class,yy.class,in.class})origin(type,jar);
        new gr("SlimeRemovalReference","");int rows=0;
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder",true,null);
        Random shared=(Random)field(holder,"randomNumberGenerator").get(null);
        System.out.println("SLIME_REMOVAL_REFERENCE_BEGIN schema=1 profiles="+profiles.size());
        for(String line:profiles){
            String[] cells=line.split("\t");need(cells.length==7,"seven profile columns");
            String name=cells[0];int[] n=Arrays.stream(cells).skip(1).mapToInt(Integer::parseInt).toArray();
            chunks.clear();children.clear();observed.clear();entropy.clear();
            primary=world("SlimeRemovalReference");primary.B=n[2]==1;
            secondary=n[5]==6?world("SlimeRemovalSecondary"):null;
            parent=new uw(n[2]==2?null:primary);
            String constructor=seed(parent.bs);parent.e(n[0]);parent.e(8.5,64.,8.5);
            parent.Y=n[1];parent.be=n[4]!=0;parent.bs.setSeed(38);primary.r.setSeed(2468);
            if(parent.aI!=null)need(primary.b(parent),"actual initial parent insertion");
            if(n[5]==7)parent.bD.b(16,Integer.valueOf(2));
            observerMode=n[5];primary.a(new Observer(primary));if(secondary!=null)secondary.a(new Observer(secondary));
            for(int step=0;step<n[3];++step){
                String mathBefore=seed(shared),randomBefore=seed(parent.bs),exception="NONE";
                try{parent.K();}catch(NullPointerException e){exception="NPE";}
                    catch(ClassCastException e){exception="CCE";}
                    catch(IllegalStateException e){need(observerMode==1,"declared callback failure only");exception="CALLBACK";}
                Integer size=null;try{size=parent.v();}catch(ClassCastException e){need(exception.equals("CCE"),"reached wrong-type watcher");}
                int loaded=primary.b.size()+(secondary==null?0:secondary.b.size());
                System.out.println("SLIME_REMOVAL "+json(map("profile",name,"step",step,
                    "set_size",n[0],"size",size,"health",parent.Y,
                    "remote",parent.aI==null?null:Boolean.valueOf(parent.aI.B),"exception",exception,
                    "dead",parent.be,"random",seed(parent.bs),"world_loaded",loaded,"children",new ArrayList<>(observed))));
                System.out.println("SLIME_REMOVAL_ENTROPY "+json(map("profile",name,"step",step,
                    "constructor_random",constructor,"parent_before",randomBefore,
                    "math_before",mathBefore,"math_after",seed(shared),"children",new ArrayList<>(entropy))));
                ++rows;
            }
        }
        need(network==0&&nativeLinks==0&&rejectedWrites==0,"external-effect guards");
        System.out.println("Original Slime removal: "+rows+" rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9");
    }
}
