import java.nio.file.*;
import java.security.Permission;
import java.util.*;
/** Authored scalar operands around unchanged registered original Item. */
public final class DyeIconObservation {
  static int network,natives,execs,writes;
  static void need(boolean b,String s){if(!b)throw new AssertionError(s);}
  @SuppressWarnings("removal") public static void main(String[] args)throws Exception{
    Locale.setDefault(Locale.ROOT);need(args.length==2,"jar,profiles");
    Path jar=Path.of(args[0]).toRealPath();List<String> rows=Files.readAllLines(Path.of(args[1]));
    System.setSecurityManager(new SecurityManager(){public void checkPermission(Permission p){}
      public void checkConnect(String h,int p){network++;throw new SecurityException("network");}
      public void checkListen(int p){network++;throw new SecurityException("listen");}
      public void checkLink(String n){natives++;throw new SecurityException("native");}
      public void checkExec(String n){execs++;throw new SecurityException("exec");}
      public void checkWrite(String p){writes++;throw new SecurityException("write");}
      public void checkDelete(String p){writes++;throw new SecurityException("delete");}});
    for(Class<?> c:new Class<?>[]{gm.class,km.class,iz.class})need(Path.of(c.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"original origin");
    Class.forName("uu"); // Declared original Block-before-Item initialization order.
    gm item=gm.c[351];need(item.getClass()==km.class,"actual registered Dye");
    int initial=item.bh,index=0;
    System.out.println("DYE_ICON_INIT\t{\"id\":351,\"initial_bh\":"+initial+"}");
    for(String row:rows){String[] p=row.split("\t");need(p.length==3,"name,bh,damage");int bh=Integer.parseInt(p[1]),damage=Integer.parseInt(p[2]);
      item.bh=bh;iz stack=new iz(351,2,damage);
      System.out.println("DYE_ICON\t{\"index\":"+(index++)+",\"name\":\""+p[0]+"\",\"bh\":"+bh+",\"damage\":"+damage+",\"direct_icon\":"+item.a(damage)+",\"item_stack_icon\":"+item.b(stack)+",\"stack_icon\":"+stack.b()+",\"color\":"+item.f(damage)+",\"full3d\":"+item.b()+",\"rotate\":"+item.c()+"}");
    }
    item.bh=initial;
    System.out.println("DYE_ICON_COMPLETE rows="+index+" network="+network+" native="+natives+" execs="+execs+" writes="+writes+" restored_bh="+item.bh);
    need(network==0&&natives==0&&execs==0&&writes==0,"no external effects");
  }
}
