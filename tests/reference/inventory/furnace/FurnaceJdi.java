import com.sun.jdi.Bootstrap;
import com.sun.jdi.ArrayReference;
import com.sun.jdi.PrimitiveValue;
import com.sun.jdi.StringReference;
import com.sun.jdi.IntegerValue;
import com.sun.jdi.Method;
import com.sun.jdi.ObjectReference;
import com.sun.jdi.ReferenceType;
import com.sun.jdi.StackFrame;
import com.sun.jdi.Value;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.VMDisconnectedException;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.connect.LaunchingConnector;
import com.sun.jdi.event.BreakpointEvent;
import com.sun.jdi.event.ClassPrepareEvent;
import com.sun.jdi.event.Event;
import com.sun.jdi.event.EventSet;
import com.sun.jdi.event.MethodEntryEvent;
import com.sun.jdi.event.MethodExitEvent;
import com.sun.jdi.event.VMDeathEvent;
import com.sun.jdi.event.VMDisconnectEvent;
import com.sun.jdi.request.BreakpointRequest;
import com.sun.jdi.request.ClassPrepareRequest;
import com.sun.jdi.request.EventRequest;
import com.sun.jdi.request.EventRequestManager;
import com.sun.jdi.request.MethodEntryRequest;
import com.sun.jdi.request.MethodExitRequest;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Passive JDI events only: no target invocation, field store or original breakpoint. */
public final class FurnaceJdi {
    static void need(boolean value,String message) { if (!value) throw new AssertionError(message); }
    static String quoted(String value) {
        need(!value.contains("\"") && !value.contains("\n") && !value.contains("\r"),"literal observer argument");
        return "\"" + value + "\"";
    }
    static String key(Method method) { return method.declaringType().name() + "." + method.name() + method.signature(); }
    static Map<String,Object> codes = new LinkedHashMap<String,Object>();
    static void code(Method method) throws Exception {
        if (!codes.containsKey(key(method))) {
            byte[] bytes = method.bytecodes();
            codes.put(key(method),FurnaceObservation.map("code_bytes",bytes.length,"code_sha256",
                HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes))));
        }
    }
    static final class Drain extends Thread {
        final InputStream input;
        final Path path;
        Throwable failure;
        Drain(InputStream input,Path path) { this.input=input;this.path=path;setDaemon(true); }
        public void run() {
            try (InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)) {
                byte[] block=new byte[4096];int count;long total=0;
                while ((count=in.read(block))!=-1) { total+=count;need(total<=4194304,"bounded target transport");out.write(block,0,count); }
            } catch (Throwable error) { failure=error; }
        }
    }
    static void boundary(EventRequestManager manager,ReferenceType type,String name) {
        List<Method> methods=type.methodsByName(name,"(I)V");need(methods.size()==1,"one observer boundary");
        BreakpointRequest request=manager.createBreakpointRequest(methods.get(0).location());
        request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);request.enable();
    }
    static final Map<ObjectReference,String> labels=new LinkedHashMap<ObjectReference,String>();
    static Object value(Value input) {
        if(input==null)return null;
        if(input instanceof PrimitiveValue) {
            PrimitiveValue p=(PrimitiveValue)input;
            Object v=p.type().name().equals("boolean")?p.booleanValue():p.longValue();
            return FurnaceObservation.map("type",p.type().name(),"value",v);
        }
        if(input instanceof StringReference)return FurnaceObservation.map("type","java.lang.String","value",((StringReference)input).value());
        ObjectReference object=(ObjectReference)input;String label=labels.get(object);
        if(label==null){label="O"+(labels.size()+1);labels.put(object,label);}
        Map<String,Object> out=FurnaceObservation.map("type",object.referenceType().name(),"identity",label);
        if(object.referenceType().name().equals("iz")) {
            List<Object> fields=new ArrayList<Object>();
            for(String name:new String[]{"c","a","d","b"})fields.add(((IntegerValue)object.getValue(object.referenceType().fieldByName(name))).value());
            out.put("fields",fields);
        }
        if(object instanceof ArrayReference)out.put("length",((ArrayReference)object).length());
        return out;
    }
    static void seed(ReferenceType type) {
        labels.clear();
        ObjectReference tile=(ObjectReference)type.getValue(type.fieldByName("activeTile"));
        ObjectReference player=(ObjectReference)type.getValue(type.fieldByName("activePlayer"));
        if(tile!=null)labels.put(tile,"SK");if(player!=null)labels.put(player,"IX");
        ArrayReference supplied=(ArrayReference)type.getValue(type.fieldByName("suppliedStacks"));
        ArrayReference names=(ArrayReference)type.getValue(type.fieldByName("suppliedLabels"));
        need(supplied.length()==names.length(),"declared Stack labels");
        for(int i=0;i<supplied.length();i++)labels.put((ObjectReference)supplied.getValue(i),((StringReference)names.getValue(i)).value());
    }
    public static void main(String[] args) throws Exception {
        need(args.length==5,"classpath jdk output inputs mode");
        Path output=Path.of(args[2]);Files.createDirectories(output);
        need(args[4].equals("interpreter")||args[4].equals("default"),"declared VM mode");
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[1]);
        options.get("options").setValue((args[4].equals("interpreter")?"-Xint ":"")+
            "-Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -classpath "+quoted(args[0]));
        options.get("main").setValue("FurnaceObservation "+quoted(args[3]));options.get("suspend").setValue("true");
        String[] owners={"sk","ow","iz","ey","bm","vq","gp","ix","nu","sp","gm","dw"};
        List<String> selected=List.of(owners);
        VirtualMachine vm=null;Process process=null;Drain stdout=null,stderr=null;
        int active=-1,records=0,total=0;List<Object> calls=new ArrayList<Object>();
        try(java.io.PrintWriter journal=new java.io.PrintWriter(Files.newBufferedWriter(output.resolve("calls.ndjson"),StandardOpenOption.CREATE_NEW))) {
            vm=connector.launch(options);process=vm.process();
            stdout=new Drain(process.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(process.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            EventRequestManager manager=vm.eventRequestManager();
            ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("FurnaceObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
            List<MethodEntryRequest> entries=new ArrayList<MethodEntryRequest>();
            for(String owner:owners) {
                MethodEntryRequest request=manager.createMethodEntryRequest();request.addClassFilter(owner);request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);entries.add(request);
            }
            vm.resume();boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(240);
            while(!done) {
                need(System.nanoTime()<deadline,"bounded original JDI cohort");
                EventSet events=vm.eventQueue().remove(1000);if(events==null)continue;
                for(Event event:events) {
                    if(event instanceof ClassPrepareEvent) {
                        ReferenceType type=((ClassPrepareEvent)event).referenceType();boundary(manager,type,"begin");boundary(manager,type,"end");
                    } else if(event instanceof BreakpointEvent) {
                        BreakpointEvent item=(BreakpointEvent)event;int index=((IntegerValue)item.thread().frame(0).getArgumentValues().get(0)).value();
                        if(item.location().method().name().equals("begin")) {
                            need(active==-1&&index==records,"ordered invocation boundary");active=index;calls.clear();seed(item.location().declaringType());
                            for(MethodEntryRequest request:entries)request.enable();
                        } else {
                            need(active==index,"matching invocation end");for(MethodEntryRequest request:entries)request.disable();
                            journal.println(FurnaceObservation.json(FurnaceObservation.map("index",active,"entries",new ArrayList<Object>(calls))));
                            need(!journal.checkError(),"complete observed source journal");records++;total+=calls.size();active=-1;
                        }
                    } else if(event instanceof MethodEntryEvent) {
                        MethodEntryEvent item=(MethodEntryEvent)event;if(active<0)continue;
                        Method method=item.method();code(method);StackFrame frame=item.thread().frame(0);
                        List<Object> arguments=new ArrayList<Object>();for(Value argument:frame.getArgumentValues())arguments.add(value(argument));
                        List<Object> parents=new ArrayList<Object>();
                        for(int i=1;i<item.thread().frameCount();i++) {
                            StackFrame parent=item.thread().frame(i);
                            if(selected.contains(parent.location().declaringType().name())) {
                                parents.add(FurnaceObservation.map("method",key(parent.location().method()),"pc",parent.location().codeIndex()));break;
                            }
                        }
                        calls.add(FurnaceObservation.map("method",key(method),"receiver",value(frame.thisObject()),"arguments",arguments,"source_parent",parents));
                        need(calls.size()<=512,"bounded actual source entries per case");
                    } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            need(process.waitFor(20,TimeUnit.SECONDS)&&process.exitValue()==0,"original target completed");
            stdout.join(10000);stderr.join(10000);need(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"complete original pipes");
            need(active==-1&&records>0,"all boundaries ended");
            Files.writeString(output.resolve("live-Code.json"),FurnaceObservation.json(codes)+"\n",StandardOpenOption.CREATE_NEW);
            System.out.println("FURNACE_TRACE_COMPLETE rows="+records+" entries="+total+" live-code="+codes.size());
        } finally {
            if(process!=null&&process.isAlive()){process.destroyForcibly();process.waitFor(10,TimeUnit.SECONDS);}
            if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}
        }
    }
}
