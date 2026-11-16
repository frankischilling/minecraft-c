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
public final class FurnaceLifecycleJdi {
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
            codes.put(key(method),FurnaceLifecycleObservation.map("code_bytes",bytes.length,"code_sha256",
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
    static final Map<ObjectReference,String> tiles=new LinkedHashMap<ObjectReference,String>();
    static ObjectReference world,chunk;
    static String tile(ObjectReference value) {
        if(value==null)return null;need(value.referenceType().name().equals("sk"),"actual SK identity");
        String name=tiles.get(value);if(name==null){name="T"+tiles.size();tiles.put(value,name);}return name;
    }
    static Value get(ObjectReference owner,String name){return owner.getValue(owner.referenceType().fieldByName(name));}
    static List<Object> list(ObjectReference owner) {
        int size=((IntegerValue)get(owner,"size")).value();need(size<=16,"bounded actual Tile List");
        ArrayReference array=(ArrayReference)get(owner,"elementData");List<Object> out=new ArrayList<Object>();
        for(int i=0;i<size;i++)out.add(tile((ObjectReference)array.getValue(i)));return out;
    }
    static List<Object> mapValues() {
        ObjectReference map=(ObjectReference)get(chunk,"l");ArrayReference table=(ArrayReference)get(map,"table");
        List<Object> out=new ArrayList<Object>();if(table==null)return out;
        for(Value value:table.getValues()) {
            ObjectReference node=(ObjectReference)value;
            while(node!=null){out.add(tile((ObjectReference)get(node,"value")));need(out.size()<=4,"bounded actual Tile Map");node=(ObjectReference)get(node,"next");}
        }
        return out;
    }
    static Object state() {
        List<Object> active=list((ObjectReference)get(world,"c"));
        List<Object> pending=list((ObjectReference)get(world,"G"));List<Object> map=mapValues();
        Map<String,Object> observed=new LinkedHashMap<String,Object>();
        for(Map.Entry<ObjectReference,String> entry:tiles.entrySet()) {
            ObjectReference tile=entry.getKey();
            observed.put(entry.getValue(),FurnaceLifecycleObservation.map("invalid",((com.sun.jdi.BooleanValue)get(tile,"h")).value(),
                "fields",List.of(((IntegerValue)get(tile,"a")).value(),((IntegerValue)get(tile,"b")).value(),((IntegerValue)get(tile,"c")).value()),
                "world_same",world.equals(get(tile,"d"))));
        }
        ArrayReference blocks=(ArrayReference)get(chunk,"b");ObjectReference data=(ObjectReference)get(chunk,"e");
        ArrayReference metadata=(ArrayReference)get(data,"a");int at=(8<<11)|(8<<7)|64;
        ReferenceType tc=world.virtualMachine().classesByName("tc").get(0);
        return FurnaceLifecycleObservation.map("active",active,"pending",pending,"map",map,"tiles",observed,
            "iterating",((com.sun.jdi.BooleanValue)get(world,"L")).value(),"suppression",((com.sun.jdi.BooleanValue)tc.getValue(tc.fieldByName("c"))).value(),
            "block",((com.sun.jdi.ByteValue)blocks.getValue(at)).value()&255,
            "metadata",(((com.sun.jdi.ByteValue)metadata.getValue(at/2)).value()>>((at&1)*4))&15);
    }
    static boolean selected(Method method) {
        String name=key(method);
        if(name.startsWith("sk."))return true;
        if(name.startsWith("tc."))return true;
        if(name.startsWith("rw."))return true;
        return List.of("fd.g()V","fd.a(IIILow;)V","fd.p(III)V","fd.f(IIII)Z","fd.d(IIII)V",
            "lm.a(IIILow;)V","lm.e(III)V","ow.i()V","ow.j()V","ow.y_()V").contains(name);
    }
    static Object scalar(Value value) {
        if(value==null)return null;
        if(value instanceof com.sun.jdi.BooleanValue)return ((com.sun.jdi.BooleanValue)value).value();
        if(value instanceof PrimitiveValue)return ((PrimitiveValue)value).longValue();
        ObjectReference object=(ObjectReference)value;
        if(object.referenceType().name().equals("sk"))return FurnaceLifecycleObservation.map("type","sk","identity",tile(object));
        return FurnaceLifecycleObservation.map("type",object.referenceType().name());
    }
    public static void main(String[] args) throws Exception {
        need(args.length==7,"classpath jar jdk output world inputs mode");Path output=Path.of(args[3]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[2]);
        options.get("options").setValue((args[6].equals("interpreter")?"-Xint ":"")+"-Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+quoted(args[0]));
        options.get("main").setValue("FurnaceLifecycleObservation "+quoted(args[1])+" "+quoted(args[4])+" "+quoted(args[5]));options.get("suspend").setValue("true");
        VirtualMachine vm=null;Process process=null;Drain stdout=null,stderr=null;int active=-1,records=0,total=0;List<Object> phases=new ArrayList<Object>();
        try(java.io.PrintWriter journal=new java.io.PrintWriter(Files.newBufferedWriter(output.resolve("phases.ndjson"),StandardOpenOption.CREATE_NEW))) {
            vm=connector.launch(options);process=vm.process();stdout=new Drain(process.getInputStream(),output.resolve("target.stdout"));stderr=new Drain(process.getErrorStream(),output.resolve("target.stderr"));stdout.start();stderr.start();
            EventRequestManager manager=vm.eventRequestManager();ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("FurnaceLifecycleObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
            List<EventRequest> requests=new ArrayList<EventRequest>();
            for(String owner:new String[]{"fd","lm","sk","tc","rw","ow"}) {
                MethodEntryRequest entry=manager.createMethodEntryRequest();entry.addClassFilter(owner);entry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(entry);
                MethodExitRequest exit=manager.createMethodExitRequest();exit.addClassFilter(owner);exit.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);requests.add(exit);
            }
            vm.resume();boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(240);
            while(!done) {
                need(System.nanoTime()<deadline,"bounded original phase trace");EventSet events=vm.eventQueue().remove(1000);if(events==null)continue;
                for(Event event:events) {
                    if(event instanceof ClassPrepareEvent){ReferenceType type=((ClassPrepareEvent)event).referenceType();boundary(manager,type,"begin");boundary(manager,type,"end");}
                    else if(event instanceof BreakpointEvent) {
                        BreakpointEvent item=(BreakpointEvent)event;int index=((IntegerValue)item.thread().frame(0).getArgumentValues().get(0)).value();
                        if(item.location().method().name().equals("begin")) {
                            need(active==-1&&index==records,"ordered phase boundary");active=index;phases.clear();ReferenceType type=item.location().declaringType();
                            world=(ObjectReference)type.getValue(type.fieldByName("world"));ArrayReference chunks=(ArrayReference)type.getValue(type.fieldByName("chunks"));chunk=(ObjectReference)chunks.getValue(0);
                            if(tiles.isEmpty())tile((ObjectReference)type.getValue(type.fieldByName("original")));
                            for(EventRequest request:requests)request.enable();
                        } else {
                            need(active==index,"matching phase boundary");for(EventRequest request:requests)request.disable();
                            journal.println(FurnaceLifecycleObservation.json(FurnaceLifecycleObservation.map("index",active,"phases",new ArrayList<Object>(phases))));need(!journal.checkError(),"complete phase journal");total+=phases.size();records++;active=-1;
                        }
                    } else if(event instanceof MethodEntryEvent||event instanceof MethodExitEvent) {
                        Method method=event instanceof MethodEntryEvent?((MethodEntryEvent)event).method():((MethodExitEvent)event).method();
                        if(active<0||!selected(method))continue;code(method);
                        com.sun.jdi.ThreadReference thread=event instanceof MethodEntryEvent?((MethodEntryEvent)event).thread():((MethodExitEvent)event).thread();StackFrame frame=thread.frame(0);
                        ObjectReference receiver=frame.thisObject();Object object=receiver==null?null:receiver.referenceType().name().equals("sk")?tile(receiver):receiver.referenceType().name();
                        List<Object> arguments=new ArrayList<Object>();
                        if(event instanceof MethodEntryEvent)for(Value value:frame.getArgumentValues())arguments.add(scalar(value));
                        List<Object> parent=new ArrayList<Object>();
                        for(int i=1;i<thread.frameCount();i++) {
                            StackFrame caller=thread.frame(i);if(selected(caller.location().method())){parent.add(FurnaceLifecycleObservation.map("method",key(caller.location().method()),"pc",caller.location().codeIndex()));break;}
                        }
                        phases.add(FurnaceLifecycleObservation.map("event",event instanceof MethodEntryEvent?"entry":"exit","method",key(method),"receiver",object,"arguments",arguments,"source_parent",parent,"state",state()));need(phases.size()<=1024,"bounded actual phase sequence");
                    } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }
                try{events.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            need(process.waitFor(20,TimeUnit.SECONDS)&&process.exitValue()==0,"genuine World target completed");stdout.join(10000);stderr.join(10000);need(!stdout.isAlive()&&!stderr.isAlive()&&stdout.failure==null&&stderr.failure==null,"complete World pipes");need(active==-1&&records>0,"all actual phases ended");
            Files.writeString(output.resolve("live-Code.json"),FurnaceLifecycleObservation.json(codes)+"\n",StandardOpenOption.CREATE_NEW);System.out.println("FURNACE_WORLD_TRACE_COMPLETE rows="+records+" phases="+total+" live-code="+codes.size());
        } finally {
            if(process!=null&&process.isAlive()){process.destroyForcibly();process.waitFor(10,TimeUnit.SECONDS);}if(vm!=null)try{vm.dispose();}catch(VMDisconnectedException expected){}
        }
    }
}
