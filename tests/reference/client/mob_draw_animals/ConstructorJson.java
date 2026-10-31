import java.util.*;

/** Authored observation transport only. */
public final class ConstructorJson {
    public static Map<String,Object> map(Object... values) {
        Map<String,Object> result=new LinkedHashMap<>();
        for(int i=0;i<values.length;i+=2) result.put((String)values[i],values[i+1]);
        return result;
    }
    private static String quoted(String value) {
        StringBuilder out=new StringBuilder("\"");
        for(int i=0;i<value.length();++i) {
            char c=value.charAt(i);
            if(c=='"'||c=='\\') out.append('\\').append(c);
            else if(c<32||c>126) out.append(String.format(Locale.ROOT,"\\u%04x",(int)c));
            else out.append(c);
        }
        return out.append('"').toString();
    }
    public static String json(Object value) {
        if(value==null) return "null";
        if(value instanceof String) return quoted((String)value);
        if(value instanceof Number||value instanceof Boolean) return value.toString();
        if(value instanceof Map) {
            StringJoiner out=new StringJoiner(",","{","}");
            for(Map.Entry<?,?> entry:((Map<?,?>)value).entrySet()) out.add(quoted((String)entry.getKey())+":"+json(entry.getValue()));
            return out.toString();
        }
        if(value instanceof Iterable) {
            StringJoiner out=new StringJoiner(",","[","]");
            for(Object entry:(Iterable<?>)value) out.add(json(entry));
            return out.toString();
        }
        throw new IllegalArgumentException("unsupported JSON transport type");
    }
}
