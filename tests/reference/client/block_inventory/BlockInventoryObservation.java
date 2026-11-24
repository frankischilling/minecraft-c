import java.util.Locale;
import java.security.Permission;

/** Real original Block virtuals; only declared input preparation is outside calls. */
public final class BlockInventoryObservation {
    static final int[] METADATA={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        -1,-2,-17,16,31,Integer.MIN_VALUE,Integer.MAX_VALUE};
    static final int[] LATE_METADATA={0,3,Integer.MIN_VALUE,Integer.MAX_VALUE};
    static int network,nativeLinks;
    static String bounds(uu b) {
        return String.format(Locale.ROOT,"%016x\t%016x\t%016x\t%016x\t%016x\t%016x",
            Double.doubleToRawLongBits(b.bs),Double.doubleToRawLongBits(b.bt),
            Double.doubleToRawLongBits(b.bu),Double.doubleToRawLongBits(b.bv),
            Double.doubleToRawLongBits(b.bw),Double.doubleToRawLongBits(b.bx));
    }
    static double[] save(uu b) {return new double[]{b.bs,b.bt,b.bu,b.bv,b.bw,b.bx};}
    static void restore(uu b,double[] a) {b.bs=a[0];b.bt=a[1];b.bu=a[2];b.bv=a[3];b.bw=a[4];b.bx=a[5];}
    static String bits(int value) {return String.format(Locale.ROOT,"%08x",value);}
    @SuppressWarnings("removal")
    public static void main(String[] args) throws Exception {
        System.setSecurityManager(new SecurityManager() {
            public void checkPermission(Permission p) {}
            public void checkConnect(String host,int port) {++network;throw new SecurityException("network");}
            public void checkListen(int port) {++network;throw new SecurityException("listen");}
            public void checkLink(String library) {++nativeLinks;throw new SecurityException("native");}
        });
        int blocks=0,cubes=0;
        for(int id=1;id<uu.m.length;++id) {
            uu b=uu.m[id]; if(b==null) continue;
            ++blocks;
            Class<?> type=b.getClass();
            int render=b.b();
            System.out.println("DECL\t"+id+"\t"+type.getName()+"\t"+
                type.getMethod("b",int.class).getDeclaringClass().getName()+"\t"+
                type.getMethod("g").getDeclaringClass().getName()+"\t"+
                type.getMethod("a",float.class,float.class,float.class,float.class,float.class,float.class)
                    .getDeclaringClass().getName());
            System.out.println("BLOCK\t"+id+"\t"+render);
            double[] initial=save(b);
            for(int metadata:METADATA)
                System.out.println("COLOR\t"+id+"\t"+metadata+"\t"+bits(b.b(metadata)));
            for(int scene=0;scene<2;++scene) {
                restore(b,initial);
                if(scene==1) b.a(.125f,.25f,.375f,.75f,.875f,1f);
                String before=bounds(b); b.g();
                System.out.println("SHAPE\t"+id+"\t"+scene+"\t"+before+"\t"+bounds(b));
            }
            restore(b,initial);
            if(render==0) {
                ++cubes;
                for(int metadata:METADATA) for(int side=0;side<6;++side)
                    System.out.println("TEXTURE\t"+id+"\t"+metadata+"\t"+side+"\t"+bits(b.a(side,metadata)));
                int original=b.bm;
                for(int texture:new int[]{-3,197}) {
                    b.bm=texture;
                    for(int metadata:LATE_METADATA) for(int side=0;side<6;++side)
                        System.out.println("TEXTURE_LATE\t"+id+"\t"+texture+"\t"+metadata+"\t"+side+"\t"+bits(b.a(side,metadata)));
                }
                b.bm=original;
            }
            restore(b,initial);
        }
        if(blocks!=96 || network!=0 || nativeLinks!=0) throw new AssertionError("source inventory/guard");
        System.out.println("BLOCK_INVENTORY_GUARD\t"+blocks+"\t"+cubes+"\t0\t0");
    }
}
