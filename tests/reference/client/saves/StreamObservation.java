/* Authored reflection adapter. No game body, class replacement, field write,
 * captured output or constructor entropy. The actual supplied ij/as executes. */
import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
public final class StreamObservation {
    private static String hex(String text) {StringBuilder b=new StringBuilder();for(int i=0;i<text.length();i++)b.append(String.format(Locale.ROOT,"%04x",(int)text.charAt(i)));return b.toString();}
    private static Object field(Object value,String name)throws Exception {Field f=value.getClass().getDeclaredField(name);f.setAccessible(true);return f.get(value);}
    private static String value(Object tag)throws Exception {
        String c=tag.getClass().getName();
        if(c.equals("lh"))return "E";
        if(c.equals("qp"))return "B:"+field(tag,"a");
        if(c.equals("ul"))return "S:"+field(tag,"a");
        if(c.equals("pp"))return "I:"+field(tag,"a");
        if(c.equals("mi"))return "J:"+field(tag,"a");
        if(c.equals("p"))return "F:"+String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits((Float)field(tag,"a")));
        if(c.equals("sz"))return "D:"+String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits((Double)field(tag,"a")));
        if(c.equals("xb"))return "T:"+hex((String)field(tag,"a"));
        if(c.equals("hn")){StringBuilder b=new StringBuilder("A:");for(byte x:(byte[])field(tag,"a"))b.append(String.format(Locale.ROOT,"%02x",x&255));return b.toString();}
        if(c.equals("sp")){StringBuilder b=new StringBuilder("L:"+(Byte)field(tag,"b")+"[");for(Object x:(List<?>)field(tag,"a"))b.append(value(x)).append(';');return b.append(']').toString();}
        if(c.equals("nu")){Map<?,?> map=(Map<?,?>)field(tag,"a");List<String> keys=new ArrayList<>();for(Object k:map.keySet())keys.add((String)k);Collections.sort(keys);StringBuilder b=new StringBuilder("C:{");for(String k:keys)b.append(hex(k)).append('=').append(value(map.get(k))).append(';');return b.append('}').toString();}
        throw new IOException("Unexpected actual tag class "+c);
    }
    public static void main(String[] args)throws Exception {
        if(args.length!=3)throw new IllegalArgumentException("jar mode fixture");
        String mode=args[1];if(!mode.equals("ij")&&!mode.equals("as")&&!mode.equals("gzip"))throw new IllegalArgumentException("mode");
        Path jar=Path.of(args[0]).toRealPath(), fixture=Path.of(args[2]).toRealPath();
        try(URLClassLoader loader=new URLClassLoader(new URL[]{jar.toUri().toURL()},ClassLoader.getPlatformClassLoader())){
            Class<?> ij=Class.forName("ij",false,loader), as=Class.forName("as",false,loader);
            for(Class<?> c:new Class<?>[]{ij,as}){URL origin=c.getProtectionDomain().getCodeSource().getLocation();if(!Path.of(origin.toURI()).toRealPath().equals(jar))throw new SecurityException("runtime origin");System.out.println("ORIGIN\t"+c.getName()+"\t"+jar.toUri().toASCIIString());}
            FileInputStream in=new FileInputStream(fixture.toFile());
            try {
                Object tag=null;String raw=null;Throwable thrown=null;
                try {
                    if(mode.equals("gzip")){GZIPInputStream gzip=new GZIPInputStream(in);ByteArrayOutputStream bytes=new ByteArrayOutputStream();for(int b;(b=gzip.read())!=-1;)bytes.write(b);StringBuilder h=new StringBuilder("R:");for(byte b:bytes.toByteArray())h.append(String.format(Locale.ROOT,"%02x",b&255));raw=h.toString();}
                    else if(mode.equals("as")){Method m=as.getDeclaredMethod("a",InputStream.class);m.setAccessible(true);tag=m.invoke(null,in);}
                    else {Method m=ij.getDeclaredMethod("b",DataInput.class);m.setAccessible(true);tag=m.invoke(null,new DataInputStream(new GZIPInputStream(in)));}
                }catch(InvocationTargetException e){thrown=e.getCause();}catch(IOException e){thrown=e;}
                boolean valid=in.getFD().valid();
                if(thrown==null)System.out.println("RESULT\tOK\t"+(raw==null?value(tag):raw)+"\t"+(valid?"OPEN":"CLOSED"));
                else System.out.println("RESULT\tTHROW\t"+thrown.getClass().getName()+"\t"+(valid?"OPEN":"CLOSED"));
            } finally {in.close();}
            List<String> loaded=new ArrayList<>();for(String name:new String[]{"ij","as","lh","qp","ul","pp","mi","p","sz","hn","xb","sp","nu"}){Class<?> c=Class.forName(name,false,loader);if(!Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar))throw new SecurityException("tag origin");loaded.add(name);}System.out.println("END\t"+loaded.size());
        }
    }
}
