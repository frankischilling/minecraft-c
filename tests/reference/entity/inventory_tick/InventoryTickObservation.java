import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Authored finite inputs; all game owners and invoked bodies are original. */
public final class InventoryTickObservation {
    static fd world;static dc player;static int rows;
    static Field field(Class<?> c,String n)throws Exception {Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static void require(boolean v,String message){if(!v)throw new AssertionError(message);}
    static int flag(boolean v){return v?1:0;}
    static iz stack(int id,int count,int damage,int animation){iz s=new iz(id,count,damage);s.b=animation;return s;}
    static void inventory()throws Exception {
        ix empty=new ix(null);empty.e();iz s=stack(262,3,7,7);ix inv=player.c;
        inv.a[0]=inv.a[5]=inv.b[0]=s;inv.e=true;inv.c=-1;
        for(int c=0;c<4;++c){if(c==2)inv.a[0]=null;if(c==3)inv.a[5]=null;inv.e();
            System.out.printf("CMP INV %d %d %d %d %d%n",c,s.b,flag(inv.a[0]==s),flag(inv.a[5]==s),flag(inv.b[0]==s));++rows;}
        empty.a[0]=s;s.b=9;boolean failed=false;
        try{empty.e();}catch(NullPointerException expected){failed=true;}
        require(failed&&s.b==9,"null Player prefix");System.out.println("CMP NULLPLAYER 9");++rows;
        inv.a[0]=s;player.a((fd)null);inv.e();player.a(world);
        System.out.println("CMP NULLWORLD "+s.b);++rows;inv.a[0]=inv.b[0]=null;
    }
    static void items(){
        for(int i=0;i<gm.c.length;++i){gm item=gm.c[i];if(item==null||item instanceof wr)continue;
            iz s=stack(i,-3,0x13579,i%3);item.a((iz)null,(fd)null,(sn)null,Integer.MIN_VALUE,true);
            s.a((fd)null,(sn)null,Integer.MIN_VALUE,true);
            System.out.printf("CMP ITEM %d %d %d %d%n",i,s.b,s.a,s.i());++rows;
        }
        int[] animations={Integer.MIN_VALUE,-1,0,1,Integer.MAX_VALUE};
        for(int i=0;i<animations.length;++i){iz s=stack(262,1,0,animations[i]);s.a((fd)null,(sn)null,0,false);
            System.out.printf("CMP ANIMATION %d %d%n",i,s.b);++rows;}
        for(int c=0;c<4;++c){iz s=stack(c==0?-1:c==1?-1:c==2?500:gm.c.length,1,0,2);gm[] saved=gm.c;
            if(c==0)gm.c=null;boolean failed=false;
            try{s.a((fd)null,(sn)null,0,false);}
            catch(NullPointerException expected){require(c==0||c==2,"wrong null exception branch");failed=true;}
            catch(ArrayIndexOutOfBoundsException expected){require(c==1||c==3,"wrong bounds exception branch");failed=true;}
            finally{gm.c=saved;}
            require(failed,"missing actual exception");System.out.printf("CMP FAILURE %d %d%n",c,s.b);++rows;
        }
    }
    static void mapRow(int c,iz s,iu map,hc manager,boolean same)throws Exception {
        Map<?,?> named=(Map<?,?>)field(hc.class,"b").get(manager),ids=(Map<?,?>)field(hc.class,"d").get(manager);
        List<?> ordered=(List<?>)field(hc.class,"c").get(manager);Object trackers=field(iu.class,"j").get(map);
        System.out.printf("CMP MAP %d %d %d %d %d %d %d %d %d %d %d %d %d%n",c,s.i(),map.b,map.c,
            (int)map.e,(int)map.d,flag(map.b()),map.f.length,named.size(),ordered.size(),ids.size(),flag(same),
            flag(map.h!=map.i&&map.h!=trackers&&map.i!=trackers));++rows;
    }
    static void maps(mx handler)throws Exception {
        wr item=(wr)gm.c[358];iz s=stack(358,1,73,4);world.B=true;
        item.a((iz)null,world,(sn)null,0,true);s.a(world,(sn)null,0,true);world.B=false;
        System.out.println("CMP REMOTE "+s.b);++rows;
        world.x().a(17,64,-33);hc manager=world.z;
        iu first=item.a(s,world);require(first.a.equals("map_0"),"real new Map identity");mapRow(0,s,first,manager,false);
        first.a(false);iu second=item.a(s,world);require(second==first,"same cached MapData");mapRow(1,s,second,manager,true);
        s.b(Integer.MIN_VALUE);world.x().a(99,64,101);second=item.a(s,world);
        require(second!=first&&second.a.equals("map_1"),"distinct MapData");mapRow(2,s,second,manager,false);
        s.b=2;sn falling=new ju(world,8,8,8,1);item.a(s,world,falling,0,false);
        System.out.println("CMP NONPLAYER "+s.b);++rows;
        manager.a();hc loaded=new hc(handler);world.z=loaded;
        iu disk=item.a(s,world);world.z=manager;require(disk!=second,"real disk-loaded owner");mapRow(3,s,disk,loaded,false);
    }
    public static void main(String[] args)throws Exception {
        Path root=Path.of(args[0]);Files.createDirectories(root);
        mx handler=new mx(root.toFile(),"Inventory",false);
        world=new fd(handler,"Inventory",xa.a(0),23L);world.c(0,0);
        // Authored valid filesystem input, with the same data-directory
        // precondition on both sides. No source handler or callback replacement.
        Files.createDirectories(root.resolve("Inventory/data"));
        player=new dc(null,world,new gr(null,null),0);
        Path jar=Path.of(args[1]).toRealPath();
        for(Class<?> type:new Class<?>[]{fd.class,dc.class,gs.class,ls.class,sn.class,ix.class,iz.class,gm.class,wr.class,iu.class,hc.class,hm.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class "+type);
        inventory();items();maps(handler);System.out.println("Original inventory tick: "+rows+" rows");
    }
}
