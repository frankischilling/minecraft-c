import java.util.Locale;
import java.lang.reflect.Field;
/** Actual server concrete constructors with their genuine nullable World. */
public final class RelativeServerObservation {
    static int cursor(Class<?> owner,String name)throws Exception{Field f=owner.getDeclaredField(name);f.setAccessible(true);return f.getInt(null);}
    public static void main(String[] args)throws Exception{
        lq[] actors={new ez((dj)null),new gl((dj)null),new pr((dj)null),new gl((dj)null)};int rows=0;
        for(int actor=0;actor<actors.length;++actor){lq e=actors[actor];int kind=actor==3?4:actor;
            if(actor==3)for(int operation=0;operation<3;++operation){cz.a();ba.a();boolean failed=false;
                try{if(operation==0)e.f_();else if(operation==1)e.ae();else e.d(0,0,0);}
                catch(NullPointerException expected){failed=true;}
                if(!failed)throw new AssertionError("actual null World must fail");
                System.out.printf("PREFIX 1 1 %d %d %d%n",operation,cursor(cz.class,"h"),cursor(ba.class,"e"));++rows;}
            for(int i=0;i<EnvironmentCases.FLOATS.length;++i)for(int j=0;j<EnvironmentCases.FLOATS.length;++j){
                e.aV=Float.intBitsToFloat(EnvironmentCases.YAWS[(i+2*j)%EnvironmentCases.YAWS.length]);
                e.aS=Double.longBitsToDouble(EnvironmentCases.VELOCITIES[i%EnvironmentCases.VELOCITIES.length]);e.aT=-0.0;
                e.aU=Double.longBitsToDouble(EnvironmentCases.VELOCITIES[j%EnvironmentCases.VELOCITIES.length]);
                e.a(Float.intBitsToFloat(EnvironmentCases.FLOATS[i]),Float.intBitsToFloat(EnvironmentCases.FLOATS[j]),
                    Float.intBitsToFloat(EnvironmentCases.FLOATS[(3*i+5*j+5)%EnvironmentCases.FLOATS.length]));
                System.out.printf(Locale.ROOT,"REL 1 %d %d %016x %016x %016x%n",kind,i*EnvironmentCases.FLOATS.length+j,
                    Double.doubleToRawLongBits(e.aS),Double.doubleToRawLongBits(e.aT),Double.doubleToRawLongBits(e.aU));++rows;
            }
        }
        System.out.println("Original server relative: "+rows+" rows");
    }
}
