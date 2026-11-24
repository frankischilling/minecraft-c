import java.lang.reflect.Field;
import java.nio.file.Path;
import java.security.Permission;
import java.util.Arrays;
import java.util.Locale;

/** Genuine ew and unchanged World/provider/Chunk/Registry methods. */
public final class ChunkCacheObservation {
    static int network,nativeLinks,rows;
    static Field field(Class<?> type,String name)throws Exception {
        Field f=type.getDeclaredField(name);f.setAccessible(true);return f;
    }
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    interface Op { String get()throws Exception; }
    static void row(String name,Op operation)throws Exception {
        String result;
        try {result=operation.get();}
        catch(NullPointerException e){result="NULL";}
        catch(ArrayIndexOutOfBoundsException e){result="INDEX";}
        catch(NegativeArraySizeException e){result="NEGATIVE";}
        System.out.println("EW "+name+" "+result);++rows;
    }
    static String bits(float f){return String.format("%08x",Float.floatToRawIntBits(f));}
    static String constructor(fd world,int x0,int z0,int x1,int z1)throws Exception {
        ew value=new ew(world,x0,Integer.MIN_VALUE,z0,x1,Integer.MAX_VALUE,z1);
        lm[][] cells=(lm[][])field(ew.class,"c").get(value);
        boolean same=field(ew.class,"d").get(value)==world;
        for(int x=0;x<cells.length;++x)for(int z=0;z<cells[x].length;++z)
            same&=cells[x][z]==world.c((x0>>4)+x,(z0>>4)+z);
        boolean distinct=cells.length<2||cells[0]!=cells[1];
        return field(ew.class,"a").getInt(value)+" "+field(ew.class,"b").getInt(value)+" "+
            cells.length+" "+(cells.length==0?-1:cells[0].length)+" "+(same?1:0)+" "+(distinct?1:0);
    }
    static int material(ln m)throws Exception {
        String[] names={"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","A","B"};
        for(int i=0;i<names.length;++i)if(field(ln.class,names[i]).get(null)==m)return i;
        throw new AssertionError("unknown canonical Material");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==2,"JAR and private authored World");Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(args[0]).toRealPath(),directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission p){}
            @Override public void checkConnect(String h,int p){++network;throw new SecurityException();}
            @Override public void checkListen(int p){++network;throw new SecurityException();}
            @Override public void checkLink(String lib){++nativeLinks;throw new SecurityException();}
            @Override public void checkWrite(String f){require(Path.of(f).toAbsolutePath().normalize().startsWith(directory),"private World writes only");}
        });
        for(Class<?> type:new Class<?>[]{ew.class,fd.class,lm.class,uu.class,ln.class,ys.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original archive authority");
        row("CTOR_NULL",()->constructor(null,0,0,15,15));
        row("CTOR_ZERO_X",()->constructor(null,16,0,0,15));
        row("CTOR_ZERO_Z",()->constructor(null,0,16,31,0));
        row("CTOR_NEG_X",()->constructor(null,32,0,0,15));
        row("CTOR_NEG_Z",()->constructor(null,16,32,0,0));
        fd world=new fd(new mx(directory.getParent().toFile(),directory.getFileName().toString(),false),"",23L);
        lm[] chunks={world.c(0,0),world.c(0,1),world.c(1,0),world.c(1,1)};
        row("CTOR_FOUR",()->constructor(world,0,0,31,31));
        row("CTOR_INCLUSIVE",()->constructor(world,0,0,16,16));
        row("CTOR_ONE",()->constructor(world,31,31,31,31));
        ew cache=new ew(world,0,-1,0,31,129,31);
        for(lm chunk:chunks){Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);Arrays.fill(chunk.f.a,(byte)0);Arrays.fill(chunk.g.a,(byte)0);}
        field(fd.class,"f").setInt(world,3);
        for(int id=0;id<=96;++id) {
            int y=id+1,index=(8<<11)|(8<<7)|y;
            chunks[0].b[index]=(byte)id;
            chunks[0].e.a(8,y,8,id&15);chunks[0].f.a(8,y,8,(id*7)&15);chunks[0].g.a(8,y,8,(id*3)&15);
        }
        // All source receiver storage is real. Only input bytes/light/f are authored.
        // Clearing the actual World provider AFTER construction makes accidental
        // cache-accessor reacquisition observable as a genuine source failure.
        cl provider=world.v;world.v=null;
        for(int id=0;id<=96;++id) {
            final int y=id+1,n=id;
            row("BLOCK_"+id,()->Integer.toString(cache.a(8,y,8)));
            row("META_"+id,()->Integer.toString(cache.e(8,y,8)));
            row("RAWLIGHT_"+id,()->Integer.toString(cache.a(8,y,8,false)));
            row("LIGHT_"+id,()->Integer.toString(cache.d(8,y,8)));
            row("BRIGHT_"+id,()->bits(cache.c(8,y,8)));
            row("MINBRIGHT_"+id,()->bits(cache.a(8,y,8,7)));
            row("MATERIAL_"+id,()->Integer.toString(material(cache.f(8,y,8))));
            row("OPAQUE_"+id,()->cache.g(8,y,8)?"1":"0");
            row("NORMAL_"+id,()->cache.h(8,y,8)?"1":"0");
        }
        final int[][] points={{8,-1,8},{8,128,8},{8,Integer.MIN_VALUE,8},{8,Integer.MAX_VALUE,8},
            {-1,64,8},{32,64,8},{8,64,-1},{8,64,32},{-32000001,-1,8},{32000000,-1,8},
            {8,-1,32000000},{8,-1,32000001},{8,128,32000000},{8,128,32000001}};
        for(int i=0;i<points.length;++i){final int[] p=points[i];
            row("BOUND_BLOCK_"+i,()->Integer.toString(cache.a(p[0],p[1],p[2])));
            row("BOUND_META_"+i,()->Integer.toString(cache.e(p[0],p[1],p[2])));
            row("BOUND_LIGHT_"+i,()->Integer.toString(cache.a(p[0],p[1],p[2],false)));
            row("BOUND_TILE_"+i,()->cache.b(p[0],p[1],p[2])==null?"NULL_RESULT":"TILE");
        }
        int[] subtraction={0,3,20,Integer.MIN_VALUE,Integer.MAX_VALUE};
        for(int i=0;i<subtraction.length;++i){field(fd.class,"f").setInt(world,subtraction[i]);
            final int n=i;
            row("SUB_HIGH_"+i,()->Integer.toString(cache.a(8,128,8,false)));
            row("SUB_RAW_"+i,()->Integer.toString(cache.a(8,4,8,false)));
        }
        field(fd.class,"f").setInt(world,3);
        row("MIN_INDEX",()->bits(cache.a(8,1,8,16)));
        row("MANAGER",()->cache.a()==world.a()?"1":"0");
        world.v=provider;
        // Actual on-demand Chest factory/Map publication; repeated get must
        // return the SAME object, not an observer-created TileEntity.
        chunks[0].b[(8<<11)|(8<<7)|110]=54;
        ow tile=cache.b(8,110,8);
        require(tile!=null,"real lazy Chest");world.v=null;
        row("TILE_SAME",()->cache.b(8,110,8)==tile?"1":"0");
        world.v=provider;
        require(network==0&&nativeLinks==0,"no network/native work");
        System.out.println("Original EW: "+rows+" rows; network=0; native=0");
    }
}
