import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Read/call unchanged concrete owners. Voxel bytes are declared prepared
 * inputs on the actual acquired Chunk; no game classes are substituted. */
public final class MaterialObservation {
    static fd world;static lm chunk;static ju entity;static int rows;
    static Field field(Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f;}
    static int cursor()throws Exception{return field(bt.class,"e").getInt(null);}
    static void clear(){Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);}
    static void cell(int x,int y,int z,int id,int metadata){chunk.b[(x<<11)|(z<<7)|y]=(byte)id;chunk.e.a(x,y,z,metadata);}
    static int flag(boolean value){return value?1:0;}
    static void flow(int c)throws Exception{
        clear();int id=c>=11?53:c==5?9:c==6?10:c==7?11:c==8?1:8;int neighbor=c==12?8:id;
        cell(6,64,6,id,c==3||c==4||c==9?8:0);
        if(c!=3&&c!=4&&c!=9){cell(5,64,6,neighbor,c==2?7:0);cell(6,64,5,neighbor,0);cell(6,64,7,neighbor,c==1?7:0);
            if(c==10)cell(7,63,6,id,5);else cell(7,64,6,neighbor,7);}
        if(c==3)cell(6,64,5,1,0);if(c==4)cell(6,64,5,79,0);
        ln material=uu.m[id].bA;
        entity.aP=entity.aQ=entity.aR=0;bt.b();
        boolean result=world.a(eq.a(6.1,64.1,6.1,6.2,64.2,6.2),material,entity);
        System.out.printf(Locale.ROOT,"FLOW 0 %d %d %016x %016x %016x %d%n",c,flag(result),
            Double.doubleToRawLongBits(entity.aP),Double.doubleToRawLongBits(entity.aQ),Double.doubleToRawLongBits(entity.aR),cursor());++rows;
    }
    static void scans()throws Exception{
        clear();cell(6,64,6,8,7);
        double[][] boxes={{6.1,64.1,6.1,6.2,64.2,6.2},{6.1,65,6.1,6.2,65.2,6.2},
            {6.9,64.9,6.9,6.1,64.1,6.1},{6,64,6,5,63,5},{16,64,6,16.2,64.2,6.2},
            {6,-0.2,6,6.2,-0.1,6.2},{-32000000.2,64,6,-32000000.1,64.2,6.2},
            {6,64,-32000000.2,6.2,64.2,-32000000.1},{Double.NaN,64,6,-1,64.2,6.2},
            {6,Double.POSITIVE_INFINITY,6,6.2,Double.POSITIVE_INFINITY,6.2},
            {6,Double.NEGATIVE_INFINITY,6,6.2,Double.NEGATIVE_INFINITY,6.2}};
        for(int i=0;i<boxes.length;++i){double[] b=boxes[i];eq bounds=eq.a(b[0],b[1],b[2],b[3],b[4],b[5]);
            if(i==4){System.out.printf("SCAN 0 %d 2 %d%n",i,flag(world.a(bounds,ln.g,null)));++rows;continue;}
            System.out.printf("SCAN 0 %d 0 %d%n",i,flag(world.a(bounds,ln.g)));++rows;
            System.out.printf("SCAN 0 %d 1 %d%n",i,flag(world.b(bounds)));++rows;}
        for(int level=0;level<16;++level){cell(6,64,6,8,level);
            double[] ys={64,64.2,64.5,64.9,65,Double.NaN,Double.POSITIVE_INFINITY,Double.NEGATIVE_INFINITY};
            for(int i=0;i<ys.length;++i){entity.e(6.1,ys[i],6.1);
                System.out.printf("EYE 0 %d %d %d%n",level,i,flag(entity.a(ln.g)));++rows;}}
    }
    static void vectors()throws Exception{
        double[][] values={{0,0,0},{3,4,12},{0.00009,0,0},{0.0001,0,0},{-0.0,-0.0,-0.0},
            {Double.NaN,0,0},{Double.POSITIVE_INFINITY,0,0},{Double.MAX_VALUE,0,0},
            {Double.longBitsToDouble(0xfff8000000000123L),Double.longBitsToDouble(0x7ff8000000000456L),0},
            {1e-300,2e-300,3e-300}};
        for(int i=0;i<values.length;++i){bt.b();double[] v=values[i];bt a=bt.b(v[0],v[1],v[2]);
            double length=a.d();bt n=a.c();System.out.printf(Locale.ROOT,"VEC 0 %d %016x %016x %016x %016x %d%n",i,
                Double.doubleToRawLongBits(length),Double.doubleToRawLongBits(n.a),Double.doubleToRawLongBits(n.b),Double.doubleToRawLongBits(n.c),cursor());++rows;}
    }
    static void player()throws Exception{
        dc player=new dc(null,world,new gr(null,null),0);clear();
        double[] ys={64,64.2,64.5,64.9,65,Double.NaN,Double.POSITIVE_INFINITY,Double.NEGATIVE_INFINITY};
        for(int level=0;level<16;++level){cell(6,64,6,8,level);
            for(int i=0;i<ys.length;++i){player.e(6.1,ys[i],6.1);
                System.out.printf("PLAYER 0 %d %d %d%n",level,i,flag(player.a(ln.g)));++rows;}}
    }
    public static void main(String[] args)throws Exception{
        Path root=Path.of(args[0]);Files.createDirectories(root);
        world=new fd(new mx(root.toFile(),"Material",false),"Material",xa.a(0),23L);
        chunk=world.c(0,0);entity=new ju(world,6.1,64,6.1,1);
        for(int i=0;i<13;++i){
            if(i==12){Field a=field(ss.class,"a");Object before=a.get(uu.m[53]);a.set(uu.m[53],uu.m[8]);
                try{flow(i);}finally{a.set(uu.m[53],before);}}
            else flow(i);
        }
        scans();vectors();player();
        System.out.println("Original material movement: "+rows+" rows");
    }
}
