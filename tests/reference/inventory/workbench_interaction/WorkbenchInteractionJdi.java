import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only method/constructor audit: no target invocation, field store or entropy write. */
public final class WorkbenchInteractionJdi {
    static final String[] TYPES={"ob","dw","it","oo","id","da","aa","gp","pv","yv","ix","mq","wl","iz","gs","dc","hl","sn"};
    static int active=-1,records;
    static List<Object> calls=new ArrayList<>(),constructors=new ArrayList<>();
    static Map<String,Object> codes=new TreeMap<>();
    static ObjectReference actor;
    static List<String> maths=new ArrayList<>();
    static void require(boolean value,String text){if(!value)throw new AssertionError(text);}
    static Field field(ReferenceType type,String owner,String name){
        for(Field value:type.allFields())if(value.declaringType().name().equals(owner)&&value.name().equals(name))return value;
        throw new AssertionError("actual field "+owner+"."+name);
    }
    static Value get(ObjectReference value,String owner,String name){return value.getValue(field(value.referenceType(),owner,name));}
    static long seed(ObjectReference value){ObjectReference atomic=(ObjectReference)get(value,"java.util.Random","seed");return ((LongValue)get(atomic,"java.util.concurrent.atomic.AtomicLong","value")).value();}
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static Map<String,Object> map(Object... fields){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<fields.length;i+=2)result.put((String)fields[i],fields[i+1]);return result;}
    static String json(Object value){
        if(value==null)return "null";
        if(value instanceof String)return "\""+((String)value).replace("\\","\\\\").replace("\"","\\\"")+"\"";
        if(value instanceof Number||value instanceof Boolean)return value.toString();
        List<String> values=new ArrayList<>();
        if(value instanceof Map){for(Object entry:((Map<?,?>)value).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)entry;values.add(json(e.getKey())+":"+json(e.getValue()));}return "{"+String.join(",",values)+"}";}
        for(Object item:(Iterable<?>)value)values.add(json(item));return "["+String.join(",",values)+"]";
    }
    static List<Object> vector(ObjectReference value,String[] names){List<Object> result=new ArrayList<>();for(String name:names)result.add(d(((DoubleValue)get(value,"sn",name)).value()));return result;}
    static String key(Method value){return value.declaringType().name()+"."+value.name()+value.signature();}
    static void code(Method method)throws Exception{
        if(!method.isNative()&&!method.isAbstract()&&!codes.containsKey(key(method))){byte[] bytes=method.bytecodes();codes.put(key(method),map("code_bytes",bytes.length,"code_sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))));}
    }
    static String quoted(String value){require(!value.contains("\"")&&!value.contains("\n")&&!value.contains("\r"),"literal command path");return "\""+value+"\"";}
    static final class Drain extends Thread{
        final InputStream input;final Path path;Throwable failure;
        Drain(InputStream input,Path path){this.input=input;this.path=path;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] bytes=new byte[4096];int count;long total=0;while((count=in.read(bytes))!=-1){total+=count;require(total<=4*1048576,"bounded unchanged-target transport");out.write(bytes,0,count);}}catch(Throwable error){failure=error;}}
    }
    static void boundary(EventRequestManager manager,ReferenceType type,String name){
        for(Method method:type.methodsByName(name)){BreakpointRequest request=manager.createBreakpointRequest(method.location());request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);request.enable();}
    }
    public static void main(String[] args)throws Exception{
        require(args.length==6,"classpath jar jdk-home output game inputs");Path output=Path.of(args[3]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[2]);options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+quoted(args[0]));
        options.get("main").setValue("WorkbenchInteractionObservation "+quoted(args[1])+" "+quoted(args[4])+" "+quoted(args[5]));options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process process=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("calls.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);process=vm.process();stdout=new Drain(process.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(process.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetBytecodes()&&vm.canGetMethodReturnValues(),"actual live Code and read-only return capability");
            EventRequestManager manager=vm.eventRequestManager();ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("WorkbenchInteractionObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);prepare.enable();
            List<EventRequest> enabled=new ArrayList<>();
            for(String type:TYPES){MethodEntryRequest request=manager.createMethodEntryRequest();request.addClassFilter(type);request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);enabled.add(request);}
            MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter("hl");exit.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);enabled.add(exit);
            MethodExitRequest math=manager.createMethodExitRequest();math.addClassFilter("java.lang.Math");math.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);enabled.add(math);
            boolean done=false;long deadline=System.nanoTime()+240_000_000_000L;
            while(!done){
                require(System.nanoTime()<deadline,"bounded read-only audit");EventSet events;
                try{events=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}
                if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent){ReferenceType type=((ClassPrepareEvent)event).referenceType();boundary(manager,type,"begin");boundary(manager,type,"end");}
                    else if(event instanceof BreakpointEvent){BreakpointEvent item=(BreakpointEvent)event;
                        if(item.location().method().name().equals("begin")){require(active==-1,"distinct invocation boundaries");active=((IntegerValue)item.thread().frame(0).getArgumentValues().get(0)).value();require(active==records,"ordered actual cases");calls.clear();constructors.clear();actor=null;for(EventRequest request:enabled)request.enable();}
                        else{require(active==records&&actor==null,"complete invocation and Item constructors");for(EventRequest request:enabled)request.disable();journal.println(json(map("index",active,"calls",calls,"constructors",constructors)));++records;active=-1;}
                    }else if(event instanceof MethodEntryEvent && active>=0){MethodEntryEvent item=(MethodEntryEvent)event;Method method=item.method();code(method);
                        Location caller=item.thread().frame(1).location();calls.add(map("method",key(method),"caller",key(caller.method()),"pc",caller.codeIndex()));
                        if(method.declaringType().name().equals("hl")&&method.name().equals("<init>")){require(actor==null&&method.signature().equals("(Lfd;DDDLiz;)V"),"actual full Item constructor");actor=item.thread().frame(0).thisObject();maths.clear();}
                    }else if(event instanceof MethodExitEvent && active>=0){MethodExitEvent item=(MethodExitEvent)event;Method method=item.method();
                        if(actor!=null&&method.declaringType().name().equals("java.lang.Math")&&method.name().equals("random"))maths.add(d(((DoubleValue)item.returnValue()).value()));
                        else if(actor!=null&&method.declaringType().name().equals("hl")&&method.name().equals("<init>")){
                            require(maths.size()==4,"four actual unchanged-constructor Math returns");constructors.add(map("math-values",new ArrayList<>(maths),"phase",f(((FloatValue)get(actor,"hl","d")).value()),
                                "yaw",f(((FloatValue)get(actor,"sn","aS")).value()),"position",vector(actor,new String[]{"aM","aN","aO"}),"velocity",vector(actor,new String[]{"aP","aQ","aR"}),
                                "private-random",String.format(Locale.ROOT,"%016x",seed((ObjectReference)get(actor,"sn","bs")))));actor=null;
                        }
                    }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(process.waitFor(20,TimeUnit.SECONDS)&&process.exitValue()==0,"unchanged original target completed");stdout.join(10000);stderr.join(10000);
            require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained target streams");require(records==117&&active==-1&&!journal.checkError(),"all declared calls plus genuine OO lifecycle observed");
            Files.writeString(output.resolve("live-Code.json"),json(codes)+"\n");
        }finally{if(process!=null&&process.isAlive()){process.destroyForcibly();process.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}}
    }
}
