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

/** Read-only calls, field stores and exception observations. No target invocation/write. */
public final class BedNightJdi {
    final VirtualMachine vm;final PrintWriter out;
    final List<EventRequest> requests=new ArrayList<>();final Set<String> pins=new HashSet<>();
    final List<Map<String,Object>> calls=new ArrayList<>(),events=new ArrayList<>();
    final Map<String,Map<String,Object>> pending=new LinkedHashMap<>();final Map<Long,String> labels=new HashMap<>();
    ReferenceType helper,cw;ObjectReference actor,world,bounds;
    int active=-1,rows,clock,fresh;long bytes;boolean cold;
    BedNightJdi(VirtualMachine v,PrintWriter o){vm=v;out=o;}
    static Map<String,Object> map(Object... x){return BedNightObservation.map(x);}
    void emit(Object v){String s=BedNightObservation.json(v);bytes+=s.length()+1;if(bytes>16L*1024*1024)throw new IllegalStateException("journal stream bound");out.println(s);out.flush();if(out.checkError())throw new IllegalStateException("journal output");}
    void event(Map<String,Object> v){if(events.size()>=2048)throw new IllegalStateException("passive event extent");v.put("tick",clock++);events.add(v);}
    static Object site(Location l){return map("class",l.declaringType().name(),"method",l.method().name(),"descriptor",l.method().signature(),"pc",(int)l.codeIndex());}
    void pin(Method m)throws Exception{String k=m.declaringType().name()+"."+m.name()+m.signature();if(!pins.add(k))return;byte[] b=m.bytecodes();emit(map("event","code","class",m.declaringType().name(),"method",m.name(),"descriptor",m.signature(),"bytes",b.length,"sha256",HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b))));}
    void label(Value v,String name){if(v instanceof ObjectReference)labels.putIfAbsent(((ObjectReference)v).uniqueID(),name);}
    Object value(Value v)throws Exception{
        if(v==null||v instanceof VoidValue)return null;
        if(v instanceof BooleanValue)return ((BooleanValue)v).value();if(v instanceof IntegerValue)return ((IntegerValue)v).value();
        if(v instanceof FloatValue)return map("F",String.format(Locale.ROOT,"%08x",Float.floatToRawIntBits(((FloatValue)v).value())));
        if(v instanceof DoubleValue)return map("D",String.format(Locale.ROOT,"%016x",Double.doubleToRawLongBits(((DoubleValue)v).value())));
        if(v instanceof StringReference){List<Integer> units=new ArrayList<>();for(char c:((StringReference)v).value().toCharArray())units.add((int)c);return map("utf16",units);}
        ObjectReference r=(ObjectReference)v;String type=r.referenceType().name();String name=labels.get(r.uniqueID());
        if(name==null){name=type+"-"+fresh++;labels.put(r.uniqueID(),name);}
        Map<String,Object> result=map("class",type,"identity",name,"raw_object_id",r.uniqueID());
        if(type.equals("cw")){for(String f:new String[]{"a","b","c","d","e"})if(r.equals(r.referenceType().getValue(r.referenceType().fieldByName(f))))result.put("static",f);}
        return result;
    }
    String key(Method m,ThreadReference t)throws Exception{return t.uniqueID()+":"+t.frameCount()+":"+m.declaringType().name()+m.name()+m.signature();}
    boolean root(Method m){return m.declaringType().name().equals("gs")&&((m.name().equals("b")&&m.signature().equals("(III)Lcw;"))||(m.name().equals("a")&&m.signature().equals("(ZZZ)V")));}
    boolean selected(Method m,ThreadReference t)throws Exception{
        if(t.frameCount()<2)return false;Location p=t.frame(1).location();String c=p.declaringType().name(),s=p.method().signature(),n=m.name(),d=m.signature(),o=m.declaringType().name();
        if(root(m)&&c.equals("BedNightObservation"))return true;
        if(o.equals("cw")&&(n.equals("<clinit>")||n.equals("<init>")))return true;
        if(c.equals("gs")&&(s.equals("(III)Lcw;")||s.equals("(ZZZ)V")))return
            (o.equals("sn")&&((n.equals("b")&&d.equals("(FF)V"))||(n.equals("e")&&d.equals("(DDD)V"))))||
            (o.equals("ls")&&((n.equals("b")&&d.equals("(FF)V"))||(n.equals("W")&&d.equals("()Z"))))||
            (o.equals("fd")&&(Set.of("i","e","a","y").contains(n)||(n.equals("f")&&d.equals("()Z"))))||
            (o.equals("ve")&&Set.of("a","d","f").contains(n))||
            (o.equals("gs")&&(n.equals("e")||n.equals("E")||(n.equals("N")&&d.equals("()Z"))||(n.equals("a")&&d.equals("(Lbr;)V"))))||o.equals("br");
        if(c.equals("ls")&&p.method().name().equals("b")&&s.equals("(FF)V")&&o.equals("sn")&&n.equals("b")&&d.equals("(FF)V"))return true;
        if(c.equals("gs")&&s.equals("(Lbr;)V")&&o.equals("br"))return true;
        if(c.equals("sn")&&s.equals("(DDD)V")&&o.equals("eq")&&n.equals("c")&&d.equals("(DDDDDD)Leq;"))return true;
        if(c.equals("fd")&&p.method().name().equals("y")&&o.equals("gs")&&n.equals("N"))return true;
        if(c.equals("ve")&&((s.equals("(Lfd;IIIZ)V"))||s.equals("(Lfd;IIII)Lbr;")))return o.equals("fd")||o.equals("br")||(o.equals("ve")&&n.equals("d"));
        return false;
    }
    void begin(int next)throws Exception{
        if(active>=0){for(Map<String,Object> c:pending.values())if(c.get("exception")==null)throw new IllegalStateException("unreturned nonexception call");emit(map("event","journal","case",active,"cw_before_initialized",!cold,"cw_after_initialized",cw!=null&&cw.isInitialized(),"calls",new ArrayList<>(calls),"events",new ArrayList<>(events)));++rows;}
        active=next;calls.clear();events.clear();pending.clear();labels.clear();clock=0;fresh=0;
        if(next>=0){actor=(ObjectReference)helper.getValue(helper.fieldByName("actor"));world=(ObjectReference)helper.getValue(helper.fieldByName("world"));bounds=(ObjectReference)helper.getValue(helper.fieldByName("originalBounds"));label(actor,"actor");label(world,"world");label(bounds,"initial-bounds");label(helper.getValue(helper.fieldByName("other")),"other");label(helper.getValue(helper.fieldByName("beforeV")),"initial-v");label(helper.getValue(helper.fieldByName("beforeQ")),"initial-Q");label(helper.getValue(helper.fieldByName("beforeBN")),"initial-private-bN");cold=cw==null||!cw.isInitialized();}
        for(EventRequest r:requests)r.setEnabled(next>=0);
    }
    void prepare(ReferenceType type)throws Exception{
        EventRequestManager m=vm.eventRequestManager();String name=type.name();
        if(name.equals("BedNightObservation")){helper=type;ModificationWatchpointRequest w=m.createModificationWatchpointRequest(type.fieldByName("active"));w.setSuspendPolicy(EventRequest.SUSPEND_ALL);w.enable();return;}
        if(name.equals("cw"))cw=type;
        Map<String,Set<String>> fields=Map.of("sn",Set.of("aM","aN","aO","aP","aQ","aR","bf","bg","bh"),"gs",Set.of("u","a","v","w","y","b"),"fd",Set.of("J"),"br",Set.of("a","b","c"),"eq",Set.of("a","b","c","d","e","f"),"cw",Set.of("a","b","c","d","e","f"));
        for(Field f:type.fields())if(fields.getOrDefault(name,Set.of()).contains(f.name())){ModificationWatchpointRequest r=m.createModificationWatchpointRequest(f);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(r);if(active>=0)r.enable();}
    }
    void run(int count)throws Exception{
        EventRequestManager m=vm.eventRequestManager();for(String n:new String[]{"BedNightObservation","sn","gs","fd","br","eq","cw"}){ClassPrepareRequest p=m.createClassPrepareRequest();p.addClassFilter(n);p.setSuspendPolicy(EventRequest.SUSPEND_ALL);p.enable();}
        for(String n:new String[]{"sn","ls","gs","fd","ve","br","eq","cw"}){MethodEntryRequest e=m.createMethodEntryRequest();e.addClassFilter(n);e.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(e);MethodExitRequest x=m.createMethodExitRequest();x.addClassFilter(n);x.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(x);}
        for(String name:new String[]{"gs","sn"}){ExceptionRequest e=m.createExceptionRequest(null,true,true);e.addClassFilter(name);e.setSuspendPolicy(EventRequest.SUSPEND_ALL);requests.add(e);}
        boolean alive=true;while(alive){EventSet set=vm.eventQueue().remove(30000);if(set==null)throw new IllegalStateException("event timeout");for(Event e:set){
            if(e instanceof ClassPrepareEvent)prepare(((ClassPrepareEvent)e).referenceType());
            else if(e instanceof ModificationWatchpointEvent){ModificationWatchpointEvent w=(ModificationWatchpointEvent)e;String owner=w.field().declaringType().name();if(owner.equals("BedNightObservation"))begin(((IntegerValue)w.valueToBe()).value());else if(active>=0){ObjectReference o=w.object();boolean take=owner.equals("cw")||owner.equals("br")||(owner.equals("fd")&&o.equals(world))||(owner.equals("eq")&&o.equals(bounds))||((owner.equals("sn")||owner.equals("gs"))&&o.equals(actor));if(take)event(map("kind","STORE","site",site(w.location()),"owner",owner,"field",w.field().name(),"receiver",value(o),"before",value(w.valueCurrent()),"value",value(w.valueToBe())));}}
            else if(e instanceof MethodEntryEvent&&active>=0){MethodEntryEvent in=(MethodEntryEvent)e;if(selected(in.method(),in.thread())){pin(in.method());List<Object> args=new ArrayList<>();for(Value v:in.thread().frame(0).getArgumentValues())args.add(value(v));Object caller=in.thread().frameCount()>1?site(in.thread().frame(1).location()):null;Map<String,Object> c=map("id",calls.size(),"caller",caller,"site",site(in.location()),"receiver",value(in.thread().frame(0).thisObject()),"arguments",args,"entered",clock,"returned",null,"completed",false,"exception",null,"result",null);if(calls.size()>=256||pending.put(key(in.method(),in.thread()),c)!=null)throw new IllegalStateException("call extent/frame");calls.add(c);event(map("kind","ENTER","call",c.get("id")));}}
            else if(e instanceof MethodExitEvent&&active>=0){MethodExitEvent x=(MethodExitEvent)e;Map<String,Object> c=pending.remove(key(x.method(),x.thread()));if(c!=null){c.put("returned",clock);c.put("completed",true);c.put("result",value(x.returnValue()));event(map("kind","RETURN","call",c.get("id")));}}
            else if(e instanceof ExceptionEvent&&active>=0){ExceptionEvent x=(ExceptionEvent)e;String name=x.exception().referenceType().name().replace('.','/');for(Map<String,Object> c:pending.values())c.put("exception",name);event(map("kind","EXCEPTION","site",site(x.location()),"class",name));}
            else if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)alive=false;
        }try{set.resume();}catch(VMDisconnectedException done){alive=false;}}
        if(rows!=count||active!=-1)throw new IllegalStateException("complete rows "+rows);
    }
    public static void main(String[] a)throws Exception{
        if(a.length!=7)throw new IllegalArgumentException("jdk classpath jar profiles classnames output count");Path output=Path.of(a[5]);Files.createDirectories(output);
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> o=connector.defaultArguments();String q="\"";o.get("home").setValue(a[0]);o.get("main").setValue("BedNightObservation "+q+a[2]+q+" "+q+a[3]+q+" "+q+a[4]+q);o.get("options").setValue("-Xint -Xverify:all -Xmx512m -Djava.awt.headless=true -Djava.security.manager=allow --add-opens=java.base/java.util=ALL-UNNAMED -classpath "+q+a[1]+q);o.get("suspend").setValue("true");
        VirtualMachine vm=connector.launch(o);Process p=vm.process();AtomicReference<Throwable> failure=new AtomicReference<>();Thread stdout=drain(p.getInputStream(),output.resolve("target.stdout"),p,failure),stderr=drain(p.getErrorStream(),output.resolve("target.stderr"),p,failure);stdout.start();stderr.start();boolean done=false;
        try(PrintWriter w=new PrintWriter(Files.newBufferedWriter(output.resolve("journal.jsonl"),StandardCharsets.UTF_8))){new BedNightJdi(vm,w).run(Integer.parseInt(a[6]));done=true;}
        finally{if(!done){try{vm.exit(1);}catch(VMDisconnectedException ignored){}p.destroyForcibly();}else try{vm.dispose();}catch(VMDisconnectedException ignored){}}
        p.waitFor();stdout.join(5000);stderr.join(5000);if(stdout.isAlive()||stderr.isAlive()||failure.get()!=null||p.exitValue()!=0)throw new IllegalStateException("target/drain",failure.get());System.out.println("Read-only Bed night JDI: "+a[6]+" calls");
    }
    static Thread drain(InputStream in,Path path,Process p,AtomicReference<Throwable> failure){return new Thread(()->{try(OutputStream out=Files.newOutputStream(path)){byte[] b=new byte[8192];long bytes=0;for(int n;(n=in.read(b))>=0;){bytes+=n;if(bytes>16L*1024*1024)throw new IOException("pipe extent");out.write(b,0,n);}}catch(Throwable e){failure.compareAndSet(null,e);p.destroyForcibly();}});}
}
