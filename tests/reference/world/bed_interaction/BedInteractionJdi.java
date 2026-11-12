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

/** Private read-only caller/return journal; no target calls, stores or redefine. */
public final class BedInteractionJdi {
    final VirtualMachine vm;final PrintWriter output;
    final List<EventRequest> requests=new ArrayList<>();
    final List<Map<String,Object>> journal=new ArrayList<>();
    final Map<String,Map<String,Object>> pending=new HashMap<>();
    final Set<String> pinned=new HashSet<>();int active=-1,rows,clock;long bytes;
    BedInteractionJdi(VirtualMachine v,PrintWriter o){vm=v;output=o;}
    void emit(Map<String,Object> v){String text=BedInteractionObservation.json(v);bytes+=text.length()+1;if(bytes>4*1024*1024)throw new IllegalStateException("journal bytes");output.println(text);output.flush();if(output.checkError())throw new IllegalStateException("journal output");}
    void pin(Method m)throws Exception{String k=m.declaringType().name()+"."+m.name()+m.signature();if(pinned.add(k)){byte[] b=m.bytecodes();emit(BedInteractionObservation.map("event","code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b))));}}
    void begin(int next){if(active>=0){if(!pending.isEmpty())throw new IllegalStateException("unfinished selected calls");emit(BedInteractionObservation.map("event","journal","case",active,"calls",new ArrayList<>(journal)));++rows;}active=next;clock=0;journal.clear();for(EventRequest r:requests)r.setEnabled(next>=0);}
    static String utf16(String s){StringBuilder r=new StringBuilder();for(int i=0;i<s.length();++i)r.append(String.format(Locale.ROOT,"%04x",(int)s.charAt(i)));return r.toString();}
    Object value(Value v)throws Exception {
        if(v==null||v instanceof VoidValue)return null;
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof FloatValue)return String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value()));
        if(v instanceof StringReference)return BedInteractionObservation.map("utf16",utf16(((StringReference)v).value()));
        if(v instanceof ObjectReference){ObjectReference o=(ObjectReference)v;ReferenceType t=o.referenceType();if(t.name().equals("cw"))for(String n:new String[]{"a","b","c","d","e"}){Value current=t.getValue(t.fieldByName(n));if(current instanceof ObjectReference&&((ObjectReference)current).uniqueID()==o.uniqueID())return "cw."+n;}return t.name();}
        throw new IllegalStateException("unhandled selected value "+v);
    }
    Map<String,Object> selected(Method m,ThreadReference thread)throws Exception {
        if(thread.frameCount()<2)return null;Location caller=thread.frame(1).location();
        String owner=caller.declaringType().name(),parent=caller.method().signature(),type=m.declaringType().name(),name=m.name(),sig=m.signature();int pc=(int)caller.codeIndex();
        boolean take=owner.equals("ve")&&((type.equals("fd")&&Set.of("a","e","f","d").contains(name))||(type.equals("xa")&&name.equals("f")&&sig.equals("()Z"))||(type.equals("uu")&&name.equals("g"))||(type.equals("gs")&&name.equals("b")&&sig.equals("(III)Lcw;"))||(type.equals("dc")&&name.equals("b")&&sig.equals("(Ljava/lang/String;)V")));
        take|=owner.equals("gs")&&parent.equals("(III)Lcw;")&&type.equals("fd")&&name.equals("f")&&sig.equals("()Z");
        take|=owner.equals("dc")&&parent.equals("(Ljava/lang/String;)V")&&type.equals("uq")&&name.equals("c");
        take|=owner.equals("uq")&&parent.equals("(Ljava/lang/String;)V")&&((type.equals("nh")&&name.equals("a"))||(type.equals("uq")&&name.equals("a")));
        take|=owner.equals("uu")&&type.equals("java.util.Random")&&name.equals("nextFloat");
        take|=owner.equals("uu")&&type.equals("fd")&&name.equals("b")&&sig.equals("(Lsn;)Z");
        if(!take)return null;
        List<Object> args=new ArrayList<>();for(Value arg:thread.frame(0).getArgumentValues())args.add(value(arg));
        return BedInteractionObservation.map("caller",owner,"caller_method",caller.method().name(),"caller_descriptor",parent,"pc",pc,"class",type,"method",name,"descriptor",sig,"arguments",args,"entered",clock++,"returned",null,"completed",false,"result",null);
    }
    String key(Method m,ThreadReference t)throws Exception{return t.uniqueID()+":"+t.frameCount()+":"+m.declaringType().name()+m.name()+m.signature();}
    void run()throws Exception {
        EventRequestManager manager=vm.eventRequestManager();for(String name:new String[]{"fd","ve","uu","gs","dc","xa","uq","nh","java.util.Random"}){
            MethodEntryRequest in=manager.createMethodEntryRequest();in.addClassFilter(name);in.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(in);
            MethodExitRequest out=manager.createMethodExitRequest();out.addClassFilter(name);out.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(out);
        }
        ClassPrepareRequest prepare=manager.createClassPrepareRequest();prepare.addClassFilter("BedInteractionObservation");prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL);prepare.enable();
        boolean alive=true;while(alive){EventSet events=vm.eventQueue().remove(30000);if(events==null)throw new IllegalStateException("event timeout");for(Event e:events){
            if(e instanceof ClassPrepareEvent){ModificationWatchpointRequest watch=manager.createModificationWatchpointRequest(((ClassPrepareEvent)e).referenceType().fieldByName("active"));watch.setSuspendPolicy(EventRequest.SUSPEND_ALL);watch.enable();}
            else if(e instanceof ModificationWatchpointEvent)begin(((IntegerValue)((ModificationWatchpointEvent)e).valueToBe()).value());
            else if(e instanceof MethodEntryEvent&&active>=0){MethodEntryEvent in=(MethodEntryEvent)e;Map<String,Object> q=selected(in.method(),in.thread());if(q!=null){pin(in.method());if(journal.size()>=256)throw new IllegalStateException("journal calls");journal.add(q);if(pending.put(key(in.method(),in.thread()),q)!=null)throw new IllegalStateException("duplicate selected frame");}else if(in.method().declaringType().name().equals("ve")&&Set.of("a","b").contains(in.method().name()))pin(in.method());}
            else if(e instanceof MethodExitEvent&&active>=0){MethodExitEvent out=(MethodExitEvent)e;Map<String,Object> q=pending.remove(key(out.method(),out.thread()));if(q!=null){q.put("result",value(out.returnValue()));q.put("returned",clock++);q.put("completed",true);}}
            else if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)alive=false;
        }try{events.resume();}catch(VMDisconnectedException done){alive=false;}}
        if(active!=-1||rows!=85)throw new IllegalStateException("complete selected cases");
    }
    public static void main(String[] args)throws Exception {
        if(args.length!=4)throw new IllegalArgumentException("jdk-home classpath jar output");Path output=Path.of(args[3]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> a=connector.defaultArguments();String q="\"";
        a.get("home").setValue(args[0]);a.get("main").setValue("BedInteractionObservation "+q+args[2]+q);a.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+q+args[1]+q);a.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(a);Process process=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();Thread stdout=drain(process.getInputStream(),output.resolve("target.stdout"),process,failure),stderr=drain(process.getErrorStream(),output.resolve("target.stderr"),process,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter writer=new PrintWriter(Files.newBufferedWriter(output.resolve("calls.jsonl"),StandardCharsets.UTF_8))){new BedInteractionJdi(vm,writer).run();done=true;}finally{if(!done)process.destroyForcibly();try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        process.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||process.exitValue()!=0)throw new IllegalStateException("target/process capture",failure.get());System.out.println("Read-only Bed interaction JDI:85 journals");
    }
    static Thread drain(InputStream in,Path p,Process process,AtomicReference<Throwable> fail){return new Thread(()->{try(OutputStream out=Files.newOutputStream(p)){byte[] b=new byte[8192];long total=0;for(int n;(n=in.read(b))>=0;){total+=n;if(total>4*1024*1024)throw new IOException("pipe bytes");out.write(b,0,n);}}catch(Throwable e){fail.compareAndSet(null,e);process.destroyForcibly();}});}
}
