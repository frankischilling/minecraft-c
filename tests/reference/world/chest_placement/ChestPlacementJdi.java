import com.sun.jdi.Bootstrap;
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
public final class ChestPlacementJdi {
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
            codes.put(key(method),ChestPlacementObservation.map("code_bytes",bytes.length,"code_sha256",
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
    public static void main(String[] args) throws Exception {
        need(args.length==7,"classpath jar jdk output world inputs mode");
        Path output=Path.of(args[3]);Files.createDirectories(output);
        need(args[6].equals("interpreter")||args[6].equals("default"),"declared VM mode");
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[2]);
        options.get("options").setValue((args[6].equals("interpreter")?"-Xint ":"")+
            "-Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow "+
            "--add-opens=java.base/java.util=ALL-UNNAMED -classpath "+quoted(args[0]));
        options.get("main").setValue("ChestPlacementObservation "+quoted(args[1])+" "+quoted(args[4])+" "+quoted(args[5]));
        options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process process=null;Drain stdout=null,stderr=null;
        int active=-1,records=0,totalQueries=0;ObjectReference world=null;
        List<Object> queries=new ArrayList<Object>(),methods=new ArrayList<Object>();
        List<Map<String,Object>> pending=new ArrayList<Map<String,Object>>();
        try (java.io.PrintWriter journal=new java.io.PrintWriter(Files.newBufferedWriter(output.resolve("queries.ndjson"),StandardOpenOption.CREATE_NEW))) {
            vm=connector.launch(options);process=vm.process();need(vm.canGetMethodReturnValues(),"JDI return-value support");
            stdout=new Drain(process.getInputStream(),output.resolve("target.stdout"));
            stderr=new Drain(process.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            EventRequestManager manager=vm.eventRequestManager();
            ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("ChestPlacementObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
            MethodEntryRequest fd=manager.createMethodEntryRequest();fd.addClassFilter("fd");fd.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
            MethodEntryRequest e=manager.createMethodEntryRequest();e.addClassFilter("e");e.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
            MethodExitRequest exits=manager.createMethodExitRequest();exits.addClassFilter("fd");exits.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
            vm.resume();boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(150);
            while (!done) {
                need(System.nanoTime()<deadline,"bounded JDI observation");
                EventSet events=vm.eventQueue().remove(1000);if(events==null)continue;
                for (Event event:events) {
                    if(event instanceof ClassPrepareEvent) {
                        ReferenceType type=((ClassPrepareEvent)event).referenceType();boundary(manager,type,"begin");boundary(manager,type,"end");
                    } else if(event instanceof BreakpointEvent) {
                        BreakpointEvent item=(BreakpointEvent)event;int index=((IntegerValue)item.thread().frame(0).getArgumentValues().get(0)).value();
                        if(item.location().method().name().equals("begin")) {
                            need(active==-1&&index==records,"ordered declared invocation");active=index;
                            world=(ObjectReference)item.location().declaringType().getValue(item.location().declaringType().fieldByName("world"));
                            need(world!=null&&world.referenceType().name().equals("fd"),"genuine declared World");
                            queries.clear();methods.clear();pending.clear();fd.enable();e.enable();exits.enable();
                        } else {
                            need(active==index&&pending.isEmpty(),"complete actual World query returns");
                            fd.disable();e.disable();exits.disable();
                            journal.println(ChestPlacementObservation.json(ChestPlacementObservation.map("index",active,
                                "queries",new ArrayList<Object>(queries),"entered_E",new ArrayList<Object>(methods))));
                            need(!journal.checkError(),"complete query journal");totalQueries+=queries.size();records++;active=-1;
                        }
                    } else if(event instanceof MethodEntryEvent) {
                        MethodEntryEvent item=(MethodEntryEvent)event;Method method=item.method();
                        if(active<0)continue;
                        if(method.declaringType().name().equals("e")&&
                            (method.name().equals("a")||method.name().equals("h"))&&method.signature().equals("(Lfd;III)Z")) {
                            code(method);List<Value> values=item.thread().frame(0).getArgumentValues();
                            need(values.get(0)==null||values.get(0).equals(world),"same nullable formal World");
                            methods.add(ChestPlacementObservation.map("method",key(method),"world_null",values.get(0)==null,
                                "xyz",List.of(((IntegerValue)values.get(1)).value(),((IntegerValue)values.get(2)).value(),((IntegerValue)values.get(3)).value())));
                        } else if(method.declaringType().name().equals("fd")&&method.name().equals("a")&&method.signature().equals("(III)I")) {
                            code(method);StackFrame frame=item.thread().frame(0);need(frame.thisObject().equals(world),"actual declared World receiver");
                            List<Value> values=frame.getArgumentValues();List<Object> stack=new ArrayList<Object>();
                            for(StackFrame parent:item.thread().frames())if(parent.location().declaringType().name().equals("e"))
                                stack.add(ChestPlacementObservation.map("method",key(parent.location().method()),"pc",parent.location().codeIndex()));
                            need(!stack.isEmpty(),"query entered under actual E body");
                            Map<String,Object> row=ChestPlacementObservation.map("xyz",List.of(((IntegerValue)values.get(0)).value(),
                                ((IntegerValue)values.get(1)).value(),((IntegerValue)values.get(2)).value()),"source_frames",stack);
                            queries.add(row);pending.add(row);need(queries.size()<=32,"bounded actual query sequence");
                        }
                    } else if(event instanceof MethodExitEvent) {
                        MethodExitEvent item=(MethodExitEvent)event;Method method=item.method();
                        if(active>=0&&method.name().equals("a")&&method.signature().equals("(III)I")) {
                            need(!pending.isEmpty(),"entered query before return");
                            pending.remove(pending.size()-1).put("result",((IntegerValue)item.returnValue()).value());
                        }
                    } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try { events.resume(); } catch(VMDisconnectedException expected) { done=true; }
            }
            need(process.waitFor(20,TimeUnit.SECONDS)&&process.exitValue()==0,"unchanged target completed");
            stdout.join(10000);stderr.join(10000);need(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"complete target pipes");
            need(active==-1&&records>0,"all debugger boundaries ended");
            Files.writeString(output.resolve("live-Code.json"),ChestPlacementObservation.json(codes)+"\n",StandardOpenOption.CREATE_NEW);
            System.out.println("CHEST_TRACE_COMPLETE rows="+records+" queries="+totalQueries+" live-code="+codes.size());
        } finally {
            if(process!=null&&process.isAlive()){process.destroyForcibly();process.waitFor(10,TimeUnit.SECONDS);}
            if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}
        }
    }
}
