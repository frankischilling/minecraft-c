import java.nio.IntBuffer;
import java.nio.file.Path;
import java.security.Permission;
import java.util.ArrayList;
import java.util.List;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.ARBOcclusionQuery;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GLContext;

/** Unchanged LWJGL2.9 ARB wrappers on a real context; authored finite geometry. */
public final class QueryObservation {
    static int network;
    static final List<String> links=new ArrayList<>();
    static void require(boolean value,String message){if(!value)throw new AssertionError(message);}
    static void guard(IntBuffer result) {
        require(result.position()==1&&result.limit()==2&&result.get(0)==0x02468ace,
                "actual IntBuffer cursor/prefix changed");
        IntBuffer view=result.duplicate();view.clear();
        require(view.get(2)==0x02468ace&&view.get(3)==0x02468ace,"result suffix changed");
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        require(args.length==1,"private SDK native directory required");
        Path natives=Path.of(args[0]).toRealPath();
        System.setSecurityManager(new SecurityManager(){
            @Override public void checkPermission(Permission permission){}
            @Override public void checkConnect(String host,int port){++network;throw new SecurityException();}
            @Override public void checkListen(int port){++network;throw new SecurityException();}
            @Override public void checkWrite(String file){throw new SecurityException();}
            @Override public void checkLink(String library){
                links.add(library);
                if(!Path.of(library).isAbsolute()||!Path.of(library).normalize().equals(natives.resolve("lwjgl64.dll")))
                    throw new SecurityException("unexpected native link");
            }
        });
        Display.setDisplayMode(new DisplayMode(64,64));Display.setTitle("Original ARB query controls");
        try {
            Display.create();
            System.out.println("QUERY_DRIVER "+GL11.glGetString(GL11.GL_VENDOR)+" | "+
                GL11.glGetString(GL11.GL_RENDERER)+" | "+GL11.glGetString(GL11.GL_VERSION));
            boolean supported=GLContext.getCapabilities().GL_ARB_occlusion_query;
            System.out.println("QUERY_CAPABILITY "+(supported?1:0));
            if(supported) {
                IntBuffer ids=BufferUtils.createIntBuffer(4),result=BufferUtils.createIntBuffer(4);
                for(int i=0;i<4;++i){ids.put(i,0x13579bdf);result.put(i,0x02468ace);}
                ids.position(1).limit(3);result.position(1).limit(2);
                ARBOcclusionQuery.glGenQueriesARB(ids);
                require(ids.position()==1&&ids.limit()==3&&ids.get(0)==0x13579bdf,"name buffer prefix/cursor");
                IntBuffer view=ids.duplicate();view.clear();require(view.get(3)==0x13579bdf,"name suffix");
                int id=ids.get(1),bits=ARBOcclusionQuery.glGetQueryiARB(35092,34916);
                require(id!=0&&bits>=0,"actual generated name/counter width");
                GL11.glEnable(-1); // Genuine sticky error; wrappers must not drain it.
                ARBOcclusionQuery.glBeginQueryARB(35092,id);
                require(ARBOcclusionQuery.glGetQueryiARB(35092,34917)==id,"active original query");
                ARBOcclusionQuery.glEndQueryARB(35092);
                require(ARBOcclusionQuery.glGetQueryiARB(35092,34917)==0,"ended original query");
                ARBOcclusionQuery.glGetQueryObjectuARB(id,34919,result);guard(result);
                int emptyAvailable=result.get(1);require(emptyAvailable==0||emptyAvailable==1,"availability encoding");
                ARBOcclusionQuery.glGetQueryObjectuARB(id,34918,result);guard(result);
                require(result.get(1)==0,"empty result");
                require(GL11.glGetError()==GL11.GL_INVALID_ENUM&&GL11.glGetError()==GL11.GL_NO_ERROR,"sticky source flag");
                System.out.println("QUERY_EMPTY "+emptyAvailable+" 0 1 2 1");
                GL11.glViewport(0,0,64,64);
                for(int feature:new int[]{GL11.GL_DEPTH_TEST,GL11.GL_CULL_FACE,GL11.GL_SCISSOR_TEST,
                    GL11.GL_BLEND,GL11.GL_ALPHA_TEST,GL11.GL_TEXTURE_2D,GL11.GL_LIGHTING,GL11.GL_FOG})GL11.glDisable(feature);
                GL11.glColorMask(false,false,false,false);
                GL11.glMatrixMode(GL11.GL_PROJECTION);GL11.glLoadIdentity();
                GL11.glMatrixMode(GL11.GL_MODELVIEW);GL11.glLoadIdentity();
                ARBOcclusionQuery.glBeginQueryARB(35092,id);
                GL11.glBegin(GL11.GL_TRIANGLES);
                for(int i=0;i<2730;++i){GL11.glVertex3f(-1,-1,0);GL11.glVertex3f(1,-1,0);GL11.glVertex3f(-1,1,0);}
                GL11.glEnd();ARBOcclusionQuery.glEndQueryARB(35092);
                ARBOcclusionQuery.glGetQueryObjectuARB(id,34919,result);guard(result);
                int available=result.get(1);require(available==0||available==1,"visible availability");
                ARBOcclusionQuery.glGetQueryObjectuARB(id,34918,result);guard(result);
                long samples=Integer.toUnsignedLong(result.get(1));
                require(bits==0?samples==0:samples>0,"actual visible query result");
                ARBOcclusionQuery.glGetQueryObjectuARB(id,34919,result);guard(result);
                require(result.get(1)==1,"result availability after retrieval");
                System.out.println("QUERY_VISIBLE "+available+" "+samples+" "+bits+" 1 2 1");
                ARBOcclusionQuery.glDeleteQueriesARB(ids);
                require(GL11.glGetError()==GL11.GL_NO_ERROR,"authored query GL error");
            }
        } finally {Display.destroy();}
        for(String link:links)System.out.println("NATIVE_LINK "+link);
        require(network==0,"no network");
        System.out.println("Original ARB queries: network=0; source_wrappers=unchanged");
    }
}
