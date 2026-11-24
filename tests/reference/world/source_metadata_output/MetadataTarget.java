import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Authored genuine prepared handler calls; original classes remain unchanged. */
public final class MetadataTarget {
    static PrintWriter out;
    static int phase = -1;
    static void require(boolean b, String why) { if (!b) throw new AssertionError(why); }
    public static void mark(int step) { phase = step; }
    static void row(Object... pairs) { out.println(ConstructorJson.json(ConstructorJson.map(pairs))); out.flush(); require(!out.checkError(), "observation transport"); }
    static String sha(Path p) throws Exception { return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(p))); }
    static final class Original implements AutoCloseable {
        final URL jar; final URLClassLoader loader;
        final Class<?> compound, info, handler;
        final Constructor<?> newCompound, newInfo, newHandler;
        final Method putInt, putLong, putByte, putString, putCompound, write;
        Original(Path archive) throws Exception {
            jar=archive.toUri().toURL(); loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader());
            compound=type("nu");info=type("ei");handler=type("mx");
            newCompound=compound.getDeclaredConstructor();newInfo=info.getDeclaredConstructor(compound);newHandler=handler.getDeclaredConstructor(File.class,String.class,boolean.class);
            newCompound.setAccessible(true);newInfo.setAccessible(true);newHandler.setAccessible(true);
            putInt=method(compound,"a",String.class,int.class);putLong=method(compound,"a",String.class,long.class);
            putByte=method(compound,"a",String.class,byte.class);putString=method(compound,"a",String.class,String.class);
            putCompound=method(compound,"a",String.class,compound);write=method(handler,"a",info,List.class);
        }
        Class<?> type(String name) throws Exception { Class<?> c=Class.forName(name,false,loader); require(c.getClassLoader()==loader && c.getProtectionDomain().getCodeSource().getLocation().equals(jar),"supplied original JAR: "+name); return c; }
        Method method(Class<?> c,String n,Class<?>...p)throws Exception {Method m=c.getDeclaredMethod(n,p);m.setAccessible(true);return m;}
        Object info(int variant)throws Exception {
            Object n=newCompound.newInstance();
            putLong.invoke(n,"RandomSeed",-9223372036854775807L);putInt.invoke(n,"SpawnX",-17);putInt.invoke(n,"SpawnY",65);putInt.invoke(n,"SpawnZ",1003);
            putLong.invoke(n,"Time",variant==2?-1234567890123L:23999L);putLong.invoke(n,"LastPlayed",-5L);putLong.invoke(n,"SizeOnDisk",9876543210L);
            putString.invoke(n,"LevelName",variant==2?"Second\u0000\u03a9\ud83d\ude00":"First\u0000\u03a9\ud83d\ude00");putInt.invoke(n,"version",-12);
            putInt.invoke(n,"rainTime",-345);putByte.invoke(n,"raining",(byte)1);putInt.invoke(n,"thunderTime",678);putByte.invoke(n,"thundering",(byte)0);
            Object player=newCompound.newInstance();putInt.invoke(player,"Dimension",-1);putInt.invoke(player,"Keep",42);putCompound.invoke(n,"Player",player);
            putString.invoke(n,"Unknown","ignored");return newInfo.newInstance(n);
        }
        Object handler(Path root,String name)throws Exception {return newHandler.newInstance(root.toFile(),name,false);}
        void call(Object h,Object i,List<?> players)throws Exception {
            try{write.invoke(h,i,players);}catch(InvocationTargetException e){throw new AssertionError("unexpected uncaught original handler exception",e.getCause());}
        }
        public void close()throws Exception{loader.close();}
    }
    static Map<String,Object> tree(Path root)throws Exception {
        Map<String,Object> r=new TreeMap<>();
        try(var paths=Files.walk(root)){for(Path p:(Iterable<Path>)paths::iterator){if(p.equals(root))continue;String name=root.relativize(p).toString().replace('\\','/');
            if(Files.isDirectory(p))r.put(name,ConstructorJson.map("kind","directory"));
            else r.put(name,ConstructorJson.map("kind","file","bytes",Files.size(p),"sha256",sha(p)));
        }}return r;
    }
    static void call(Original o,Path outDir,Path root,String caseName,int step,int variant,List<?>players)throws Exception {
        Object h=o.handler(root,caseName);callExisting(o,outDir,root,caseName,h,step,variant,players);
    }
    static void callExisting(Original o,Path outDir,Path root,String caseName,Object h,int step,int variant,List<?>players)throws Exception {
        Object i=o.info(variant);Path world=root.resolve(caseName);
        row("event","before","phase",step,"case",caseName,"files",tree(world));
        mark(step);long before=System.currentTimeMillis();o.call(h,i,players);long after=System.currentTimeMillis();mark(-1);
        Path snapshots=outDir.resolve("snapshots");Files.createDirectories(snapshots);
        for(String name:List.of("level.dat","level.dat_old","level.dat_new")){Path p=world.resolve(name);if(Files.isRegularFile(p))Files.copy(p,snapshots.resolve(step+"-"+name));}
        row("event","after","phase",step,"case",caseName,"variant",variant,"beforeMillis",before,"afterMillis",after,"returned",true,"files",tree(world));
    }
    public static void main(String[] args)throws Exception {
        Path output=Path.of(args[0]);Path jar=Path.of(args[1]);Path root=output.resolve("owned");Files.createDirectory(root);
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("target.ndjson")));Original o=new Original(jar)){
            out=writer;row("event","setup","jarSha256",sha(jar),"reflection","Declared constructors/methods and original setup invocations; class resolution/init may occur before marked calls");
            call(o,output,root,"fresh",0,1,List.of());
            Object existing=o.handler(root,"existing");Path ew=root.resolve("existing");Files.writeString(ew.resolve("level.dat"),"obsolete current");Files.writeString(ew.resolve("level.dat_old"),"obsolete old");
            callExisting(o,output,root,"existing",existing,1,1,List.of());callExisting(o,output,root,"existing",existing,2,2,List.of());
            Object oldDir=o.handler(root,"old-directory");Path ow=root.resolve("old-directory");Files.createDirectory(ow.resolve("level.dat_old"));Files.writeString(ow.resolve("level.dat_old/sentinel"),"old sentinel");Files.copy(output.resolve("snapshots/1-level.dat"),ow.resolve("level.dat"));
            callExisting(o,output,root,"old-directory",oldDir,3,2,List.of());
            Object both=o.handler(root,"both-directories");Path bw=root.resolve("both-directories");for(String n:List.of("level.dat_old","level.dat")){Files.createDirectory(bw.resolve(n));Files.writeString(bw.resolve(n).resolve("sentinel"),n+" sentinel");}
            callExisting(o,output,root,"both-directories",both,4,1,List.of());
            Object open=o.handler(root,"open-failure");Path fw=root.resolve("open-failure");Files.createDirectory(fw.resolve("level.dat_new"));Files.writeString(fw.resolve("level.dat_new/sentinel"),"new sentinel");
            callExisting(o,output,root,"open-failure",open,5,1,List.of());
            call(o,output,root,"null-first",6,1,Collections.singletonList(null));
            row("event","complete","sourceCalls",7);
        }
    }
}
