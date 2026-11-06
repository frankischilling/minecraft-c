import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import org.lwjgl.opengl.Display;
/** Authored outer caller; exact frozen Target bytes are not changed. */
public final class WindowsBurningLauncher {
 static String hash(byte[]b)throws Exception{return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(b));}
 public static void main(String[]args)throws Exception{
  Path output=Path.of(args[0]);boolean late=false;
  try{BurningBillboardTarget.main(new String[]{args[0],args[1]});throw new AssertionError("Expected frozen Linux-only map export failure was absent");}
  catch(NoSuchFileException e){
   String file=e.getFile().replace('\\','/');if(!file.endsWith("/proc/self/maps"))throw e;
   List<String>lines=Files.readAllLines(output.resolve("target.ndjson"));int count=0;
   for(String line:lines)if(line.contains("\"event\":\"draw\"")){
    if(!line.contains("\"index\":"+count+",")||!line.contains("\"outcome\":1,")||!line.contains("\"gl_error\":0"))throw new AssertionError("Finite source draw did not return cleanly: "+count);
    for(String prefix:new String[]{"baseline","draw"})if(Files.size(output.resolve(prefix+count+".rgba"))!=256*256*4)throw new AssertionError("Missing complete raw backbuffer");
    count++;
   }
   if(count!=7)throw new AssertionError("Expected seven already-completed source calls");late=true;
   try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output.resolve("late-metadata-failure.json")))){
    out.println(ConstructorJson.json(ConstructorJson.map("event","authored_late_metadata_failure","class",e.getClass().getName(),"message",e.getMessage(),"file",e.getFile(),"source_calls_already_returned",count,"raw_frames_already_written",14,"target_main_returned",false,"display_created_after_target_finally",Display.isCreated())));
   }
   try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output.resolve("late-metadata-failure.stacktrace")))){e.printStackTrace(out);}
  }
  if(!late||Display.isCreated())throw new AssertionError("Actual Target cleanup must have destroyed the SDK display");
  int binds=0;try(PrintWriter out=new PrintWriter(Files.newBufferedWriter(output.resolve("original-bindings.ndjson")))){
   for(String name:Files.readAllLines(Path.of(args[2]))){Class<?> c=Class.forName(name.replace('/','.'),false,BurningBillboardTarget.class.getClassLoader());byte[]bytes;try(InputStream stream=c.getResourceAsStream("/"+name+".class")){bytes=stream.readAllBytes();}out.println(ConstructorJson.json(ConstructorJson.map("class",name,"origin",c.getProtectionDomain().getCodeSource().getLocation().toString(),"bytes",bytes.length,"sha256",hash(bytes))));binds++;}
  }
  if(binds!=747)throw new AssertionError("Actual complete origin inventory");
  System.out.println("WINDOWS_FINITE_ORDINARY_OBSERVATION 7 calls;14 frames;747 origins;late authored metadata failure retained;SDK destroyed");
 }
}
