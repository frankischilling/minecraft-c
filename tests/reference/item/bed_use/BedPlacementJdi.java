import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only breakpoints/watchpoints; never invoke, mutate or replace target code. */
public final class BedPlacementJdi {
    static VirtualMachine vm;
    static ReferenceType helper;
    static PrintWriter journal;
    static boolean active;
    static int profile=-1, sequence, entries, returns, reads, writes, exceptions;
    static long bytes;
    static final Set<String> SOURCE=Set.of("lp","gm","iz","fd","lm","wi","ve","uu","sn","in");
    static final Set<String> pins=new HashSet<>();
    static final List<EventRequest> gated=new ArrayList<>();
    static final String[] OWNERS={"observedWorld","observedPlayer","observedStack","observedItem","observedBed"};
    static String json(Object v) {
        if(v==null)return "null";
        if(v instanceof String)return "\""+((String)v).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";
        if(v instanceof Map){StringJoiner j=new StringJoiner(",","{","}");for(Object o:((Map<?,?>)v).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)o;j.add(json(e.getKey())+":"+json(e.getValue()));}return j.toString();}
        if(v instanceof Iterable){StringJoiner j=new StringJoiner(",","[","]");for(Object o:(Iterable<?>)v)j.add(json(o));return j.toString();}
        return v.toString();
    }
    static Object value(Value v) {
        if(v==null)return null;
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof LongValue)return ((LongValue)v).value();
        if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));
        if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));
        if(v instanceof VoidValue)return "void";
        if(v instanceof StringReference)return ((StringReference)v).value();
        ObjectReference r=(ObjectReference)v;
        for(String owner:OWNERS)if(r.equals(helper.getValue(helper.fieldByName(owner))))return "ref:"+owner;
        return "ref:"+r.referenceType().name();
    }
    static String location(Location l) {return l==null?"NONE":l.declaringType().name()+"."+l.method().name()+l.method().signature()+"@"+l.codeIndex();}
    static void emit(String kind,Object... p) {
        Map<String,Object> row=new LinkedHashMap<>();row.put("seq",sequence++);row.put("profile",profile);row.put("event",kind);
        for(int i=0;i<p.length;i+=2)row.put((String)p[i],p[i+1]);
        String line=json(row);bytes+=line.length()+1;
        if(sequence>20000||bytes>8388608)throw new AssertionError("finite passive journal bound");
        journal.println(line);if(journal.checkError())throw new AssertionError("journal write");
    }
    static void pin(Method m)throws Exception {
        if(!SOURCE.contains(m.declaringType().name()))return;
        String key=m.declaringType().name()+"."+m.name()+m.signature();
        if(pins.add(key)){byte[] b=m.bytecodes();System.out.println("BED_PLACEMENT_LIVE_CODE\t"+m.declaringType().name()+"\t"+m.name()+"\t"+m.signature()+"\t"+b.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b)));}
    }
    static boolean watched(Method m) {
        String c=m.declaringType().name(),n=m.name(),s=m.signature();
        if(c.equals("lp"))return n.equals("a")&&s.equals("(Liz;Lgs;Lfd;IIII)Z");
        if(c.equals("fd"))return (n.equals("d")||n.equals("h"))&&s.equals("(III)Z")||n.equals("b")&&s.equals("(IIIII)Z");
        if(c.equals("ve"))return n.equals("b")&&s.equals("(Lfd;IIII)V");
        return c.equals("in")&&n.equals("b")&&s.equals("(D)I");
    }
    static void add(EventRequest r){r.setSuspendPolicy(EventRequest.SUSPEND_ALL);gated.add(r);if(active)r.enable();}
    static void prepare(EventRequestManager manager,ReferenceType t) {
        if(t.name().equals("BedPlacementObservation")) {
            helper=t;
            for(String name:new String[]{"entered","finished"}){
                BreakpointRequest b=manager.createBreakpointRequest(t.methodsByName(name).get(0).location());b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();
            }
            return;
        }
        for(Method m:t.methods())if(watched(m))add(manager.createBreakpointRequest(m.location()));
        if(t.methods().stream().anyMatch(BedPlacementJdi::watched)){
            MethodExitRequest e=manager.createMethodExitRequest();e.addClassFilter(t);add(e);
        }
        for(Field f:t.fields()) {
            if(t.name().equals("uu")&&f.name().equals("bn")||t.name().equals("sn")&&f.name().equals("aS")||t.name().equals("iz")&&f.name().equals("a"))add(manager.createAccessWatchpointRequest(f));
            if(t.name().equals("iz")&&f.name().equals("a"))add(manager.createModificationWatchpointRequest(f));
        }
    }
    static List<Object> args(StackFrame f){List<Object> r=new ArrayList<>();for(Value v:f.getArgumentValues())r.add(value(v));return r;}
    static void drain(Process p,InputStream stream,Path path,int bound,List<Throwable> failures) {
        try(OutputStream out=Files.newOutputStream(path)){
            byte[] buf=new byte[8192];int total=0,n;
            while((n=stream.read(buf))!=-1){total+=n;if(total>bound){p.destroyForcibly();throw new IOException("target pipe bound");}out.write(buf,0,n);}
        }catch(Throwable e){failures.add(e);p.destroyForcibly();}
    }
    public static void main(String[] a)throws Exception {
        if(a.length!=6)throw new AssertionError("output jdk classpath jar world profiles");
        Path output=Path.of(a[0]);LaunchingConnector launch=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=launch.defaultArguments();
        options.get("home").setValue(a[1]);
        options.get("main").setValue("BedPlacementObservation \""+a[3]+"\" \""+a[4]+"\" \""+a[5]+"\"");
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m --add-opens java.base/java.util=ALL-UNNAMED -Djava.awt.headless=true -Djava.security.manager=allow -cp \""+a[2]+"\"");
        options.get("suspend").setValue("true");vm=launch.launch(options);
        if(!vm.canWatchFieldAccess()||!vm.canWatchFieldModification()||!vm.canGetBytecodes()||!vm.canGetMethodReturnValues())throw new AssertionError("read-only JDI capabilities");
        Process child=vm.process();List<Throwable> failures=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),output.resolve("target.stdout"),4194304,failures));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),output.resolve("target.stderr"),1048576,failures));
        stdout.start();stderr.start();journal=new PrintWriter(Files.newBufferedWriter(output.resolve("events.jsonl")));
        long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(150);boolean died=false;
        try {
            EventRequestManager manager=vm.eventRequestManager();
            Set<String> names=new HashSet<>(SOURCE);names.add("BedPlacementObservation");
            for(String name:names){ClassPrepareRequest r=manager.createClassPrepareRequest();r.addClassFilter(name);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
            add(manager.createExceptionRequest(null,true,true));
            while(!died){
                if(System.nanoTime()>deadline)throw new IOException("target deadline");
                EventSet set;try{set=vm.eventQueue().remove(1000);}catch(VMDisconnectedException e){break;}if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}
                    if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){died=true;continue;}
                    if(event instanceof BreakpointEvent&&((BreakpointEvent)event).location().declaringType().name().equals("BedPlacementObservation")){
                        String name=((BreakpointEvent)event).location().method().name();
                        if(name.equals("entered")){profile=((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();active=true;for(EventRequest r:gated)r.enable();emit("begin");}
                        else{emit("end");active=false;for(EventRequest r:gated)r.disable();}
                        continue;
                    }
                    if(!active)continue;
                    if(event instanceof BreakpointEvent){BreakpointEvent e=(BreakpointEvent)event;Method m=e.location().method();if(!watched(m))continue;pin(m);StackFrame f=e.thread().frame(0);emit("entry","at",location(e.location()),"caller",location(e.thread().frame(1).location()),"receiver",value(f.thisObject()),"args",args(f));++entries;
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;if(!watched(e.method()))continue;pin(e.method());emit("return","at",location(e.location()),"caller",location(e.thread().frame(1).location()),"value",value(e.returnValue()));++returns;
                    }else if(event instanceof AccessWatchpointEvent){AccessWatchpointEvent e=(AccessWatchpointEvent)event;if(!e.location().declaringType().name().equals("lp"))continue;pin(e.location().method());emit("field-read","at",location(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"receiver",value(e.object()),"value",value(e.valueCurrent()));++reads;
                    }else if(event instanceof ModificationWatchpointEvent){ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;if(!e.location().declaringType().name().equals("lp"))continue;pin(e.location().method());emit("field-write","at",location(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"receiver",value(e.object()),"before",value(e.valueCurrent()),"after",value(e.valueToBe()));++writes;
                    }else if(event instanceof ExceptionEvent){ExceptionEvent e=(ExceptionEvent)event;List<String> frames=new ArrayList<>();boolean lp=false;for(StackFrame f:e.thread().frames())if(SOURCE.contains(f.location().declaringType().name())){pin(f.location().method());frames.add(location(f.location()));lp|=f.location().declaringType().name().equals("lp");}if(lp){emit("exception","at",location(e.location()),"type",e.exception().referenceType().name(),"catch",location(e.catchLocation()),"frames",frames);++exceptions;}}
                }
                try{set.resume();}catch(VMDisconnectedException e){died=true;}
            }
        } finally {
            journal.close();try{vm.dispose();}catch(VMDisconnectedException ignored){}
            if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(5,TimeUnit.SECONDS);}
            stdout.join(5000);stderr.join(5000);
        }
        if(child.isAlive()||child.exitValue()!=0||stdout.isAlive()||stderr.isAlive()||!failures.isEmpty())throw new AssertionError("target incomplete exit/pipe "+failures);
        System.out.println("BED_PLACEMENT_JDI_COMPLETE events="+sequence+" entries="+entries+" returns="+returns+" reads="+reads+" stores="+writes+" exceptions="+exceptions+" target-exit=0 source-writes=0 target-invocations=0");
    }
}
