import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Authored operands and read-only observations around unchanged original damage. */
public final class MonsterDamageObservation {
    static int network, nativeLinks, writes;
    static final class Observer implements pm {
        final List<Object> sounds = new ArrayList<>();
        public void a(String name, double x, double y, double z, float volume, float pitch) {
            sounds.add(map("name", name, "position", List.of(bits(x),bits(y),bits(z)),
                           "volume", bits(volume), "pitch", bits(pitch)));
        }
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("unexpected particle");}
        public void a(sn e){throw new AssertionError("unexpected entity publication");}
        public void b(sn e){throw new AssertionError("unexpected entity removal");}
        public void a(int x,int y,int z){throw new AssertionError("unexpected block notification");}
        public void b(int x,int y,int z,int a,int b,int c){throw new AssertionError("unexpected range notification");}
        public void e(){throw new AssertionError("unexpected refresh");}
        public void a(String s,int x,int y,int z){throw new AssertionError("unexpected record");}
        public void a(int x,int y,int z,ow t){throw new AssertionError("unexpected tile");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("unexpected level event");}
    }
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(Class<?> type,String name)throws Exception{Field f=type.getDeclaredField(name);f.setAccessible(true);return f;}
    static String bits(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static String bits(float x){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(x));}
    static Map<String,Object> map(Object... pairs){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)r.put((String)pairs[i],pairs[i+1]);return r;}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String)return "\""+((String)v).replace("\\","\\\\").replace("\"","\\\"")+"\"";
        if(v instanceof Map){StringBuilder b=new StringBuilder("{");for(Object item:((Map<?,?>)v).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)item;if(b.length()>1)b.append(',');b.append(json(e.getKey())).append(':').append(json(e.getValue()));}return b.append('}').toString();}
        if(v instanceof List){StringBuilder b=new StringBuilder("[");for(Object x:(List<?>)v){if(b.length()>1)b.append(',');b.append(json(x));}return b.append(']').toString();}
        return v.toString();
    }
    static void original(Class<?> c,Path jar)throws Exception{require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive origin: "+c.getName());}
    @SuppressWarnings({"removal","unchecked"})
    public static void main(String[] args)throws Exception{
        require(args.length==1,"original JAR");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p){++writes;throw new SecurityException("write denied");}
            @Override public void checkDelete(String p){++writes;throw new SecurityException("delete denied");}
        });
        for(Class<?> c:new Class<?>[]{fd.class,wy.class,xa.class,gr.class,sn.class,ls.class,ii.class,gz.class,uz.class,fr.class,gb.class,cn.class,bx.class,wh.class,eq.class,in.class})original(c,jar);
        new gr("MonsterDamageReference","");
        fd world=new fd(new wy(),"MonsterDamageReference",xa.a(0),23L);
        Observer observer=new Observer();world.a(observer);
        String[] kinds={"uz","fr","gb","cn"};
        String[] names={"mob.zombiehurt","mob.skeletonhurt","mob.creeper","mob.spider"};
        for(int i=0;i<kinds.length;++i){
            gz actor=(gz)Class.forName(kinds[i]).getConstructor(fd.class).newInstance(world);
            bx source=new bx(world);wh old=new wh(world);
            actor.e(8.5,64.5,8.5);source.e(11.5,64.5,12.5);
            actor.aS=30;actor.aP=.25;actor.aQ=-.375;actor.aR=-.125;
            actor.Y=20;actor.E=20;actor.by=0;field(ls.class,"au").setInt(actor,0);
            actor.aa=0;actor.ab=0;actor.bb=false;world.B=false;
            field(ii.class,"d").set(actor,old);observer.sounds.clear();
            require(actor.aG==null&&actor.aH==null,"ordinary unmounted construction");
            boolean result=actor.a(source,4);
            require(result&&observer.sounds.size()==1,"one completed nonfatal damage/sound");
            Map<?,?> sound=(Map<?,?>)observer.sounds.get(0);
            require(names[i].equals(sound.get("name")),"actual concrete sound");
            System.out.println("MONSTER_DAMAGE_RETURN "+json(map(
                "sound",names[i],"result",result,"health",actor.Y,"previous_health",actor.Z,
                "resistance",actor.by,"last_damage",field(ls.class,"au").getInt(actor),
                "hurt",actor.aa,"max_hurt",actor.ab,"angle",bits(actor.ac),
                "velocity",List.of(bits(actor.aP),bits(actor.aQ),bits(actor.aR)),
                "attacked",actor.bb,"target_is_source",field(ii.class,"d").get(actor)==source)));
            System.out.println("MONSTER_DAMAGE_SOUND "+json(map("class",kinds[i],"arguments",observer.sounds)));
        }
        require(network==0&&nativeLinks==0&&writes==0,"external-effect guards");
        System.out.println("Original Monster damage: 4 rows; network=0; native=0; write=0; genuine_world=1; seed_reset=0");
    }
}
