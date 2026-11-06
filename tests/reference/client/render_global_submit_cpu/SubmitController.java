import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;

/** Read-only JDI observes original instructions and exception stack PCs. */
public final class SubmitController {
    static ReferenceType helper;
    static int[] polls=new int[12];
    static Map<Integer,String> failures=new TreeMap<>();
    static String quote(String value){return "\""+value.replace("\\","\\\\").replace("\"","\\\"")+"\"";}
    static int index(){return helper==null?-1:((IntegerValue)helper.getValue(helper.fieldByName("index"))).value();}
    static void code(ReferenceType type)throws Exception{
        for(Method m:type.methods()){
            boolean wanted=type.name().equals("n")&&m.name().equals("a")&&(m.signature().equals("(Lls;ID)I")||m.signature().equals("()V"))||type.name().equals("dk")&&(m.name().equals("b")||m.name().equals("c"))&&m.signature().equals("()V");
            if(wanted){byte[] data=m.bytecodes();System.out.println("CODE "+type.name()+" "+m.name()+" "+m.signature()+" "+data.length+" "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data)));}
        }
    }
    public static void main(String[] args)throws Exception{
        LaunchingConnector connector=Bootstrap.virtualMachineManager().defaultConnector();Map<String,Connector.Argument> options=connector.defaultArguments();
        options.get("main").setValue("SubmitObservation "+quote(args[1])+" "+quote(args[2]));
        options.get("options").setValue("-Xint -Xverify:all -Xmx256m -Djava.security.manager=allow -classpath "+quote(args[0]));
        VirtualMachine vm=connector.launch(options);Process child=vm.process();ExecutorService streams=Executors.newFixedThreadPool(2);
        Future<byte[]> stdout=streams.submit(()->child.getInputStream().readNBytes(2*1024*1024));Future<byte[]> stderr=streams.submit(()->child.getErrorStream().readNBytes(1024*1024));
        EventRequestManager requests=vm.eventRequestManager();for(String type:new String[]{"SubmitObservation","n","dk"}){ClassPrepareRequest r=requests.createClassPrepareRequest();r.addClassFilter(type);r.setSuspendPolicy(EventRequest.SUSPEND_ALL);r.enable();}
        for(String type:new String[]{"n","dk"}){ExceptionRequest r=requests.createExceptionRequest(null,true,true);r.addClassFilter(type);r.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);r.enable();}
        boolean done=false;long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(45);
        try{
            while(!done){if(System.nanoTime()>deadline)throw new AssertionError("bounded JDI deadline");EventSet events=vm.eventQueue().remove(1000);if(events==null)continue;
                for(Event event:events){
                    if(event instanceof ClassPrepareEvent e){ReferenceType type=e.referenceType();if(type.name().equals("SubmitObservation"))helper=type;else{code(type);if(type.name().equals("n")){Method method=type.methodsByName("a","(Lls;ID)I").get(0);BreakpointRequest r=requests.createBreakpointRequest(method.locationOfCodeIndex(26));r.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);r.enable();}}}
                    else if(event instanceof BreakpointEvent){int i=index();if(i>=0)++polls[i];}
                    else if(event instanceof ExceptionEvent e){int i=index();if(i>=0){List<String> frames=new ArrayList<>();for(StackFrame f:e.thread().frames()){Location l=f.location();String type=l.declaringType().name();if(type.equals("n")||type.equals("dk"))frames.add(type+"."+l.method().name()+l.method().signature()+"@"+l.codeIndex());}if(frames.stream().anyMatch(s->s.startsWith("n.a(Lls;ID)I@"))){Collections.reverse(frames);String value=String.join(">",frames);String old=failures.putIfAbsent(i,value);if(old!=null&&!old.equals(value))throw new AssertionError("different exception chain");}}}
                    else if(event instanceof VMDeathEvent||event instanceof VMDisconnectEvent)done=true;
                }events.resume();
            }
        }catch(VMDisconnectedException e){done=true;}finally{if(!done&&child.isAlive())child.destroyForcibly();}
        if(!child.waitFor(10,TimeUnit.SECONDS)){child.destroyForcibly();throw new AssertionError("original VM did not exit");}
        int exit=child.exitValue();byte[] out=stdout.get(10,TimeUnit.SECONDS),err=stderr.get(10,TimeUnit.SECONDS);streams.shutdownNow();System.out.write(out);System.err.write(err);
        if(exit!=0||failures.size()!=12)throw new AssertionError("original failure capture incomplete");
        for(int i=0;i<12;++i)System.out.println("TRACE "+i+" "+polls[i]+" "+failures.get(i));
        System.out.println("Original submission CPU capture: 12 cases; read-only JDI");
    }
}
