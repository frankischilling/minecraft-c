import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Read-only observer: getValue/arguments/returns/bytecodes only; never invokeMethod. */
public final class MonsterJdi {
    final VirtualMachine vm; final PrintWriter output;
    final List<MethodEntryRequest> entries=new ArrayList<>();
    final List<MethodExitRequest> exits=new ArrayList<>();
    final Set<String> pins=new HashSet<>();
    int active=-1,sequence;boolean prefix;
    ObjectReference actor,target,world;
    MonsterJdi(VirtualMachine v,PrintWriter o){vm=v;output=o;}
    Value stat(String n){ReferenceType t=vm.classesByName("MonsterTarget").get(0);return t.getValue(t.fieldByName(n));}
    String role(ObjectReference o){return o==null?null:o.equals(actor)?"actor":o.equals(target)?"target":o.equals(world)?"world":"other";}
    Object value(Value v){
        if(v==null)return null;
        if(v instanceof FloatValue)return ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof LongValue)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof ByteValue)return ((ByteValue)v).value();
        if(v instanceof ShortValue)return ((ShortValue)v).value();
        if(v instanceof CharValue)return (int)((CharValue)v).value();
        if(v instanceof StringReference)return ((StringReference)v).value();
        if(v instanceof VoidValue)return null;
        ObjectReference o=(ObjectReference)v;
        return ConstructorJson.map("class",o.referenceType().name(),"role",role(o));
    }
    Value field(ObjectReference o,String c,String name){
        if(o==null)return null;
        for(Field f:o.referenceType().allFields())if(f.declaringType().name().equals(c)&&f.name().equals(name))return o.getValue(f);
        throw new IllegalStateException(c+"."+name);
    }
    Object fields(ObjectReference o){
        if(o==null)return null;Map<String,Object> result=new TreeMap<>();
        for(Field f:o.referenceType().allFields())if(!f.isStatic())result.put(f.declaringType().name()+"."+f.name(),value(o.getValue(f)));
        return result;
    }
    String seed(ObjectReference r){
        if(r==null)return null;
        ObjectReference cell=(ObjectReference)field(r,"java.util.Random","seed");
        return String.format(Locale.ROOT,"%012x",((LongValue)field(cell,"java.util.concurrent.atomic.AtomicLong","value")).value());
    }
    Map<String,Object> state(){
        return ConstructorJson.map("actor",fields(actor),"target",fields(target),
            "actor_bounds",fields((ObjectReference)field(actor,"sn","aW")),
            "target_bounds",fields((ObjectReference)field(target,"sn","aW")),
            "actor_seed",seed((ObjectReference)field(actor,"sn","bs")),
            "target_seed",seed((ObjectReference)field(target,"sn","bs")),
            "world_seed",seed((ObjectReference)field(world,"fd","r")));
    }
    Object site(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    Object arguments(StackFrame f){List<Object> a=new ArrayList<>();for(Value v:f.getArgumentValues())a.add(value(v));return a;}
    Object frames(ThreadReference t)throws Exception{
        List<Object> result=new ArrayList<>();
        for(StackFrame f:t.frames())if(Set.of("gz","ls","sn","wh","uz","ii","bg").contains(f.location().declaringType().name()))result.add(site(f.location()));
        return result;
    }
    void emit(Map<String,Object> r){r.put("sequence",sequence++);output.println(ConstructorJson.json(r));output.flush();}
    void pin(Method m)throws Exception{
        String key=m.declaringType().name()+"."+m.name()+m.signature();if(!pins.add(key))return;
        byte[] b=m.bytecodes();emit(ConstructorJson.map("event","code-pin","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b))));
    }
    void enabled(boolean b){for(MethodEntryRequest r:entries)r.setEnabled(b);for(MethodExitRequest r:exits)r.setEnabled(b);}
    void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();
        for(String c:new String[]{"gz","ls","sn","java.util.Random"}){
            MethodEntryRequest en=manager.createMethodEntryRequest();en.addClassFilter(c);en.setSuspendPolicy(EventRequest.SUSPEND_ALL);entries.add(en);
            MethodExitRequest ex=manager.createMethodExitRequest();ex.addClassFilter(c);ex.setSuspendPolicy(EventRequest.SUSPEND_ALL);exits.add(ex);
        }
        for(String c:new String[]{"MonsterTarget","ls"}){
            ClassPrepareRequest r=manager.createClassPrepareRequest();r.addClassFilter(c);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();
        }
        ExceptionRequest errors=manager.createExceptionRequest(null,true,true);errors.setSuspendPolicy(EventRequest.SUSPEND_ALL);errors.enable();
        boolean alive=true;
        while(alive){
            EventSet set=vm.eventQueue().remove(60000);if(set==null)throw new IllegalStateException("JDI timeout");
            for(Event event:set){
                if(event instanceof ClassPrepareEvent){
                    ReferenceType t=((ClassPrepareEvent)event).referenceType();
                    String f=t.name().equals("MonsterTarget")?"active":"ae";
                    ModificationWatchpointRequest r=manager.createModificationWatchpointRequest(t.fieldByName(f));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();
                }else if(event instanceof ModificationWatchpointEvent){
                    ModificationWatchpointEvent e=(ModificationWatchpointEvent)event;
                    if(e.field().declaringType().name().equals("MonsterTarget")){
                        int n=((IntegerValue)e.valueToBe()).value();
                        if(active>=0)emit(ConstructorJson.map("event","component-finished","case",active,"state",state()));
                        active=n;prefix=false;enabled(n>=0);
                        if(n>=0){actor=(ObjectReference)stat("actor");target=(ObjectReference)stat("target");world=(ObjectReference)stat("world");emit(ConstructorJson.map("event","component-entry","case",active,"state",state()));}
                    }else if(active>=0&&e.object().equals(actor))emit(ConstructorJson.map("event","cooldown-store","case",active,"site",site(e.location()),"value",value(e.valueToBe()),"frames",frames(e.thread())));
                }else if(event instanceof MethodEntryEvent&&active>=0){
                    MethodEntryEvent e=(MethodEntryEvent)event;Method m=e.method();pin(m);
                    List<StackFrame> fs=e.thread().frames();
                    boolean hurt=m.name().equals("a")&&m.signature().equals("(Lsn;I)Z")&&fs.size()>1&&fs.get(1).location().declaringType().name().equals("gz")&&fs.get(1).location().codeIndex()==61;
                    Map<String,Object> r=ConstructorJson.map("event",hurt?"hurt-prefix":"entry","case",active,"phase",prefix?"after-native-frontier":"native-prefix","site",site(e.location()),"receiver",value(fs.get(0).thisObject()),"arguments",arguments(fs.get(0)),"frames",frames(e.thread()));
                    if(hurt){r.put("state",state());prefix=true;}emit(r);
                }else if(event instanceof MethodExitEvent&&active>=0){
                    MethodExitEvent e=(MethodExitEvent)event;pin(e.method());emit(ConstructorJson.map("event","return","case",active,"phase",prefix?"after-native-frontier":"native-prefix","site",site(e.location()),"result",value(e.returnValue())));
                }else if(event instanceof ExceptionEvent&&active>=0){
                    ExceptionEvent e=(ExceptionEvent)event;emit(ConstructorJson.map("event","exception","case",active,"phase",prefix?"after-native-frontier":"native-prefix","class",e.exception().referenceType().name(),"site",site(e.location()),"state",state(),"frames",frames(e.thread())));
                }else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
            }
            try{set.resume();}catch(VMDisconnectedException expected){alive=false;}
        }
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=5)throw new IllegalArgumentException("jar classes output jdk-home profiles");
        Path out=Path.of(args[2]).toAbsolutePath();String q=Character.toString((char)34);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument>a=connector.defaultArguments();
        a.get("home").setValue(args[3]);a.get("main").setValue("MonsterTarget target.ndjson "+q+args[4]+q);
        a.get("options").setValue("-Xint -Xverify:all -Xmx512m --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -Xlog:class+load=info,class+init=info:file=class-init.log -classpath "+q+args[1]+File.pathSeparator+args[0]+q);a.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(a);Process process=vm.process();
        Thread stdout=new Thread(()->{try(OutputStream o=Files.newOutputStream(out.resolve("target.stdout"))){process.getInputStream().transferTo(o);}catch(IOException e){throw new UncheckedIOException(e);}});
        Thread stderr=new Thread(()->{try(OutputStream o=Files.newOutputStream(out.resolve("target.stderr"))){process.getErrorStream().transferTo(o);}catch(IOException e){throw new UncheckedIOException(e);}});
        stdout.start();stderr.start();boolean observed=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(out.resolve("checkpoints.ndjson")))){new MonsterJdi(vm,writer).run();if(writer.checkError())throw new IOException("transport");observed=true;}
        finally{try{vm.dispose();}catch(VMDisconnectedException expected){}if(!observed)process.destroyForcibly();}
        process.waitFor();stdout.join();stderr.join();if(process.exitValue()!=0)throw new IllegalStateException("target exit"+process.exitValue());
    }
}
