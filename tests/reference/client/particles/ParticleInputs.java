import com.sun.jdi.*;
import com.sun.jdi.connect.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;

/** Read-only JDI: observes actual default constructor U and nanoTime returns. */
public final class ParticleInputs {
    static String quote(String value) {
        if(value.indexOf('"')>=0||value.indexOf('\n')>=0||value.indexOf('\r')>=0)
            throw new IllegalArgumentException("unsupported quote/newline in observation path");
        return "\""+value+"\"";
    }
    static final class Drain extends Thread {
        final InputStream input;final Path path; volatile Throwable error;long count;
        Drain(InputStream in,Path p){input=in;path=p;setDaemon(true);}
        public void run(){try(InputStream in=input;OutputStream out=Files.newOutputStream(path,StandardOpenOption.CREATE_NEW)){
            byte[] b=new byte[4096];int n;while((n=in.read(b))!=-1){count+=n;if(count>1024*1024)throw new IOException("output bound");out.write(b,0,n);}
        }catch(Throwable t){error=t;}}
    }
    static Throwable append(Throwable first,Throwable next) {
        if(first==null)return next;
        if(first!=next)first.addSuppressed(next);
        return first;
    }
    static void closeUnstarted(Drain drain,InputStream fallback) throws IOException {
        if(drain==null||drain.getState()==Thread.State.NEW)fallback.close();
    }
    static void joinStarted(Drain drain) throws InterruptedException {
        if(drain==null||drain.getState()==Thread.State.NEW)return;
        drain.join(10000);
        if(drain.isAlive())throw new AssertionError("target reader not joined");
    }
    static void retire(VirtualMachine vm,Process process,Drain out,Drain err) throws Exception {
        Throwable failure=null;
        if(process!=null) {
            try {
                if(process.isAlive())process.destroyForcibly();
                if(!process.waitFor(10,TimeUnit.SECONDS))throw new AssertionError("target cleanup not reaped");
            }catch(Throwable problem){failure=append(failure,problem);}
            try{process.getOutputStream().close();}catch(Throwable problem){failure=append(failure,problem);}
            try{closeUnstarted(out,process.getInputStream());}catch(Throwable problem){failure=append(failure,problem);}
            try{closeUnstarted(err,process.getErrorStream());}catch(Throwable problem){failure=append(failure,problem);}
            try{joinStarted(out);}catch(Throwable problem){failure=append(failure,problem);}
            try{joinStarted(err);}catch(Throwable problem){failure=append(failure,problem);}
        } else {
            try{vm.exit(1);}catch(VMDisconnectedException expected){}
            catch(Throwable problem){failure=append(failure,problem);}
        }
        try{vm.dispose();}catch(VMDisconnectedException expected){}
        catch(Throwable problem){failure=append(failure,problem);}
        if(failure instanceof Exception)throw (Exception)failure;
        if(failure instanceof Error)throw (Error)failure;
    }
    public static void main(String[] args) throws Exception {
        if(args.length!=6)throw new IllegalArgumentException("classpath stdout stderr jar origins target-metadata");
        LaunchingConnector c=Bootstrap.virtualMachineManager().defaultConnector();
        Map<String,Connector.Argument> a=c.defaultArguments();
        a.get("main").setValue("ParticleObservation "+quote(args[3])+" "+quote(args[4]));
        a.get("options").setValue("-Xint -Xverify:all --add-opens=java.base/java.util=ALL-UNNAMED --add-opens=java.base/java.lang=ALL-UNNAMED -cp "+quote(args[0]));
        Process p=null;Drain out=null,err=null;Throwable primary=null;
        VirtualMachine vm=c.launch(a);
        try {
        p=vm.process();
        Files.writeString(Path.of(args[5]),p.pid()+"\t"+ProcessHandle.current().pid()+"\t"+
            p.toHandle().info().startInstant().orElseThrow()+"\n",StandardOpenOption.CREATE_NEW);
        out=new Drain(p.getInputStream(),Path.of(args[1]));
        err=new Drain(p.getErrorStream(),Path.of(args[2]));out.start();err.start();
        EventRequestManager requests=vm.eventRequestManager();
        MethodEntryRequest entry=requests.createMethodEntryRequest();entry.addClassFilter("java.util.Random");entry.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);entry.enable();
        MethodExitRequest exit=requests.createMethodExitRequest();exit.addClassFilter("java.util.Random");exit.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);exit.enable();
        MethodExitRequest clock=requests.createMethodExitRequest();clock.addClassFilter("java.lang.System");clock.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        long current=8682522807148012L,committed=0;int phase=0,count=0,events=0,floats=0,doubles=0,longs=0;boolean done=false;
        long deadline=System.nanoTime()+TimeUnit.SECONDS.toNanos(90);
            if(!vm.canGetMethodReturnValues())throw new AssertionError("return values unavailable");
            while(!done){
                if(System.nanoTime()>deadline||events>10000||out.error!=null||err.error!=null)throw new AssertionError("observer bound/reader failure");
                EventSet set=vm.eventQueue().remove(1000);if(set==null)continue;
                for(Event e:set){events++;
                    if(e instanceof MethodEntryEvent){Method m=((MethodEntryEvent)e).method();if(m.name().equals("<init>")&&m.signature().equals("()V")){if(phase!=0)throw new AssertionError("nested default");phase=1;}}
                    if(e instanceof MethodExitEvent){MethodExitEvent x=(MethodExitEvent)e;String n=x.method().name();
                        if(n.equals("nextFloat"))floats++;if(n.equals("nextDouble"))doubles++;if(n.equals("nextLong"))longs++;
                        if(n.equals("seedUniquifier")){if(phase!=1)throw new AssertionError("U order");committed=((LongValue)x.returnValue()).value();current*=1181783497276652981L;if(committed!=current)throw new AssertionError("fresh U progression");phase=2;clock.enable();}
                        else if(n.equals("nanoTime")&&x.method().declaringType().name().equals("java.lang.System")){if(phase!=2)throw new AssertionError("clock order");long nano=((LongValue)x.returnValue()).value();clock.disable();System.out.println("input\t"+Long.toUnsignedString(committed,16)+"\t"+Long.toUnsignedString(nano,16));phase=3;count++;if(count>64)throw new AssertionError("constructor bound");}
                        else if(n.equals("<init>")&&x.method().signature().equals("()V")){if(phase!=3)throw new AssertionError("constructor exit");phase=0;}
                    }
                    if(e instanceof VMDeathEvent||e instanceof VMDisconnectEvent)done=true;
                }
                try{set.resume();}catch(VMDisconnectedException expected){done=true;}
            }
            if(!p.waitFor(10,TimeUnit.SECONDS))throw new AssertionError("target not reaped");out.join(10000);err.join(10000);
            if(out.isAlive()||err.isAlive()||out.error!=null||err.error!=null||p.exitValue()!=0||err.count!=0||phase!=0||count!=14||floats!=48||doubles!=58||longs!=1)throw new AssertionError("target/trace incomplete: "+count+" phase "+phase+" exit "+p.exitValue());
            System.out.println("Draws: "+floats+" floats, "+doubles+" doubles, "+longs+" longs");System.out.println("Inputs: "+count+" constructors");if(System.out.checkError())throw new IOException("observer stdout");
        }catch(Throwable problem){primary=problem;throw problem;}
        finally{
            try{retire(vm,p,out,err);}catch(Throwable cleanup){
                if(primary!=null)primary.addSuppressed(cleanup);
                else if(cleanup instanceof Exception)throw (Exception)cleanup;
                else throw (Error)cleanup;
            }
        }
    }
}
