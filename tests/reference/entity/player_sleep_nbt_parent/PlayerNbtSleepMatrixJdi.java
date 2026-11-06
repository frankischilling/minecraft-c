import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Passive source call/store/exception records. No target invocation or write. */
public final class PlayerNbtSleepMatrixJdi {
    final VirtualMachine vm;final PrintWriter output;
    final List<EventRequest> requests=new ArrayList<>();final Map<String,String[]> metadata=new HashMap<>();
    final Set<String> pinned=new HashSet<>();final Map<Long,String> labels=new HashMap<>();
    final List<Map<String,Object>> calls=new ArrayList<>(),events=new ArrayList<>();
    final Map<String,Map<String,Object>> pending=new LinkedHashMap<>();
    ReferenceType helper;ObjectReference actor,world,input;int active=-1,rows,clock,fresh,returned,failed;long bytes;
    boolean entered,rootReturned;String firstException;long sourceThread;
    PlayerNbtSleepMatrixJdi(VirtualMachine v,PrintWriter w,Path pins)throws Exception{vm=v;output=w;for(String line:Files.readAllLines(pins,StandardCharsets.US_ASCII)){String[] p=line.split(" ");if(p.length!=5||metadata.put(p[0]+"."+p[1]+p[2],p)!=null)throw new IllegalStateException("method inventory");}}
    static Map<String,Object> map(Object... a){return PlayerNbtSleepMatrixObservation.map(a);}
    void emit(Object v){String s=PlayerNbtSleepMatrixObservation.json(v);bytes+=s.length()+1;if(bytes>16L*1024*1024)throw new IllegalStateException("journal byte bound");output.println(s);output.flush();if(output.checkError())throw new IllegalStateException("journal write");}
    void event(Map<String,Object> v){if(events.size()>=8192)throw new IllegalStateException("case event bound");v.put("tick",clock++);events.add(v);}
    static Object site(Location l){return map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",(int)l.codeIndex());}
    void pin(Method m)throws Exception{
        String key=m.declaringType().name()+"."+m.name()+m.signature();if(!pinned.add(key))return;
        byte[] b=m.bytecodes();String sha=HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b));String[] p=metadata.get(key);
        if(p==null||Integer.parseInt(p[3])!=b.length||!p[4].equals(sha))throw new IllegalStateException("actual pinned Code "+key);
        emit(map("event","code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",sha));
    }
    void label(Value v,String name){if(v instanceof ObjectReference)labels.putIfAbsent(((ObjectReference)v).uniqueID(),name);}
    Object value(Value v){
        if(v==null)return null;
        if(v instanceof VoidValue)return map("void",true);
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();
        if(v instanceof ByteValue)return (int)((ByteValue)v).value();
        if(v instanceof ShortValue)return (int)((ShortValue)v).value();
        if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof CharValue)return (int)((CharValue)v).value();
        if(v instanceof LongValue)return map("J",String.format(Locale.ROOT,"%016x",((LongValue)v).value()));
        if(v instanceof FloatValue)return map("F",PlayerNbtSleepMatrixObservation.f(((FloatValue)v).value()));
        if(v instanceof DoubleValue)return map("D",PlayerNbtSleepMatrixObservation.d(((DoubleValue)v).value()));
        if(v instanceof StringReference){List<Integer> s=new ArrayList<>();for(char c:((StringReference)v).value().toCharArray())s.add((int)c);return map("class","java.lang.String","utf16",s);}
        ObjectReference o=(ObjectReference)v;String name=labels.computeIfAbsent(o.uniqueID(),n->"new-"+fresh++);
        Map<String,Object> r=map("class",o.referenceType().name(),"identity",name,"raw_object_id",o.uniqueID());
        if(o instanceof ArrayReference)r.put("length",((ArrayReference)o).length());return r;
    }
    String key(Method m,ThreadReference t)throws Exception{return t.uniqueID()+":"+t.frameCount()+":"+m.declaringType().name()+"."+m.name()+m.signature();}
    boolean sourceThread(ThreadReference t){return active>=0&&t.name().equals("original-nbt-matrix-"+active);}
    boolean root(Method m,ThreadReference t)throws Exception{
        if(t.frameCount()<2)return false;Location caller=t.frame(1).location();
        if(!caller.declaringType().name().equals("PlayerNbtSleepMatrixObservation$SourceWorker")||!caller.method().name().equals("run"))return false;
        return active>=34&&active<=37?m.declaringType().name().equals("fd")&&m.name().equals("a")&&m.signature().equals("(Lgs;)V"):
            m.declaringType().name().equals("sn")&&m.name().equals("e")&&m.signature().equals("(Lnu;)V");
    }
    void begin(int number)throws Exception{
        if(active>=0){
            if(!entered)throw new IllegalStateException("source root not entered");
            for(Map<String,Object> call:pending.values())if(call.get("exception")==null)throw new IllegalStateException("unreturned nonthrowing source frame");
            ObjectReference worker=(ObjectReference)helper.getValue(helper.fieldByName("worker"));
            boolean actual=((BooleanValue)worker.getValue(worker.referenceType().fieldByName("returned"))).value();
            ObjectReference failure=(ObjectReference)worker.getValue(worker.referenceType().fieldByName("failure"));
            String actualFailure=failure==null?null:failure.referenceType().name().replace('.','/');
            if(actual!=rootReturned||!Objects.equals(firstException,actualFailure)||actual!=(actualFailure==null))throw new IllegalStateException("actual source/helper outcome disagreement");
            if(actual)returned++;else failed++;
            emit(map("event","journal","case",active,"root_returned",rootReturned,"first_exception",firstException,
                "source_thread_id",sourceThread,"calls",new ArrayList<>(calls),"events",new ArrayList<>(events),
                "jdi_target_invocations",0,"jdi_source_writes",0,"jdi_forced_returns",0));rows++;
        }
        active=number;calls.clear();events.clear();pending.clear();labels.clear();clock=fresh=0;entered=rootReturned=false;firstException=null;sourceThread=0;
        if(number>=0){
            actor=(ObjectReference)helper.getValue(helper.fieldByName("actor"));world=(ObjectReference)helper.getValue(helper.fieldByName("world"));input=(ObjectReference)helper.getValue(helper.fieldByName("input"));
            label(actor,"actor");label(world,"world");label(input,"input");
            for(String name:new String[]{"originalBounds","beforeV","beforeQ","beforeBN","beforeMain","beforeArmor","beforeInfo","beforeList"})label(helper.getValue(helper.fieldByName(name)),"initial-"+name);
        }
        for(EventRequest r:requests)r.setEnabled(number>=0);
    }
    void prepare(ReferenceType type)throws Exception{
        EventRequestManager m=vm.eventRequestManager();String name=type.name();
        if(name.equals("PlayerNbtSleepMatrixObservation")){helper=type;ModificationWatchpointRequest w=m.createModificationWatchpointRequest(type.fieldByName("active"));w.setSuspendPolicy(EventRequest.SUSPEND_ALL);w.enable();return;}
        Set<String> names=switch(name){
            case "sn"->Set.of("aM","aN","aO","aP","aQ","aR","aJ","aK","aL","bl","bm","bn","aS","aT","aU","aV","bk","bv","bz","aX","bf","bg","bh");
            case "ls"->Set.of("Y","aa","ad","ae");case "gs"->Set.of("m","u","a","v","b","g","w","y");
            case "ix"->Set.of("a","b","c","e");case "fd"->Set.of("J");case "br"->Set.of("a","b","c");case "eq"->Set.of("a","b","c","d","e","f");default->Set.of();};
        for(Field f:type.fields())if(names.contains(f.name())||(name.equals("ei")&&f.signature().equals("Lnu;"))){ModificationWatchpointRequest w=m.createModificationWatchpointRequest(f);w.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(w);if(active>=0)w.enable();}
        if(name.equals("gs")){Method read=type.methodsByName("a","(Lnu;)V").get(0);BreakpointRequest b=m.createBreakpointRequest(read.locationOfCodeIndex(93));b.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(b);if(active>=0)b.enable();}
    }
    Object coordinate(Value v){if(v==null)return null;ObjectReference o=(ObjectReference)v;return Arrays.asList(value(o.getValue(o.referenceType().fieldByName("a"))),value(o.getValue(o.referenceType().fieldByName("b"))),value(o.getValue(o.referenceType().fieldByName("c"))));}
    void run(List<String> classes)throws Exception{
        EventRequestManager m=vm.eventRequestManager();ClassPrepareRequest hp=m.createClassPrepareRequest();hp.addClassFilter("PlayerNbtSleepMatrixObservation");hp.setSuspendPolicy(EventRequest.SUSPEND_ALL);hp.enable();
        for(String n:classes){ClassPrepareRequest p=m.createClassPrepareRequest();p.addClassFilter(n);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();}
        MethodEntryRequest entry=m.createMethodEntryRequest();MethodExitRequest exit=m.createMethodExitRequest();
        for(String pattern:new String[]{"java.*","javax.*","jdk.*","sun.*","com.*","org.*","PlayerNbtSleepMatrix*"}){entry.addClassExclusionFilter(pattern);exit.addClassExclusionFilter(pattern);}
        entry.setSuspendPolicy(EventRequest.SUSPEND_ALL);exit.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(entry);requests.add(exit);
        ExceptionRequest exception=m.createExceptionRequest(null,true,true);exception.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(exception);
        boolean alive=true;long deadline=System.nanoTime()+210_000_000_000L;
        while(alive){if(System.nanoTime()>deadline)throw new IllegalStateException("controller source deadline");EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
            for(Event e:set){
                if(e instanceof ClassPrepareEvent)prepare(((ClassPrepareEvent)e).referenceType());
                else if(e instanceof ModificationWatchpointEvent){ModificationWatchpointEvent w=(ModificationWatchpointEvent)e;
                    if(w.field().declaringType().name().equals("PlayerNbtSleepMatrixObservation")){int next=((IntegerValue)w.valueToBe()).value();if(next!=-1||active>=0)begin(next);}
                    else if(entered&&sourceThread(w.thread()))event(map("kind","STORE","site",site(w.location()),"owner",w.field().declaringType().name(),"field",w.field().name(),"receiver",value(w.object()),"before",value(w.valueCurrent()),"value",value(w.valueToBe())));
                }else if(e instanceof MethodEntryEvent&&active>=0){MethodEntryEvent a=(MethodEntryEvent)e;if(!sourceThread(a.thread()))continue;
                    if(!entered){if(!root(a.method(),a.thread()))continue;entered=true;sourceThread=a.thread().uniqueID();pin(a.thread().frame(1).location().method());}
                    if(rootReturned)throw new IllegalStateException("source after actual root return");pin(a.method());
                    List<Object> args=new ArrayList<>();for(Value v:a.thread().frame(0).getArgumentValues())args.add(value(v));
                    Map<String,Object> call=map("id",calls.size(),"caller",site(a.thread().frame(1).location()),"site",site(a.location()),"receiver",value(a.thread().frame(0).thisObject()),"arguments",args,"entered",clock,"returned",null,"completed",false,"exception",null,"result",null);
                    if(calls.size()>=512||pending.put(key(a.method(),a.thread()),call)!=null)throw new IllegalStateException("call bound/frame collision");calls.add(call);event(map("kind","ENTER","call",call.get("id")));
                }else if(e instanceof MethodExitEvent&&entered){MethodExitEvent x=(MethodExitEvent)e;if(!sourceThread(x.thread()))continue;Map<String,Object> call=pending.remove(key(x.method(),x.thread()));if(call!=null){call.put("returned",clock);call.put("completed",true);call.put("result",value(x.returnValue()));event(map("kind","RETURN","call",call.get("id")));if(((Integer)call.get("id"))==0)rootReturned=true;}}
                else if(e instanceof BreakpointEvent&&entered){BreakpointEvent p=(BreakpointEvent)e;if(!sourceThread(p.thread()))continue;ReferenceType gs=actor.referenceType().allFields().stream().filter(f->f.declaringType().name().equals("gs")).findFirst().get().declaringType();
                    event(map("kind","CHECKPOINT93","site",site(p.location()),"receiver",value(p.thread().frame(0).thisObject()),"sleeping",value(actor.getValue(gs.fieldByName("u"))),"timer",value(actor.getValue(gs.fieldByName("a"))),"v",coordinate(actor.getValue(gs.fieldByName("v"))),"Q",coordinate(actor.getValue(gs.fieldByName("b")))));
                }else if(e instanceof ExceptionEvent&&entered){ExceptionEvent x=(ExceptionEvent)e;if(!sourceThread(x.thread())||rootReturned)continue;
                    if(firstException!=null)throw new IllegalStateException("more than first source exception");firstException=x.exception().referenceType().name().replace('.','/');List<Object> stack=new ArrayList<>();for(StackFrame f:x.thread().frames())stack.add(site(f.location()));
                    for(Map<String,Object> call:pending.values())call.put("exception",firstException);
                    event(map("kind","EXCEPTION","site",site(x.location()),"class",firstException,"catch",x.catchLocation()==null?null:site(x.catchLocation()),"stack",stack));
                }else if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)alive=false;
            }
            try{set.resume();}catch(VMDisconnectedException done){alive=false;}
        }
        if(rows!=40||active!=-1||returned+failed!=40)throw new IllegalStateException("completed rows "+rows);
        emit(map("event","complete","profiles",rows,"source_roots_returned",returned,"source_roots_failed",failed,"jdi_target_invocations",0,"jdi_source_writes",0,"jdi_forced_returns",0));
    }
    static Thread drain(InputStream in,Path path,Process p,AtomicReference<Throwable> failure){Thread t=new Thread(()->{try(OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){byte[] b=new byte[8192];long count=0;for(int n;(n=in.read(b))>=0;){count+=n;if(count>16L*1024*1024)throw new IOException("target pipe bound");out.write(b,0,n);}}catch(Throwable x){failure.compareAndSet(null,x);p.destroyForcibly();}});t.setDaemon(true);return t;}
    public static void main(String[] a)throws Exception{
        if(a.length!=7)throw new IllegalArgumentException("jdk classpath archive classnames methodpins profiles output");Path out=Path.of(a[6]);Files.createDirectory(out);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> o=connector.defaultArguments();String q="\"";
        o.get("home").setValue(a[0]);o.get("main").setValue("PlayerNbtSleepMatrixObservation "+q+a[2]+q+" "+q+a[3]+q+" "+q+a[5]+q);
        o.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+q+a[1]+q);o.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(o);Process p=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();Thread stdout=drain(p.getInputStream(),out.resolve("target.stdout"),p,failure),stderr=drain(p.getErrorStream(),out.resolve("target.stderr"),p,failure);stdout.start();stderr.start();boolean complete=false;
        try(PrintWriter w=new PrintWriter(Files.newBufferedWriter(out.resolve("journal.jsonl"),StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW))){new PlayerNbtSleepMatrixJdi(vm,w,Path.of(a[4])).run(Files.readAllLines(Path.of(a[3]),StandardCharsets.US_ASCII));complete=true;}
        finally{if(!complete){try{vm.exit(1);}catch(VMDisconnectedException ignored){}p.destroyForcibly();}if(!p.waitFor(10,TimeUnit.SECONDS)){p.destroyForcibly();if(!p.waitFor(10,TimeUnit.SECONDS))throw new IllegalStateException("target not reaped");}stdout.join(5000);stderr.join(5000);}
        if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||p.exitValue()!=0)throw new IllegalStateException("target/drain",failure.get());
        System.out.println("Passive NBT matrix: forty actual calls, target reaped, no source invocation/write/forced return");
    }
}
