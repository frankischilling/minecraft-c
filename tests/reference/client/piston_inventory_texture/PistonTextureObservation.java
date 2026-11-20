import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;

/** Actual registered JQ receivers, source bound hooks and live WJ element inputs. */
public final class PistonTextureObservation {
    static final int[] METADATA = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,-1,-2,-9,-17,32,40,Integer.MIN_VALUE,Integer.MAX_VALUE};
    static final int[] SIDES = {Integer.MIN_VALUE,-1,0,1,2,3,4,5,6,7,Integer.MAX_VALUE};
    static final int[] MUTATIONS = {Integer.MIN_VALUE,-1,42,Integer.MAX_VALUE};
    static int network, nativeLinks, writes, execs, index;
    static void require(boolean value, String text) { if(!value) throw new AssertionError(text); }
    static float from(int bits) { return Float.intBitsToFloat(bits); }
    static String bits(int value) { return String.format(Locale.ROOT,"%08x",value); }
    static long[] bounds(uu b) { return new long[]{Double.doubleToRawLongBits(b.bs),Double.doubleToRawLongBits(b.bt),
        Double.doubleToRawLongBits(b.bu),Double.doubleToRawLongBits(b.bv),Double.doubleToRawLongBits(b.bw),Double.doubleToRawLongBits(b.bx)}; }
    static String boundsText(uu b) { long[] values=bounds(b);StringBuilder out=new StringBuilder();
        for(long value:values){if(out.length()!=0)out.append(',');out.append(String.format(Locale.ROOT,"%016x",value));}return out.toString(); }
    static float[] shape(int scene) {
        float[] f={0,0,0,1,1,1};
        if(scene==1)f=new float[]{.125f,.25f,.375f,.75f,.875f,1};
        if(scene==2)f=new float[]{-.125f,-.25f,-.375f,1.125f,1.25f,1.375f};
        if(scene==3)f=new float[]{from(0x80000000),0,from(0x80000000),1,1,1};
        if(scene>=4&&scene<10)f[scene-4]=from(0x7fc12345);
        if(scene==10)Arrays.fill(f,from(0x7fc12345));
        if(scene==11)f=new float[]{Float.NEGATIVE_INFINITY,Float.NEGATIVE_INFINITY,Float.NEGATIVE_INFINITY,
            Float.POSITIVE_INFINITY,Float.POSITIVE_INFINITY,Float.POSITIVE_INFINITY};
        if(scene==12)f=new float[]{Float.POSITIVE_INFINITY,Float.POSITIVE_INFINITY,Float.POSITIVE_INFINITY,
            Float.NEGATIVE_INFINITY,Float.NEGATIVE_INFINITY,Float.NEGATIVE_INFINITY};
        return f;
    }
    static void prepare(uu b,int scene) {float[] f=shape(scene);b.a(f[0],f[1],f[2],f[3],f[4],f[5]);}
    static void deny(Runnable call) {try{call.run();throw new AssertionError("denial control");}catch(SecurityException expected){}}
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            public void checkLink(String p){++nativeLinks;throw new SecurityException("native denied");}
            public void checkWrite(String p){++writes;throw new SecurityException("write denied");}
            public void checkExec(String p){++execs;throw new SecurityException("exec denied");}});
        for(Class<?> c:new Class<?>[]{uu.class,jq.class,wj.class})require(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original origin");
        uu first=uu.m[29];require(first instanceof jq,"actual StickyPiston");
        int[] prefixSides={0,1,1,0},prefixMetadata={6,1,9,1};
        for(int p=0;p<4;++p){System.out.println("JQ_PREFIX_BEFORE "+p);System.out.flush();
            long[] before=bounds(first);int value=first.a(prefixSides[p],prefixMetadata[p]);
            require(Arrays.equals(before,bounds(first)),"prefix bounds mutation");
            System.out.println("JQ_PREFIX_AFTER "+p+" "+bits(value));System.out.flush();}
        for(int id:new int[]{29,33}){
            uu b=uu.m[id];require(b instanceof jq&&b.b()==16,"actual PistonBase receiver/type");
            int original=b.bm;
            for(int scene=0;scene<13;++scene)for(int bm=0;bm<3;++bm){
                prepare(b,scene);b.bm=bm==0?original:bm==1?-3:197;
                long[] before=bounds(b);System.out.println("JQ_INPUT\t"+id+"\t"+scene+"\t"+bm+"\t"+bits(b.bm)+"\t"+boundsText(b));
                for(int metadata:METADATA)for(int side:SIDES){int value=b.a(side,metadata);
                    require(Arrays.equals(before,bounds(b)),"getter bounds mutation");
                    System.out.println("JQ_TEX\t"+index+++"\t"+id+"\t"+scene+"\t"+bm+"\t"+side+"\t"+metadata+"\t"+bits(value)+"\t1");}
            }
            b.bm=original;
            for(int direction=0;direction<6;++direction)for(int extended=0;extended<2;++extended)for(int changed:MUTATIONS){
                int saved=wj.a[direction];wj.a[direction]=changed;prepare(b,0);
                try{for(int probe=0;probe<2;++probe){int side=probe==0?changed:changed+1;
                    int value=b.a(side,direction|(extended<<3));
                    System.out.println("JQ_MUTATE\t"+id+"\t"+direction+"\t"+extended+"\t"+changed+"\t"+probe+"\t"+side+"\t"+bits(value));}}
                finally{wj.a[direction]=saved;}
            }
            prepare(b,1);b.g();
            System.out.println("JQ_INVENTORY_BOUNDS\t"+id+"\t"+boundsText(b));
            for(int side=0;side<6;++side)System.out.println("JQ_INVENTORY\t"+id+"\t"+side+"\t"+bits(b.a(side,1)));
        }
        require(index==34320&&network==0&&nativeLinks==0&&writes==0&&execs==0,"source rows/guards");
        System.out.println("JQ_GUARD source rows=34320 network=0 native=0 writes=0 execs=0");
        deny(()->System.getSecurityManager().checkConnect("127.0.0.1",25565));deny(()->System.getSecurityManager().checkListen(25565));
        deny(()->System.getSecurityManager().checkLink("jq-denial"));deny(()->System.getSecurityManager().checkWrite("jq-denial"));deny(()->System.getSecurityManager().checkExec("jq-denial"));
        require(network==2&&nativeLinks==1&&writes==1&&execs==1,"five guards");
        System.out.println("JQ_GUARD controls network=2 native=1 writes=1 execs=1");
    }
}
