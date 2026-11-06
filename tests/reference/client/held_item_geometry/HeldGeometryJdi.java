import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only actual NW draw-entry buffer and live method-Code observer. */
public final class HeldGeometryJdi {
    static String hash(byte[] raw)throws Exception {StringBuilder s=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))s.append(String.format(Locale.ROOT,"%02x",b&255));return s.toString();}
    static Object value(Value v){if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));return v==null?null:v.toString();}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> t=vm.classesByName(name);return t.isEmpty()?null:t.get(0);}
    static int active(VirtualMachine vm){ReferenceType target=type(vm,"HeldGeometryTarget");return target==null?-1:((IntegerValue)target.getValue(target.fieldByName("active"))).value();}
    static Map<String,Object> where(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    static Map<String,Object> primitives(ObjectReference object){Map<String,Object> out=new TreeMap<>();for(Field f:object.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))out.put(f.name(),value(object.getValue(f)));return out;}
    static List<String> words(ObjectReference nw,int count){ArrayReference raw=(ArrayReference)nw.getValue(nw.referenceType().fieldByName("g"));List<String> result=new ArrayList<>();for(Value v:raw.getValues(0,count))result.add(String.format(Locale.ROOT,"%08x",((IntegerValue)v).value()));return result;}
    static void breakpoint(EventRequestManager manager,ReferenceType t,String name,String descriptor){List<Method> methods=t.methodsByName(name,descriptor);if(methods.size()!=1)throw new AssertionError("method identity "+t.name()+name+descriptor);BreakpointRequest b=manager.createBreakpointRequest(methods.get(0).location());b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();}
    static void drain(Process child,InputStream stream,Path path,int bound,List<Throwable> errors){try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];int total=0,n;while((n=stream.read(b))!=-1){total+=n;if(total>bound){child.destroyForcibly();throw new IOException("pipe bound");}out.write(b,0,n);}}catch(Throwable t){synchronized(errors){errors.add(t);}}}
    public static void main(String[] args)throws Exception {
        Path out=Path.of(args[0]);String q="\"";
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> a=connector.defaultArguments();
        a.get("home").setValue(args[1]);
        a.get("main").setValue("HeldGeometryTarget "+q+args[4]+q+" "+q+out.resolve("game")+q+" "+q+args[2]+q);
        a.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+q+args[2]+q+" -cp "+q+args[3]+q);
        a.get("suspend").setValue("true");VirtualMachine vm=connector.launch(a);Process child=vm.process();
        List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),out.resolve("target.stdout"),4194304,errors));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),out.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();
        int batches=0,entries=0,returns=0;int[] next=new int[6];Set<String> pinned=new HashSet<>();
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.ndjson")))) {
            EventRequestManager requests=vm.eventRequestManager();
            for(String name:new String[]{"ra","nw"}){ClassPrepareRequest p=requests.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();}
            MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter("ra");exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.enable();
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);boolean alive=true;
            while(alive){if(System.nanoTime()>deadline)throw new IOException("observer deadline");EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){ReferenceType t=((ClassPrepareEvent)event).referenceType();if(t.name().equals("ra"))breakpoint(requests,t,"a","(Lls;Liz;)V");else breakpoint(requests,t,"a","()V");}
                    else if(event instanceof BreakpointEvent){BreakpointEvent b=(BreakpointEvent)event;int profile=active(vm);if(profile<0)continue;
                        Method method=b.location().method();String key=method.declaringType().name()+method.name()+method.signature();
                        if(pinned.add(key))journal.println(ConstructorJson.json(ConstructorJson.map("event","live-code","class",method.declaringType().name(),"method",method.name(),"descriptor",method.signature(),"bytes",method.bytecodes().length,"sha256",hash(method.bytecodes()))));
                        StackFrame frame=b.thread().frame(0);ObjectReference receiver=frame.thisObject();
                        if(method.declaringType().name().equals("ra")){
                            entries++;List<Value> arguments=frame.getArgumentValues();ObjectReference player=(ObjectReference)arguments.get(0),stack=(ObjectReference)arguments.get(1);
                            journal.println(ConstructorJson.json(ConstructorJson.map("event","ra-entry","profile",profile,"receiver",receiver.uniqueID(),"app",((ObjectReference)receiver.getValue(receiver.referenceType().fieldByName("a"))).uniqueID(),"player",player.uniqueID(),"player-class",player.referenceType().name(),"stack",stack.uniqueID(),"stack-class",stack.referenceType().name(),"stack-id",value(stack.getValue(stack.referenceType().fieldByName("c"))))));
                        } else {
                            ReferenceType owner=type(vm,"nw");ObjectReference singleton=(ObjectReference)owner.getValue(owner.fieldByName("a"));if(!singleton.equals(receiver))throw new AssertionError("actual singleton NW receiver");
                            int count=((IntegerValue)receiver.getValue(owner.fieldByName("o"))).value();if(count!=48&&count!=768)throw new AssertionError("actual batch word count "+count);
                            List<Object> frames=new ArrayList<>();for(StackFrame f:b.thread().frames())if(f.location().declaringType().name().equals("ra"))frames.add(where(f.location()));
                            journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-draw","profile",profile,"batch",next[profile]++,"receiver",receiver.uniqueID(),"state",primitives(receiver),"source-frames",frames,"words",words(receiver,count))));batches++;
                        }
                    } else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;int profile=active(vm);if(profile>=0&&e.method().name().equals("a")&&e.method().signature().equals("(Lls;Liz;)V")){returns++;journal.println(ConstructorJson.json(ConstructorJson.map("event","ra-return","profile",profile)));}}
                    else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
                }
                journal.flush();if(journal.checkError())throw new IOException("journal write");try{set.resume();}catch(VMDisconnectedException e){alive=false;}
            }
        } finally {try{vm.dispose();}catch(VMDisconnectedException e){}if(!child.waitFor(30,TimeUnit.SECONDS))child.destroyForcibly();stdout.join(5000);stderr.join(5000);}
        if(child.exitValue()!=0||!errors.isEmpty()||batches!=36||entries!=6||returns!=6||Arrays.stream(next).anyMatch(n->n!=6))throw new AssertionError("complete direct draw capture: exit="+child.exitValue()+" batches="+batches+" entries="+entries+" returns="+returns+" pipes="+errors);
    }
}
