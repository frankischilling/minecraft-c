import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GL21;
import org.lwjgl.opengl.GLContext;
import org.lwjgl.opengl.PixelFormat;

/** Owns a real LWJGL Display; all queried bindings are saved and preserved. */
public final class TextureLookupObservation {
    private static Object call(LookupObservation o, String label, String name) throws Exception {
        String exception = null; Integer id = null;
        System.err.println("LOOKUP_BOUNDARY call-before " + label);
        try { id = (Integer)LookupObservation.invoke(o.engine, "b", new Class<?>[]{String.class}, new Object[]{name}); }
        catch (Exception e) { exception = e.getClass().getName(); }
        System.err.println("LOOKUP_BOUNDARY call-after " + label);
        Object state = o.state();
        int before = GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D);
        String gpu = null; Integer width = null, height = null;
        if (id != null && GL11.glIsTexture(id)) {
            // Explicit observer readback only, after the source state snapshot.
            int[] names = {GL11.GL_PACK_ALIGNMENT, GL11.GL_PACK_ROW_LENGTH,
                GL11.GL_PACK_SKIP_PIXELS, GL11.GL_PACK_SKIP_ROWS};
            int[] values = new int[names.length];
            for (int i = 0; i < names.length; ++i) values[i] = GL11.glGetInteger(names[i]);
            int pbo = GL11.glGetInteger(GL21.GL_PIXEL_PACK_BUFFER_BINDING);
            GL11.glBindTexture(GL11.GL_TEXTURE_2D, id);
            try {
                GL15.glBindBuffer(GL21.GL_PIXEL_PACK_BUFFER, 0);
                for (int i = 0; i < names.length; ++i) GL11.glPixelStorei(names[i], i == 0 ? 1 : 0);
                width = GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D, 0, GL11.GL_TEXTURE_WIDTH);
                height = GL11.glGetTexLevelParameteri(GL11.GL_TEXTURE_2D, 0, GL11.GL_TEXTURE_HEIGHT);
                int size = Math.multiplyExact(Math.multiplyExact(width, height), 4);
                if (size < 0 || size > 1048576) throw new IllegalStateException("bounded real level-zero extent");
                ByteBuffer pixels = BufferUtils.createByteBuffer(size);
                GL11.glGetTexImage(GL11.GL_TEXTURE_2D, 0, GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, pixels);
                gpu = LookupObservation.hexBytes(pixels, size);
            } finally {
                for (int i = 0; i < names.length; ++i) GL11.glPixelStorei(names[i], values[i]);
                GL15.glBindBuffer(GL21.GL_PIXEL_PACK_BUFFER, pbo);
                GL11.glBindTexture(GL11.GL_TEXTURE_2D, before);
            }
            for (int i = 0; i < names.length; ++i)
                if (GL11.glGetInteger(names[i]) != values[i]) throw new IllegalStateException("observer pack-state mutation");
            if (GL11.glGetInteger(GL21.GL_PIXEL_PACK_BUFFER_BINDING) != pbo)
                throw new IllegalStateException("observer pack-buffer mutation");
        }
        int after = GL11.glGetInteger(GL11.GL_TEXTURE_BINDING_2D);
        if (before != after) throw new IllegalStateException("observer binding mutation");
        ByteBuffer transfer = (ByteBuffer)LookupObservation.get(o.engine, "f");
        return LookupObservation.map("label", label, "outcome", exception == null ? "return" : "throw",
            "exception", exception, "id", id, "state", state, "bound_before", before, "bound_after", after,
            "width", width, "height", height, "gpu", gpu,
            "transfer", id == null ? null : LookupObservation.hexBytes(transfer, transfer.limit()),
            "gl_error", GL11.glGetError());
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 2) throw new IllegalArgumentException("directory profile");
        String profile = args[1];
        LookupObservation o = new LookupObservation(args[0], false);
        System.err.println("LOOKUP_BOUNDARY device-before");
        Display.setDisplayMode(new DisplayMode(64, 64));
        Display.setTitle("Unchanged source String texture observation");
        Display.create(new PixelFormat());
        try {
            Display.makeCurrent();
            if (!GLContext.getCapabilities().OpenGL21) throw new IllegalStateException("real GL2.1 readback prerequisite");
            System.err.println("LOOKUP_BOUNDARY device-after");
            System.err.println("LOOKUP_BOUNDARY profile-before");
            Object device = LookupObservation.map("vendor", GL11.glGetString(GL11.GL_VENDOR),
                "renderer", GL11.glGetString(GL11.GL_RENDERER), "version", GL11.glGetString(GL11.GL_VERSION));
            List<Object> events = new ArrayList<>();
            if (profile.equals("default") || profile.equals("zip")) {
                o.requirePack(profile);
                events.add(call(o, "ordinary", "/gui/background.png"));
                events.add(call(o, "cache", new String("/gui/background.png")));
                events.add(call(o, "missing", "/lookup-absent.png"));
            } else if (profile.equals("frontiers")) {
                events.add(call(o, "strip", "##/gui/background.png"));
                events.add(call(o, "clamp", "%clamp%/gui/background.png"));
                events.add(call(o, "corrupt", "%blur%/lookup-truncated.png"));
                events.add(call(o, "null-key", null));
            } else if (profile.equals("priority")) {
                events.add(call(o, "ordinary", "/gui/background.png"));
                java.lang.reflect.Field list = LookupObservation.field(o.engine.getClass(), "l");
                list.set(o.engine, null);
                events.add(call(o, "null-list-cache", "/gui/background.png"));
                list.set(o.engine, o.packs);
                java.lang.reflect.Field selected = LookupObservation.field(o.packs.getClass(), "a");
                Object saved = selected.get(o.packs); selected.set(o.packs, null);
                try {
                    events.add(call(o, "null-pack-cache", "/gui/background.png"));
                    events.add(call(o, "null-pack-miss", "/different-absent.png"));
                } finally { selected.set(o.packs, saved); }
            } else throw new IllegalArgumentException("unknown texture profile");
            System.err.println("LOOKUP_BOUNDARY profile-after");
            o.finish("texture", profile, events, device);
        } finally { Display.destroy(); }
    }
}
