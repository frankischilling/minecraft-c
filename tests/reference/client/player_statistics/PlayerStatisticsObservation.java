import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Authored observer; every game owner and executed body is from the supplied JAR. */
public final class PlayerStatisticsObservation {
    static kq app;
    static dc player;
    static xi writer;
    static vu notice;
    static Object get(Object object, Class<?> owner, String name) throws Exception {
        Field field=owner.getDeclaredField(name);field.setAccessible(true);return field.get(object);
    }
    static void set(Object object, Class<?> owner, String name, Object value) throws Exception {
        Field field=owner.getDeclaredField(name);field.setAccessible(true);field.set(object,value);
    }
    static void require(boolean condition,String message) {
        if(!condition)throw new AssertionError(message);
    }
    static int bit(boolean value) {return value?1:0;}
    static void effect() throws Exception {
        Object water=get(app,net.minecraft.client.Minecraft.class,"ad");
        require(water!=null&&water.getClass()==vs.class,"actual ad type");
        byte[] bytes=(byte[])get(water,aw.class,"a");
        Object[] arrays={bytes,get(water,vs.class,"g"),get(water,vs.class,"h"),get(water,vs.class,"i"),get(water,vs.class,"j")};
        Set<Object> distinct=Collections.newSetFromMap(new IdentityHashMap<>());
        boolean zero=true;
        for(byte item:bytes)zero=zero&&item==0;
        for(Object array:arrays) {
            require(array!=null,"actual water array");distinct.add(array);
            if(array instanceof float[])for(float item:(float[])array)zero=zero&&Float.floatToRawIntBits(item)==0;
        }
        require(uu.B.bn==8&&uu.B.bm==205,"actual flowing-water Block8 texture205");
        System.out.println("EFFECT\twater\t"+uu.B.bn+"\t"+get(water,aw.class,"b")+"\t"+
            bit((Boolean)get(water,aw.class,"c"))+"\t"+get(water,aw.class,"d")+"\t"+
            get(water,aw.class,"e")+"\t"+get(water,aw.class,"f")+"\t"+get(water,vs.class,"k")+"\t"+
            bytes.length+"\t"+((float[])arrays[1]).length+"\t"+((float[])arrays[2]).length+"\t"+
            ((float[])arrays[3]).length+"\t"+((float[])arrays[4]).length+"\t"+bit(distinct.size()==5)+"\t"+bit(zero));
    }
    static void project(String label,String outcome) throws Exception {
        ny root=ep.f,child=ep.g,next=ep.h;
        ny shown=(ny)get(notice,vu.class,"f");
        System.out.println("STAT\t"+label+"\t"+outcome+"\t"+
            ((Map<?,?>)get(writer,xi.class,"a")).size()+"\t"+
            ((Map<?,?>)get(writer,xi.class,"b")).size()+"\t"+bit((Boolean)get(writer,xi.class,"c"))+"\t"+
            bit(writer.a(root))+"\t"+writer.a((vr)root)+"\t"+
            bit(writer.a(child))+"\t"+writer.a((vr)child)+"\t"+
            bit(writer.a(next))+"\t"+writer.a((vr)next)+"\t"+writer.a(jl.f)+"\t"+
            (shown==null?-1:shown.e)+"\t"+bit((Boolean)get(notice,vu.class,"i"))+"\t"+
            bit((Long)get(notice,vu.class,"g")!=0));
    }
    static void award(String label,vr statistic,int amount,boolean expectNull) throws Exception {
        String outcome="OK";
        try {player.a(statistic,amount);require(!expectNull,"missing NPE "+label);}
        catch(NullPointerException error) {
            require(expectNull,"unexpected NPE "+label);
            StackTraceElement top=error.getStackTrace()[0];
            require(top.getClassName().equals("dc")&&top.getMethodName().equals("a"),"failure source "+label);
            outcome="NULL";
        }
        project(label,outcome);
    }
    static void cases() throws Exception {
        set(player,dc.class,"b",null);app.I=null;app.u=null;
        award("null",null,Integer.MIN_VALUE,false);
        award("general-null-client",jl.f,1,true);
        award("root-null-client",ep.f,1,true);
        award("child-null-client",ep.g,1,true);
        set(player,dc.class,"b",app);
        award("general-null-I",jl.f,1,true);
        award("root-null-I",ep.f,1,true);
        award("child-null-I",ep.g,1,true);
        app.I=writer;
        award("general-zero",jl.f,0,false);
        award("general-negative",jl.f,-7,false);
        award("general-large",jl.f,Integer.MAX_VALUE,false);
        award("general-wrap",jl.f,10,false);
        award("blocked-parent",ep.g,1,false);
        award("root-null-u",ep.f,0,true);
        app.u=notice;
        award("root-zero",ep.f,0,false);
        app.u=null;
        award("root-negative-present",ep.f,-3,false);
        app.u=notice;
        award("child-zero-parent-negative",ep.g,0,false);
        app.u=null;
        award("child-negative-present",ep.g,-2,false);
        award("next-null-u",ep.h,-4,true);
        app.u=notice;
        award("next-negative",ep.h,-4,false);
        app.u=null;
        award("next-zero-present",ep.h,0,false);
        app.u=notice;
    }
    static void joinWorkers() throws Exception {
        long deadline=System.nanoTime()+10_000_000_000L;
        Object sync=get(writer,xi.class,"d");
        for(;;) {
            boolean active=false;
            for(Thread thread:Thread.getAllStackTraces().keySet()) {
                String name=thread.getClass().getName();
                if(name.equals("vk")||name.equals("vj")) {active=true;thread.join(100);}
            }
            if(!active&&!(Boolean)get(sync,cj.class,"a"))return;
            require(System.nanoTime()<deadline,"worker deadline");Thread.sleep(1);
        }
    }
    static void binding(Path classes,Path output) throws Exception {
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output,StandardCharsets.UTF_8))) {
            for(String name:Files.readAllLines(classes,StandardCharsets.US_ASCII)) {
                Class<?> type=Class.forName(name.replace('/','.'),false,PlayerStatisticsObservation.class.getClassLoader());
                byte[] bytes;
                try(InputStream stream=type.getResourceAsStream("/"+name.replace('.','/')+".class")) {
                    require(stream!=null,"class bytes "+name);bytes=stream.readAllBytes();
                }
                out.println(name+"\t"+bytes.length+"\t"+
                    HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))+"\t"+
                    type.getProtectionDomain().getCodeSource().getLocation().toURI());
            }
            require(!out.checkError(),"binding output");
        }
    }
    public static void main(String[] args) throws Exception {
        require(args.length==3,"game-directory classes-list binding-output");
        Path directory=Path.of(args[0]);Files.createDirectories(directory);
        /* Genuine Minecraft constructor executes jl.a186. Its notice/model and
         * Timer-hack worker are constructed normally; no startup method runs. */
        app=new kq(null,null,null,854,480,false,null);
        try {
            require(app.I==null&&app.z==null,"actual constructor fields");notice=app.u;
            effect();
            app.k=new gr("Player123","-");
            set(app,net.minecraft.client.Minecraft.class,"Z",directory.toFile());
            writer=new xi(app.k,directory.toFile());app.I=writer;
            ep.f.a(new kh(app));
            /* Finite observer joins only after source publication/kh. No such
             * join is inserted into production startup. */
            joinWorkers();
            fd world=new fd(new wy(),"PlayerStatisticsReference",xa.a(0),5143L);
            world.a(new br(0,64,0));world.B=false;
            player=new dc(app,world,app.k,0); // Genuine four independent Math draws.
            cases();binding(Path.of(args[1]),Path.of(args[2]));
            System.out.println("PLAYER_STATISTICS_ORIGINAL clean");
        } finally {
            if(writer!=null)joinWorkers();
            app.J=false;
            for(Thread thread:Thread.getAllStackTraces().keySet())
                if(thread.getName().equals("Timer hack thread")) {
                    thread.interrupt();thread.join(2000);require(!thread.isAlive(),"timer retired");
                }
        }
    }
}
