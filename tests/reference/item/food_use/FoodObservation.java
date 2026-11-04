import java.lang.reflect.*;
import java.nio.file.*;
import java.security.Permission;
import java.util.*;
import net.minecraft.client.Minecraft;

/** Authored inputs around unchanged original ItemStack in-air calls. */
public final class FoodObservation {
    static int network,natives,execs,deniedWrites,worldWrites;
    static Field field(Class<?> owner,String name)throws Exception {Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static void need(boolean value,String text){if(!value)throw new AssertionError(text);}
    @SuppressWarnings("removal") public static void main(String[] args)throws Exception {
        Locale.setDefault(Locale.ROOT);need(args.length==3,"jar,privateworld,profiles");
        Path jar=Path.of(args[0]).toRealPath(),dir=Path.of(args[1]).toRealPath();
        List<String> profiles=Files.readAllLines(Path.of(args[2]));
        System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
            public void checkConnect(String h,int p){++network;throw new SecurityException("network");}
            public void checkListen(int p){++network;throw new SecurityException("listen");}
            public void checkLink(String n){++natives;throw new SecurityException("native");}
            public void checkExec(String n){++execs;throw new SecurityException("exec");}
            public void checkWrite(String p){if(Path.of(p).toAbsolutePath().normalize().startsWith(dir)){++worldWrites;return;}++deniedWrites;throw new SecurityException("outside privateworld");}
            public void checkDelete(String p){checkWrite(p);}});
        for(Class<?> t:new Class<?>[]{yw.class,bz.class,qk.class,gm.class,iz.class,fd.class,dc.class,gs.class,ls.class,sn.class,xi.class,jl.class})
            need(Path.of(t.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original class origin");
        kq app=new kq(null,null,null,854,480,false,null);
        try {
            app.k=new gr("FoodReference","-");field(Minecraft.class,"Z").set(app,dir.toFile());
            xi stats=new xi(app.k,dir.toFile());app.I=stats;
            fd world=new fd(new mx(dir.getParent().toFile(),dir.getFileName().toString(),false),"",23L);
            dc player=new dc(app,world,app.k,0);app.f=world;app.h=player;
            Field health=field(ls.class,"Y"),period=field(ls.class,"E"),resistance=field(sn.class,"by");
            int index=0;
            for(String line:profiles) {
                if(line.isBlank()||line.startsWith("#"))continue;
                String[] c=line.split("\\t");need(c.length==7,"seven inputs");
                int[] p=new int[7];for(int i=0;i<7;i++)p[i]=Integer.parseInt(c[i]);
                gm item=gm.c[p[0]];need(item instanceof yw,"actual Food receiver");
                iz stack=new iz(p[0],p[1],Integer.MIN_VALUE);stack.b=9;
                health.setInt(player,p[2]);period.setInt(player,p[3]);resistance.setInt(player,p[4]);
                world.B=p[6]!=0;
                int used=stats.a(jl.E[p[0]]);String exception="NONE";iz returned=null;
                try {returned=stack.a((fd)null,p[5]!=0?null:player);}catch(NullPointerException e){exception=e.getClass().getName();}
                int delta=stats.a(jl.E[p[0]])-used;
                System.out.println("FOOD\t"+index+"\t"+p[0]+"\t"+stack.a+"\t"+health.getInt(player)+"\t"+resistance.getInt(player)+"\t"+
                    (returned==null?-1:returned.c)+"\t"+(returned==null?-1:returned.a)+"\t"+(returned==null?-1:returned.i())+"\t"+(returned==null?-1:returned.b)+"\t"+
                    (returned==stack)+"\t"+exception+"\t"+delta+"\t"+world.B);
                ++index;
            }
            need(network==0&&natives==0&&execs==0&&deniedWrites==0,"no external source attempt");
            System.out.println("FOOD_COMPLETE rows="+index+" network="+network+" native="+natives+" execs="+execs+" denied-writes="+deniedWrites+" game-code-replacements=0 world-writes="+worldWrites);
        } finally {
            app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);need(!t.isAlive(),"Timer worker ended");}
        }
    }
}
