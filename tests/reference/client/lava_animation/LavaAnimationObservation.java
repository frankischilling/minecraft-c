import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Authored finite field/holder observer. No game or RNG body is replaced. */
public final class LavaAnimationObservation {
    static final String[] NAMES={"flat","flat-color","waves","waves-color","waves-repeat","subnormal","nan","nan-color",
        "null-g","null-h","null-i","null-j","null-a","short-a2","short-g0","short-h1","short-i0","short-j0","null-h-i","short-j1","clamp","clamp-color","signed-zero"};
    static final String[] FAULTS={"","","","","","","","","g","h","i","j","a","a2","g0","h1","i0","j0","hi","j1","","",""};
    static final int[] PATTERNS={0,0,1,1,1,2,3,3,0,0,0,0,0,0,0,0,0,0,0,0,4,4,5};
    static final int[] FRAMES={2,1,5,3,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    static Random math;
    static Object get(Object object,Class<?> owner,String name) throws Exception {
        Field field=owner.getDeclaredField(name);field.setAccessible(true);return field.get(object);
    }
    static void set(Object object,Class<?> owner,String name,Object value) throws Exception {
        Field field=owner.getDeclaredField(name);field.setAccessible(true);field.set(object,value);
    }
    static void require(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static void initialize(cg lava,int which) throws Exception {
        int pattern=PATTERNS[which];
        float[] g=(float[])get(lava,cg.class,"g"),h=(float[])get(lava,cg.class,"h"),i=(float[])get(lava,cg.class,"i"),j=(float[])get(lava,cg.class,"j");
        for(int n=0;n<256;++n) {
            g[n]=pattern==1?(n*17%41-20)*0.0625f:0;
            h[n]=pattern==1?-9.0f:0;i[n]=pattern==1?(n%7-3)*0.03125f:0;j[n]=pattern==1?(n%5-2)*0.125f:0;
            if(pattern==2){g[n]=Float.intBitsToFloat((n%7+1)|((n&1)!=0?0x80000000:0));i[n]=Float.intBitsToFloat(n%3+1);}
            if(pattern==4)i[n]=Float.intBitsToFloat(new int[]{0x40000000,0xc0000000,0x7f800000,0xff800000}[n%4]);
            if(pattern==5){g[n]=-0.0f;h[n]=-0.0f;i[n]=-0.0f;j[n]=-0.0f;}
        }
        if(pattern==3){g[0]=Float.intBitsToFloat(0x7fc12345);g[1]=Float.intBitsToFloat(0xffc54321);g[15]=Float.intBitsToFloat(0x7fc00abc);i[0]=Float.intBitsToFloat(0xffc12345);j[0]=Float.intBitsToFloat(0x7fcabcde);}
    }
    static int id(Object object,IdentityHashMap<Object,Integer> ids) {if(object==null)return 0;Integer result=ids.get(object);require(result!=null,"array identity");return result;}
    static void trace(PrintWriter out,cg lava,int which,int frame,String outcome,IdentityHashMap<Object,Integer> ids) throws Exception {
        long state=((AtomicLong)get(math,Random.class,"seed")).get();
        out.printf(Locale.ROOT,"CASE\t%s\t%d\t%s\t%012x\t%016x\t%d\t%d%n",NAMES[which],frame,outcome,
            state,Double.doubleToRawLongBits((Double)get(math,Random.class,"nextNextGaussian")),(Boolean)get(math,Random.class,"haveNextNextGaussian")?1:0,lava.c?1:0);
        for(String name:new String[]{"g","h","i","j"}) {
            float[] array=(float[])get(lava,cg.class,name);
            StringBuilder row=new StringBuilder("ARRAY\t"+NAMES[which]+"\t"+frame+"\t"+name+"\t"+id(array,ids)+"\t"+(array==null?-1:array.length)+"\t");
            if(array!=null)for(float value:array)row.append(String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value)));
            out.println(row);
        }
        byte[] bytes=lava.a;
        StringBuilder row=new StringBuilder("RGBA\t"+NAMES[which]+"\t"+frame+"\t"+id(bytes,ids)+"\t"+(bytes==null?-1:bytes.length)+"\t");
        if(bytes!=null)for(byte value:bytes)row.append(String.format(Locale.ROOT,"%02x",value&255));
        out.println(row);require(!out.checkError(),"trace output");
    }
    static void binding(Path classes,Path path) throws Exception {
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(path,StandardCharsets.UTF_8))) {
            for(String name:Files.readAllLines(classes,StandardCharsets.US_ASCII)) {
                Class<?> type=Class.forName(name,false,LavaAnimationObservation.class.getClassLoader());byte[] bytes;
                try(InputStream in=type.getResourceAsStream("/"+name.replace('.','/')+".class")){require(in!=null,"class resource");bytes=in.readAllBytes();}
                String origin=type.getClassLoader()==null?"jrt:/java.base":type.getProtectionDomain().getCodeSource().getLocation().toURI().toString();
                out.println(name+"\t"+bytes.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))+"\t"+origin);
            }
            require(!out.checkError(),"binding output");
        }
    }
    public static void main(String[] args) throws Exception {
        require(args.length==3,"trace classes binding");
        Class<?> holder=Class.forName("java.lang.Math$RandomNumberGeneratorHolder");
        math=(Random)get(null,holder,"randomNumberGenerator");
        require(math.getClass()==Random.class&&Random.class.getClassLoader()==null&&Math.class.getClassLoader()==null,"genuine bootstrap Math/Random holder");
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(Path.of(args[0]),StandardCharsets.UTF_8))) {
            for(int which=0;which<NAMES.length;++which) {
                cg lava=new cg();
                require(lava.b==237&&!lava.c&&lava.d==0&&lava.e==1&&lava.f==0,"actual cg/aw defaults");
                require(lava.a!=null&&lava.a.length==1024,"aw byte extent");
                for(byte value:lava.a)require(value==0,"initial aw bytes");
                for(String name:new String[]{"g","h","i","j"}) {
                    float[] array=(float[])get(lava,cg.class,name);require(array!=null&&array.length==256,"cg array extent");
                    for(float value:array)require(Float.floatToRawIntBits(value)==0,"initial cg floats");
                }
                out.printf(Locale.ROOT,"CTOR\t%s\t%d\t%d\t%d\t%d\t%d%n",NAMES[which],lava.b,lava.c?1:0,lava.d,lava.e,lava.f);
                lava.c=NAMES[which].endsWith("-color");initialize(lava,which);
                Math.random();math.setSeed(0x123456789abL+which*101L); // Declared genuine-holder fixture.
                IdentityHashMap<Object,Integer> ids=new IdentityHashMap<>();ids.put(lava.a,1);
                int number=2;for(String name:new String[]{"g","h","i","j"})ids.put(get(lava,cg.class,name),number++);
                String fault=FAULTS[which];
                if(fault.equals("hi")){set(lava,cg.class,"h",null);set(lava,cg.class,"i",null);}
                else if(!fault.isEmpty()) {
                    Object extra=fault.length()==1?null:fault.charAt(0)=='a'?new byte[fault.charAt(1)-'0']:new float[fault.charAt(1)-'0'];
                    if(extra!=null)ids.put(extra,6);
                    if(fault.charAt(0)=='a')lava.a=(byte[])extra;else set(lava,cg.class,fault.substring(0,1),extra);
                }
                trace(out,lava,which,0,"SETUP",ids);
                for(int frame=1;frame<=FRAMES[which];++frame) {
                    String outcome="OK";
                    try{lava.a();require(fault.isEmpty(),"missing array failure");}
                    catch(NullPointerException error){require(fault.length()==1||fault.equals("hi"),"unexpected NPE");require(error.getStackTrace()[0].getClassName().equals("cg"),"NPE owner");outcome="NULL";}
                    catch(ArrayIndexOutOfBoundsException error){require(fault.length()==2&&!fault.equals("hi"),"unexpected bounds");require(error.getStackTrace()[0].getClassName().equals("cg"),"bounds owner");outcome="BOUNDS";}
                    trace(out,lava,which,frame,outcome,ids);
                }
            }
            out.println("END\t"+NAMES.length);require(!out.checkError(),"completed trace");
        }
        binding(Path.of(args[1]),Path.of(args[2]));System.out.println("Lava animation original: "+NAMES.length+" cases");
    }
}
