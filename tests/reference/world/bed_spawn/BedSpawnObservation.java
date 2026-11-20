import java.io.*;
import java.lang.reflect.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

/** Prepared genuine original Worlds and normal Chunks; target bodies are unchanged. */
public final class BedSpawnObservation {
    static volatile int active = -1;
    static Path archive, directory;
    static int network, nativeLinks, rejectedWrites;
    static fd world;
    static final List<lm> chunks = new ArrayList<>();
    static Field field(Class<?> type, String name)throws Exception {
        Field result=type.getDeclaredField(name);result.setAccessible(true);return result;
    }
    static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    static String hex(byte[] value){return HexFormat.of().formatHex(value);}
    static String seed(Random value)throws Exception {
        return String.format(Locale.ROOT,"%016x",((AtomicLong)field(Random.class,"seed").get(value)).get());
    }
    static List<Object> random(Random value)throws Exception {
        return Arrays.asList(seed(value),field(Random.class,"haveNextNextGaussian").getBoolean(value),
            String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(field(Random.class,"nextNextGaussian").getDouble(value))));
    }
    static String planes()throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");
        for(lm chunk:chunks){hash.update(ByteBuffer.allocate(8).putInt(chunk.j).putInt(chunk.k).array());hash.update(chunk.b);hash.update(chunk.e.a);}
        return hex(hash.digest());
    }
    static lm chunk(int x,int z){for(lm value:chunks)if(value.j==(x>>4)&&value.k==(z>>4))return value;throw new AssertionError("declared Chunk footprint");}
    static void cell(int x,int y,int z,int id,int metadata){
        need(y>=0&&y<128,"declared cell height");lm value=chunk(x,z);int index=((x&15)*16+(z&15))*128+y;
        value.b[index]=(byte)id;int offset=index>>1,shift=(index&1)*4;
        value.e.a[offset]=(byte)((value.e.a[offset]&~(15<<shift))|((metadata&15)<<shift));
    }
    static void bind(Path names,Path destination)throws Exception {
        try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(destination,StandardCharsets.UTF_8))){
            for(String name:Files.readAllLines(names,StandardCharsets.US_ASCII)){
                Class<?> type=Class.forName(name,false,BedSpawnObservation.class.getClassLoader());
                need(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(archive),"original class origin "+name);
                byte[] bytes;try(InputStream stream=type.getResourceAsStream("/"+name+".class")){need(stream!=null,"original class resource");bytes=stream.readAllBytes();}
                out.println(name+"\t"+bytes.length+"\t"+hex(MessageDigest.getInstance("SHA-256").digest(bytes)));
            }
            need(!out.checkError(),"class binding output");
        }
    }
    @SuppressWarnings("removal")
    static final class Guard extends SecurityManager {
        public void checkPermission(Permission permission){}
        public void checkConnect(String host,int port){++network;throw new SecurityException("unexpected network");}
        public void checkListen(int port){++network;throw new SecurityException("unexpected listener");}
        public void checkLink(String name){++nativeLinks;throw new SecurityException("unexpected native link");}
        public void checkWrite(String name){if(!Path.of(name).toAbsolutePath().normalize().startsWith(directory)){++rejectedWrites;throw new SecurityException("unexpected write");}}
        public void checkDelete(String name){checkWrite(name);}
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        need(args.length==5,"jar work profiles class-names binding");
        archive=Path.of(args[0]).toRealPath();directory=Path.of(args[1]).toAbsolutePath().normalize();Files.createDirectories(directory);
        System.setSecurityManager(new Guard());
        world=new fd(new wy(),"BedSpawnReference",xa.a(0),5143L);world.a(new br(0,64,0));
        for(int x=-2;x<=1;++x)for(int z=-2;z<=1;++z){lm value=world.c(x,z);need(value.getClass()==lm.class,"genuine normal Chunk");chunks.add(value);}
        cl provider=world.w();need(provider.getClass()==ok.class,"actual normal Map ChunkProvider");
        int number=0;
        for(String line:Files.readAllLines(Path.of(args[2]),StandardCharsets.US_ASCII)){
            int[] n=Arrays.stream(line.split(" ")).mapToInt(Integer::parseInt).toArray();
            need(n.length>=8&&n[0]==number&&n.length==8+5*n[7],"fixed input profile");
            for(lm value:chunks){Arrays.fill(value.b,(byte)0);Arrays.fill(value.e.a,(byte)0);}
            for(int i=0;i<n[7];++i){int k=8+5*i;cell(n[k],n[k+1],n[k+2],n[k+3],n[k+4]);}
            cell(n[2],n[3],n[4],chunk(n[2],n[4]).b[((n[2]&15)*16+(n[4]&15))*128+n[3]]&255,n[5]);
            world.r.setSeed(2468);br position=new br(n[2],n[3],n[4]);
            String before=planes();List<Object> randomBefore=random(world.r);Object result=null;String exception="NONE";
            active=number;
            try{br found=n[1]==0?ve.f(world,n[2],n[3],n[4],n[6]):gs.a(world,position);if(found!=null)result=Arrays.asList(found.a,found.b,found.c);need(found==null||found!=position,"fresh selected coordinate");}
            catch(NullPointerException failure){exception="NPE";}
            finally{active=-1;}
            String after=planes();List<Object> randomAfter=random(world.r);
            need(before.equals(after)&&randomBefore.equals(randomAfter)&&world.w()==provider,"read-only body preservation");
            need(position.a==n[2]&&position.b==n[3]&&position.c==n[4],"input coordinate preservation");
            List<Integer> input=new ArrayList<>();for(int value:n)input.add(value);
            System.out.println("BED_SPAWN "+json(map("schema","BED_SPAWN_V1","case",number,"input",input,
                "result",result,"exception",exception,"planes_before",before,"planes_after",after,
                "random_before",randomBefore,"random_after",randomAfter,"provider_same",world.w()==provider,
                "input_same",position.a==n[2]&&position.b==n[3]&&position.c==n[4],"fresh_result",result!=null)));
            ++number;
        }
        need(number==30,"complete profile inventory");bind(Path.of(args[3]),Path.of(args[4]));
        need(network==0&&nativeLinks==0&&rejectedWrites==0,"external effects");
        System.out.println("Original Bed spawn: 30 profiles; network=0; native=0; rejected_writes=0");
    }
    static Map<String,Object> map(Object... entries){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<entries.length;i+=2)result.put((String)entries[i],entries[i+1]);return result;}
    static String json(Object value){
        if(value==null)return "null";if(value instanceof Number||value instanceof Boolean)return value.toString();
        if(value instanceof String){StringBuilder result=new StringBuilder("\"");for(char c:((String)value).toCharArray())if(c=='"'||c=='\\')result.append('\\').append(c);else if(c<32||c>126)result.append(String.format(Locale.ROOT,"\\u%04x",(int)c));else result.append(c);return result.append('"').toString();}
        if(value instanceof Map){StringJoiner result=new StringJoiner(",","{","}");for(Map.Entry<?,?> entry:((Map<?,?>)value).entrySet())result.add(json(entry.getKey())+":"+json(entry.getValue()));return result.toString();}
        if(value instanceof Iterable){StringJoiner result=new StringJoiner(",","[","]");for(Object entry:(Iterable<?>)value)result.add(json(entry));return result.toString();}
        throw new IllegalArgumentException("JSON transport type");
    }
}
