import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** All receivers, Worlds, pools and virtual calls are the unchanged originals. */
public final class EnvironmentObservation {
    static fd world;static lm chunk;static int rows;
    static Field field(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f;}
    static void clear(){Arrays.fill(chunk.b,(byte)0);Arrays.fill(chunk.e.a,(byte)0);}
    static void cell(int x,int y,int z,int id,int metadata){chunk.b[(x<<11)|(z<<7)|y]=(byte)id;chunk.e.a(x,y,z,metadata);}
    static void flow(int id){for(int y=60;y<69;++y){cell(6,y,6,id,0);cell(5,y,6,id,0);cell(6,y,5,id,0);cell(6,y,7,id,0);cell(7,y,6,id,7);}}
    static int boxCursor()throws Exception{return field(eq.class,"h").getInt(null);}
    static int vectorCursor()throws Exception{return field(bt.class,"e").getInt(null);}
    static int flag(boolean b){return b?1:0;}
    static void reset(){eq.b();bt.b();}
    static void probe(String label,int kind,int c,boolean value,sn e)throws Exception{
        System.out.printf(Locale.ROOT,"%s 0 %d %d %d %016x %016x %016x %d %d%n",label,kind,c,flag(value),
            Double.doubleToRawLongBits(e.aP),Double.doubleToRawLongBits(e.aQ),Double.doubleToRawLongBits(e.aR),boxCursor(),vectorCursor());++rows;
    }
    static void probes(sn e,int kind)throws Exception{
        e.e(6.5,64.1,6.5);
        for(int c=0;c<6;++c){clear();if(c==1||c==5)flow(8);if(c==2)flow(9);if(c==3)flow(10);if(c==4)cell(6,64,6,1,0);
            reset();e.aP=e.aQ=e.aR=0;boolean result=e.k_();if(c==5)result=e.k_();probe("WATER",kind,c,result,e);}
        for(int c=0;c<4;++c){clear();if(c!=0)flow(c==1?10:c==2?11:8);reset();e.aP=2;e.aQ=-3;e.aR=4;probe("LAVA",kind,c,e.ah(),e);}
        Field stored=field(sn.class,"bx");boolean before=stored.getBoolean(e);
        try{for(int value=0;value<2;++value){stored.setBoolean(e,value!=0);System.out.printf("FLAG 0 %d %d %d%n",kind,value,flag(e.ag()));++rows;}}
        finally{stored.setBoolean(e,before);}
    }
    static void clearance(sn e,int kind)throws Exception{
        e.e(6.5,64.1,6.5);
        for(int c=0;c<5;++c){clear();if(c==1)cell(6,64,6,1,0);if(c==2||c==3)flow(c==2?8:10);if(c==4)cell(7,64,6,1,0);
            reset();boolean result=e.f(c==4?1.0:0.0,0,0);
            System.out.printf("CLEAR 0 %d %d %d %d %d%n",kind,c,flag(result),boxCursor(),((List<?>)field(fd.class,"K").get(world)).size());++rows;}
    }
    static void relative(sn e,int kind){
        for(int i=0;i<EnvironmentCases.FLOATS.length;++i)for(int j=0;j<EnvironmentCases.FLOATS.length;++j){
            e.aS=Float.intBitsToFloat(EnvironmentCases.YAWS[(i+2*j)%EnvironmentCases.YAWS.length]);
            e.aP=Double.longBitsToDouble(EnvironmentCases.VELOCITIES[i%EnvironmentCases.VELOCITIES.length]);e.aQ=-0.0;
            e.aR=Double.longBitsToDouble(EnvironmentCases.VELOCITIES[j%EnvironmentCases.VELOCITIES.length]);
            e.a(Float.intBitsToFloat(EnvironmentCases.FLOATS[i]),Float.intBitsToFloat(EnvironmentCases.FLOATS[j]),
                Float.intBitsToFloat(EnvironmentCases.FLOATS[(3*i+5*j+5)%EnvironmentCases.FLOATS.length]));
            System.out.printf(Locale.ROOT,"REL 0 %d %d %016x %016x %016x%n",kind,i*EnvironmentCases.FLOATS.length+j,
                Double.doubleToRawLongBits(e.aP),Double.doubleToRawLongBits(e.aQ),Double.doubleToRawLongBits(e.aR));++rows;
        }
    }
    static void prefixes(sn e)throws Exception{
        for(int operation=0;operation<3;++operation){reset();boolean failed=false;
            try{if(operation==0)e.k_();else if(operation==1)e.ah();else e.f(0,0,0);}
            catch(NullPointerException expected){failed=true;}
            if(!failed)throw new AssertionError("actual null World must fail");
            System.out.printf("PREFIX 0 1 %d %d %d%n",operation,boxCursor(),vectorCursor());++rows;}
    }
    public static void main(String[] args)throws Exception{
        Path root=Path.of(args[0]);Files.createDirectories(root);
        world=new fd(new mx(root.toFile(),"Environment",false),"Environment",xa.a(0),23L);chunk=world.c(0,0);
        sn[] actors={new hl(world,6.5,64.1,6.5,new iz(262,5,7)),new ju(world,6.5,64.1,6.5,1),
            new yl(world,6.5,64.1,6.5,0),new dc(null,world,new gr(null,null),0)};
        for(int kind=0;kind<actors.length;++kind){probes(actors[kind],kind);clearance(actors[kind],kind);relative(actors[kind],kind);}
        sn nullWorld=new ju((fd)null);prefixes(nullWorld);relative(nullWorld,4);
        clear();actors[1].e(6.5,64.1,6.5);actors[3].e(6.5,64.1,6.5);
        if(!world.b(actors[1])||!world.b(actors[3]))throw new AssertionError("actual publication");
        for(int c=0;c<2;++c){reset();boolean result=(c==0?actors[3]:actors[1]).f(0,0,0);
            System.out.printf("PUBLISHED 0 %d %d %d %d%n",c,flag(result),boxCursor(),((List<?>)field(fd.class,"K").get(world)).size());++rows;}
        System.out.println("Original entity environment: "+rows+" rows");
    }
}
