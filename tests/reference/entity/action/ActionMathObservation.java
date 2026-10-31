import java.util.Locale;
public final class ActionMathObservation {
 static final long[] EDGES={0,0x8000000000000000L,1,0x8000000000000001L,0x000fffffffffffffL,0x0010000000000000L,
  0x3e1fffffffffffffL,0x3e20000000000000L,0x3fdbffffffffffffL,0x3fdc000000000000L,0x3fe5ffffffffffffL,0x3fe6000000000000L,
  0x3ff2ffffffffffffL,0x3ff3000000000000L,0x40037fffffffffffL,0x4003800000000000L,0x440fffffffffffffL,0x4410000000000000L,
  0x7fefffffffffffffL,0xffefffffffffffffL,0x7ff0000000000000L,0xfff0000000000000L,0x7ff8123456789abcL,0xfff8456789abcdefL,
  0x7ff0123456789abcL,0xfff0456789abcdefL};
 static final int[] ANGLES={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x43b3ffff,0x43b40000,0x43b40001,0xc3b40000,
  0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0xffc23456,0x7f812345,0xff823456};
 public static void main(String[] args){
  Locale.setDefault(Locale.ROOT);long state=0x910d334a5b67c8efL;
  for(int i=0;i<8192;++i){
   state^=state<<13;state^=state>>>7;state^=state<<17;
   long bits=i<EDGES.length?EDGES[i]:state;double value=Double.longBitsToDouble(bits);
   long result=Double.doubleToRawLongBits(Math.atan(value));
   if(result!=Double.doubleToRawLongBits(StrictMath.atan(value)))throw new AssertionError("actual Math/StrictMath differ");
   System.out.printf("ATAN %d %016x %016x%n",i,bits,result);
   int fb=i<ANGLES.length?ANGLES[i]:(int)(state>>>32);float f=Float.intBitsToFloat(fb);
   System.out.printf("ANGLE %d %08x %08x%n",i,fb,Float.floatToRawIntBits(f%360.0f));
  }
  System.out.println("Action math: 16384 records");
 }
}
