import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.atomic.AtomicReference;
import java.util.zip.ZipFile;

/** Read-only JDI: actual original frames/Code and actual constructor/Random results. */
public final class AnimalDeathJdi {
    final VirtualMachine vm;final PrintWriter output;final Set<String> originals,pinned=new HashSet<>();
    MethodEntryRequest entry;MethodExitRequest exit,randomExit,systemExit;int active=-1,sequence;long bytes;
    AnimalDeathJdi(VirtualMachine v,PrintWriter o,Set<String> names){vm=v;output=o;originals=names;}
    ReferenceType type(String name){List<ReferenceType> found=vm.classesByName(name);return found.isEmpty()?null:found.get(0);}
    Value stat(String c,String name){ReferenceType t=type(c);return t==null||!t.isPrepared()?null:t.getValue(t.fieldByName(name));}
    static Value field(ObjectReference r,String c,String name){if(r==null)return null;for(Field f:r.referenceType().allFields())if(f.name().equals(name)&&f.declaringType().name().equals(c))return r.getValue(f);throw new IllegalArgumentException(c+"."+name);}
    static Object value(Value v){
        if(v==null||v instanceof VoidValue)return null;if(v instanceof IntegerValue)return((IntegerValue)v).value();if(v instanceof BooleanValue)return((BooleanValue)v).value();if(v instanceof ByteValue)return(int)((ByteValue)v).value();if(v instanceof ShortValue)return(int)((ShortValue)v).value();if(v instanceof CharValue)return(int)((CharValue)v).value();
        if(v instanceof FloatValue)return ConstructorJson.map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return ConstructorJson.map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof LongValue)return ConstructorJson.map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));if(v instanceof StringReference)return((StringReference)v).value();ObjectReference r=(ObjectReference)v;return ConstructorJson.map("class",r.referenceType().name(),"oid",r.uniqueID());
    }
    String seed(ObjectReference r){if(r==null)return null;return String.format(Locale.ROOT,"%016x",((LongValue)field((ObjectReference)field(r,"java.util.Random","seed"),"java.util.concurrent.atomic.AtomicLong","value")).value());}
    Map<String,Object> fields(ObjectReference r){if(r==null)return null;Map<String,Object> out=new TreeMap<>();for(Field f:r.referenceType().allFields())if(!f.isStatic())out.put(f.declaringType().name()+"."+f.name(),value(r.getValue(f)));return out;}
    static Map<String,Object> location(Location l){return ConstructorJson.map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",l.codeIndex());}
    List<Object> frames(ThreadReference t)throws Exception{List<Object> out=new ArrayList<>();for(StackFrame f:t.frames())if(originals.contains(f.location().declaringType().name()))out.add(location(f.location()));return out;}
    List<Object> arguments(StackFrame f){List<Object> out=new ArrayList<>();for(Value v:f.getArgumentValues())out.add(value(v));return out;}
    void emit(Map<String,Object> event){event.put("sequence",sequence++);String line=ConstructorJson.json(event);bytes+=line.length()+1;if(sequence>200000||bytes>64L*1024*1024)throw new IllegalStateException("bounded JDI journal");output.println(line);output.flush();if(output.checkError())throw new IllegalStateException("JDI journal write");}
    void pin(Method m)throws Exception{if(!pinned.add(m.declaringType().name()+"."+m.name()+m.signature()))return;byte[] code=m.bytecodes();emit(ConstructorJson.map("event","code-pin","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",code.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code))));}
    ObjectReference shared(){return(ObjectReference)stat("AnimalDeathObservation","shared");}
    void begin(int next){
        if(active>=0)emit(ConstructorJson.map("event","active-return","case",active,"math_seed",seed(shared())));
        active=next;entry.setEnabled(next>=0);exit.setEnabled(next>=0);randomExit.setEnabled(next>=0);systemExit.setEnabled(next>=0);
        if(next>=0)emit(ConstructorJson.map("event","active-entry","case",active,"math_seed",seed(shared()),"actor_random",value(field((ObjectReference)stat("AnimalDeathObservation","actor"),"sn","bs")),"actor_seed",seed((ObjectReference)field((ObjectReference)stat("AnimalDeathObservation","actor"),"sn","bs"))));
    }
    void run()throws Exception{
        EventRequestManager manager=vm.eventRequestManager();entry=manager.createMethodEntryRequest();exit=manager.createMethodExitRequest();randomExit=manager.createMethodExitRequest();randomExit.addClassFilter("java.util.Random");systemExit=manager.createMethodExitRequest();systemExit.addClassFilter("java.lang.System");
        for(String n:new String[]{"java.*","javax.*","jdk.*","sun.*","com.*","org.*","AnimalDeath*","ConstructorJson"}){entry.addClassExclusionFilter(n);exit.addClassExclusionFilter(n);}for(EventRequest r:new EventRequest[]{entry,exit,randomExit,systemExit})r.setSuspendPolicy(EventRequest.SUSPEND_ALL);
        ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("AnimalDeathObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();ExceptionRequest errors=manager.createExceptionRequest(null,true,true);errors.setSuspendPolicy(EventRequest.SUSPEND_ALL);errors.enable();
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(30000);if(set==null)throw new IllegalStateException("JDI event timeout");for(Event event:set){
            if(event instanceof ClassPrepareEvent){ModificationWatchpointRequest r=manager.createModificationWatchpointRequest(((ClassPrepareEvent)event).referenceType().fieldByName("active"));r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
            else if(event instanceof ModificationWatchpointEvent)begin(((IntegerValue)((ModificationWatchpointEvent)event).valueToBe()).value());
            else if(event instanceof MethodEntryEvent&&active>=0){MethodEntryEvent e=(MethodEntryEvent)event;if(originals.contains(e.method().declaringType().name())){pin(e.method());emit(ConstructorJson.map("event","entry","case",active,"location",location(e.location()),"receiver",value(e.thread().frame(0).thisObject()),"arguments",arguments(e.thread().frame(0)),"source_frames",frames(e.thread())));}}
            else if(event instanceof MethodExitEvent&&active>=0){MethodExitEvent e=(MethodExitEvent)event;Method m=e.method();String owner=m.declaringType().name(),name=m.name();StackFrame top=e.thread().frame(0);
                if(originals.contains(owner)){emit(ConstructorJson.map("event","return","case",active,"location",location(e.location()),"result",value(e.returnValue()),"source_frames",frames(e.thread())));if(owner.equals("hl")&&name.equals("<init>")){ObjectReference item=top.thisObject();emit(ConstructorJson.map("event","item-constructor-return","case",active,"item",value(item),"fields",fields(item),"stack_fields",fields((ObjectReference)field(item,"hl","a")),"private_random",value(field(item,"sn","bs")),"private_seed",seed((ObjectReference)field(item,"sn","bs")),"math_seed",seed(shared()),"source_frames",frames(e.thread())));}}
                else if(owner.equals("java.util.Random")&&Set.of("nextInt","nextFloat","nextDouble").contains(name)){ObjectReference r=top.thisObject();emit(ConstructorJson.map("event","random","case",active,"oid",r.uniqueID(),"role",r.equals(shared())?"math":"private","method",name,"descriptor",m.signature(),"arguments",arguments(top),"result",value(e.returnValue()),"seed",seed(r),"source_frames",frames(e.thread())));}
                else if(owner.equals("java.util.Random")&&name.equals("<init>")&&m.signature().equals("()V")){ObjectReference r=top.thisObject();emit(ConstructorJson.map("event","default-random","case",active,"oid",r.uniqueID(),"seed",seed(r),"source_frames",frames(e.thread())));}
                else if(name.equals("seedUniquifier")||name.equals("nanoTime")){boolean selected=name.equals("seedUniquifier");for(StackFrame f:e.thread().frames())if(f.location().declaringType().name().equals("java.util.Random")&&f.location().method().name().equals("<init>")&&f.location().method().signature().equals("()V"))selected=true;if(selected)emit(ConstructorJson.map("event","clock","case",active,"kind",name,"value",String.format(Locale.ROOT,"%016x",((LongValue)e.returnValue()).value()),"source_frames",frames(e.thread())));}
            }else if(event instanceof ExceptionEvent&&active>=0){ExceptionEvent e=(ExceptionEvent)event;List<Object> f=frames(e.thread());if(!f.isEmpty())emit(ConstructorJson.map("event","exception","case",active,"class",e.exception().referenceType().name(),"location",location(e.location()),"source_frames",f));}
            else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException done){alive=false;}}
    }
    public static void main(String[] args)throws Exception{
        if(args.length!=6)throw new IllegalArgumentException("jar classes output jdk-home profiles target-classpath");Path out=Path.of(args[2]).toAbsolutePath();Set<String> names=new HashSet<>();try(ZipFile jar=new ZipFile(args[0])){jar.stream().filter(e->e.getName().endsWith(".class")).forEach(e->names.add(e.getName().substring(0,e.getName().length()-6).replace('/','.')));}
        String q="\"";LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument>a=connector.defaultArguments();a.get("home").setValue(args[3]);a.get("main").setValue("AnimalDeathObservation "+q+args[0]+q+" "+q+args[4]+q);a.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -classpath "+q+args[5]+q);a.get("suspend").setValue("true");VirtualMachine vm=connector.launch(a);Process process=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();
        Thread stdout=drain(process.getInputStream(),out.resolve("target.stdout"),process,failure),stderr=drain(process.getErrorStream(),out.resolve("target.stderr"),process,failure);stdout.start();stderr.start();boolean observed=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(out.resolve("checkpoints.ndjson"),StandardCharsets.UTF_8))){new AnimalDeathJdi(vm,writer,names).run();observed=true;}finally{if(!observed){try{vm.exit(1);}catch(VMDisconnectedException done){}process.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException done){}}
        process.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||process.exitValue()!=0)throw new IllegalStateException("debugged target/drain failure",failure.get());
    }
    static Thread drain(InputStream in,Path path,Process process,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];long count=0;for(int n;(n=in.read(b))>=0;){count+=n;if(count>2L*1024*1024)throw new IOException("target pipe bound");out.write(b,0,n);}}catch(Throwable t){failure.compareAndSet(null,t);process.destroyForcibly();}});}
}
