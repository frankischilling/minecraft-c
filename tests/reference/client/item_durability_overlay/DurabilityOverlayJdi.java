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
public final class DurabilityOverlayJdi {
    static String hash(byte[] raw)throws Exception {StringBuilder s=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(raw))s.append(String.format(Locale.ROOT,"%02x",b&255));return s.toString();}
    static Object value(Value v){if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof LongValue)return ((LongValue)v).value();if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));if(v instanceof DoubleValue)return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value()));return v==null?null:v.toString();}
    static ReferenceType type(VirtualMachine vm,String name){List<ReferenceType> t=vm.classesByName(name);return t.isEmpty()?null:t.get(0);}
    static int active(VirtualMachine vm){ReferenceType target=type(vm,"DurabilityOverlayTarget");return target==null?-1:((IntegerValue)target.getValue(target.fieldByName("active"))).value();}
    static Map<String,Object> where(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    static Map<String,Object> primitives(ObjectReference object){Map<String,Object> out=new TreeMap<>();for(Field f:object.referenceType().allFields())if(!f.isStatic()&&Set.of("boolean","int","long","float","double").contains(f.typeName()))out.put(f.name(),value(object.getValue(f)));return out;}
    static List<String> words(ObjectReference nw,int count){ArrayReference raw=(ArrayReference)nw.getValue(nw.referenceType().fieldByName("g"));List<String> result=new ArrayList<>();for(Value v:raw.getValues(0,count))result.add(String.format(Locale.ROOT,"%08x",((IntegerValue)v).value()));return result;}
    static void breakpoint(EventRequestManager manager,ReferenceType t,String name,String descriptor){List<Method> methods=t.methodsByName(name,descriptor);if(methods.size()!=1)throw new AssertionError("method identity "+t.name()+name+descriptor);BreakpointRequest b=manager.createBreakpointRequest(methods.get(0).location());b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();}
    static void drain(Process child,InputStream stream,Path path,int bound,List<Throwable> errors){try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];int total=0,n;while((n=stream.read(b))!=-1){total+=n;if(total>bound){child.destroyForcibly();throw new IOException("pipe bound");}out.write(b,0,n);}}catch(Throwable t){synchronized(errors){errors.add(t);}}}
    static void breakpointAt(EventRequestManager manager,ReferenceType t,String name,String descriptor,long pc){
        List<Method> methods=t.methodsByName(name,descriptor);if(methods.size()!=1)throw new AssertionError("method identity");
        Location location=pc==0?methods.get(0).location():methods.get(0).locationOfCodeIndex(pc);
        if(location==null)throw new AssertionError("instruction location");
        BreakpointRequest b=manager.createBreakpointRequest(location);b.setSuspendPolicy(EventRequest.SUSPEND_ALL);b.enable();
    }
    static List<Object> frames(ThreadReference thread)throws Exception {
        List<Object> out=new ArrayList<>();for(StackFrame f:thread.frames())if(Set.of("bb","iz","gm").contains(f.location().declaringType().name()))out.add(where(f.location()));return out;
    }
    static void pin(PrintWriter journal,Set<String> pinned,Method m)throws Exception {
        String key=m.declaringType().name()+m.name()+m.signature();
        if(pinned.add(key))journal.println(ConstructorJson.json(ConstructorJson.map("event","live-code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",m.bytecodes().length,"sha256",hash(m.bytecodes()))));
    }
    public static void main(String[] args)throws Exception {
        Path out=Path.of(args[0]);String q="\"";
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument>a=connector.defaultArguments();
        a.get("home").setValue(args[1]);
        a.get("main").setValue("DurabilityOverlayTarget "+q+args[4]+q+" "+q+out.resolve("game")+q+" "+q+args[2]+q);
        a.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+q+args[2]+q+" -cp "+q+args[3]+q);
        a.get("suspend").setValue("true");VirtualMachine vm=connector.launch(a);Process child=vm.process();
        if(!vm.canGetMethodReturnValues())throw new AssertionError("read-only return values required");
        List<Throwable>errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),out.resolve("target.stdout"),4194304,errors));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),out.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();
        int batches=0,entries=0,returns=0,rectangles=0,rectangleReturns=0;int[]next=new int[9];Set<String>pinned=new HashSet<>();
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.ndjson")))){
            EventRequestManager requests=vm.eventRequestManager();List<MethodExitRequest>scopedExits=new ArrayList<>();
            for(String name:new String[]{"bb","nw","iz","gm","java.lang.Math"}){
                ClassPrepareRequest p=requests.createClassPrepareRequest();p.addClassFilter(name);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();
                if(!name.equals("nw")){MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter(name);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);if(name.equals("bb"))exit.enable();else scopedExits.add(exit);}
            }
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(180);boolean alive=true;
            while(alive){if(System.nanoTime()>deadline)throw new IOException("observer deadline");EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set){
                    if(event instanceof ClassPrepareEvent){ReferenceType t=((ClassPrepareEvent)event).referenceType();
                        if(t.name().equals("bb")){breakpointAt(requests,t,"b","(Lsj;Lji;Liz;II)V",0);breakpointAt(requests,t,"a","(Lnw;IIIII)V",0);}
                        if(t.name().equals("nw"))breakpointAt(requests,t,"a","()V",0);
                        if(t.name().equals("iz"))breakpointAt(requests,t,"g","()Z",8);
                    } else if(event instanceof BreakpointEvent){BreakpointEvent b=(BreakpointEvent)event;int profile=active(vm);if(profile<0)continue;
                        Method method=b.location().method();pin(journal,pinned,method);StackFrame frame=b.thread().frame(0);ObjectReference receiver=frame.thisObject();
                        if(method.declaringType().name().equals("bb")){
                            List<Value>v=frame.getArgumentValues();
                            if(method.name().equals("b")){
                                entries++;for(MethodExitRequest request:scopedExits)request.enable();ObjectReference stack=(ObjectReference)v.get(2);
                                journal.println(ConstructorJson.json(ConstructorJson.map("event","overlay-entry","profile",profile,"receiver",receiver.uniqueID(),"font",((ObjectReference)v.get(0)).uniqueID(),"engine",((ObjectReference)v.get(1)).uniqueID(),"stack",stack.uniqueID(),"id",value(stack.getValue(stack.referenceType().fieldByName("c"))),"damage",value(stack.getValue(stack.referenceType().fieldByName("d"))),"count",value(stack.getValue(stack.referenceType().fieldByName("a"))),"x",value(v.get(3)),"y",value(v.get(4)))));
                            }else{
                                ObjectReference nw=(ObjectReference)v.get(0);rectangles++;
                                journal.println(ConstructorJson.json(ConstructorJson.map("event","rectangle-entry","profile",profile,"batch",next[profile],"renderer",receiver.uniqueID(),"nw",nw.uniqueID(),"x",value(v.get(1)),"y",value(v.get(2)),"width",value(v.get(3)),"height",value(v.get(4)),"color",value(v.get(5)),"before",words(nw,48),"state-before",primitives(nw),"source-frames",frames(b.thread()))));
                            }
                        }else if(method.declaringType().name().equals("iz")){
                            journal.println(ConstructorJson.json(ConstructorJson.map("event","initial-damage","profile",profile,"damage",value(receiver.getValue(receiver.referenceType().fieldByName("d"))),"source-frames",frames(b.thread()))));
                        }else{
                            ReferenceType owner=type(vm,"nw");ObjectReference singleton=(ObjectReference)owner.getValue(owner.fieldByName("a"));if(!singleton.equals(receiver))throw new AssertionError("actual singleton NW");
                            int count=((IntegerValue)receiver.getValue(owner.fieldByName("o"))).value();if(count!=48)throw new AssertionError("actual rectangle48 "+count);
                            journal.println(ConstructorJson.json(ConstructorJson.map("event","nw-draw","profile",profile,"batch",next[profile]++,"receiver",receiver.uniqueID(),"state",primitives(receiver),"source-frames",frames(b.thread()),"words",words(receiver,count))));batches++;
                        }
                    }else if(event instanceof MethodExitEvent){MethodExitEvent e=(MethodExitEvent)event;int profile=active(vm);if(profile<0)continue;Method m=e.method();
                        String cl=m.declaringType().name();
                        if(cl.equals("bb")&&m.name().equals("b")&&m.signature().equals("(Lsj;Lji;Liz;II)V")){returns++;for(MethodExitRequest request:scopedExits)request.disable();journal.println(ConstructorJson.json(ConstructorJson.map("event","overlay-return","profile",profile)));}
                        else if(cl.equals("bb")&&m.name().equals("a")&&m.signature().equals("(Lnw;IIIII)V")){rectangleReturns++;journal.println(ConstructorJson.json(ConstructorJson.map("event","rectangle-return","profile",profile,"batch",next[profile]-1)));}
                        else if(cl.equals("java.lang.Math")&&m.name().equals("round")&&m.signature().equals("(D)J")){journal.println(ConstructorJson.json(ConstructorJson.map("event","math-round","profile",profile,"value",value(e.returnValue()),"source-frames",frames(e.thread()))));}
                        else if((cl.equals("gm")&&m.name().equals("f")&&m.signature().equals("()I"))||(cl.equals("iz")&&Set.of("e","g","h","j").contains(m.name())&&(m.signature().equals("()I")||m.signature().equals("()Z")))){
                            pin(journal,pinned,m);journal.println(ConstructorJson.json(ConstructorJson.map("event","getter","profile",profile,"class",cl,"method",m.name(),"descriptor",m.signature(),"value",value(e.returnValue()),"source-frames",frames(e.thread()))));
                        }
                    }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
                }
                journal.flush();if(journal.checkError())throw new IOException("journal write");try{set.resume();}catch(VMDisconnectedException e){alive=false;}
            }
        }finally{try{vm.dispose();}catch(VMDisconnectedException e){}if(!child.waitFor(30,TimeUnit.SECONDS))child.destroyForcibly();stdout.join(5000);stderr.join(5000);}
        if(child.exitValue()!=0||!errors.isEmpty()||batches!=24||rectangles!=24||rectangleReturns!=24||entries!=9||returns!=9||next[0]!=0||Arrays.stream(next).skip(1).anyMatch(n->n!=3))throw new AssertionError("complete direct overlay capture: exit="+child.exitValue()+" draws="+batches+" entries="+entries+" returns="+returns+" rectangles="+rectangles+" rectangleReturns="+rectangleReturns+" pipes="+errors);
    }
}
