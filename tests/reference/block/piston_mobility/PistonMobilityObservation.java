import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.Permission;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Authored read-only observer of ordinarily registered original Block objects. */
public final class PistonMobilityObservation {
    static final String JAR="af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c";
    static int writes, network, natives, execs;
    static void need(boolean v,String m){if(!v)throw new IllegalStateException(m);}
    static String hash(byte[] v)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(v));}
    static Map<String,Object> map(Object...v){Map<String,Object> r=new LinkedHashMap<>();for(int i=0;i<v.length;i+=2)r.put((String)v[i],v[i+1]);return r;}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String){StringBuilder r=new StringBuilder("\"");for(char c:((String)v).toCharArray()){if(c=='"'||c=='\\')r.append('\\').append(c);else if(c<32||c>=127)r.append(String.format("\\u%04x",(int)c));else r.append(c);}return r.append('"').toString();}
        if(v instanceof Map){StringBuilder r=new StringBuilder("{");for(Map.Entry<?,?> e:((Map<?,?>)v).entrySet()){if(r.length()>1)r.append(',');r.append(json(e.getKey())).append(':').append(json(e.getValue()));}return r.append('}').toString();}
        if(v instanceof Iterable){StringBuilder r=new StringBuilder("[");for(Object e:(Iterable<?>)v){if(r.length()>1)r.append(',');r.append(json(e));}return r.append(']').toString();}
        need(v instanceof Number||v instanceof Boolean,"closed JSON type");return v.toString();
    }
    static void origin(Class<?> c,URLClassLoader loader,URL jar,Map<String,Object> out)throws Exception{
        for(;c!=null&&!c.getName().startsWith("java.");c=c.getSuperclass()){
            need(c.getClassLoader()==loader&&c.getProtectionDomain().getCodeSource().getLocation().equals(jar),"actual original class origin");
            try(InputStream in=c.getResourceAsStream("/"+c.getName()+".class")){need(in!=null,"original class resource");out.put(c.getName(),hash(in.readAllBytes()));}
        }
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception{
        need(args.length==1,"original client JAR input");Path path=Path.of(args[0]).toRealPath();need(hash(Files.readAllBytes(path)).equals(JAR),"verified unchanged client JAR");URL jar=path.toUri().toURL();
        try(URLClassLoader loader=new URLClassLoader(new URL[]{jar},ClassLoader.getPlatformClassLoader())){
            System.setSecurityManager(new SecurityManager(){
                public void checkPermission(Permission p){}
                public void checkWrite(String p){++writes;throw new SecurityException("write");}
                public void checkDelete(String p){++writes;throw new SecurityException("delete");}
                public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
                public void checkListen(int p){++network;throw new SecurityException("listen");}
                public void checkLink(String n){++natives;throw new SecurityException("native");}
                public void checkExec(String n){++execs;throw new SecurityException("exec");}
            });
            Class<?> block=Class.forName("uu",true,loader);Object[] cells=(Object[])block.getField("m").get(null);need(cells.length==256,"actual Block.m length");
            Method virtual=block.getMethod("h");List<Object> rows=new ArrayList<>();Map<String,Object> origins=new LinkedHashMap<>();
            for(int i=0;i<cells.length;++i){Object receiver=cells[i];if(receiver==null)continue;
                Class<?> actual=receiver.getClass();origin(actual,loader,jar,origins);Object material=block.getField("bA").get(receiver);need(material!=null,"ordinary registered Material");origin(material.getClass(),loader,jar,origins);
                int before=block.getField("bn").getInt(receiver);Object first=virtual.invoke(receiver),second=virtual.invoke(receiver);Method resolved=actual.getMethod("h");
                rows.add(map("id",i,"runtime_class",actual.getName(),"declaring_class",resolved.getDeclaringClass().getName(),"registered_identity",cells[i]==receiver,
                    "id_before",before,"id_after",block.getField("bn").getInt(receiver),"material_same",block.getField("bA").get(receiver)==material,
                    "material_class",material.getClass().getName(),"material_mobility",material.getClass().getMethod("j").invoke(material),"mobility",first,"repeated_mobility",second));
            }
            System.out.println("PISTON_MOBILITY_ORIGINS "+json(map("jar_sha256",JAR,"classes",origins,"origin_role","verified-original-jar")));
            for(Object r:rows)System.out.println("PISTON_MOBILITY_ROW "+json(r));
            System.out.println("PISTON_MOBILITY_COMPLETE "+json(map("rows",rows.size(),"h_calls",rows.size()*2,"material_j_calls",rows.size(),"writes",writes,"network",network,"natives",natives,"execs",execs)));
        }
    }
}
