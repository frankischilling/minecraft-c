import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only constructor observation. No game invocation or target-state write. */
public final class ExplosionJdi {
    static ObjectReference actor;
    static long ownInitial;
    static boolean initialCaptured;
    static int profile, records;
    static final List<String> maths=new ArrayList<>(), own=new ArrayList<>();
    static String codeHash;
    static int codeBytes;
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static Field field(ReferenceType type,String owner,String name){
        for(Field value:type.allFields())
            if(value.declaringType().name().equals(owner)&&value.name().equals(name))return value;
        throw new AssertionError("missing actual field "+owner+"."+name);
    }
    static Value get(ObjectReference value,String owner,String name){return value.getValue(field(value.referenceType(),owner,name));}
    static long seed(ObjectReference value){
        ObjectReference atomic=(ObjectReference)get(value,"java.util.Random","seed");
        return ((LongValue)get(atomic,"java.util.concurrent.atomic.AtomicLong","value")).value();
    }
    static String d(double value){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(value));}
    static String f(float value){return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(value));}
    static List<String> vector(ObjectReference value,String[] names){
        List<String> result=new ArrayList<>();for(String name:names)result.add(d(((DoubleValue)get(value,"sn",name)).value()));return result;
    }
    static Map<String,Object> state(ObjectReference value){
        List<String> rgb=new ArrayList<>();for(String name:new String[]{"i","j","k"})rgb.add(f(((FloatValue)get(value,"xw",name)).value()));
        return ConstructorJson.map("velocity",vector(value,new String[]{"aP","aQ","aR"}),
            "c",f(((FloatValue)get(value,"xw","c")).value()),"d",f(((FloatValue)get(value,"xw","d")).value()),
            "scale",f(((FloatValue)get(value,"xw","g")).value()),"rgb",rgb,
            "age",((IntegerValue)get(value,"xw","e")).value(),
            "lifetime",((IntegerValue)get(value,"xw","f")).value());
    }
    static String quoted(String value){
        if(value.indexOf('"')>=0||value.indexOf('\r')>=0||value.indexOf('\n')>=0)throw new IllegalArgumentException("path quoting");
        return "\""+value+"\"";
    }
    static final class Drain extends Thread {
        final InputStream input;final Path path;Throwable failure;
        Drain(InputStream input,Path path){this.input=input;this.path=path;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){
            byte[] bytes=new byte[4096];int count;long total=0;
            while((count=in.read(bytes))!=-1){total+=count;require(total<=2*1048576,"bounded target transport");out.write(bytes,0,count);}
        }catch(Throwable error){failure=error;}}
    }
    static void request(EventRequestManager manager,String type,boolean entry){
        if(entry){MethodEntryRequest request=manager.createMethodEntryRequest();request.addClassFilter(type);request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);request.enable();}
        else{MethodExitRequest request=manager.createMethodExitRequest();request.addClassFilter(type);request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);request.enable();}
    }
    public static void main(String[] args)throws Exception{
        require(args.length==4,"classpath jar jdk-home output");
        Path output=Path.of(args[3]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[2]);
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+quoted(args[0]));
        options.get("main").setValue("ExplosionObservation "+quoted(args[1]));
        options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process child=null;Drain stdout=null,stderr=null;
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(output.resolve("constructors.ndjson"),StandardOpenOption.CREATE_NEW))){
            vm=connector.launch(options);child=vm.process();
            stdout=new Drain(child.getInputStream(),output.resolve("target.stdout"));
            stderr=new Drain(child.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            require(vm.canGetMethodReturnValues()&&vm.canGetBytecodes(),"JDI read-only return/code capability");
            EventRequestManager manager=vm.eventRequestManager();
            request(manager,"gy",true);request(manager,"gy",false);
            request(manager,"java.util.Random",true);request(manager,"java.util.Random",false);
            request(manager,"java.lang.Math",false);
            boolean done=false;long deadline=System.nanoTime()+120_000_000_000L;
            while(!done){
                require(System.nanoTime()<deadline,"bounded original debugger run");
                EventSet events;
                try{events=vm.eventQueue().remove(1000);}catch(VMDisconnectedException expected){break;}
                if(events==null)continue;
                for(Event event:events){
                    if(event instanceof MethodEntryEvent){
                        MethodEntryEvent item=(MethodEntryEvent)event;Method method=item.method();
                        if(method.declaringType().name().equals("gy")&&method.name().equals("<init>")){
                            require(actor==null,"no nested GY constructor");actor=item.thread().frame(0).thisObject();
                            ReferenceType helper=vm.classesByName("ExplosionObservation").get(0);
                            profile=((IntegerValue)helper.getValue(helper.fieldByName("constructor_case"))).value();
                            maths.clear();own.clear();initialCaptured=false;
                            byte[] code=method.bytecodes();codeBytes=code.length;codeHash=HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code));
                        }else if(actor!=null&&method.declaringType().name().equals("java.util.Random")&&method.name().equals("nextFloat")){
                            ObjectReference random=item.thread().frame(0).thisObject();
                            if(random.equals(get(actor,"sn","bs"))&&!initialCaptured){ownInitial=seed(random);initialCaptured=true;}
                        }
                    }else if(event instanceof MethodExitEvent){
                        MethodExitEvent item=(MethodExitEvent)event;Method method=item.method();
                        if(actor!=null&&method.declaringType().name().equals("java.lang.Math")&&method.name().equals("random"))
                            maths.add(d(((DoubleValue)item.returnValue()).value()));
                        else if(actor!=null&&method.declaringType().name().equals("java.util.Random")&&method.name().equals("nextFloat")){
                            ObjectReference random=item.thread().frame(0).thisObject();
                            if(random.equals(get(actor,"sn","bs")))own.add(f(((FloatValue)item.returnValue()).value()));
                        }else if(actor!=null&&method.declaringType().name().equals("gy")&&method.name().equals("<init>")){
                            require(initialCaptured&&maths.size()==8&&own.size()==8,"actual GY eight Math and eight private Float returns");
                            ObjectReference random=(ObjectReference)get(actor,"sn","bs");
                            journal.println(ConstructorJson.json(ConstructorJson.map("profile",profile,"math_values",new ArrayList<>(maths),"own_values",new ArrayList<>(own),
                                "own_initial",String.format(Locale.ROOT,"%012x",ownInitial),"own_after",String.format(Locale.ROOT,"%012x",seed(random)),
                                "state",state(actor),"code_bytes",codeBytes,"code_sha256",codeHash)));
                            ++records;actor=null;
                        }
                    }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            require(child.waitFor(20,TimeUnit.SECONDS)&&child.exitValue()==0,"original target success");
            stdout.join(10000);stderr.join(10000);
            require(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"retained debugger transport");
            require(records==14&&actor==null&&!journal.checkError(),"fourteen complete actual constructors including warm invocation");
        }finally{
            if(child!=null&&child.isAlive()){child.destroyForcibly();child.waitFor(10,TimeUnit.SECONDS);}
            if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}
        }
    }
}
