import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.Buffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

/** Declared component inputs over unchanged, genuinely constructed game owners. */
public final class TorchParticleObservation {
    public static int constructor_case = -1;
    public static String constructor_kind = "smoke_default";
    public static int component_case = -1;
    public static xw observed_particle;
    public static nw observed_nw;
    static int network, nativeLinks, writes, execs;
    static Field field(Class<?> type,String name)throws Exception {
        Field value=type.getDeclaredField(name);value.setAccessible(true);return value;
    }
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static float fw(String word){return Float.intBitsToFloat((int)Long.parseUnsignedLong(word,16));}
    static double dw(String word){return Double.longBitsToDouble(Long.parseUnsignedLong(word,16));}
    static String seed(Random value)throws Exception {
        return String.format(Locale.ROOT,"%012x",((AtomicLong)field(Random.class,"seed").get(value)).get());
    }
    static Random math()throws Exception {
        Class<?> type=Class.forName("java.lang.Math$RandomNumberGeneratorHolder");
        return (Random)field(type,"randomNumberGenerator").get(null);
    }
    static float base(xw value)throws Exception{return field(value.getClass(),"a").getFloat(value);}
    static List<String> xyz(double x,double y,double z){return List.of(d(x),d(y),d(z));}
    static List<String> bounds(sn value){return List.of(d(value.aW.a),d(value.aW.b),d(value.aW.c),d(value.aW.d),d(value.aW.e),d(value.aW.f));}
    static Map<String,Object> state(xw value)throws Exception {
        return ConstructorJson.map("position",xyz(value.aM,value.aN,value.aO),
            "previous",xyz(value.aJ,value.aK,value.aL),"velocity",xyz(value.aP,value.aQ,value.aR),
            "bounds",bounds(value),"size",List.of(f(value.bf),f(value.bg),f(value.bh)),"y_size",f(value.bo),
            "texture",value.b,"c",f(value.c),"d",f(value.d),"age",value.e,"lifetime",value.f,
            "scale",f(value.g),"gravity",f(value.h),"rgb",List.of(f(value.i),f(value.j),f(value.k)),
            "base_scale",f(base(value)),"dead",value.be,"ground",value.aX,"no_clip",value.bq,
            "collision",List.of(value.aY,value.aZ,value.ba,value.bb,value.bc,value.bd));
    }
    /** A debugger breakpoint here enables observation only after real World setup. */
    public static void ready(){}
    static xw construct(fd world,String kind,int index)throws Exception {
        constructor_case=index;constructor_kind=kind;
        String before=index<0?null:seed(math());
        xw result;
        if(kind.equals("smoke_default"))result=new xn(world,8.5,64.5,8.5,.125,-.25,.0625);
        else if(kind.equals("flame"))result=new qu(world,8.5,64.5,8.5,.125,-.25,.0625);
        else {
            float scale=kind.equals("large_smoke")?2.5f:kind.equals("smoke_zero")?0:kind.equals("smoke_negative")?-.5f:1;
            require(kind.equals("large_smoke")||kind.equals("smoke_zero")||kind.equals("smoke_negative")||kind.equals("smoke_scaled"),"closed constructor kind");
            result=new xn(world,8.5,64.5,8.5,.125,-.25,.0625,scale);
        }
        System.out.println("TORCH_PARTICLE_CONSTRUCTOR "+ConstructorJson.json(ConstructorJson.map(
            "profile",index,"kind",kind,"math_before",before,"math_after",seed(math()),
            "own_after",seed(result.bs),"state",state(result))));
        return result;
    }
    @SuppressWarnings({"rawtypes","unchecked"})
    static fd world()throws Exception {
        fd result=new fd(new wy(),"TorchParticleReference",xa.a(0),23L);
        Object provider=field(fd.class,"v").get(result);
        Map cache=(Map)field(ok.class,"e").get(provider);
        List loaded=(List)field(ok.class,"f").get(provider);
        cache.clear();loaded.clear();
        for(int x=0;x<2;++x)for(int z=0;z<2;++z){
            lm chunk=new lm(result,new byte[32768],x,z);chunk.n=true;
            cache.put(yy.a(x,z),chunk);loaded.add(chunk);
        }
        field(fd.class,"f").setInt(result,3);
        return result;
    }
    static void light(fd world,int mode)throws Exception {
        for(int x=0;x<2;++x)for(int z=0;z<2;++z){
            lm chunk=world.c(x,z);
            Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);
            Arrays.fill(chunk.f.a,(byte)(mode==1?255:0));
            Arrays.fill(chunk.g.a,(byte)(mode==2?255:0));
        }
    }
    static Object[] references(nw value)throws Exception {
        return new Object[]{field(nw.class,"d").get(value),field(nw.class,"e").get(value),
            field(nw.class,"f").get(value),field(nw.class,"g").get(value),field(nw.class,"y").get(value)};
    }
    static String buffers(Object[] values){
        List<Object> rows=new ArrayList<>();
        for(Object value:values){
            if(value==null)rows.add(null);
            else if(value instanceof Buffer){Buffer b=(Buffer)value;rows.add(List.of(b.position(),b.limit(),b.capacity()));}
            else if(value instanceof int[])rows.add(List.of(((int[])value).length));
            else throw new AssertionError("actual NW reference type");
        }
        return ConstructorJson.json(rows);
    }
    static void prepare(xw p,String[] v)throws Exception {
        boolean tick=v[2].equals("tick");
        p.b(.2f,.2f);p.bf=.1f;p.bo=0;
        p.e(tick?8.5:3,tick?64.5:4,tick?8.5:5);
        p.aJ=tick?-1:1;p.aK=tick?-2:2;p.aL=tick?-3:3;
        p.aP=.125;p.aQ=dw(v[12]);p.aR=.0625;
        if(tick)p.bq=true; // Explicit no-clip component scope, not ordinary collision evidence.
        p.aX=v[9].equals("1");p.be=false;
        p.e=Integer.parseInt(v[3]);p.f=Integer.parseInt(v[4]);p.b=Integer.parseInt(v[5]);
        p.g=fw(v[6]);field(p.getClass(),"a").setFloat(p,fw(v[7]));
        p.c=.25f;p.d=.75f;p.h=.125f;p.i=.25f;p.j=.5f;p.k=.75f;
        xw.l=.25;xw.m=.5;xw.n=.75;
    }
    static void deny(Runnable call){try{call.run();throw new AssertionError("guard denied expected");}catch(SecurityException expected){}}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);require(args.length==2,"jar profiles");
        Path jar=Path.of(args[0]).toRealPath();List<String> profiles=Files.readAllLines(Path.of(args[1]));
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network");}
            public void checkListen(int port){++network;throw new SecurityException("listen");}
            public void checkLink(String library){++nativeLinks;throw new SecurityException("native");}
            public void checkExec(String value){++execs;throw new SecurityException("exec");}
            public void checkWrite(String path){++writes;throw new SecurityException("write");}
            public void checkDelete(String path){checkWrite(path);}
        });
        for(Class<?> type:new Class<?>[]{xn.class,qu.class,xw.class,sn.class,nw.class,fd.class,lm.class,ok.class,wy.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"unchanged original origin");
        fd world=world();ready();
        construct(world,"smoke_default",-1); // Actual warm invocation precedes any shared-Math seed read.
        Constructor<nw> nwConstructor=nw.class.getDeclaredConstructor(int.class);nwConstructor.setAccessible(true);
        int[] prior=new int[2097152];
        int index=0,components=0;long untouchedTotal=0;
        for(String line:profiles){
            if(line.isBlank())continue;String[] v=line.split("\t");require(v.length==14,"fourteen declared profile columns");
            require(v[2].equals("constructor")||v[2].equals("tick")||v[2].equals("draw")||v[2].equals("draw_null_nw")||v[2].equals("brightness"),"closed component operation");
            xw p=construct(world,v[1],index);observed_particle=p;
            if(v[2].equals("constructor")){++index;continue;}
            light(world,Integer.parseInt(v[10]));prepare(p,v);
            if(v[11].equals("1"))p.aI=null;
            Map<String,Object> before=state(p);String ownBefore=seed(p.bs),mathBefore=seed(math()),worldBefore=seed(world.r);
            String exception="none",brightness=null;List<String> packed=new ArrayList<>();Object nwState=null;
            component_case=index;
            if(v[2].startsWith("draw")){
                nw tess=nwConstructor.newInstance(2097152);observed_nw=tess;
                int[] backing=(int[])field(nw.class,"g").get(tess);
                require(backing.length==prior.length,"genuine declared NW backing");
                tess.b();Object[] refs=references(tess);String buffersBefore=buffers(refs);
                System.arraycopy(backing,0,prior,0,backing.length);
                float[] axes=v[13].equals("0")?new float[]{1,1,0,0,0}:new float[]{.75f,.5f,-.25f,.125f,-.375f};
                try{p.a(v[2].equals("draw_null_nw")?null:tess,fw(v[8]),axes[0],axes[1],axes[2],axes[3],axes[4]);}
                catch(NullPointerException expected){exception="NullPointerException";}
                int count=field(nw.class,"o").getInt(tess);require(count==0||count==48,"bounded actual sprite append");
                for(int j=0;j<Math.max(count,100);++j)packed.add(String.format(Locale.ROOT,"%08x",backing[j]));
                for(int j=count;j<backing.length;++j)require(prior[j]==backing[j],"actual own-prior inactive word");
                untouchedTotal+=backing.length-count;Object[] after=references(tess);
                for(int j=0;j<refs.length;++j)require(refs[j]==after[j],"same actual NW owner reference");
                require(buffersBefore.equals(buffers(after)),"actual retained buffer cursors");
                nwState=ConstructorJson.map("vertices",field(nw.class,"h").getInt(tess),"source_vertices",field(nw.class,"p").getInt(tess),
                    "words",count,"uv",field(nw.class,"m").getBoolean(tess),"color",field(nw.class,"l").getBoolean(tess),
                    "normal",field(nw.class,"n").getBoolean(tess),"begun",field(nw.class,"w").getBoolean(tess));
            }else if(v[2].equals("tick")){
                try{p.w_();}catch(ArithmeticException expected){exception="ArithmeticException";}
            }else{
                try{brightness=f(p.a(fw(v[8])));}catch(NullPointerException expected){exception="NullPointerException";}
            }
            component_case=-1;
            require(ownBefore.equals(seed(p.bs))&&mathBefore.equals(seed(math()))&&worldBefore.equals(seed(world.r)),"component consumes no entropy");
            System.out.println("TORCH_PARTICLE_COMPONENT "+ConstructorJson.json(ConstructorJson.map(
                "profile",index,"name",v[0],"kind",v[1],"operation",v[2],"exception",exception,
                "before",before,"after",state(p),"brightness",brightness,"nw",nwState,"packed",packed)));
            p.aI=world;++components;++index;
        }
        require(index==36&&components==30&&network==0&&nativeLinks==0&&writes==0&&execs==0,"complete guarded component cohort");
        System.out.println("TORCH_PARTICLE_GUARD source network=0 native=0 write=0 exec=0");
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));
        deny(()->System.getSecurityManager().checkLink("torch-particle-denial"));deny(()->System.getSecurityManager().checkExec("torch-particle-denial"));
        deny(()->System.getSecurityManager().checkWrite("torch-particle-denial"));
        require(network==2&&nativeLinks==1&&writes==1&&execs==1,"five actual denial controls");
        System.out.println("TORCH_PARTICLE_COMPLETE constructors=37 measured=36 components=30 own-unchanged-words="+untouchedTotal+" network-controls=2 native-controls=1 write-controls=1 exec-controls=1");
    }
}
