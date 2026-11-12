import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;

/** Read-only JDI method events. No class transformation or forced returns. */
public final class UpdateController {
    static final Map<Long,Integer> ids=new HashMap<>();
    static final List<String> entries=new ArrayList<>(),exits=new ArrayList<>(),comparisons=new ArrayList<>();
    static final Deque<int[]> pending=new ArrayDeque<>();
    static final Deque<Integer> compiling=new ArrayDeque<>();
    static final Set<String> recorded=new HashSet<>();
    static ReferenceType helper;
    static String kind="",trace="-";
    static int index,sourcePC=-1;
    static boolean active;
    static void require(boolean value,String message){if(!value)throw new IllegalStateException(message);}
    static String text(String name){return ((StringReference)helper.getValue(helper.fieldByName(name))).value();}
    static int number(String name){return ((IntegerValue)helper.getValue(helper.fieldByName(name))).value();}
    static int id(Value object){return object==null?0:ids.getOrDefault(((ObjectReference)object).uniqueID(),-1);}
    static String joined(List<String> values){return values.isEmpty()?"-":String.join(",",values);}
    static String quote(String value){return "\""+value.replace("\\","\\\\").replace("\"","\\\"")+"\"";}
    static int parentPC(ThreadReference thread) throws Exception {
        for(StackFrame frame:thread.frames()) {
            Method method=frame.location().method();
            if(method.declaringType().name().equals("n")&&method.name().equals("a")&&method.signature().equals("(Lls;Z)Z"))
                return (int)frame.location().codeIndex();
        }
        return -1;
    }
    static void code(ReferenceType type) throws Exception {
        for(Method method:type.methods()) {
            boolean game=(type.name().equals("n")&&method.name().equals("a")&&method.signature().equals("(Lls;Z)Z"))||
                (type.name().equals("md")&&(method.name().equals("a")||method.name().equals("compare")))||
                (type.name().equals("dk")&&method.name().equals("a")&&(method.signature().equals("()V")||method.signature().equals("(Lsn;)F")))||
                (type.name().equals("ew")&&method.name().equals("<init>"));
            boolean jdk=type.name().equals("java.util.TimSort")||
                (type.name().equals("java.util.ArrayList")&&method.name().equals("sort"));
            if(!game&&!jdk||method.isNative()||method.isAbstract())continue;
            String key=type.name()+" "+method.name()+" "+method.signature();
            if(!recorded.add(key))continue;
            byte[] bytes=method.bytecodes();
            System.out.println("CODE "+key+" "+bytes.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
        }
    }
    public static void main(String[] args) throws Exception {
        require(args.length==3,"classpath, JAR, input required");
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("main").setValue("UpdateObservation "+quote(args[1])+" "+quote(args[2]));
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.security.manager=allow -classpath "+quote(args[0]));
        VirtualMachine vm=connector.launch(options);Process child=vm.process();
        ExecutorService streams=Executors.newFixedThreadPool(2);
        Future<byte[]> stdout=streams.submit(()->child.getInputStream().readNBytes(4*1024*1024));
        Future<byte[]> stderr=streams.submit(()->child.getErrorStream().readNBytes(1024*1024));
        EventRequestManager requests=vm.eventRequestManager();
        for(String type:new String[]{"UpdateObservation","n","md","dk","ew","java.util.TimSort","java.util.ArrayList"}) {
            ClassPrepareRequest request=requests.createClassPrepareRequest();request.addClassFilter(type);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
        }
        for(String type:new String[]{"dk","md","n"}) {
            MethodEntryRequest entry=requests.createMethodEntryRequest();entry.addClassFilter(type);entry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);entry.enable();
            MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter(type);exit.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);exit.enable();
        }
        for(String type:new String[]{"n","dk","md","ew","java.util.TimSort"}) {
            ExceptionRequest exception=requests.createExceptionRequest(null,true,true);exception.addClassFilter(type);exception.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);exception.enable();
        }
        System.out.println("RUNTIME "+vm.version().replace('\n',' ') +" "+vm.name());
        for(String type:new String[]{"java.util.ArrayList","java.util.TimSort"})for(ReferenceType value:vm.classesByName(type))if(value.isPrepared())code(value);
        long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(160);boolean connected=true;int completed=0;
        try {
            while(connected) {
                require(System.nanoTime()<deadline,"controller160-second bound");
                EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set) {
                    if(event instanceof ClassPrepareEvent prepared) {
                        ReferenceType type=prepared.referenceType();code(type);
                        if(type.name().equals("UpdateObservation")) {
                            helper=type;
                            for(String marker:new String[]{"beginCase","endCase"}) {
                                BreakpointRequest point=requests.createBreakpointRequest(type.methodsByName(marker).get(0).location());point.putProperty("marker",marker);point.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);point.enable();
                            }
                        }
                    } else if(event instanceof BreakpointEvent point) {
                        if(point.request().getProperty("marker").equals("beginCase")) {
                            kind=text("kind");index=number("index");ids.clear();entries.clear();exits.clear();comparisons.clear();pending.clear();compiling.clear();trace="-";sourcePC=-1;
                            ArrayReference actors=(ArrayReference)helper.getValue(helper.fieldByName("actors"));int ordinal=0;
                            for(Value actor:actors.getValues())ids.put(((ObjectReference)actor).uniqueID(),++ordinal);active=true;
                        } else {
                            active=false;
                            if(kind.equals("UPDATE"))System.out.println(text("row")+" "+sourcePC+" "+joined(entries)+" "+joined(exits));
                            else System.out.println(text("row"));
                            System.out.println("CMPS "+kind+" "+index+" "+joined(comparisons));
                            System.out.println("TRACE "+kind+" "+index+" "+trace);++completed;
                        }
                    } else if(active&&event instanceof MethodEntryEvent entry) {
                        Method method=entry.method();String type=method.declaringType().name();
                        if(type.equals("dk")&&method.name().equals("a")&&method.signature().equals("()V")) {
                            int pc=parentPC(entry.thread());int actor=id(entry.thread().frame(0).thisObject());require(actor>0,"actual DK identity");entries.add(actor+"@"+pc);compiling.push(actor);
                        } else if(type.equals("md")&&method.name().equals("a")&&method.signature().equals("(Ldk;Ldk;)I")) {
                            List<Value> values=entry.thread().frame(0).getArgumentValues();pending.push(new int[]{id(values.get(0)),id(values.get(1))});
                        }
                    } else if(active&&event instanceof MethodExitEvent exit) {
                        Method method=exit.method();String type=method.declaringType().name();
                        if(type.equals("dk")&&method.name().equals("a")&&method.signature().equals("()V")){require(!compiling.isEmpty(),"paired DK entry/exit");exits.add(Integer.toString(compiling.pop()));}
                        else if(type.equals("md")&&method.name().equals("a")&&method.signature().equals("(Ldk;Ldk;)I")) {
                            require(!pending.isEmpty(),"paired MD entry/exit");int[] pair=pending.pop();comparisons.add(pair[0]+":"+pair[1]+":"+((IntegerValue)exit.returnValue()).value());
                        } else if(type.equals("n")&&method.name().equals("a")&&method.signature().equals("(Lls;Z)Z"))sourcePC=(int)exit.location().codeIndex();
                    } else if(active&&event instanceof ExceptionEvent exception) {
                        sourcePC=parentPC(exception.thread());List<String> frames=new ArrayList<>();
                        while(!pending.isEmpty()){int[] pair=pending.pop();comparisons.add(pair[0]+":"+pair[1]+":!");}
                        for(StackFrame frame:exception.thread().frames()) {
                            String type=frame.location().declaringType().name();
                            if(Set.of("n","md","dk","ew","java.util.TimSort").contains(type))frames.add(type+":"+frame.location().codeIndex());
                        }
                        trace=String.join(">",frames);
                    } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)connected=false;
                }
                try{set.resume();}catch(VMDisconnectedException expected){connected=false;}
            }
            require(child.waitFor(5,TimeUnit.SECONDS)&&child.exitValue()==0,"original child completion");
            String output=new String(stdout.get(5,TimeUnit.SECONDS),StandardCharsets.UTF_8);
            System.out.print(output);System.err.write(stderr.get(5,TimeUnit.SECONDS));
            require(completed==64,"all64 observed cases");
            require(output.trim().equals("Original update observer: 42 updates; 14 sorts; 8 compares; network=0; native=0"),"original guard receipt");
            System.out.println("Original N update capture: 64 cases; untouched methods; read-only JDI");
        } finally {if(child.isAlive())child.destroyForcibly();streams.shutdownNow();}
    }
}
