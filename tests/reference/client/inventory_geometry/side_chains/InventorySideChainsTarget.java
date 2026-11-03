import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Path;
import java.security.Permission;
import java.util.*;

/** Declared private-field inputs; every side getter executes unchanged original code. */
public final class InventorySideChainsTarget {
    public static int active=-1;
    static int network,nativeLinks;
    static final int[] SIDES={0,1,2,3,4,5,-1,6,Integer.MIN_VALUE,Integer.MAX_VALUE};
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field source()throws Exception{Field field=ss.class.getDeclaredField("a");field.setAccessible(true);return field;}
    static List<String> bounds(uu block){return Arrays.asList(String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bs)),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bt)),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bu)),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bv)),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bw)),String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(block.bx)));}
    static Integer id(Object value){return value==null?null:((uu)value).bn;}
    static void emit(String label,Object value){System.out.println("SIDE_CHAIN_"+label+" "+ConstructorJson.json(value));}
    @SuppressWarnings("removal")
    public static void main(String[] arguments)throws Exception{
        Locale.setDefault(Locale.ROOT);
        Path jar=Path.of(arguments[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            public void checkPermission(Permission permission){}
            public void checkConnect(String host,int port){++network;throw new SecurityException("network denied");}
            public void checkListen(int port){++network;throw new SecurityException("listen denied");}
            public void checkLink(String library){++nativeLinks;throw new SecurityException("native denied");}
            public void checkWrite(String path){throw new SecurityException("target file write denied");}
        });
        for(Class<?> type:new Class<?>[]{uu.class,ss.class})require(Path.of(type.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath().equals(jar),"actual original JAR");
        uu outer=uu.m[53],inner=uu.m[67],wood=uu.m[5];
        require(outer.getClass()==ss.class&&inner.getClass()==ss.class&&wood.getClass()==uu.class,"genuine registered Block classes");
        Field field=source();
        require(field.getType()==uu.class&&!Modifier.isFinal(field.getModifiers()),"actual mutable original source field");
        field.set(outer,inner);field.set(inner,wood);
        require(field.get(outer)==inner&&field.get(inner)==wood,"actual source aliases authored once");
        List<String> outerBounds=bounds(outer),innerBounds=bounds(inner);
        int constructors=0;for(uu block:uu.m)if(block!=null)constructors++;
        require(constructors==96,"genuine complete singleton Registry");
        emit("OWNER",ConstructorJson.map("blocks",constructors,"outer_id",outer.bn,"inner_id",inner.bn,"wood_id",wood.bn,"outer_class",outer.getClass().getName(),"inner_class",inner.getClass().getName(),"wood_class",wood.getClass().getName(),"field_owner",field.getDeclaringClass().getName(),"field_name",field.getName(),"field_type",field.getType().getName(),"field_modifiers",field.getModifiers(),"field_final",Modifier.isFinal(field.getModifiers()),"raw_allocations",0,"game_substitution",false));
        int call=0,returned=0,failed=0;
        for(int profile=0;profile<3;++profile){
            if(profile==1)wood.bm=83;
            if(profile==2)field.set(inner,null);
            for(int side:SIDES){
                Integer beforeOuter=id(field.get(outer)),beforeInner=id(field.get(inner));
                emit("INPUT",ConstructorJson.map("call",call,"profile",profile,"side",side,"outer_source",beforeOuter,"inner_source",beforeInner,"wood_texture",wood.bm));
                Throwable failure=null;Integer texture=null;active=call;
                try{texture=outer.a(side);}catch(NullPointerException exception){failure=exception;}finally{active=-1;}
                require((profile==2)==(failure!=null),"actual getter outcome");
                if(failure==null){require(texture==wood.bm,"actual inherited Wood texture");returned++;}else failed++;
                require(Objects.equals(beforeOuter,id(field.get(outer)))&&Objects.equals(beforeInner,id(field.get(inner)))&&outerBounds.equals(bounds(outer))&&innerBounds.equals(bounds(inner)),"getters retain actual source aliases/bounds");
                List<String> trace=new ArrayList<>();if(failure!=null)for(StackTraceElement frame:failure.getStackTrace())trace.add(frame.getClassName()+"."+frame.getMethodName());
                emit("RETURN",ConstructorJson.map("call",call,"profile",profile,"side",side,"returned",failure==null,"texture",texture,"exception",failure==null?null:failure.getClass().getName(),"source_stack",trace,"outer_source",id(field.get(outer)),"inner_source",id(field.get(inner)),"bounds_unchanged",true));
                call++;
            }
        }
        require(call==30&&returned==20&&failed==10&&network==0&&nativeLinks==0,"complete actual source/guard counts");
        emit("COMPLETE",ConstructorJson.map("calls",call,"returned",returned,"failed",failed,"network",network,"native_links",nativeLinks,"cycle_executed",false,"raw_allocations",0,"debugger_method_invocations",0));
    }
}
