import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;

/** Unchanged scalar World and actual virtual Dimension sky methods. */
public final class WorldSkyObservation {
    static final int[] ANGLES={0xc0000000,0xbf800000,0xbf50c49c,0xbf400000,0xbf000000,0xbe800000,0x80000000,0x00000000,0x3dcccccd,0x3e19999a,0x3e3851ec,0x3e3ced91,0x3e3cf030,0x3e428f5c,0x3e4ccccd,0x3e600000,0x3e75c28f,0x3e800000,0x3e851eb8,0x3e8f5c29,0x3e99999a,0x3e9eb852,0x3ea187e8,0x3ea18937,0x3ea3d70a,0x3eb33333,0x3ecccccd,0x3f000000,0x3f19999a,0x3f266666,0x3f2e147b,0x3f2f3b64,0x3f2f3c0c,0x3f333333,0x3f3851ec,0x3f3d70a4,0x3f400000,0x3f428f5c,0x3f47ae14,0x3f4ccccd,0x3f4f5c29,0x3f50c3f4,0x3f50c49c,0x3f51eb85,0x3f59999a,0x3f666666,0x3f800000,0x3fa00000,0x40000000};
    static final float[] PARTIAL={0.0f,0.625f,1.0f};
    static final long[] TIMES={-24001L,-1L,0L,3000L,6000L,12000L,18000L,Long.MAX_VALUE};
    static int network,nativeLinks;
    static void require(boolean value) {if(!value)throw new AssertionError("original sky invariant");}
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static Object unsafe()throws Exception {
        Class<?> type=Class.forName("sun.misc.Unsafe");return field(type,"theUnsafe").get(null);
    }
    static Object raw(Class<?> type)throws Exception {
        return unsafe().getClass().getMethod("allocateInstance",Class.class).invoke(unsafe(),type);
    }
    static boolean coldMath()throws Exception {
        return (Boolean)unsafe().getClass().getMethod("shouldBeInitialized",Class.class).invoke(unsafe(),in.class);
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==1);Path jar=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException("network denied");}
            @Override public void checkListen(int p){++network;throw new SecurityException("listen denied");}
            @Override public void checkLink(String n){++nativeLinks;throw new SecurityException("native denied");}
            @Override public void checkWrite(String p){throw new SecurityException("write denied");}
        });
        for(Class<?> type:new Class<?>[]{fd.class,ei.class,xa.class,rh.class,wd.class,ay.class,in.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar));
        fd world=(fd)raw(fd.class);ei info=new ei(23L,"Sky");world.x=info;
        xa[] dimensions={new rh(),new wd(),new ay()};
        require(coldMath());require(dimensions[2].a(0.25f,0.0f)==null);
        for(int d=0;d<3;++d)require(dimensions[d].c()==(d!=2));
        require(coldMath());
        float[] previous={0,0.2f,1,-0.25f},current={0,0.8f,1,0.75f};
        for(int d=0;d<3;++d){
            field(fd.class,"t").set(world,dimensions[d]);
            for(int t=0;t<TIMES.length;++t){
                info.a(TIMES[t]);
                for(int p=0;p<3;++p){
                    int index=t*3+p;world.i=previous[index%4];world.j=current[index%4];
                    float angle=world.b(PARTIAL[p]);if(d==0&&index==0)require(coldMath());
                    float rain=world.g(PARTIAL[p]),stars=world.e(PARTIAL[p]);
                    require(!coldMath());
                    System.out.printf("SKY %d %d %08x %08x %08x %d%n",d,index,
                        Float.floatToRawIntBits(angle),Float.floatToRawIntBits(rain),Float.floatToRawIntBits(stars),dimensions[d].c()?1:0);
                }
            }
        }
        for(int d=0;d<3;++d){
            float[] h=(float[])field(xa.class,"h").get(dimensions[d]);require(h.length==4);
            for(int a=0;a<ANGLES.length;++a)for(int p=0;p<2;++p){
                float[] returned=dimensions[d].a(Float.intBitsToFloat(ANGLES[a]),PARTIAL[p]);
                require(returned==null||returned==h);require(field(xa.class,"h").get(dimensions[d])==h);
                System.out.printf("SUN %d %d %d %d %08x %08x %08x %08x%n",d,a,p,returned!=null?1:0,
                    Float.floatToRawIntBits(h[0]),Float.floatToRawIntBits(h[1]),Float.floatToRawIntBits(h[2]),Float.floatToRawIntBits(h[3]));
            }
        }
        rh prefix=(rh)raw(rh.class);require(prefix.a(0.0f,0.0f)==null&&prefix.c());
        boolean nullArray=false;try{prefix.a(0.25f,0);}catch(NullPointerException expected){nullArray=true;}
        require(nullArray);ay skyPrefix=(ay)raw(ay.class);require(skyPrefix.a(0.25f,0)==null&&!skyPrefix.c());
        require(network==0&&nativeLinks==0);
        System.out.println("Original World sky: 366 rows; prefixes=pass; network=0; native=0");
    }
}
