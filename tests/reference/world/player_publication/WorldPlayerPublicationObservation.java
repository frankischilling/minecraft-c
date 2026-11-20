import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.List;
import java.util.Locale;
import java.util.StringJoiner;

/** Authored read-only observations after actual original World operations. */
public final class WorldPlayerPublicationObservation {
    private static Object field(Object object, Class<?> owner, String name) throws Exception {
        Field field=owner.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(object);
    }
    private static void snapshot(fd world, dc player, String phase) throws Exception {
        List<?> players=(List<?>)field(world,fd.class,"d");
        List<?> entities=(List<?>)field(world,fd.class,"b");
        lm chunk=world.c(player.bG,player.bI);
        List<?>[] slices=(List<?>[])field(chunk,lm.class,"m");
        List<?> selected=slices[player.bH];
        for(List<?> list:new List<?>[]{players,entities,selected})
            for(Object entry:list) if(entry!=player) throw new AssertionError("source identity");
        System.out.println("PUB\t"+phase+"\td="+players.size()+"\tb="+entities.size()+
            "\tJ="+field(world,fd.class,"J")+"\tslice="+selected.size()+"\tmember="+player.bF+
            "\tx="+player.bG+"\ty="+player.bH+"\tz="+player.bI);
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=0) throw new IllegalArgumentException("no arguments");
        Path jar=Path.of(fd.class.getProtectionDomain().getCodeSource().getLocation().toURI());
        StringJoiner digest=new StringJoiner("");
        for(byte value:MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(jar)))
            digest.add(String.format(Locale.ROOT,"%02x",value&255));
        if(!digest.toString().equals("af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"))
            throw new AssertionError("original runtime JAR origin");
        fd world=new fd(new wy(),"PlayerPublication",xa.a(0),5143L);
        if(world.x.h()!=null || !field(world,fd.class,"v").getClass().getName().equals("ok"))
            throw new AssertionError("real Info/provider setup");
        world.y();
        if((Boolean)field(world,fd.class,"J")) throw new AssertionError("empty sleeping scan");
        world.a(new br(2,64,2));
        dc player=new dc(null,world,new gr("Publish",""),0);
        world.a((gs)player);
        snapshot(world,player,"attached");
        if(!world.b((sn)player)) throw new AssertionError("duplicate result");
        snapshot(world,player,"duplicate");
        world.a((pm)null);
        try {
            world.b((sn)player);
            throw new AssertionError("null WorldAccess did not fail");
        } catch(NullPointerException expected) {
            StackTraceElement site=expected.getStackTrace()[0];
            if(!site.getClassName().equals("fd") || !site.getMethodName().equals("c"))
                throw new AssertionError("actual callback receiver",expected);
        }
        snapshot(world,player,"null-access");
    }
}
