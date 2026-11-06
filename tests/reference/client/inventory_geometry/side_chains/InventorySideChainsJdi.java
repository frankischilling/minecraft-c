import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only source-entry and actual nested NPE-location observer. */
public final class InventorySideChainsJdi {
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> types=vm.classesByName(name);return types.isEmpty()?null:types.get(0);}
    static int active(VirtualMachine vm){ReferenceType target=type(vm,"InventorySideChainsTarget");return target==null?-1:((IntegerValue)target.getValue(target.fieldByName("active"))).value();}
    static int integer(ObjectReference object,String name){return ((IntegerValue)object.getValue(object.referenceType().fieldByName(name))).value();}
    static String hash(byte[] value)throws Exception{StringBuilder text=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(value))text.append(String.format(Locale.ROOT,"%02x",b&255));return text.toString();}
    static Map<String,Object> where(Location location){return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex());}
    static void breakpoint(EventRequestManager manager,ReferenceType type){List<Method> methods=type.methodsByName("a","(I)I");if(methods.size()!=1)throw new AssertionError("one declared source getter");BreakpointRequest request=manager.createBreakpointRequest(methods.get(0).location());request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
    static void drain(Process process,InputStream input,Path path,List<Throwable> errors){try(OutputStream output=Files.newOutputStream(path)){byte[] data=new byte[4096];int size=0,n;while((n=input.read(data))!=-1){size+=n;if(size>1048576){process.destroyForcibly();throw new IOException("pipe bound");}output.write(data,0,n);}}catch(Throwable problem){errors.add(problem);}}
    public static void main(String[] arguments)throws Exception{
        Path out=Path.of(arguments[0]);String quote="\"";
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(arguments[1]);options.get("main").setValue("InventorySideChainsTarget "+quote+arguments[3]+quote);
        options.get("options").setValue("-Xint -Xverify:all -Xmx128m -Djava.awt.headless=true -Djava.security.manager=allow -cp "+quote+arguments[2]+quote);
        options.get("suspend").setValue("true");VirtualMachine vm=connector.launch(options);Process process=vm.process();
        List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(process,process.getInputStream(),out.resolve("target.stdout"),errors)),stderr=new Thread(()->drain(process,process.getErrorStream(),out.resolve("target.stderr"),errors));stdout.start();stderr.start();
        int ssEntries=0,uuEntries=0,exceptions=0;Set<String> pinned=new HashSet<>();
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.ndjson")))){
            EventRequestManager manager=vm.eventRequestManager();
            for(String name:new String[]{"uu","ss"}){ClassPrepareRequest request=manager.createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();}
            ExceptionRequest request=manager.createExceptionRequest(null,true,true);request.addClassFilter("ss");request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(90);boolean alive=true;
            while(alive){if(System.nanoTime()>deadline)throw new IOException("observer deadline");EventSet events=vm.eventQueue().remove(1000);if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent)breakpoint(manager,((ClassPrepareEvent)event).referenceType());
                    else if(event instanceof BreakpointEvent){BreakpointEvent reached=(BreakpointEvent)event;int call=active(vm);if(call<0)continue;Method method=reached.location().method();String owner=method.declaringType().name();
                        if(pinned.add(owner))journal.println(ConstructorJson.json(ConstructorJson.map("event","live-code","class",owner,"method",method.name(),"descriptor",method.signature(),"bytes",method.bytecodes().length,"sha256",hash(method.bytecodes()))));
                        StackFrame frame=reached.thread().frame(0);ObjectReference receiver=frame.thisObject();
                        ObjectReference source=owner.equals("ss")?(ObjectReference)receiver.getValue(receiver.referenceType().fieldByName("a")):null;
                        journal.println(ConstructorJson.json(ConstructorJson.map("event","getter-entry","call",call,"class",owner,"receiver",receiver.uniqueID(),"id",integer(receiver,"bn"),"side",((IntegerValue)frame.getArgumentValues().get(0)).value(),"source_id",source==null?null:integer(source,"bn"),"texture",integer(receiver,"bm"))));if(owner.equals("ss"))ssEntries++;else uuEntries++;
                    }else if(event instanceof ExceptionEvent){ExceptionEvent reached=(ExceptionEvent)event;int call=active(vm);if(call<0)continue;List<Object> frames=new ArrayList<>();for(StackFrame frame:reached.thread().frames())if(frame.location().declaringType().name().equals("ss"))frames.add(ConstructorJson.map("site",where(frame.location()),"receiver_id",integer(frame.thisObject(),"bn")));
                        journal.println(ConstructorJson.json(ConstructorJson.map("event","source-exception","call",call,"exception",reached.exception().referenceType().name(),"site",where(reached.location()),"frames",frames)));exceptions++;
                    }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
                }
                journal.flush();if(journal.checkError())throw new IOException("journal write");try{events.resume();}catch(VMDisconnectedException ended){alive=false;}
            }
        }finally{try{vm.dispose();}catch(VMDisconnectedException ended){}if(!process.waitFor(15,TimeUnit.SECONDS))process.destroyForcibly();stdout.join(5000);stderr.join(5000);}
        if(process.exitValue()!=0||!errors.isEmpty()||ssEntries!=60||uuEntries!=20||exceptions!=10)throw new AssertionError("complete nested source observation exit="+process.exitValue()+" ss="+ssEntries+" uu="+uuEntries+" npe="+exceptions+" pipes="+errors);
    }
}
