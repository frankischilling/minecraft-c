import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Authored ByteArrayOutputStream-backed failure boundary; source as is unchanged. */
public final class StreamFaultTarget {
    static PrintWriter out;
    public static void mark(int phase) { }
    static void require(boolean b,String s){if(!b)throw new AssertionError(s);}
    static void row(Object...args){out.println(ConstructorJson.json(ConstructorJson.map(args)));out.flush();require(!out.checkError(),"transport");}
    static final class Sink extends OutputStream {
        final ByteArrayOutputStream committed=new ByteArrayOutputStream();
        final int failWrite;final boolean failFlush,failClose;
        int writes,flushes,closes;boolean closed;
        final List<Object>events=new ArrayList<>();
        Sink(int w,boolean f,boolean c){failWrite=w;failFlush=f;failClose=c;}
        public void write(int b)throws IOException{write(new byte[]{(byte)b},0,1);}
        public void write(byte[]b,int offset,int length)throws IOException{
            committed.write(b,offset,length);++writes;events.add(ConstructorJson.map("op","write","call",writes,"length",length,"committed",committed.size()));
            if(writes==failWrite)throw new IOException("write-"+writes);
        }
        public void flush()throws IOException{++flushes;events.add(ConstructorJson.map("op","flush","call",flushes));if(failFlush&&flushes==1)throw new IOException("flush-1");}
        public void close()throws IOException{committed.close();closed=true;++closes;events.add(ConstructorJson.map("op","close","call",closes));if(failClose&&closes==1)throw new IOException("close-1");}
    }
    static Map<String,Object>exception(Throwable t){if(t==null)return null;List<Object>s=new ArrayList<>();for(Throwable q:t.getSuppressed())s.add(ConstructorJson.map("class",q.getClass().getName(),"message",q.getMessage()));return ConstructorJson.map("class",t.getClass().getName(),"message",t.getMessage(),"suppressed",s);}
    static Class<?>original(URLClassLoader loader,URL jar,String name)throws Exception{Class<?>c=Class.forName(name,false,loader);require(c.getClassLoader()==loader&&c.getProtectionDomain().getCodeSource().getLocation().equals(jar),"supplied original class "+name);return c;}
    public static void main(String[]args)throws Exception{
        Path output=Path.of(args[0]),archive=Path.of(args[1]);URL jar=archive.toUri().toURL();
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("target.ndjson")));URLClassLoader loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader())){
            out=writer;Class<?>compound=original(loader,jar,"nu"),compressed=original(loader,jar,"as");Constructor<?>ctor=compound.getDeclaredConstructor();ctor.setAccessible(true);
            Method putInt=compound.getDeclaredMethod("a",String.class,int.class);putInt.setAccessible(true);Method putArray=compound.getDeclaredMethod("a",String.class,byte[].class);putArray.setAccessible(true);Method write=compressed.getDeclaredMethod("a",compound,OutputStream.class);write.setAccessible(true);
            String[]names={"complete","header-write","serialization-write","normal-close","write-and-finally-close","normal-finish-write","null-stream","flush","flush-and-close"};
            int[]failWrites={0,1,2,0,2,2,0,0,0};boolean[]large={false,false,true,false,true,false,false,false,false};
            for(int phase=0;phase<names.length;++phase){Object root=ctor.newInstance();putInt.invoke(root,"Value",-1234567);if(large[phase]){byte[]a=new byte[131072];int state=0x12345678;for(int i=0;i<a.length;++i){state^=state<<13;state^=state>>>17;state^=state<<5;a[i]=(byte)state;}putArray.invoke(root,"Array",a);}
                Sink sink=new Sink(failWrites[phase],phase==7||phase==8,phase==3||phase==4||phase==8);Throwable failure=null;
                mark(phase);try{write.invoke(null,root,phase==6?null:sink);}catch(InvocationTargetException e){failure=e.getCause();}finally{mark(-1);}
                byte[]bytes=sink.committed.toByteArray();Files.write(output.resolve(phase+"-committed.bin"),bytes);
                row("event","outcome","phase",phase,"case",names[phase],"returned",failure==null,"exception",exception(failure),"writes",sink.writes,"flushes",sink.flushes,"closes",sink.closes,"closed",sink.closed,"committedBytes",bytes.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)),"operations",sink.events);
            }
            row("event","complete","sourceCalls",9);
        }
    }
}
