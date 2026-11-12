import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Reads original method events and NW stores; never writes or invokes target state. */
public final class CactusJdi {
    static VirtualMachine vm;
    static int sequence, nextCall, queries, stores, failures;
    static final Set<String> SOURCE=Set.of("cv","ew","fd","lm","nw","uu","or","ln","px","mx");
    static final Set<String> pinned=new HashSet<>();
    static final Map<Long,Deque<Integer>> calls=new HashMap<>();
    static ReferenceType helper(){return vm.classesByName("CactusObservation").get(0);}
    static int active(){return ((IntegerValue)helper().getValue(helper().fieldByName("active"))).value();}
    static ObjectReference owner(String name){return (ObjectReference)helper().getValue(helper().fieldByName(name));}
    static String hex(double x){return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(x));}
    static Object value(Value v){
        if(v==null)return null;
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof LongValue)return ((LongValue)v).value();
        if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));
        if(v instanceof DoubleValue)return hex(((DoubleValue)v).value());
        if(v instanceof VoidValue)return "void";
        ObjectReference object=(ObjectReference)v;
        for(String name:new String[]{"observedCache","observedBlock","observedTess"})if(object.equals(owner(name)))return "ref:"+name;
        if(object.referenceType().name().equals("ln"))return "ref:ln";
        throw new AssertionError("unclassified source reference "+object.referenceType().name());
    }
    static List<Object> arguments(StackFrame frame){List<Object> out=new ArrayList<>();for(Value v:frame.getArgumentValues())out.add(value(v));return out;}
    static Map<String,Object> map(Object... pairs){Map<String,Object> out=new LinkedHashMap<>();for(int i=0;i<pairs.length;i+=2)out.put((String)pairs[i],pairs[i+1]);return out;}
    static String json(Object v){
        if(v==null)return "null";
        if(v instanceof String){String s=(String)v;return "\""+s.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";}
        if(v instanceof Map){StringJoiner s=new StringJoiner(",","{","}");for(Object e:((Map<?,?>)v).entrySet()){Map.Entry<?,?> p=(Map.Entry<?,?>)e;s.add(json(p.getKey())+":"+json(p.getValue()));}return s.toString();}
        if(v instanceof Iterable){StringJoiner s=new StringJoiner(",","[","]");for(Object x:(Iterable<?>)v)s.add(json(x));return s.toString();}
        return v.toString();
    }
    static String location(Location p){return p==null?"NONE":p.declaringType().name()+"."+p.method().name()+p.method().signature()+"@"+p.codeIndex();}
    static Map<String,Object> nw(){ObjectReference n=owner("observedTess");Map<String,Object> result=new TreeMap<>();for(Field f:n.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))result.put(f.name(),value(n.getValue(f)));return result;}
    static void emit(String event,int profile,Object... pairs){Map<String,Object> out=map("seq",sequence++,"profile",profile,"event",event);out.putAll(map(pairs));System.out.println("CW_EVENT\t"+json(out));}
    static void pin(Method m)throws Exception{String key=m.declaringType().name()+"."+m.name()+m.signature();if(pinned.add(key)){byte[] code=m.bytecodes();System.out.println("CW_LIVE_CODE\t"+m.declaringType().name()+"\t"+m.name()+"\t"+m.signature()+"\t"+code.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code)));}}
    static boolean watched(Method m){String t=m.declaringType().name();return t.equals("ew")&&!m.name().startsWith("<")||t.equals("cv")&&(m.name().equals("m")&&m.signature().equals("(Luu;III)Z")||m.name().equals("c")&&m.signature().equals("(Luu;IIIFFF)Z"))||t.equals("nw")&&m.name().equals("c")&&m.signature().equals("(FFF)V");}
    static void prepare(EventRequestManager manager,ReferenceType type){
        for(Method m:type.methods())if(watched(m)){BreakpointRequest r=manager.createBreakpointRequest(m.location());r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
        if(type.name().equals("nw"))for(String name:new String[]{"s","t","u"}){ModificationWatchpointRequest r=manager.createModificationWatchpointRequest(type.fieldByName(name));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
    }
    static void drain(Process child,InputStream in,Path path,int bound,List<Throwable> errors){try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];int total=0,n;while((n=in.read(b))!=-1){total+=n;if(total>bound){child.destroyForcibly();throw new IOException("pipe bound");}out.write(b,0,n);}}catch(Throwable t){synchronized(errors){errors.add(t);}}}
    public static void main(String[] args)throws Exception{
        if(args.length!=6)throw new AssertionError("output,jdk-home,classpath,jar,world,profiles");
        Path output=Path.of(args[0]);LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[1]);options.get("main").setValue("CactusObservation \""+args[3]+"\" \""+args[4]+"\" \""+args[5]+"\"");
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -cp \""+args[2]+"\"");options.get("suspend").setValue("true");
        vm=connector.launch(options);if(!vm.canWatchFieldModification()||!vm.canGetBytecodes()||!vm.canGetMethodReturnValues())throw new AssertionError("required read-only JDI capabilities");
        Process child=vm.process();List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),output.resolve("target.stdout"),4194304,errors));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),output.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();
        boolean died=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(150);
        try{
            EventRequestManager manager=vm.eventRequestManager();
            for(String name:new String[]{"ew","cv","nw"}){ClassPrepareRequest p=manager.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();MethodExitRequest r=manager.createMethodExitRequest();r.addClassFilter(name);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
            for(String name:SOURCE){ExceptionRequest r=manager.createExceptionRequest(null,true,true);r.addClassFilter(name);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
            while(!died){if(System.nanoTime()>deadline)throw new IOException("observer deadline");EventSet set;try{set=vm.eventQueue().remove(1000);}catch(VMDisconnectedException end){break;}if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}
                    if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){died=true;continue;}
                    if(vm.classesByName("CactusObservation").isEmpty())continue;int profile=active();if(profile<0)continue;
                    if(event instanceof BreakpointEvent){BreakpointEvent e=(BreakpointEvent)event;Method m=e.location().method();if(!watched(m))continue;pin(m);StackFrame frame=e.thread().frame(0);ObjectReference receiver=frame.thisObject();String type=m.declaringType().name();
                        if(type.equals("ew")&&!receiver.equals(owner("observedCache")))throw new AssertionError("different genuine cache");if(type.equals("nw")&&!receiver.equals(owner("observedTess")))throw new AssertionError("different genuine tessellator");
                        int call=nextCall++;calls.computeIfAbsent(e.thread().uniqueID(),k->new ArrayDeque<>()).push(call);if(type.equals("ew"))queries++;
                        emit("entry",profile,"call",call,"at",location(e.location()),"caller",location(e.thread().frame(1).location()),"receiver",type.equals("cv")?"ref:cv":value(receiver),"args",arguments(frame),"nw",nw());
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;Method m=e.method();if(!watched(m))continue;pin(m);Deque<Integer> stack=calls.get(e.thread().uniqueID());if(stack==null||stack.isEmpty())throw new AssertionError("unmatched source return");int call=stack.pop();
                        emit("return",profile,"call",call,"at",location(e.location()),"result",value(e.returnValue()),"nw",nw());
                    }else if(event instanceof ModificationWatchpointEvent){ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;if(!watched(e.location().method()))continue;pin(e.location().method());if(!e.object().equals(owner("observedTess")))throw new AssertionError("different NW store owner");
                        emit("store",profile,"call",calls.get(e.thread().uniqueID()).peek(),"at",location(e.location()),"field",e.field().name(),"old",value(e.valueCurrent()),"new",value(e.valueToBe()),"nw",nw());stores++;
                    }else if(event instanceof ExceptionEvent){ExceptionEvent e=(ExceptionEvent)event;pin(e.location().method());List<String> frames=new ArrayList<>();for(StackFrame frame:e.thread().frames())if(SOURCE.contains(frame.location().declaringType().name())){pin(frame.location().method());frames.add(location(frame.location()));}
                        emit("exception",profile,"at",location(e.location()),"type",e.exception().referenceType().name(),"catch",location(e.catchLocation()),"frames",frames,"nw",nw());failures++;calls.remove(e.thread().uniqueID());
                    }
                }
                try{set.resume();}catch(VMDisconnectedException end){died=true;}
            }
        }finally{try{vm.dispose();}catch(VMDisconnectedException ignored){}if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(5,TimeUnit.SECONDS);}stdout.join(5000);stderr.join(5000);}
        if(child.isAlive()||child.exitValue()!=0||stdout.isAlive()||stderr.isAlive()||!errors.isEmpty()||failures!=5)throw new AssertionError("incomplete source/pipe trace: failures="+failures+" errors="+errors);
        System.out.println("CW_JDI_COMPLETE events="+sequence+" entries="+nextCall+" queries="+queries+" stores="+stores+" exceptions="+failures+" target-exit=0 source-writes=0");
    }
}
