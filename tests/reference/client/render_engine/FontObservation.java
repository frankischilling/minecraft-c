import java.awt.*;
import java.awt.font.*;
import java.awt.image.*;
import java.io.*;
import java.lang.reflect.*;
import java.nio.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;

/** Authored constructor/default-font observation; pixels are private output. */
public final class FontObservation {
 static Object get(Object o,Class<?> c,String n)throws Exception{Field f=c.getDeclaredField(n);f.setAccessible(true);return f.get(o);}
 public static void main(String[]args)throws Exception {
  if(args.length!=1)throw new IllegalArgumentException("owned output directory");
  Path root=Path.of(args[0]);Files.createDirectory(root);
  for(String name:new String[]{"ji","ge","kv","qb"}){
   Class<?> type=Class.forName(name,false,FontObservation.class.getClassLoader());
   byte[] code;try(InputStream in=type.getResourceAsStream("/"+name+".class")){code=in.readAllBytes();}
   System.out.println("BIND\t"+name+"\t"+type.getProtectionDomain().getCodeSource().getLocation()+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code)));
  }
  ji owner=new ji(null,null);
  BufferedImage actual=(BufferedImage)get(owner,ji.class,"m");
  BufferedImage authored=new BufferedImage(64,64,BufferedImage.TYPE_INT_ARGB);
  Graphics2D graphics=authored.createGraphics();
  try {
   Font font=graphics.getFont();FontMetrics metrics=graphics.getFontMetrics();
   System.out.println("FONT "+font+" METRICS "+metrics.getAscent()+" "+metrics.getDescent()+" "+metrics.getLeading()+" "+metrics.stringWidth("missingtex"));
   System.out.println("HINTS "+graphics.getRenderingHints()+" TRANSFORM "+graphics.getTransform());
   Method method=Class.forName("sun.font.FontUtilities").getDeclaredMethod("getFont2D",Font.class);method.setAccessible(true);
   Object font2d=method.invoke(null,font);System.out.println("FONT2D "+font2d.getClass().getName());
   Object physical=font2d;
   if(font2d.getClass().getName().equals("sun.font.CompositeFont")){
    Method slot=font2d.getClass().getDeclaredMethod("getSlotFont",int.class);slot.setAccessible(true);physical=slot.invoke(font2d,0);
   }
   Field platform=Class.forName("sun.font.PhysicalFont").getDeclaredField("platName");platform.setAccessible(true);
   Path fontFile=Path.of((String)platform.get(physical));
   System.out.println("FONTFILE\t"+fontFile+"\t"+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(fontFile))));
   GlyphVector glyphs=font.createGlyphVector(graphics.getFontRenderContext(),"missingtex");
   for(int i=0;i<glyphs.getNumGlyphs();++i)System.out.println("GLYPH "+i+" "+glyphs.getGlyphCode(i)+" "+glyphs.getGlyphPosition(i)+" "+glyphs.getGlyphMetrics(i).getAdvanceX()+" "+glyphs.getGlyphPixelBounds(i,graphics.getFontRenderContext(),1,10));
   graphics.setColor(Color.WHITE);graphics.fillRect(0,0,64,64);graphics.setColor(Color.BLACK);graphics.drawString("missingtex",1,10);
  }finally{graphics.dispose();}
  int[] a=actual.getRGB(0,0,64,64,null,0,64),b=authored.getRGB(0,0,64,64,null,0,64);
  if(!Arrays.equals(a,b))throw new AssertionError("actual ji versus direct JDK drawing");
  ByteBuffer bytes=ByteBuffer.allocate(16384).order(ByteOrder.BIG_ENDIAN);int black=0;
  for(int v:a){bytes.putInt(v);if(v==0xff000000)++black;else if(v!=0xffffffff)throw new AssertionError("unexpected non-monochrome pixel");}
  Files.write(root.resolve("missing.argb"),bytes.array());
  System.out.println("IMAGE 64 64 BLACK "+black+" SHA256 "+HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes.array())));
  for(String n:new String[]{"b","c","d","g","h"})System.out.println("OWNER "+n+" "+((Collection.class.isInstance(get(owner,ji.class,n)))?((Collection<?>)get(owner,ji.class,n)).size():((Map<?,?>)get(owner,ji.class,n)).size()));
  for(String n:new String[]{"e","f"}){Buffer x=(Buffer)get(owner,ji.class,n);System.out.println("BUFFER "+n+" "+x.position()+" "+x.limit()+" "+x.capacity()+" "+x.isDirect());}
  IntBuffer ints=(IntBuffer)get(owner,ji.class,"e");ByteBuffer bytesBuffer=(ByteBuffer)get(owner,ji.class,"f");
  if(ints.get(0)!=0)throw new AssertionError("original zero direct int storage");
  for(int n=0;n<bytesBuffer.capacity();++n)if(bytesBuffer.get(n)!=0)throw new AssertionError("original zero transfer storage");
  if(get(owner,ji.class,"l")!=null||get(owner,ji.class,"i")!=null)throw new AssertionError("actual nullable fields");
  Object ledgerA=get(null,ge.class,"a"),bList=get(null,ge.class,"b");
  if(ledgerA==bList||((java.util.List<?>)ledgerA).size()!=0||((java.util.List<?>)bList).size()!=0)throw new AssertionError("actual distinct ge Lists");
  if((Boolean)get(null,ji.class,"a")||ints.order()!=ByteOrder.nativeOrder()||bytesBuffer.order()!=ByteOrder.nativeOrder())throw new AssertionError("real static/default direct order");
  System.out.println("NAMESPACE ji-false ge-empty-distinct buffers-native-zero");
  System.out.println("DONE actual ji constructor, direct JDK default-font control");
 }
}
