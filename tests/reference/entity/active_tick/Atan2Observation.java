public final class Atan2Observation {
    static final long[] EDGE={0,0x8000000000000000L,1,0x8000000000000001L,
        0x000fffffffffffffL,0x0010000000000000L,0x3c30000000000000L,0xbc30000000000000L,
        0x3ff0000000000000L,0xbff0000000000000L,0x3fe0000000000000L,0xbfe0000000000000L,
        0x43b0000000000000L,0xc3b0000000000000L,0x7fefffffffffffffL,0xffefffffffffffffL,
        0x7ff0000000000000L,0xfff0000000000000L,0x7ff8123456789abcL,0xfff8456789abcdefL,
        0x7ff0123456789abcL,0xfff0456789abcdefL};
    static void row(int i,long yb,long xb){
        double y=Double.longBitsToDouble(yb),x=Double.longBitsToDouble(xb);
        long result=Double.doubleToRawLongBits(Math.atan2(y,x));
        if(result!=Double.doubleToRawLongBits(StrictMath.atan2(y,x)))throw new AssertionError("JDK Math/StrictMath differs "+i);
        System.out.printf("ATAN2 %d %016x %016x %016x%n",i,yb,xb,result);
    }
    public static void main(String[] args){
        int count=0;for(long y:EDGE)for(long x:EDGE)row(count++,y,x);
        long state=0x781a3e6c904b5d2fL;
        for(int i=0;i<8192;++i){state^=state<<13;state^=state>>>7;state^=state<<17;long y=state;
            state^=state<<13;state^=state>>>7;state^=state<<17;row(count++,y,state);}
        System.out.println("Player atan2: "+count+" records");
    }
}
