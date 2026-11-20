import java.nio.file.Path;

/** Genuine first active use in a fresh JVM, with no prior World/Block bootstrap. */
public final class ColdItemObservation {
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args)throws Exception {
        iz stack=new iz(262,1,0);stack.b=5;
        boolean first=false,second=false;
        try{stack.a((fd)null,(sn)null,0,false);}
        catch(ExceptionInInitializerError error){
            require(error.getCause() instanceof NullPointerException,"actual first failure cause");
            StackTraceElement top=error.getCause().getStackTrace()[0];
            require(top.getClassName().equals("iz")&&top.getMethodName().equals("<init>"),"actual null Item constructor receiver");first=true;
        }
        require(first&&stack.b==4,"first real failure animation prefix");
        System.out.println("COLD 0 initializer-null-item "+stack.b);
        try{stack.a((fd)null,(sn)null,0,false);}
        catch(NoClassDefFoundError error){second=true;}
        require(second&&stack.b==3,"cached real failure animation prefix");
        System.out.println("COLD 1 cached-initializer "+stack.b);
        Path jar=Path.of(args[0]).toRealPath();
        for(Class<?> type:new Class<?>[]{iz.class,gm.class,wc.class,uu.class,jl.class,ep.class,ny.class})
            require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"foreign original class");
        System.out.println("Original cold Item: 2 calls");
    }
}
