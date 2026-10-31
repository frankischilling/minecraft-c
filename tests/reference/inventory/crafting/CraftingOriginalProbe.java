import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.io.*;
import java.security.MessageDigest;

public final class CraftingOriginalProbe {
    static boolean client;
    static Class<?> stackClass, gridClass, resultClass, inventoryClass, baseClass, slotClass;
    static Method damage, gridSet, gridGet, gridXY, gridSplit, managerFind;
    static Constructor<?> stackConstructor;
    static Object manager;
    static List<?> recipes;
    static String pick(String a, String b) { return client ? a : b; }
    static Field field(Class<?> type, String name) throws Exception {
        Field f = type.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static Object value(Class<?> type, Object owner, String name) throws Exception {
        return field(type,name).get(owner);
    }
    static Method method(Class<?> type, String name, Class<?>... parameters) throws Exception {
        Method m = type.getDeclaredMethod(name,parameters); m.setAccessible(true); return m;
    }
    static String failed(Method m, Object target, Object... arguments) throws Exception {
        try { m.invoke(target, arguments); return "OK"; }
        catch (InvocationTargetException e) { return e.getCause().getClass().getSimpleName(); }
    }
    static String stack(Object s) throws Exception {
        if (s == null) return "-";
        return value(stackClass,s,"c")+","+value(stackClass,s,"a")+","+damage.invoke(s)+","+value(stackClass,s,"b");
    }
    static Object newStack(int id,int count,int data) throws Exception {
        return stackConstructor.newInstance(id,count,data);
    }
    static boolean fresh(Object output) throws Exception {
        if (output == null) return true;
        for (Object recipe: recipes)
            if (method(recipe.getClass(),"b").invoke(recipe)==output) return false;
        return true;
    }
    static void graph(String name,Object container,Object inventory) throws Exception {
        List<?> slots=(List<?>)value(baseClass,container,"e");
        List<?> snapshots=(List<?>)value(baseClass,container,"d");
        Object grid=value(container.getClass(),container,"a"), result=value(container.getClass(),container,"b");
        List<?> listeners=(List<?>)value(baseClass,container,"g");
        Set<?> blocked=(Set<?>)value(baseClass,container,"b");
        boolean distinct=slots!=snapshots && snapshots!=listeners && (Object)listeners!=(Object)blocked;
        System.out.println("G\t"+name+"\t"+slots.size()+"\t"+snapshots.size()+"\t"+listeners.size()+"\t"+blocked.size()+"\t"+value(baseClass,container,"f")+"\t"+value(baseClass,container,"a")+"\t"+distinct);
        for(int i=0;i<slots.size();++i) {
            Object slot=slots.get(i), inv=value(slotClass,slot,"e");
            boolean crafting=slot.getClass().getName().equals(pick("yv","py"));
            boolean armor=slot.getClass().getName().equals(pick("pv","js"));
            int armorType=armor?((Integer)value(slot.getClass(),slot,"d")):-1;
            boolean relation=crafting?value(slot.getClass(),slot,"d")==grid:armor?value(slot.getClass(),slot,"e")==container:true;
            String kind=crafting?"crafting":armor?"armor":"normal";
            String invKind=inv==grid?"grid":inv==result?"result":inv==inventory?"player":"BAD";
            System.out.println("S\t"+name+"\t"+value(slotClass,slot,"a")+"\t"+kind+"\t"+invKind+"\t"+value(slotClass,slot,"d")+"\t"+value(slotClass,slot,"b")+"\t"+value(slotClass,slot,"c")+"\t"+armorType+"\t"+relation);
        }
    }
    static void inventoryTrace() throws Exception {
        Object grid=gridClass.getConstructor(baseClass,int.class,int.class).newInstance(null,2,2);
        Object input=newStack(17,10,2);
        System.out.println("I\tgrid\t"+method(gridClass,"a").invoke(grid)+"\t"+method(gridClass,"c").invoke(grid)+"\t"+method(gridClass,"d").invoke(grid));
        System.out.println("I\tset\t"+failed(gridSet,grid,0,input)+"\t"+(gridGet.invoke(grid,0)==input)+"\t"+stack(input));
        System.out.println("I\thigh\t"+stack(gridGet.invoke(grid,4))+"\t"+stack(gridXY.invoke(grid,2,0))+"\t"+stack(gridXY.invoke(grid,0,2)));
        System.out.println("I\tnegative\t"+failed(gridXY,grid,0,-1)+"\t"+(gridXY.invoke(grid,0,Integer.MIN_VALUE)==input));
        System.out.println("I\tsplit-negative\t"+failed(gridSplit,grid,0,-3)+"\t"+stack(input)+"\t"+stack(gridSplit.invoke(grid,1,1)));
        Object result=resultClass.getConstructor().newInstance();
        Method set=method(resultClass,"a",int.class,stackClass), take=method(resultClass,"a",int.class,int.class);
        set.invoke(result,0,input); Object removed=take.invoke(result,0,0);
        System.out.println("I\tresult\t"+method(resultClass,"a").invoke(result)+"\t"+method(resultClass,"c").invoke(result)+"\t"+method(resultClass,"d").invoke(result)+"\t"+(removed==input)+"\t"+stack(removed)+"\t"+failed(method(resultClass,pick("f_","d_"),int.class),result,1));
        Object inventory=inventoryClass.getConstructor(Class.forName(pick("gs","em"),false,CraftingOriginalProbe.class.getClassLoader())).newInstance(new Object[]{null});
        System.out.println("I\tplayer\t"+Array.getLength(value(inventoryClass,inventory,"a"))+"\t"+Array.getLength(value(inventoryClass,inventory,"b"))+"\t"+value(inventoryClass,inventory,"c")+"\t"+value(inventoryClass,inventory,"e")+"\t"+(value(inventoryClass,inventory,"f")==null)+"\t"+(value(inventoryClass,inventory,"d")==null));
        method(inventoryClass,"a",int.class,stackClass).invoke(inventory,39,input);
        Object split=method(inventoryClass,"a",int.class,int.class).invoke(inventory,39,4);
        method(inventoryClass,pick("y_","i")).invoke(inventory);
        System.out.println("I\tplayer-split\t"+stack(input)+"\t"+stack(split)+"\t"+value(inventoryClass,inventory,"e")+"\t"+failed(method(inventoryClass,pick("f_","d_"),int.class),inventory,40));
        try { gridClass.getConstructor(baseClass,int.class,int.class).newInstance(null,Integer.MAX_VALUE,2); System.out.println("I\tnegative-array\tOK"); }
        catch(InvocationTargetException e) {System.out.println("I\tnegative-array\t"+e.getCause().getClass().getSimpleName());}
    }
    static void writeBinding() throws Exception {
        String names=System.getProperty("crafting.probe.classes"), output=System.getProperty("crafting.probe.binding");
        if(names==null && output==null)return;
        if(names==null || output==null)throw new IllegalArgumentException("binding properties");
        StringBuilder result=new StringBuilder("CRAFTING_BINDING_V1\n");
        for(String name:Files.readAllLines(Paths.get(names),StandardCharsets.US_ASCII)) {
            if(!name.matches("[A-Za-z0-9_$/]+"))throw new IllegalArgumentException("binding class name");
            Class<?> type=Class.forName(name.replace('/','.'),false,CraftingOriginalProbe.class.getClassLoader());
            ByteArrayOutputStream bytes=new ByteArrayOutputStream();
            try(InputStream stream=type.getResourceAsStream("/"+name+".class")) {
                if(stream==null)throw new IllegalArgumentException("binding class resource");
                byte[] block=new byte[4096];int count;
                while((count=stream.read(block))!=-1) {bytes.write(block,0,count);if(bytes.size()>262144)throw new IllegalArgumentException("binding byte bound");}
            }
            StringBuilder hash=new StringBuilder();
            for(byte value:MessageDigest.getInstance("SHA-256").digest(bytes.toByteArray()))hash.append(String.format(java.util.Locale.ROOT,"%02x",value&255));
            result.append(name).append('\t').append(bytes.size()).append('\t').append(hash).append('\t').append(type.getProtectionDomain().getCodeSource().getLocation().toURI().toString()).append('\n');
        }
        Files.write(Paths.get(output),result.toString().getBytes(StandardCharsets.UTF_8));
    }
    public static void main(String[] args) throws Exception {
        client=args[0].equals("client");
        Class.forName(pick("jl","gg"));
        stackClass=Class.forName(pick("iz","fy"));gridClass=Class.forName(pick("mq","hz"));
        resultClass=Class.forName(pick("wl","of"));inventoryClass=Class.forName(pick("ix","fx"));
        baseClass=Class.forName(pick("dw","cl"));slotClass=Class.forName(pick("gp","el"));
        damage=method(stackClass,pick("i","h"));stackConstructor=stackClass.getConstructor(int.class,int.class,int.class);
        gridSet=method(gridClass,"a",int.class,stackClass);gridGet=method(gridClass,pick("f_","d_"),int.class);
        gridXY=method(gridClass,"b",int.class,int.class);gridSplit=method(gridClass,"a",int.class,int.class);
        Class<?> managerClass=Class.forName(pick("hk","ey"));manager=method(managerClass,"a").invoke(null);
        managerFind=method(managerClass,"a",gridClass);recipes=(List<?>)method(managerClass,"b").invoke(manager);
        System.out.println("CRAFTING_TRACE_V1\t"+args[0]);inventoryTrace();
        Class<?> playerClass=Class.forName(pick("gs","em"),false,CraftingOriginalProbe.class.getClassLoader());
        Object inventory=inventoryClass.getConstructor(playerClass).newInstance(new Object[]{null});
        Class<?> playerContainerClass=Class.forName(pick("aa","o")), workbenchClass=Class.forName(pick("it","fu"));
        Object player=playerContainerClass.getConstructor(inventoryClass,boolean.class).newInstance(inventory,true);
        Object workbench=workbenchClass.getConstructor(inventoryClass,Class.forName(pick("fd","dj"),false,CraftingOriginalProbe.class.getClassLoader()),int.class,int.class,int.class).newInstance(inventory,null,5,-3,9);
        graph("player",player,inventory);graph("workbench",workbench,inventory);
        for(String line:Files.readAllLines(Paths.get(args[1]),StandardCharsets.UTF_8)) {
            String[] columns=line.split("\t");int width=Integer.parseInt(columns[1]), index=Integer.parseInt(columns[2]);
            Object container=width==2?player:workbench;
            Object grid=value(container.getClass(),container,"a"),result=value(container.getClass(),container,"b");
            for(int i=0;i<width*width;++i)gridSet.invoke(grid,i,null);
            Map<Integer,Object> aliases=new HashMap<Integer,Object>();String[] before=new String[width*width];Object[] inputs=new Object[width*width];
            for(int i=0;i<width*width;++i) {
                String token=columns[i+3];if(token.equals("-"))continue;
                String[] values=token.split(",");int group=Integer.parseInt(values[3]);Object s=aliases.get(group);
                if(s==null) {s=newStack(Integer.parseInt(values[0]),Integer.parseInt(values[1]),Integer.parseInt(values[2]));aliases.put(group,s);}
                inputs[i]=s;before[i]=stack(s);gridSet.invoke(grid,i,s);
            }
            Object recipe=recipes.get(index);boolean matches=(Boolean)method(recipe.getClass(),"a",gridClass).invoke(recipe,grid);
            Object direct=method(recipe.getClass(),"b",gridClass).invoke(recipe,grid), found=managerFind.invoke(manager,grid);
            Object preview=method(resultClass,pick("f_","d_"),int.class).invoke(result,0);
            Object repeatedInput=gridGet.invoke(grid,width*width-1);gridSet.invoke(grid,width*width-1,repeatedInput);
            Object repeated=method(resultClass,pick("f_","d_"),int.class).invoke(result,0);
            boolean unchanged=true;for(int i=0;i<inputs.length;++i)if(inputs[i]!=null&&!before[i].equals(stack(inputs[i])))unchanged=false;
            System.out.println("C\t"+columns[0]+"\t"+width+"\t"+index+"\t"+matches+"\t"+stack(direct)+"\t"+stack(found)+"\t"+stack(preview)+"\t"+fresh(direct)+"\t"+fresh(found)+"\t"+(preview==null||preview!=direct&&preview!=found)+"\t"+(preview==null?repeated==null:repeated!=preview)+"\t"+unchanged);
        }
        System.out.println("END");
        writeBinding();
    }
}
