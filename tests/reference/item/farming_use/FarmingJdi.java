import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only breakpoints, field observations and method returns; no target invocation or writes. */
public final class FarmingJdi {
    static VirtualMachine vm;static ReferenceType helper;static PrintWriter journal;
    static int seq,nextCall,lastProfile=-1,entries,exceptions,reads,stores;static long bytes;
    static final Set<String> SOURCE=Set.of("la","rk","iz","gm","fd","ct","uu","dc","gs","ls","sn","xi","jl","vr","lm","mx","ok","wi","ln");
    static final Set<String> pinned=new HashSet<>();
    static final Deque<Map.Entry<String,Integer>> stack=new ArrayDeque<>();
    static final String[] OWNERS={"observedWorld","observedPlayer","observedStack","observedItem","observedGrass","observedDirt","observedFarmland","observedSound"};
    static int active(){if(helper==null)helper=vm.classesByName("FarmingObservation").get(0);return ((IntegerValue)helper.getValue(helper.fieldByName("active"))).value();}
    static Object value(Value v){
        if(v==null)return null;if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();
        if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));if(v instanceof VoidValue)return "void";if(v instanceof StringReference)return ((StringReference)v).value();
        ObjectReference r=(ObjectReference)v;for(String name:OWNERS)if(r.equals(helper.getValue(helper.fieldByName(name))))return "ref:"+name;return "ref:"+r.referenceType().name();
    }
    static String json(Object v){if(v==null)return "null";if(v instanceof String)return "\""+((String)v).replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";if(v instanceof Map){StringJoiner s=new StringJoiner(",","{","}");for(Object x:((Map<?,?>)v).entrySet()){Map.Entry<?,?> e=(Map.Entry<?,?>)x;s.add(json(e.getKey())+":"+json(e.getValue()));}return s.toString();}if(v instanceof Iterable){StringJoiner s=new StringJoiner(",","[","]");for(Object x:(Iterable<?>)v)s.add(json(x));return s.toString();}return v.toString();}
    static String at(Location p){return p==null?"NONE":p.declaringType().name()+"."+p.method().name()+p.method().signature()+"@"+p.codeIndex();}
    static String key(Method m){return m.declaringType().name()+"."+m.name()+m.signature();}
    static void emit(String event,int profile,Object... p){Map<String,Object> r=new LinkedHashMap<>();r.put("seq",seq++);r.put("profile",profile);r.put("event",event);for(int i=0;i<p.length;i+=2)r.put((String)p[i],p[i+1]);String line=json(r);bytes+=line.length()+1;if(seq>50000||bytes>33554432)throw new AssertionError("bounded observation journal");journal.println(line);if(journal.checkError())throw new AssertionError("journal write");}
    static void pin(Method m)throws Exception{if(!SOURCE.contains(m.declaringType().name()))return;String k=key(m);if(pinned.add(k)){byte[] b=m.bytecodes();System.out.println("FU_LIVE_CODE\t"+m.declaringType().name()+"\t"+m.name()+"\t"+m.signature()+"\t"+b.length+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b)));}}
    static boolean watched(Method m){String t=m.declaringType().name(),n=m.name(),s=m.signature();
        if(t.equals("la")||t.equals("rk"))return n.equals("a")&&s.equals("(Liz;Lgs;Lfd;IIII)Z");
        if(t.equals("iz"))return n.equals("a")&&Set.of("(Lgs;Lfd;IIII)Z","(ILsn;)V","()Lgm;").contains(s)||Set.of("e","j").contains(n)&&Set.of("()Z","()I").contains(s);
        if(t.equals("fd"))return n.equals("a")&&Set.of("(III)I","(DDDLjava/lang/String;FF)V").contains(s)||n.equals("d")&&s.equals("(III)Z")||n.equals("f")&&s.equals("(IIII)Z");
        if(t.equals("ct"))return (n.equals("b")||n.equals("c"))&&s.equals("()F")||n.equals("d")&&s.equals("()Ljava/lang/String;");
        if(t.equals("gm"))return n.equals("f")&&s.equals("()I");
        return (t.equals("dc")||t.equals("xi"))&&n.equals("a")&&s.equals("(Lvr;I)V");
    }
    static void prepare(EventRequestManager manager,ReferenceType type){
        for(Method m:type.methods())if(watched(m)){BreakpointRequest q=manager.createBreakpointRequest(m.location());q.setSuspendPolicy(EventRequest.SUSPEND_ALL);q.enable();}
        String t=type.name();for(Field f:type.fields()){
            boolean access=t.equals("uu")&&Set.of("v","w","aB","bn","by").contains(f.name())||t.equals("fd")&&f.name().equals("B")||t.equals("rk")&&f.name().equals("a");
            boolean modify=t.equals("iz")&&Set.of("a","d").contains(f.name());
            if(access){AccessWatchpointRequest q=manager.createAccessWatchpointRequest(f);q.setSuspendPolicy(EventRequest.SUSPEND_ALL);q.enable();}
            if(modify){ModificationWatchpointRequest q=manager.createModificationWatchpointRequest(f);q.setSuspendPolicy(EventRequest.SUSPEND_ALL);q.enable();}
        }
    }
    static List<Object> args(StackFrame f){List<Object> r=new ArrayList<>();for(Value v:f.getArgumentValues())r.add(value(v));return r;}
    static void drain(Process p,InputStream in,Path path,int bound,List<Throwable> errors){try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];int total=0,n;while((n=in.read(b))!=-1){total+=n;if(total>bound){p.destroyForcibly();throw new IOException("pipe bound");}out.write(b,0,n);}}catch(Throwable e){errors.add(e);}}
    public static void main(String[] a)throws Exception{
        if(a.length!=6)throw new AssertionError("output,jdk,classpath,jar,world,profiles");Path output=Path.of(a[0]);LaunchingConnector launch=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=launch.defaultArguments();options.get("home").setValue(a[1]);options.get("main").setValue("FarmingObservation \""+a[3]+"\" \""+a[4]+"\" \""+a[5]+"\"");options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -cp \""+a[2]+"\"");options.get("suspend").setValue("true");vm=launch.launch(options);
        if(!vm.canWatchFieldAccess()||!vm.canWatchFieldModification()||!vm.canGetBytecodes()||!vm.canGetMethodReturnValues())throw new AssertionError("read-only JDI capabilities");Process child=vm.process();List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());Thread stdout=new Thread(()->drain(child,child.getInputStream(),output.resolve("target.stdout"),16777216,errors)),stderr=new Thread(()->drain(child,child.getErrorStream(),output.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();journal=new PrintWriter(Files.newBufferedWriter(output.resolve("events.jsonl")));boolean died=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(300);
        try{EventRequestManager manager=vm.eventRequestManager();for(String name:SOURCE){ClassPrepareRequest p=manager.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();MethodExitRequest q=manager.createMethodExitRequest();q.addClassFilter(name);q.setSuspendPolicy(EventRequest.SUSPEND_ALL);q.enable();}ExceptionRequest ex=manager.createExceptionRequest(null,true,true);ex.setSuspendPolicy(EventRequest.SUSPEND_ALL);ex.enable();
            while(!died){if(System.nanoTime()>deadline)throw new IOException("JDI deadline");EventSet set;try{set=vm.eventQueue().remove(1000);}catch(VMDisconnectedException e){break;}if(set==null)continue;
                for(Event event:set){if(event instanceof ClassPrepareEvent){prepare(manager,((ClassPrepareEvent)event).referenceType());continue;}if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent){died=true;continue;}if(helper==null&&vm.classesByName("FarmingObservation").isEmpty())continue;int profile=active();if(profile<0)continue;if(profile!=lastProfile){stack.clear();lastProfile=profile;}
                    if(event instanceof BreakpointEvent){BreakpointEvent e=(BreakpointEvent)event;Method m=e.location().method();if(!watched(m))continue;pin(m);StackFrame f=e.thread().frame(0);int call=nextCall++;stack.push(new AbstractMap.SimpleImmutableEntry<>(key(m),call));emit("entry",profile,"call",call,"at",at(e.location()),"caller",at(e.thread().frame(1).location()),"receiver",value(f.thisObject()),"args",args(f));++entries;
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;Method m=e.method();if(!watched(m))continue;pin(m);while(!stack.isEmpty()&&!stack.peek().getKey().equals(key(m)))stack.pop();if(stack.isEmpty())throw new AssertionError("unmatched watched return "+key(m));int call=stack.pop().getValue();emit("return",profile,"call",call,"at",at(e.location()),"value",value(e.returnValue()));
                    }else if(event instanceof AccessWatchpointEvent){AccessWatchpointEvent e=(AccessWatchpointEvent)event;if(!SOURCE.contains(e.location().declaringType().name()))continue;pin(e.location().method());emit("field-read",profile,"at",at(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"receiver",value(e.object()),"value",value(e.valueCurrent()));++reads;
                    }else if(event instanceof ModificationWatchpointEvent){ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;if(!SOURCE.contains(e.location().declaringType().name()))continue;pin(e.location().method());emit("field-write",profile,"at",at(e.location()),"field",e.field().declaringType().name()+"."+e.field().name(),"receiver",value(e.object()),"before",value(e.valueCurrent()),"after",value(e.valueToBe()));++stores;
                    }else if(event instanceof ExceptionEvent){ExceptionEvent e=(ExceptionEvent)event;String t=e.location().declaringType().name();if(!SOURCE.contains(t)&&!t.equals("FarmingObservation$Observer"))continue;List<String> frames=new ArrayList<>();boolean reached=false;for(StackFrame f:e.thread().frames()){String owner=f.location().declaringType().name();if(SOURCE.contains(owner)){pin(f.location().method());frames.add(at(f.location()));if(owner.equals("la")||owner.equals("rk"))reached=true;}}if(!reached&&!t.equals("iz"))continue;emit("exception",profile,"at",at(e.location()),"type",e.exception().referenceType().name(),"catch",at(e.catchLocation()),"frames",frames);++exceptions;}
                }try{set.resume();}catch(VMDisconnectedException e){died=true;}
            }
        }finally{journal.close();try{vm.dispose();}catch(VMDisconnectedException ignored){}if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();child.waitFor(5,TimeUnit.SECONDS);}stdout.join(5000);stderr.join(5000);}
        if(child.isAlive()||child.exitValue()!=0||stdout.isAlive()||stderr.isAlive()||!errors.isEmpty())throw new AssertionError("incomplete child/pipe "+errors);System.out.println("FU_JDI_COMPLETE events="+seq+" entries="+entries+" reads="+reads+" stores="+stores+" exceptions="+exceptions+" target-exit=0 source-writes=0 target-invocations=0");
    }
}
