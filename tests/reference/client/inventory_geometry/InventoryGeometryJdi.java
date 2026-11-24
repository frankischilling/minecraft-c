import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;

/** Read-only breakpoints over actual source receivers, buffers and source callsites. */
public final class InventoryGeometryJdi {
    static final String INVENTORY="(Luu;IF)V", FACE="(Luu;DDDI)V";
    static String hash(byte[] raw)throws Exception {
        StringBuilder result=new StringBuilder();
        for(byte value:MessageDigest.getInstance("SHA-256").digest(raw)) result.append(String.format(Locale.ROOT,"%02x",value&255));
        return result.toString();
    }
    static Object value(Value value) {
        if(value instanceof BooleanValue) return ((BooleanValue)value).value();
        if(value instanceof IntegerValue) return ((IntegerValue)value).value();
        if(value instanceof LongValue) return ((LongValue)value).value();
        if(value instanceof FloatValue) return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)value).value()));
        if(value instanceof DoubleValue) return String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)value).value()));
        return value==null?null:value.toString();
    }
    static ReferenceType type(VirtualMachine vm,String name) {
        List<ReferenceType> result=vm.classesByName(name); return result.isEmpty()?null:result.get(0);
    }
    static int active(VirtualMachine vm) {
        ReferenceType target=type(vm,"InventoryGeometryTarget");
        return target==null?-1:((IntegerValue)target.getValue(target.fieldByName("active"))).value();
    }
    static Map<String,Object> where(Location location) {
        return ConstructorJson.map("class",location.declaringType().name(),"method",location.method().name(),"descriptor",location.method().signature(),"pc",location.codeIndex());
    }
    static Map<String,Object> primitives(ObjectReference object) {
        Map<String,Object> result=new TreeMap<>();
        for(Field field:object.referenceType().allFields()) if(!field.isStatic()&&Set.of("boolean","int","long","float","double").contains(field.typeName())) result.put(field.name(),value(object.getValue(field)));
        return result;
    }
    static List<String> words(ObjectReference object,int count) {
        ArrayReference raw=(ArrayReference)object.getValue(object.referenceType().fieldByName("g"));
        List<String> result=new ArrayList<>();
        for(Value value:raw.getValues(0,count)) result.add(String.format(Locale.ROOT,"%08x",((IntegerValue)value).value()));
        return result;
    }
    static List<Object> bounds(ObjectReference block) {
        List<Object> result=new ArrayList<>();
        for(String name:new String[]{"bs","bt","bu","bv","bw","bx"}) result.add(value(block.getValue(block.referenceType().fieldByName(name))));
        return result;
    }
    static List<Object> arguments(StackFrame frame) {
        List<Object> result=new ArrayList<>();
        for(Value v:frame.getArgumentValues()) result.add(v instanceof ObjectReference?((ObjectReference)v).uniqueID():value(v));
        return result;
    }
    static List<Object> sourceFrames(ThreadReference thread)throws Exception {
        List<Object> result=new ArrayList<>();
        for(StackFrame frame:thread.frames()) if(Set.of("cv","ss","nw","uu").contains(frame.location().declaringType().name())) result.add(where(frame.location()));
        return result;
    }
    static ObjectReference block(ThreadReference thread)throws Exception {
        for(StackFrame frame:thread.frames()) if(frame.location().declaringType().name().equals("cv")&&frame.location().method().signature().equals(INVENTORY)) return (ObjectReference)frame.getArgumentValues().get(0);
        throw new AssertionError("actual entered inventory frame");
    }
    static ObjectReference singleton(VirtualMachine vm) {
        ReferenceType type=type(vm,"nw"); return (ObjectReference)type.getValue(type.fieldByName("a"));
    }
    static void breakpoint(EventRequestManager manager,ReferenceType type,String name,String descriptor) {
        List<Method> methods=type.methodsByName(name,descriptor);
        if(methods.size()!=1) throw new AssertionError("method identity "+type.name()+name+descriptor);
        BreakpointRequest request=manager.createBreakpointRequest(methods.get(0).location());
        request.setSuspendPolicy(EventRequest.SUSPEND_ALL); request.enable();
    }
    static void drain(Process child,InputStream stream,Path path,int bound,List<Throwable> errors) {
        try(OutputStream out=Files.newOutputStream(path)) {
            byte[] bytes=new byte[8192];int total=0,count;
            while((count=stream.read(bytes))!=-1) { total+=count; if(total>bound) {child.destroyForcibly();throw new IOException("pipe bound");} out.write(bytes,0,count); }
        } catch(Throwable problem) { synchronized(errors) { errors.add(problem); } }
    }
    public static void main(String[] args)throws Exception {
        Path out=Path.of(args[0]);String quote="\"";
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("home").setValue(args[1]);
        options.get("main").setValue("InventoryGeometryTarget "+quote+args[4]+quote+" "+quote+out.resolve("game")+quote+" "+quote+args[2]+quote+" "+args[5]);
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow -Dorg.lwjgl.librarypath="+quote+args[2]+quote+" -cp "+quote+args[3]+quote);
        options.get("suspend").setValue("true");VirtualMachine vm=connector.launch(options);Process child=vm.process();
        List<Throwable> errors=Collections.synchronizedList(new ArrayList<>());
        Thread stdout=new Thread(()->drain(child,child.getInputStream(),out.resolve("target.stdout"),8388608,errors));
        Thread stderr=new Thread(()->drain(child,child.getErrorStream(),out.resolve("target.stderr"),1048576,errors));stdout.start();stderr.start();
        int entries=0,returns=0,draws=0,faces=0,exceptions=0;int[] next=new int[17];Set<String> pinned=new HashSet<>();
        try(PrintWriter journal=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.ndjson")))) {
            EventRequestManager requests=vm.eventRequestManager();
            for(String name:new String[]{"cv","nw","uu","ss","org.lwjgl.opengl.GL11"}) {
                ClassPrepareRequest request=requests.createClassPrepareRequest();request.addClassFilter(name);request.setSuspendPolicy(EventRequest.SUSPEND_ALL);request.enable();
            }
            MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter("cv");exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.enable();
            ExceptionRequest exception=requests.createExceptionRequest(null,true,true);exception.addClassFilter("ss");exception.setSuspendPolicy(EventRequest.SUSPEND_ALL);exception.enable();
            long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(240);boolean alive=true;
            while(alive) {
                if(System.nanoTime()>deadline) throw new IOException("observer deadline");
                EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event event:set) {
                    if(event instanceof ClassPrepareEvent) {
                        ReferenceType type=((ClassPrepareEvent)event).referenceType();
                        if(type.name().equals("cv")) {breakpoint(requests,type,"a",INVENTORY);for(String name:new String[]{"a","b","c","d","e","f"})breakpoint(requests,type,name,FACE);}
                        if(type.name().equals("nw")) {breakpoint(requests,type,"a","()V");breakpoint(requests,type,"b","()V");breakpoint(requests,type,"c","(FFF)V");}
                        if(type.name().equals("uu")) breakpoint(requests,type,"a","(FFFFFF)V");
                        if(type.name().equals("ss")) breakpoint(requests,type,"a","(I)I");
                        if(type.name().equals("org.lwjgl.opengl.GL11")) breakpoint(requests,type,"glTranslatef","(FFF)V");
                    } else if(event instanceof BreakpointEvent) {
                        BreakpointEvent reached=(BreakpointEvent)event;int profile=active(vm);if(profile<0)continue;
                        Method method=reached.location().method();String owner=method.declaringType().name();String signature=method.signature();
                        if(!owner.startsWith("org.lwjgl.")) {
                            String key=owner+method.name()+signature;
                            if(pinned.add(key)) journal.println(ConstructorJson.json(ConstructorJson.map("event","live-code","class",owner,"method",method.name(),"descriptor",signature,"bytes",method.bytecodes().length,"sha256",hash(method.bytecodes()))));
                        }
                        StackFrame frame=reached.thread().frame(0);ObjectReference receiver=frame.thisObject();
                        Map<String,Object> record=ConstructorJson.map("event","","profile",profile,"source-frames",sourceFrames(reached.thread()));
                        if(owner.equals("cv")&&signature.equals(INVENTORY)) {
                            record.put("event","cv-entry");record.put("receiver",receiver.uniqueID());record.put("arguments",arguments(frame));record.put("cv",primitives(receiver));record.put("bounds",bounds(block(reached.thread())));entries++;
                        } else if(owner.equals("cv")&&signature.equals(FACE)) {
                            ObjectReference tess=singleton(vm);
                            record.put("event","cv-face");record.put("face",method.name());record.put("arguments",arguments(frame));record.put("receiver",receiver.uniqueID());record.put("cv",primitives(receiver));record.put("bounds",bounds(block(reached.thread())));record.put("nw",primitives(tess));record.put("prior_words",words(tess,48));faces++;
                        } else if(owner.equals("nw")) {
                            if(!receiver.equals(singleton(vm)))throw new AssertionError("actual singleton NW receiver");
                            record.put("receiver",receiver.uniqueID());record.put("state",primitives(receiver));record.put("bounds",bounds(block(reached.thread())));
                            if(method.name().equals("a")) {
                                int count=((IntegerValue)receiver.getValue(receiver.referenceType().fieldByName("o"))).value();if(count!=48)throw new AssertionError("actual four-source-vertex batch "+count);
                                record.put("event","nw-draw");record.put("batch",next[profile]++);record.put("words",words(receiver,count));draws++;
                            } else if(method.name().equals("b")) {record.put("event","nw-begin");record.put("prior_words",words(receiver,48));}
                            else {record.put("event","nw-add-offset");record.put("arguments",arguments(frame));}
                        } else if(owner.equals("uu")) {record.put("event","block-bounds");record.put("receiver",receiver.uniqueID());record.put("arguments",arguments(frame));record.put("before",bounds(receiver));}
                        else if(owner.equals("ss")) {record.put("event","stair-texture");record.put("receiver",receiver.uniqueID());record.put("arguments",arguments(frame));ObjectReference source=(ObjectReference)receiver.getValue(receiver.referenceType().fieldByName("a"));record.put("source-null",source==null);record.put("source-id",source==null?null:value(source.getValue(source.referenceType().fieldByName("bn"))));}
                        else {record.put("event","gl-translate");record.put("arguments",arguments(frame));}
                        journal.println(ConstructorJson.json(record));
                    } else if(event instanceof MethodExitEvent) {
                        MethodExitEvent reached=(MethodExitEvent)event;int profile=active(vm);
                        if(profile>=0&&reached.method().name().equals("a")&&reached.method().signature().equals(INVENTORY)) {returns++;journal.println(ConstructorJson.json(ConstructorJson.map("event","cv-return","profile",profile)));}
                    } else if(event instanceof ExceptionEvent) {
                        ExceptionEvent reached=(ExceptionEvent)event;int profile=active(vm);if(profile>=0) {
                            exceptions++;journal.println(ConstructorJson.json(ConstructorJson.map("event","source-exception","profile",profile,"exception",reached.exception().referenceType().name(),"site",where(reached.location()),"source-frames",sourceFrames(reached.thread()))));
                        }
                    } else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent) alive=false;
                }
                journal.flush();if(journal.checkError())throw new IOException("journal write");try{set.resume();}catch(VMDisconnectedException ended){alive=false;}
            }
        } finally {try{vm.dispose();}catch(VMDisconnectedException ended){}if(!child.waitFor(30,TimeUnit.SECONDS))child.destroyForcibly();stdout.join(5000);stderr.join(5000);}
        if(child.exitValue()!=0||!errors.isEmpty()||entries!=17||returns!=16||draws!=216||faces!=216||exceptions!=1) throw new AssertionError("complete source capture: exit="+child.exitValue()+" entries="+entries+" returns="+returns+" draws="+draws+" faces="+faces+" exceptions="+exceptions+" pipes="+errors);
        for(int i=0;i<17;++i)if(next[i]!=(i==16?0:new int[]{6,12,12,24}[i%4]))throw new AssertionError("actual profile draws "+i);
    }
}
