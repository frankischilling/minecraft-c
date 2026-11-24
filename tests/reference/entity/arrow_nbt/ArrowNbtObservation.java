import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Authored inputs around complete unchanged Arrow NBT, parent and factory calls. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class ArrowNbtObservation {
    static volatile int active=-1;
    static sl current;
    static nu input;
    static sn result;
    static fd worldA,worldB;
    static ls shooter;
    static eq originalBounds;
    static int[] profile;
    static final String[] OWN={"xTile","yTile","zTile","inTile","inData","shake","inGround","player"};
    static final int[][] WRITE={
        {101,-202,303,404,505,222}, {65535,65536,-65537,256,257,-1},
        {-65536,-65535,65537,-256,-257,255}, {Integer.MIN_VALUE,Integer.MAX_VALUE,Integer.MIN_VALUE,Integer.MAX_VALUE,Integer.MIN_VALUE,Integer.MAX_VALUE},
        {7,8,9,10,11,12}, {7,8,9,10,11,12}, {7,8,9,10,11,12}, {7,8,9,10,11,12}};
    static void need(boolean b,String s){ArrowConstructionObservation.need(b,s);}
    static Object get(Object o,Class<?> c,String n)throws Exception{return ArrowConstructionObservation.get(o,c,n);}
    static void put(Object o,Class<?> c,String n,Object v)throws Exception{ArrowConstructionObservation.put(o,c,n,v);}
    static Map<String,Object> map(Object... a){return ConstructorJson.map(a);}
    static String d(double v){return ArrowConstructionObservation.d(v);}
    static String f(float v){return ArrowConstructionObservation.f(v);}
    static Map<String,ij> compound(nu n)throws Exception{return (Map<String,ij>)get(n,nu.class,"a");}
    static Object scalar(Object v){
        if(v==null||v instanceof Boolean||v instanceof Byte||v instanceof Short||v instanceof Integer)return v;
        if(v instanceof Long)return map("J",String.format(Locale.ROOT,"%016x",(Long)v));
        if(v instanceof Float)return map("F",f((Float)v));if(v instanceof Double)return map("D",d((Double)v));
        if(v instanceof String){List<Integer> a=new ArrayList<>();for(char c:((String)v).toCharArray())a.add((int)c);return map("utf16",a);}
        throw new AssertionError("declared scalar tag");
    }
    static Object tag(ij t)throws Exception{
        if(t==null)return null;
        if(t instanceof nu){Map<String,ij> source=compound((nu)t);List<Object> raw=new ArrayList<>();TreeMap<String,Object> values=new TreeMap<>();
            if(source!=null)for(Map.Entry<String,ij> e:source.entrySet()){Object value=tag(e.getValue());raw.add(map("key",scalar(e.getKey()),"tag",value));values.put(e.getKey(),value);}
            return map("kind",10,"map_null",source==null,"values",values,"source_order",raw);}
        if(t instanceof sp){List<Object> values=new ArrayList<>();for(Object e:(List)get(t,sp.class,"a"))values.add(tag((ij)e));
            return map("kind",9,"element_type",get(t,sp.class,"b"),"values",values);}
        Object value=get(t,t.getClass(),"a");int kind=value instanceof Byte?1:value instanceof Short?2:value instanceof Integer?3:value instanceof Long?4:value instanceof Float?5:value instanceof Double?6:value instanceof String||value==null&&t instanceof xb?8:-1;
        need(kind>=1,"prepared scalar tag kind");return map("kind",kind,"value",scalar(value));
    }
    static Object bounds(eq b){return b==null?null:List.of(d(b.a),d(b.b),d(b.c),d(b.d),d(b.e),d(b.f));}
    static Object snapshot(sn e)throws Exception{
        if(e==null)return null;
        return map("entity",ArrowConstructionObservation.snapshot(e),"base",map("fall",f((Float)get(e,sn.class,"bk")),
            "fire",get(e,sn.class,"bv"),"air",get(e,sn.class,"bz"),"on_ground",get(e,sn.class,"aX"),"y_size",f((Float)get(e,sn.class,"bo"))),
            "bounds_same",e.aW==originalBounds,"original_bounds",bounds(originalBounds));
    }
    static Object context()throws Exception{return ArrowConstructionObservation.context();}
    static sp doubles(double... values)throws Exception{sp a=new sp();for(double v:values){nu n=new nu();n.a("v",v);a.a(compound(n).get("v"));}return a;}
    static sp floats(float... values)throws Exception{sp a=new sp();for(float v:values){nu n=new nu();n.a("v",v);a.a(compound(n).get("v"));}return a;}
    static nu own(int mode)throws Exception{
        nu n=new nu();if(mode==5)return n;
        int[] coordinates=mode==1?new int[]{-32768,-1,0}:mode==2?new int[]{32767,0,1}:new int[]{17,-19,32767};
        for(int j=0;j<3;j++)n.a(OWN[j],(short)coordinates[j]);
        n.a("inTile",(byte)(mode==3?-1:-128));n.a("inData",(byte)-1);n.a("shake",(byte)-2);
        n.a("inGround",(byte)(mode==3?-1:mode==4?2:1));n.a("player",(byte)(mode==3?-1:mode==4?2:1));
        if(mode==6)compound(n).remove("xTile");return n;
    }
    static nu parent(int mode)throws Exception{
        nu n=own(0);n.a("Pos",doubles(8.5,64.9375,-8.25));n.a("Motion",doubles(.1,.2,.3));n.a("Rotation",floats(450,-450));
        n.a("FallDistance",3.5f);n.a("Fire",(short)-19);n.a("Air",(short)298);n.a("OnGround",true);
        if(mode==1)n.a("Rotation",(byte)7);
        if(mode==2)compound(n).remove("Rotation");
        if(mode==3)n.a("Motion",doubles(.1));
        if(mode==4){sp a=doubles(.1);nu q=new nu();q.a("v",.2f);a.a(compound(q).get("v"));a.a(compound(doublesTag(.3)).get("v"));n.a("Motion",a);}
        if(mode==5){sp a=doubles(.1);((List)get(a,sp.class,"a")).add(null);a.a(compound(doublesTag(.3)).get("v"));n.a("Motion",a);}
        if(mode==6){sp a=new sp();nu q=new nu();q.a("v",8.5f);a.a(compound(q).get("v"));n.a("Pos",a);}
        if(mode==7)n.a("player",(short)1);
        if(mode==8){n.a("Motion",doubles(Double.POSITIVE_INFINITY,Double.longBitsToDouble(0x7ff8000000001234L),-0.0));n.a("Rotation",floats(810,-810));}
        if(mode==9){n.a("Motion",doubles(-10.000000000000002,10.0,-10.0));n.a("Rotation",floats(-0.0f,Float.intBitsToFloat(0x7fc01234)));}
        return n;
    }
    static nu doublesTag(double v){nu n=new nu();n.a("v",v);return n;}
    static void prepare(int[] p)throws Exception{
        ArrowConstructionObservation.selectedWorld=p[4]==0?null:worldA;
        result=null;current=null;originalBounds=null;
        if(p[1]!=5){current=new sl(ArrowConstructionObservation.selectedWorld);ArrowConstructionObservation.current=current;
            current.c(7.25,70.5,-3.75,11.25f,-12.5f);current.aP=.25;current.aQ=-.375;current.aR=.5;
            put(current,sl.class,"c",shooter);put(current,sl.class,"j",37);put(current,sl.class,"k",75);
            int[] w=WRITE[p[1]==1?p[2]:0];for(int j=0;j<6;j++)put(current,sl.class,new String[]{"d","e","f","g","h","b"}[j],w[j]);
            current.a=p[1]!=1||p[2]<4||(p[2]&1)!=0;put(current,sl.class,"i",p[1]!=1||p[2]<4||(p[2]&2)!=0);
            put(current,sn.class,"bk",2.25f);put(current,sn.class,"bv",65537);put(current,sn.class,"bz",-65537);put(current,sn.class,"aX",true);
            put(current,sn.class,"bo",p[1]==3&&p[2]==1?.3125f:0f);
            if(p[1]==4&&p[2]==1)current.be=true;originalBounds=current.aW;
        }else ArrowConstructionObservation.current=null;
        input=p[1]==0?own(p[2]):p[1]==2?parent(p[2]):p[1]==5?parent(0):new nu();
        if(p[1]==5){input.a("id",p[2]==3?"NoSuchArrowType":"Arrow");if(p[2]==1)for(String name:OWN)compound(input).remove(name);if(p[2]==2)input.a("player",(short)1);}
        if((p[1]==1||p[1]==3||p[1]==4))input.a("retained_unknown",1234567);
        if(p[3]>=1&&p[3]<=8)input.a(OWN[p[3]-1],123);
        if(p[3]==9)input=null;if(p[3]==10)compound(input).put("xTile",null);if(p[3]==11)put(input,nu.class,"a",null);
    }
    @SuppressWarnings("removal")public static void main(String[] args)throws Exception{
        need(args.length==4,"archive work profiles classes");ArrowConstructionObservation.archive=Path.of(args[0]).toRealPath();ArrowConstructionObservation.directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new ArrowConstructionObservation.Guard());ArrowConstructionObservation.bind(Path.of(args[3]));
        ArrowConstructionObservation.shared=(Random)get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");ArrowConstructionObservation.bootstrap();
        worldA=ArrowConstructionObservation.worldA;worldB=ArrowConstructionObservation.worldB;shooter=ArrowConstructionObservation.shooters[1];ArrowConstructionObservation.shooter=shooter;
        int count=0,returnedCount=0,failedCount=0;
        try{for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            profile=Arrays.stream(line.trim().split("\\s+")).mapToInt(Integer::parseInt).toArray();need(profile.length==5&&profile[0]==count,"ordered complete profiles");prepare(profile);
            Object before=context(),arrowBefore=snapshot(current),tagBefore=tag(input);boolean returned=false;Boolean saved=null;String exception="NONE",message=null;List<String> frames=new ArrayList<>();
            active=count;try{switch(profile[1]){case 0:current.a(input);break;case 1:current.b(input);break;case 2:current.e(input);break;case 3:current.d(input);break;case 4:saved=current.c(input);break;case 5:result=jc.a(input,ArrowConstructionObservation.selectedWorld);break;default:throw new AssertionError("entry");}returned=true;}
            catch(Throwable failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();for(StackTraceElement frame:failure.getStackTrace()){if(frame.getClassName().equals("ArrowNbtObservation"))break;frames.add(frame.getClassName()+"."+frame.getMethodName());}}
            finally{active=-1;}
            if(returned)++returnedCount;else ++failedCount;
            List<Integer> words=new ArrayList<>();for(int word:profile)words.add(word);
            System.out.println("ARROW_NBT "+ConstructorJson.json(map("schema","ARROW_NBT_RAW_V1","case",count,"input",words,"domain",count<44?"paired":"source-only",
                "returned",returned,"exception",exception,"saved",saved,"before",before,"after",context(),"arrow_before",arrowBefore,"arrow_after",snapshot(current),
                "result",snapshot(result),"tag_before",tagBefore,"tag_after",tag(input),"exception_message",message,"exception_frames",frames,
                "source_raw",map("arrow",ArrowConstructionObservation.raw(current),"result",ArrowConstructionObservation.raw(result)))));++count;
        }need(count==48&&ArrowConstructionObservation.network==0&&ArrowConstructionObservation.nativeLinks==0&&ArrowConstructionObservation.rejectedWrites==0&&ArrowConstructionObservation.execs==0,"complete source/effect guards");
        System.out.println("ARROW_NBT_COMPLETE cases=48 paired=44 source_only=4 returned="+returnedCount+" failed="+failedCount+" network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0");
        }finally{ArrowConstructionObservation.workers();ArrowConstructionObservation.app.J=false;for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker retirement");}}
    }
}
