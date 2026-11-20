import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Authored operands; all constructors, ticks, movement and sprite bodies are original. */
public final class ExplosionObservation {
    public static int constructor_case = -1;
    static int network, nativeLinks, writes;
    static Field field(Class<?> type, String name) throws Exception {
        Field value = type.getDeclaredField(name); value.setAccessible(true); return value;
    }
    static void require(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static String d(double value) { return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value)); }
    static String f(float value) { return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value)); }
    static String seed(Random value) throws Exception {
        return String.format(Locale.ROOT,"%012x",((AtomicLong)field(Random.class,"seed").get(value)).get());
    }
    static Random math() throws Exception {
        Class<?> type = Class.forName("java.lang.Math$RandomNumberGeneratorHolder");
        return (Random)field(type,"randomNumberGenerator").get(null);
    }
    static List<String> xyz(double x,double y,double z) { return List.of(d(x),d(y),d(z)); }
    static List<String> bounds(sn value) { return List.of(d(value.aW.a),d(value.aW.b),d(value.aW.c),d(value.aW.d),d(value.aW.e),d(value.aW.f)); }
    static Map<String,Object> state(gy value) throws Exception {
        return ConstructorJson.map("position",xyz(value.aM,value.aN,value.aO),
            "previous",xyz(value.aJ,value.aK,value.aL),"velocity",xyz(value.aP,value.aQ,value.aR),
            "bounds",bounds(value),"size",List.of(f(value.bf),f(value.bg),f(value.bh)),
            "texture",value.b,"c",f(value.c),"d",f(value.d),"age",value.e,"lifetime",value.f,
            "scale",f(value.g),"gravity",f(value.h),"rgb",List.of(f(value.i),f(value.j),f(value.k)),
            "dead",value.be,"ground",value.aX,"own_seed",seed(value.bs));
    }
    static gy construct(fd world) throws Exception {
        String before = seed(math());
        gy result = new gy(world,8.5,64.5,8.5,.125,-.25,.0625);
        System.out.println("EXPLOSION_CONSTRUCTOR " + ConstructorJson.json(ConstructorJson.map(
            "profile",constructor_case,"math_before",before,"math_after",seed(math()),
            "state",state(result))));
        return result;
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd world() throws Exception {
        fd result = new fd(new wy(),"ExplosionReference",xa.a(0),23L);
        Object provider = field(fd.class,"v").get(result);
        Map cache = (Map)field(ok.class,"e").get(provider);
        List loaded = (List)field(ok.class,"f").get(provider);
        cache.clear(); loaded.clear();
        lm chunk = new lm(result,new byte[32768],0,0); chunk.n = true;
        Arrays.fill(chunk.f.a,(byte)255); Arrays.fill(chunk.g.a,(byte)255);
        cache.put(yy.a(0,0),chunk); loaded.add(chunk);
        return result;
    }
    static void ticks() throws Exception {
        String[] names = {"age0","age7","age8","negative-age","wrap-age","zero-life","ground-age0"};
        int[] ages = {0,7,8,-1,Integer.MAX_VALUE,0,0};
        for (int index=0; index<names.length; ++index) {
            constructor_case = index;
            gy p = construct(null);
            p.b(.2f,.2f); p.bf = .1f; p.bo = 0;
            p.e(8.5,64.5,8.5); p.aJ = -1; p.aK = -2; p.aL = -3;
            p.aP=.125; p.aQ=-.25; p.aR=.0625;
            p.bq=true; p.aX=index==6; p.be=false;
            p.e=ages[index]; p.f=index==5?0:8;
            String ownBefore=seed(p.bs), mathBefore=seed(math());
            String exception="none";
            try {p.w_();} catch (ArithmeticException expected) {exception="ArithmeticException";}
            require(seed(p.bs).equals(ownBefore)&&seed(math()).equals(mathBefore),"tick consumes no entropy");
            System.out.println("EXPLOSION_TICK "+ConstructorJson.json(ConstructorJson.map(
                "profile",names[index],"exception",exception,"age",p.e,"lifetime",p.f,
                "icon",p.b,"dead",p.be,"ground",p.aX,"position",xyz(p.aM,p.aN,p.aO),
                "previous",xyz(p.aJ,p.aK,p.aL),"velocity",xyz(p.aP,p.aQ,p.aR),"bounds",bounds(p))));
        }
    }
    static void draws(fd world) throws Exception {
        int[] icons={0,7,16,-1,256,7};
        float[] partials={0,.5f,1,.5f,1,-.5f}, scales={1,.5f,2,1,1,-1};
        Constructor<nw> nwConstructor=nw.class.getDeclaredConstructor(int.class);
        nwConstructor.setAccessible(true);
        for(int index=0; index<icons.length; ++index) {
            constructor_case=7+index;
            gy p=construct(world);
            p.e(3,4,5); p.aJ=1; p.aK=2; p.aL=3;
            p.b=icons[index]; p.g=scales[index]; p.i=.25f; p.j=.5f; p.k=.75f;
            xw.l=.25; xw.m=.5; xw.n=.75;
            float[] orientation=index<4?new float[]{1,1,0,0,0}:new float[]{.75f,.5f,-.25f,.125f,-.375f};
            String before=ConstructorJson.json(state(p)), ownBefore=seed(p.bs), mathBefore=seed(math()),worldBefore=seed(world.r);
            float brightness=p.a(partials[index]);
            /* A fresh genuine NW invokes its original private constructor. No
             * draw/flush is called: the measured body writes CPU vertices only. */
            nw buffer=nwConstructor.newInstance(256); buffer.b();
            p.a(buffer,partials[index],orientation[0],orientation[1],orientation[2],orientation[3],orientation[4]);
            require(field(nw.class,"h").getInt(buffer)==6 && field(nw.class,"o").getInt(buffer)==48 &&
                    field(nw.class,"p").getInt(buffer)==4,"actual default NW quad expansion");
            int[] words=(int[])field(nw.class,"g").get(buffer);
            StringBuilder packed=new StringBuilder();
            for(int j=0;j<48;++j){if(j!=0)packed.append(',');packed.append(String.format(Locale.ROOT,"%08x",words[j]));}
            require(before.equals(ConstructorJson.json(state(p)))&&ownBefore.equals(seed(p.bs))&&
                    mathBefore.equals(seed(math()))&&worldBefore.equals(seed(world.r)),"draw reads particle and entropy");
            System.out.println("EXPLOSION_DRAW profile="+index+" brightness="+f(brightness)+" words="+packed);
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
            public void checkListen(int port){++network;throw new SecurityException("listener");}
            public void checkLink(String library){++nativeLinks;throw new SecurityException("native link");}
            public void checkWrite(String path){++writes;throw new SecurityException("write");}
        });
        for(Class<?> type:new Class<?>[]{gy.class,xw.class,sn.class,nw.class,fd.class,lm.class,ok.class,wy.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive origin "+type.getName());
        fd world=world();
        constructor_case=-1;
        new gy(null,8.5,64.5,8.5,.125,-.25,.0625); // Genuine warm invocation initializes shared Math in source order.
        ticks(); draws(world);
        require(network==0&&nativeLinks==0&&writes==0,"CPU-only external-effect guards");
        System.out.println("Original explosion particle: 7 ticks; 6 draws; network=0; native=0; write=0");
    }
}
