import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

/** Authored observation of genuine original region owners; no source mutation. */
public final class RegionOutputTarget {
    static PrintWriter out;static Path output,world;static int phase=-1,rows;
    static long emitted;
    static final IdentityHashMap<Object,Integer> ids=new IdentityHashMap<>();
    static final List<DataOutputStream> streams=new ArrayList<>();
    public static void mark(int p,String stage,Object anchor){phase=p;}
    static void require(boolean b,String s){if(!b)throw new AssertionError(s);}
    static int id(Object o){if(o==null)return 0;Integer n=ids.get(o);if(n==null){n=ids.size()+1;ids.put(o,n);}return n;}
    static void row(Object...values){String s=ConstructorJson.json(ConstructorJson.map(values));emitted+=s.length()+1;require(++rows<=256&&s.length()<1048576&&emitted<=16777216,"bounded complete observation");out.println(s);out.flush();require(!out.checkError(),"transport");}
    static String sha(byte[]b)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b));}
    static Field field(Class<?>c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static Object get(Class<?>c,String n,Object o)throws Exception{return field(c,n).get(o);}
    static List<Integer> ints(int[]a){List<Integer>r=new ArrayList<>();for(int v:a)r.add(v);return r;}
    static Map<String,Object> fingerprint(byte[]b)throws Exception{return ConstructorJson.map("bytes",b.length,"sha256",sha(b));}
    static Map<String,Object> query(RandomAccessFile f,String operation){try{return ConstructorJson.map("status","returned","value",operation.equals("pointer")?f.getFilePointer():f.length());}catch(IOException e){return ConstructorJson.map("status","exception","class",e.getClass().getName(),"message",e.getMessage());}}
    static final class Original implements AutoCloseable {
        final URL jar;final URLClassLoader loader;final Class<?>cache,qj,hz,compound;
        final Method output,growth,flush,write,putArray;final Constructor<?>newCompound;
        Original(Path archive)throws Exception{
            jar=archive.toUri().toURL();loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader());cache=type("rj");qj=type("qj");hz=type("hz");compound=type("nu");Class<?>as=type("as");
            output=method(cache,"d",File.class,int.class,int.class);growth=method(cache,"b",File.class,int.class,int.class);flush=method(cache,"a");write=method(as,"a",compound,DataOutput.class);putArray=method(compound,"a",String.class,byte[].class);newCompound=compound.getDeclaredConstructor();newCompound.setAccessible(true);
        }
        Class<?>type(String n)throws Exception{Class<?>c=Class.forName(n,false,loader);require(c.getClassLoader()==loader&&c.getProtectionDomain().getCodeSource().getLocation().equals(jar),"exact original origin "+n);return c;}
        Method method(Class<?>c,String n,Class<?>...p)throws Exception{Method m=c.getDeclaredMethod(n,p);m.setAccessible(true);return m;}
        Object invoke(Method m,Object receiver,Object...args)throws Exception{try{return m.invoke(receiver,args);}catch(InvocationTargetException e){throw new AssertionError("unexpected uncaught original source exception",e.getCause());}}
        DataOutputStream begin(int x,int z)throws Exception{DataOutputStream s=(DataOutputStream)invoke(output,null,world.toFile(),x,z);require(s!=null,"actual rj output");streams.add(s);return s;}
        Object tree(int length)throws Exception{Object n=newCompound.newInstance();byte[]a=new byte[length];int state=0x12345678;for(int i=0;i<a.length;++i){state^=state<<13;state^=state>>>17;state^=state<<5;a[i]=(byte)state;}invoke(putArray,n,"Payload",a);return n;}
        void write(DataOutputStream s,int length)throws Exception{invoke(write,null,tree(length),(DataOutput)s);}
        int growth(int x,int z)throws Exception{return(Integer)invoke(growth,null,world.toFile(),x,z);}
        void flush()throws Exception{invoke(flush,null);}
        Map<String,Object>snapshot(DataOutputStream s,String label)throws Exception{
            Object def=get(FilterOutputStream.class,"out",s);require(def.getClass()==DeflaterOutputStream.class,"actual default DeflaterOutputStream");Object h=get(FilterOutputStream.class,"out",def);require(h.getClass()==hz,"actual Hz child");Object q=get(hz,"a",h);require(q.getClass()==qj,"actual strong Hz QJ field");RandomAccessFile raf=(RandomAccessFile)get(qj,"c",q);Object fd=get(RandomAccessFile.class,"fd",raf);
            byte[]buf=(byte[])get(ByteArrayOutputStream.class,"buf",h);int count=(Integer)get(ByteArrayOutputStream.class,"count",h);require(count>=0&&count<=buf.length&&buf.length<=1048576,"bounded actual Hz prefix");byte[]prefix=Arrays.copyOf(buf,count);Files.write(RegionOutputTarget.output.resolve("snapshots").resolve(label+"-hz.bin"),prefix);
            List<Boolean>free=new ArrayList<>();for(Object v:(List<?>)get(qj,"f",q)){require(v instanceof Boolean,"actual free-sector Boolean");free.add((Boolean)v);}require(free.size()<=256,"bounded finite free list");
            Map<String,Object>descriptor=new TreeMap<>();for(Field f:FileDescriptor.class.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())&&(f.getType()==boolean.class||f.getType()==int.class||f.getType()==long.class)){f.setAccessible(true);descriptor.put(f.getName(),f.get(fd));}
            Deflater compressor=(Deflater)get(DeflaterOutputStream.class,"def",def);Map<String,Object>codec=new TreeMap<>();for(Field f:Deflater.class.getDeclaredFields())if(!Modifier.isStatic(f.getModifiers())&&(f.getType()==boolean.class||f.getType()==int.class||f.getType()==long.class)){f.setAccessible(true);codec.put(f.getName(),f.get(compressor));}
            Map<String,Object>cacheItems=new TreeMap<>();Map<?,?>map=(Map<?,?>)get(cache,"a",null);for(Map.Entry<?,?>e:map.entrySet()){Object referent=((java.lang.ref.Reference<?>)e.getValue()).get();cacheItems.put(((File)e.getKey()).toPath().getFileName().toString(),id(referent));}
            return ConstructorJson.map("dos",id(s),"deflaterStream",id(def),"hz",id(h),"qj",id(q),"raf",id(raf),"fd",id(fd),"deflater",id(compressor),"file",((File)get(qj,"b",q)).toPath().getFileName().toString(),"localX",get(hz,"b",h),"localZ",get(hz,"c",h),"outerClosed",get(FilterOutputStream.class,"closed",s),"deflaterClosed",get(DeflaterOutputStream.class,"closed",def),"usesDefaultDeflater",get(DeflaterOutputStream.class,"usesDefaultDeflater",def),"dosWritten",get(DataOutputStream.class,"written",s),"deflateBuffer",((byte[])get(DeflaterOutputStream.class,"buf",def)).length,"hzCapacity",buf.length,"hzCount",count,"hzBytes",fingerprint(prefix),"growth",get(qj,"g",q),"lastModified",get(qj,"h",q),"offsets",ints(((int[])get(qj,"d",q)).clone()),"timestamps",ints(((int[])get(qj,"e",q)).clone()),"free",free,"rafPointer",query(raf,"pointer"),"rafLength",query(raf,"length"),"descriptor",descriptor,"codec",codec,"cache",cacheItems);
        }
        public void close()throws Exception{loader.close();}
    }
    static Map<String,Object>files(String label)throws Exception{
        Map<String,Object>r=new TreeMap<>();Path regions=world.resolve("region");if(!Files.exists(regions))return r;
        try(var paths=Files.list(regions)){for(Path p:(Iterable<Path>)paths::iterator){require(Files.isRegularFile(p)&&Files.size(p)<=1048576,"bounded actual region file");byte[]bytes=Files.readAllBytes(p);String name=p.getFileName().toString();Files.write(output.resolve("snapshots").resolve(label+"-"+name),bytes);r.put(name,fingerprint(bytes));}}return r;
    }
    static void snapshot(Original o,DataOutputStream s,String stage,int x,int z,Object...extra)throws Exception{
        String label=phase+"-"+stage;mark(phase,stage,s);Map<String,Object>details=ConstructorJson.map(extra);row("event","snapshot","phase",phase,"stage",stage,"x",x,"z",z,"owner",o.snapshot(s,label),"files",files(label),"details",details);
    }
    static void normal(Original o,int p,String name,int x,int z,int bytes)throws Exception{
        phase=p;mark(p,"before-begin",null);DataOutputStream s=o.begin(x,z);snapshot(o,s,"begin",x,z,"case",name,"payload",bytes);mark(p,"before-write",s);o.write(s,bytes);snapshot(o,s,"written",x,z);
        mark(p,"before-close",s);long before=System.currentTimeMillis();s.close();long after=System.currentTimeMillis();snapshot(o,s,"closed",x,z,"beforeMillis",before,"afterMillis",after);
        mark(p,"before-repeat-close",s);s.close();snapshot(o,s,"repeat-closed",x,z);mark(p,"before-growth",s);int growth=o.growth(x,z);snapshot(o,s,"growth",x,z,"returned",growth);mark(p,"before-growth-reset",s);int reset=o.growth(x,z);snapshot(o,s,"growth-reset",x,z,"returned",reset);
    }
    public static void main(String[]args)throws Exception{
        output=Path.of(args[0]);world=output.resolve("owned");Files.createDirectory(world);Files.createDirectory(output.resolve("snapshots"));
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("target.ndjson")));Original original=new Original(Path.of(args[1]))){out=writer;normal(original,0,"fresh",0,0,32);normal(original,1,"rewrite",0,0,32);normal(original,2,"grow",0,0,20000);normal(original,3,"shrink",0,0,32);normal(original,4,"reuse",1,0,12000);normal(original,5,"negative",-1,-33,32);
            phase=6;mark(phase,"before-begin",null);DataOutputStream abandoned=original.begin(2,0);snapshot(original,abandoned,"begin",2,0);mark(phase,"before-write",abandoned);original.write(abandoned,12000);snapshot(original,abandoned,"abandoned-unclosed",2,0);
            phase=7;mark(phase,"before-begin",null);DataOutputStream old=original.begin(0,0);snapshot(original,old,"begin",0,0);mark(phase,"before-write",old);original.write(old,32);snapshot(original,old,"written",0,0);mark(phase,"before-cache-flush",old);original.flush();snapshot(original,old,"cache-flushed",0,0);
            phase=8;mark(phase,"before-begin",null);DataOutputStream replacement=original.begin(4,0);snapshot(original,replacement,"begin",4,0);mark(phase,"before-write",replacement);original.write(replacement,32);snapshot(original,replacement,"replacement-unclosed",4,0);
            phase=9;mark(phase,"before-old-close",old);old.close();snapshot(original,old,"old-closed-source-returned",0,0);snapshot(original,replacement,"replacement-still-unclosed",4,0);
            mark(-1,"cleanup",null);original.flush();row("event","complete","outputs",streams.size(),"normalCommits",6,"caughtClosedRafCommit",1,"unclosedOutputs",2,"strongIdentities",ids.size());
        }
    }
}
