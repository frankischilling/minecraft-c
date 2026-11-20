import com.sun.jdi.*;
import java.util.*;

/** Actual boot-JDK String backing fields only; no target invokes, writes or StringReference.value(). */
public final class JdiStringUnits {
    private static void need(boolean value,String message){if(!value)throw new AssertionError(message);}
    private static Map<String,Object> map(Object...values){Map<String,Object> result=new LinkedHashMap<>();for(int i=0;i<values.length;i+=2)result.put((String)values[i],values[i+1]);return result;}
    private static Field field(ReferenceType type,String name,String signature,boolean statics){Field f=type.fieldByName(name);need(f!=null&&f.declaringType().name().equals(type.name())&&f.signature().equals(signature)&&f.isStatic()==statics,"actual selected boot-JDK field shape: "+type.name()+"."+name);return f;}
    private static Map<String,Object> shape(Field field){return map("declaring_type",field.declaringType().name(),"name",field.name(),"descriptor",field.signature(),"static",field.isStatic(),"final",field.isFinal());}
    private static int integer(Value value){need(value instanceof IntegerValue,"actual static integer shift");return ((IntegerValue)value).value();}
    private static int coder(Value value){need(value instanceof ByteValue,"actual String coder byte");int coder=((ByteValue)value).value();need(coder==0||coder==1,"admitted actual String LATIN1/UTF16 coder");return coder;}
    private static ArrayReference backing(Value value){need(value instanceof ArrayReference,"actual String byte[] backing reference");ArrayReference array=(ArrayReference)value;need(array.referenceType().signature().equals("[B")&&array.referenceType().classLoader()==null&&array.length()<=8192,"bounded actual boot byte[] backing");return array;}
    private static byte[] bytes(ArrayReference array){List<Value> values=array.getValues();byte[] raw=new byte[values.size()];for(int i=0;i<raw.length;i++){need(values.get(i) instanceof ByteValue,"actual backing array byte");raw[i]=((ByteValue)values.get(i)).value();}return raw;}
    private static String hex(byte[] raw){StringBuilder out=new StringBuilder();for(byte b:raw)out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    private static Map<String,Object> bracket(ArrayReference value,int coder,int hi,int lo){return map("value_id",Long.toString(value.uniqueID()),"coder",coder,"backing_length",value.length(),"hi_byte_shift",hi,"lo_byte_shift",lo);}

    /** Exact four-key result: is_null, length, utf16_units, storage. Null differs from empty. */
    public static Map<String,Object> read(StringReference reference)throws Exception {
        if(reference==null)return map("is_null",true,"length",null,"utf16_units",null,"storage",null);
        ReferenceType string=reference.referenceType();need(string.name().equals("java.lang.String")&&string.classLoader()==null,"actual boot java.lang.String receiver");
        Field valueField=field(string,"value","[B",false),coderField=field(string,"coder","B",false);
        VirtualMachine vm=reference.virtualMachine();List<ReferenceType> types=vm.classesByName("java.lang.StringUTF16");need(types.size()==1,"one actual prepared boot StringUTF16");ReferenceType utf16=types.get(0);need(utf16.classLoader()==null&&utf16.isPrepared(),"actual prepared boot StringUTF16 authority");
        Field hiField=field(utf16,"HI_BYTE_SHIFT","I",true),loField=field(utf16,"LO_BYTE_SHIFT","I",true);
        int hi=integer(utf16.getValue(hiField)),lo=integer(utf16.getValue(loField));need((hi==0&&lo==8)||(hi==8&&lo==0),"actual StringUTF16 complementary byte shifts");
        ArrayReference value=backing(reference.getValue(valueField));int coder=coder(reference.getValue(coderField));Map<String,Object> before=bracket(value,coder,hi,lo);byte[] raw=bytes(value);
        need(coder==0?raw.length<=4096:raw.length%2==0&&raw.length/2<=4096,"closed maximum4096 actual UTF16 units");
        List<String> units=new ArrayList<>();if(coder==0){for(byte b:raw)units.add(String.format(Locale.ROOT,"%04x",b&255));}else{for(int i=0;i<raw.length;i+=2){int unit=((raw[i]&255)<<hi)|((raw[i+1]&255)<<lo);units.add(String.format(Locale.ROOT,"%04x",unit));}}
        byte[] again=bytes(value);ArrayReference afterValue=backing(reference.getValue(valueField));int afterCoder=coder(reference.getValue(coderField));int afterHi=integer(utf16.getValue(hiField)),afterLo=integer(utf16.getValue(loField));Map<String,Object> after=bracket(afterValue,afterCoder,afterHi,afterLo);
        need(value.equals(afterValue)&&before.equals(after)&&Arrays.equals(raw,again),"read-only actual String field/array/shift brackets stable");
        Map<String,Object> storage=map("string_class",string.name(),"string_boot_loader",true,"string_id",Long.toString(reference.uniqueID()),"value_field",shape(valueField),"coder_field",shape(coderField),"backing_class",value.referenceType().name(),"backing_id",Long.toString(value.uniqueID()),"backing_length",raw.length,"bytes_hex",hex(raw),"coder",coder,"utf16_class",utf16.name(),"utf16_boot_loader",true,"hi_shift_field",shape(hiField),"lo_shift_field",shape(loField),"hi_byte_shift",hi,"lo_byte_shift",lo,"before",before,"after",after,"byte_reads_equal",true);
        return map("is_null",false,"length",units.size(),"utf16_units",units,"storage",storage);
    }
}
