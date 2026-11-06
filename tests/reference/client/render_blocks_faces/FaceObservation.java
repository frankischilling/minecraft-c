import java.lang.reflect.*;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;

/** Unchanged CV face/NW methods over explicitly prepared actual source fields. */
public final class FaceObservation {
    static int network,nativeLinks,index;
    static Class<?> cv= cv.class, block=uu.class, nw=nw.class;
    static Object renderer,tess,stone;static int[] originalWords;static int originalCapacity;
    static final double[][] BOUNDS={{0,0,0,1,1,1},{.125,.25,.375,.75,.875,1},{-.125,-.25,-.375,1.125,1.25,1.375},{.875,.75,.625,.125,.25,.375}};
    static final String[] BOUNDS_NAMES={"bs","bt","bu","bv","bw","bx"};
    static final String[] ROTATIONS={"l","k","g","h","j","i"};
    static final String[] COLORS={"P","Q","R","S","T","U","V","W","X","Y","Z","aa"};
    static final int[] COLOR_BITS={0x3e000000,0x3e800000,0x3f000000,0x3f400000,0x3f600000,0x3f200000,0x3ec00000,0x3d800000,0x3f800000,0x3f000001,0xbe800000,0x7fc12345};
    static final int[][] FLOATS={{0,0,0},{0x3f000000,0x3e800000,0x3f400000},{0xbe800000,0x3fa00000,0x3f800000},{0x7fc12345,0x7f800000,0xff800000},{0x7f812345,0xffc54321,1},{0x7f7fffff,0xff7fffff,0x80000000},{0x3f000001,0x3effffff,0x3f800001},{0x3b808081,0x3f7fffff,0x80000001},{0x7149f2ca,0xf149f2ca,0x3f600000},{0x00800000,0x80800000,0x3f7f7f7f},{0x3f7ffffe,0x3b808080,0x3b808082},{0x4b008081,0xcb008081,0x3eaaaaab}};
    static void check(boolean b,String m){if(!b)throw new AssertionError(m);}
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(String n)throws Exception{return field(nw,n).get(tess);}
    static void set(String n,Object v)throws Exception{field(nw,n).set(tess,v);}
    static void prepare(int bounds,int side,int rotation,boolean flip,boolean ao,int textureCase,int kind)throws Exception{
        renderer=cv.getConstructor().newInstance();
        for(int i=0;i<6;++i)field(block,BOUNDS_NAMES[i]).setDouble(stone,BOUNDS[bounds][i]);
        field(cv,ROTATIONS[side]).setInt(renderer,rotation);field(cv,"e").setBoolean(renderer,flip);field(cv,"m").setBoolean(renderer,ao);
        field(cv,"d").setInt(renderer,textureCase==3?302:-1);
        for(int i=0;i<COLORS.length;++i)field(cv,COLORS[i]).setFloat(renderer,Float.intBitsToFloat(COLOR_BITS[i]));
        int capacity=kind>=2?new int[]{0,4,8,16,32,47}[kind-2]:originalCapacity;
        int[] words=kind==1?null:kind>=2?new int[capacity]:originalWords;
        if(words!=null)Arrays.fill(words,0,Math.min(words.length,48),0x5a6b7c8d);
        set("g",words);set("B",capacity);set("w",false);
        nw.getMethod("b",double.class,double.class,double.class).invoke(tess,bounds==3?16777216.0:16.0,bounds==3?-64.0:-48.0,bounds==3?-256.0:-32.0);
        nw.getMethod("b").invoke(tess);
        nw.getMethod("a",int.class,int.class,int.class).invoke(tess,31,127,233);
    }
    static void row(int side,String exception)throws Exception{
        int[] words=(int[])get("g");StringJoiner data=new StringJoiner(",");
        if(words==null)data.add("N");else if(words.length==0)data.add("-");else for(int i=0;i<Math.min(words.length,48);++i)data.add(String.format(Locale.ROOT,"%08x",words[i]));
        System.out.printf(Locale.ROOT,"FACE %d %d prepared %d %d %d %08x %016x %016x %d %d %d %d %s %s%n",index++,side,(int)get("h"),(int)get("p"),(int)get("o"),(int)get("k"),Double.doubleToRawLongBits((double)get("i")),Double.doubleToRawLongBits((double)get("j")),(boolean)get("l")?1:0,(boolean)get("m")?1:0,(boolean)get("n")?1:0,(boolean)get("q")?1:0,exception,data);
    }
    static void face(int bounds,int side,int rotation,boolean flip,boolean ao,int textureCase,int kind)throws Exception{
        prepare(bounds,side,rotation,flip,ao,textureCase,kind);int texture=textureCase==1?-1:textureCase==2?Integer.MAX_VALUE:83;
        String exception="NONE";
        try{cv.getMethod(""+(char)('a'+side),block,double.class,double.class,double.class,int.class).invoke(renderer,kind==0?null:stone,bounds==3?-16777215.75:-3.0,bounds==3?65.0:50.0,bounds==3?256.75:39.0,texture);}
        catch(InvocationTargetException e){Throwable cause=e.getCause();if(cause instanceof NullPointerException)exception="NPE";else if(cause instanceof ArrayIndexOutOfBoundsException)exception="AIOOBE";else throw e;}
        check(exception.equals(kind==-1||kind==7?"NONE":kind<2?"NPE":"AIOOBE"),"wrong source exception");row(side,exception);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}public void checkListen(int p){++network;throw new SecurityException("listen denied");}public void checkWrite(String p){throw new SecurityException("write denied");}public void checkLink(String p){++nativeLinks;throw new SecurityException("native denied");}});
        for(Class<?> c:new Class<?>[]{cv,block,nw})check(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");
        stone=field(block,"u").get(null);tess=field(nw,"a").get(null);originalWords=(int[])get("g");originalCapacity=(int)get("B");
        for(int bounds=0;bounds<4;++bounds)for(int side=0;side<6;++side)for(int rotation=0;rotation<4;++rotation)for(int flip=0;flip<2;++flip)for(int ao=0;ao<2;++ao)for(int texture=0;texture<4;++texture)face(bounds,side,rotation,flip!=0,ao!=0,texture,-1);
        for(int rotation:new int[]{-1,4})for(int side=0;side<6;++side)for(int ao=0;ao<2;++ao)face(1,side,rotation,true,ao!=0,0,-1);
        for(int side=0;side<6;++side)for(int ao=0;ao<2;++ao)for(int kind=0;kind<8;++kind)face(1,side,0,false,ao!=0,0,kind);
        check(index==1656,"face count");
        int colorIndex=0;for(int blocked=0;blocked<2;++blocked)for(int[] bits:FLOATS){prepare(0,0,0,false,false,0,-1);set("q",blocked!=0);nw.getMethod("a",float.class,float.class,float.class).invoke(tess,Float.intBitsToFloat(bits[0]),Float.intBitsToFloat(bits[1]),Float.intBitsToFloat(bits[2]));System.out.printf(Locale.ROOT,"FCOLOR %d %d %08x %d %d NONE%n",colorIndex++,blocked,(int)get("k"),(boolean)get("l")?1:0,(boolean)get("q")?1:0);}
        check(colorIndex==24&&network==0&&nativeLinks==0,"external attempt/count");
        System.out.println("Original CV faces: 1656 rows; 24 colors; network=0; native=0");
    }
}
