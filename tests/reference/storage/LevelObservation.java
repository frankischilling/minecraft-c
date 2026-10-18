import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class LevelObservation {
    private LevelObservation() { }
    private static final String[] CODECS = {"defaults","full","bool-zero","wrong-seed","wrong-int","wrong-name","wrong-bool","wrong-player","wrong-dimension","missing-data"};
    private static final String[] HANDLERS = {"primary","primary-absent","primary-wrong-type","primary-corrupt","missing-data","wrong-data","both-absent","both-bad","new-only","false-flag","true-flag","conflict","missing-lock","short-lock","trailing-lock","old-lock"};
    private static final class OriginalFailure extends Exception {
        private static final long serialVersionUID=1L;
        OriginalFailure(Throwable cause) { super(cause); }
    }
    private static void require(boolean ok,String message) { if(!ok)throw new IllegalStateException(message); }
    private static Map<String,Object> object(Object... pairs) {
        Map<String,Object> out=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)out.put((String)pairs[i],pairs[i+1]);return out;
    }
    private static String json(Object value) {
        if(value==null)return "null";
        if(value instanceof String) {
            StringBuilder b=new StringBuilder("\"");for(char c:((String)value).toCharArray()) {
                if(c=='"'||c=='\\')b.append('\\').append(c);
                else if(c<32||c>=127)b.append(String.format("\\u%04x",(int)c));else b.append(c);
            }return b.append('"').toString();
        }
        if(value instanceof Boolean||value instanceof Number)return value.toString();
        if(value instanceof Map<?,?>) {
            StringBuilder b=new StringBuilder("{");boolean first=true;
            for(Map.Entry<?,?> e:((Map<?,?>)value).entrySet()) {if(!first)b.append(',');first=false;b.append(json(e.getKey())).append(':').append(json(e.getValue()));}return b.append('}').toString();
        }
        if(value instanceof List<?>) {StringBuilder b=new StringBuilder("[");boolean first=true;for(Object x:(List<?>)value){if(!first)b.append(',');first=false;b.append(json(x));}return b.append(']').toString();}
        throw new IllegalArgumentException("known JSON value");
    }
    private static String sha(byte[] value)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));}
    private static String units(String value) {StringBuilder b=new StringBuilder();for(char c:value.toCharArray())b.append(String.format("%04x",(int)c));return b.toString();}
    private static String descriptor(Class<?> c) {
        if(c==void.class)return "V";if(c==boolean.class)return "Z";if(c==byte.class)return "B";if(c==int.class)return "I";if(c==long.class)return "J";
        return c.isArray()?c.getName().replace('.','/'):'L'+c.getName().replace('.','/')+';';
    }
    private static Object invoke(Method method,Object receiver,Object... args)throws Exception {
        try{return method.invoke(receiver,args);}catch(InvocationTargetException ex){if(ex.getCause() instanceof Exception)throw new OriginalFailure(ex.getCause());throw (Error)ex.getCause();}
    }
    private static Object create(Constructor<?> constructor,Object... args)throws Exception {
        try{return constructor.newInstance(args);}catch(InvocationTargetException ex){if(ex.getCause() instanceof Exception)throw new OriginalFailure(ex.getCause());throw (Error)ex.getCause();}
    }
    private static Map<String,Object> failure(OriginalFailure ex){return object("status","exception","class",ex.getCause().getClass().getName());}

    private static final class Probe {
        final URLClassLoader loader;final URL jar;final Path root;final boolean client;
        final Class<?> compound,info,handler,base,utility;
        final Constructor<?> compoundCtor,infoCtor,handlerCtor;
        final Method getData,setData,read,write,retained,list,load,save,saveList,check;
        final Map<String,Object> bindings=new LinkedHashMap<>(),origins=new LinkedHashMap<>(),fields=new LinkedHashMap<>();
        final Map<String,Field> infoFields=new LinkedHashMap<>();final Field token;
        Probe(URLClassLoader loader,URL jar,Path root,boolean client)throws Exception {
            this.loader=loader;this.jar=jar;this.root=root;this.client=client;
            compound=type(client?"nu":"iq");info=type(client?"ei":"ct");handler=type(client?"mx":"ie");base=type(client?"fm":"dq");utility=type(client?"as":"ag");
            require(handler.getSuperclass()==base,"exact original metadata handler inheritance");
            compoundCtor=constructor("compound_ctor",compound);
            infoCtor=constructor("info_ctor",info,compound);
            handlerCtor=constructor("handler_ctor",handler,java.io.File.class,String.class,boolean.class);
            getData=method("get_compound",compound,"k",compound,String.class);
            setData=method("put_compound",compound,"a",void.class,String.class,compound);
            read=method("read_gzip",utility,"a",compound,InputStream.class);
            write=method("write_gzip",utility,"a",void.class,compound,OutputStream.class);
            retained=method("info_retained",info,"a",compound);
            list=method("info_list",info,"a",compound,List.class);
            load=method("handler_load",handler,"c",info);
            save=method("handler_save_retained",handler,"a",void.class,info);
            saveList=method("handler_save_list",handler,"a",void.class,info,List.class);
            check=method("handler_check_lock",handler,"b",void.class);
            for(String name:new String[]{"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o"}) {
                Field f=info.getDeclaredField(name);require(f.getModifiers()==2,"private info field");f.setAccessible(true);infoFields.put(name,f);fields.put("info."+name,object("class",info.getName(),"name",name,"descriptor",descriptor(f.getType()),"modifiers",f.getModifiers()));
            }
            token=base.getDeclaredField("e");require(token.getType()==long.class&&token.getModifiers()==18,"private final handler token");token.setAccessible(true);
            fields.put("handler.e",object("class",base.getName(),"name","e","descriptor","J","modifiers",token.getModifiers()));
        }
        Class<?> type(String name)throws Exception {
            Class<?> t=Class.forName(name,false,loader);require(t.getClassLoader()==loader&&jar.equals(t.getProtectionDomain().getCodeSource().getLocation()),"supplied-JAR exact original class origin");origins.put(name,object("origin",jar.toString(),"loader","isolated"));return t;
        }
        Constructor<?> constructor(String role,Class<?> owner,Class<?>... args)throws Exception {
            Constructor<?> c=owner.getConstructor(args);require(c.getModifiers()==1,"public original constructor");StringBuilder sig=new StringBuilder("(");for(Class<?> a:args)sig.append(descriptor(a));sig.append(")V");bindings.put(role,object("class",owner.getName(),"method","<init>","signature",sig.toString(),"modifiers",c.getModifiers()));return c;
        }
        Method method(String role,Class<?> owner,String name,Class<?> result,Class<?>... args)throws Exception {
            Method m=owner.getMethod(name,args);require(m.getReturnType()==result&&(m.getModifiers()==1||m.getModifiers()==9),"exact original method kind");require(m.getDeclaringClass().getClassLoader()==loader,"original receiver relationship");StringBuilder sig=new StringBuilder("(");for(Class<?> a:args)sig.append(descriptor(a));sig.append(')').append(descriptor(result));bindings.put(role,object("class",m.getDeclaringClass().getName(),"method",name,"signature",sig.toString(),"modifiers",m.getModifiers()));return m;
        }
        Object nbt(Path path)throws Exception {require(Files.size(path)<=65536,"bounded input metadata");try(InputStream stream=Files.newInputStream(path)){Object v=invoke(read,null,stream);require(v!=null&&v.getClass()==compound,"exact original root compound");return v;}}
        Object fromFile(Path path)throws Exception{return create(infoCtor,invoke(getData,nbt(path),"Data"));}
        void output(Path path,Object data)throws Exception {
            Object rootTag=create(compoundCtor);invoke(setData,rootTag,"Data",data);try(OutputStream stream=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){invoke(write,null,rootTag,stream);}require(Files.size(path)<=65536,"bounded original output metadata");
        }
        Object field(Object v,String name)throws Exception{return infoFields.get(name).get(v);}
        Map<String,Object> snapshot(Object v)throws Exception {
            require(v.getClass()==info,"exact original metadata object");Object player=field(v,"h");if(player!=null)require(player.getClass()==compound,"retained original Player compound");
            return object("random_seed_hex",String.format("%016x",(long)field(v,"a")),"spawn",Arrays.asList(field(v,"b"),field(v,"c"),field(v,"d")),"time_hex",String.format("%016x",(long)field(v,"e")),"last_played_hex",String.format("%016x",(long)field(v,"f")),"size_on_disk_hex",String.format("%016x",(long)field(v,"g")),"player_present",player!=null,"dimension",field(v,"i"),"level_name_utf16",units((String)field(v,"j")),"version",field(v,"k"),"raining",field(v,"l"),"rain_time",field(v,"m"),"thundering",field(v,"n"),"thunder_time",field(v,"o"));
        }
        Object codecs()throws Exception {
            List<Object> rows=new ArrayList<>();for(String name:CODECS) {
                Path directory=root.resolve("codec-"+name);Map<String,Object> row=object("case",name);rows.add(row);
                Object value;try{value=fromFile(directory.resolve("input.dat"));}catch(OriginalFailure ex){row.put("constructor",failure(ex));continue;}
                row.put("constructor",object("status","ok","info",snapshot(value)));Object player=field(value,"h");
                for(boolean emptyList:new boolean[]{false,true}) {
                    String mode=emptyList?"empty-list":"retained";long before=System.currentTimeMillis();Object payload=invoke(emptyList?list:retained,value,emptyList?new Object[]{Collections.emptyList()}:new Object[0]);long after=System.currentTimeMillis();output(directory.resolve(mode+".dat"),payload);
                    row.put(mode,object("status","ok","clock_before",before,"clock_after",after,"info_after",snapshot(value),"player_identity_retained",field(value,"h")==player));
                }
            }return rows;
        }
        Map<String,Object> lock(Object value,Path world)throws Exception {
            Map<String,Object> r;try{invoke(check,value);r=object("status","ok");}catch(OriginalFailure ex){r=failure(ex);}
            Path file=world.resolve("session.lock");r.put("handler_token_hex",String.format("%016x",token.getLong(value)));r.put("file_hex",Files.exists(file)?HexFormat.of().formatHex(Files.readAllBytes(file)):null);return r;
        }
        Object handlers()throws Exception {
            List<Object> rows=new ArrayList<>();for(String name:HANDLERS) {
                Path directory=root.resolve("handler-"+name),world=directory.resolve("world");Map<String,Object> row=object("case",name);rows.add(row);
                long before=System.currentTimeMillis();Object h=create(handlerCtor,directory.toFile(),"world",name.equals("true-flag"));long after=System.currentTimeMillis();
                row.put("constructor",object("clock_before",before,"clock_after",after,"token_hex",String.format("%016x",token.getLong(h)),"players_directory",Files.isDirectory(world.resolve("players")),"data_directory",Files.isDirectory(world.resolve("data"))));
                row.put("initial_lock",lock(h,world));
                Object loaded=invoke(load,h);row.put("load",loaded==null?object("status","null"):object("status","info","info",snapshot(loaded)));
                if(name.equals("primary")) {
                    require(loaded!=null,"full primary supplies metadata");
                    for(boolean emptyList:new boolean[]{false,true}) {
                        String mode=emptyList?"save-list":"save-retained";Object prior=snapshot(loaded);before=System.currentTimeMillis();invoke(emptyList?saveList:save,h,emptyList?new Object[]{loaded,Collections.emptyList()}:new Object[]{loaded});after=System.currentTimeMillis();
                        Path saved=directory.resolve(mode+".dat");Files.copy(world.resolve("level.dat"),saved);row.put(mode,object("status","returned","clock_before",before,"clock_after",after,"info_before",prior,"info_after",snapshot(loaded)));
                    }
                    row.put("reload",snapshot(invoke(load,h)));
                }
                if(name.equals("conflict")) {
                    Thread.sleep(25);before=System.currentTimeMillis();Object second=create(handlerCtor,directory.toFile(),"world",false);after=System.currentTimeMillis();row.put("second_constructor",object("clock_before",before,"clock_after",after,"token_hex",String.format("%016x",token.getLong(second))));row.put("first_after_second",lock(h,world));row.put("second_lock",lock(second,world));
                } else if(name.equals("missing-lock")) {Files.delete(world.resolve("session.lock"));row.put("changed_lock",lock(h,world));}
                else if(name.equals("short-lock")) {byte[] bytes=Files.readAllBytes(world.resolve("session.lock"));Files.write(world.resolve("session.lock"),Arrays.copyOf(bytes,7));row.put("changed_lock",lock(h,world));}
                else if(name.equals("trailing-lock")) {Files.write(world.resolve("session.lock"),new byte[]{1,2,3,4,5},StandardOpenOption.APPEND);row.put("changed_lock",lock(h,world));}
            }return rows;
        }
    }
    public static void main(String[] args)throws Exception {
        require(args.length==4,"jar side owned-root output");boolean client=args[1].equals("client");require(client||args[1].equals("server"),"side");
        Path jarArgument=Path.of(args[0]).toAbsolutePath().normalize(),rootArgument=Path.of(args[2]).toAbsolutePath().normalize();
        Path jar=jarArgument.toRealPath(),root=rootArgument.toRealPath(),output=Path.of(args[3]).toAbsolutePath().normalize();
        require(jar.equals(jarArgument)&&root.equals(rootArgument),"canonical nonlinked original archive and owned root");
        Path marker=root.resolve(".level-reference-owned");
        require(Files.isRegularFile(marker,LinkOption.NOFOLLOW_LINKS)&&Files.size(marker)==33&&Files.readString(marker,StandardCharsets.US_ASCII).equals("fresh metadata reference fixture\n")&&output.getParent().equals(root)&&!Files.exists(output,LinkOption.NOFOLLOW_LINKS),"owned fresh observation outputs");
        try(var paths=Files.walk(root)){
            List<Path> entries=paths.limit(257).toList();require(entries.size()<=256,"bounded fixture inventory");long total=0;
            for(Path path:entries){require(!Files.isSymbolicLink(path),"no linked fixture paths");if(!Files.isDirectory(path,LinkOption.NOFOLLOW_LINKS)){require(Files.isRegularFile(path,LinkOption.NOFOLLOW_LINKS)&&Files.size(path)<=65536,"bounded regular fixture file");total+=Files.size(path);}}
            require(total<=8*1024*1024,"bounded complete fixture bytes");
        }
        String expected=client?"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c":"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d";require(Files.size(jar)<2*1024*1024&&sha(Files.readAllBytes(jar)).equals(expected),"pinned original jar");
        Map<String,Object> result;URL url=jar.toUri().toURL();try(URLClassLoader loader=new URLClassLoader(new URL[]{url},ClassLoader.getPlatformClassLoader())) {
            require(loader.getParent()==ClassLoader.getPlatformClassLoader(),"isolated platform parent");Probe p=new Probe(loader,url,root,client);result=object("schema",1,"side",args[1],"jar_sha256",expected,"platform_parent",true,"class_origins",p.origins,"bindings",p.bindings,"fields",p.fields,"codecs",p.codecs(),"handlers",p.handlers());
        }
        require(sha(Files.readAllBytes(jar)).equals(expected),"original jar preserved");byte[] encoded=(json(result)+"\n").getBytes(StandardCharsets.UTF_8);require(encoded.length<=2*1024*1024,"bounded complete semantic output");Files.write(output,encoded,StandardOpenOption.CREATE_NEW);System.out.println("PASS "+args[1]+" metadata codec/handler observations");
    }
}
