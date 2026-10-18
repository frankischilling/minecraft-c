import com.sun.jdi.*;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.BufferedWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Read-only loopback JDI observations of the original chunk lifecycle. */
public final class ChunkLifecycleController {
    static final int STREAM_LIMIT = 32 * 1024 * 1024, TRACE_LIMIT = 64 * 1024 * 1024;
    static final String[] SPECS = {
        "client|external_tile_invalidate|ow|i|()V|1|6|071d3f6a1e224612e34b37df9796156ae68f8bed718d422487e0c77a91971ee0|0,5",
        "client|external_entity_register|fd|c|(Lsn;)V|4|41|a2e151e7d0801da7996e5cdb637d092b2cbe22890a181326cdd313ad44b74346|0",
        "base,dedicated|external_tile_invalidate|jh|h|()V|1|6|111d5244a2b65d3ada4135e9b8e41819945016aeb3c327d789e06ed0362793ca|0,5",
        "base,dedicated|external_entity_register|dj|c|(Llq;)V|4|41|34a3ffab959368744c5fbf2642f34ac7df85e9ccbdb50c4a30d469665b8a4548|0",
        "dedicated|external_entity_registry|dp|c|(Llq;)V|4|18|78e4954cd1d797179312f5d4870c60a36ac18ad4bf723cd11b4634fe08f9be1e|0,17",
        "client|acquire|ok|c|(II)Llm;|1|394|06e08e7b1b2956f8a581b21a56578b29d153e418e53d7ae7ed298d0d78ea7817|0,20,78,100,112,128,391",
        "client|save_pass|ok|a|(ZLyb;)Z|1|119|db7438732819f36b38f8a1e124315976965e9a00f30936632abb784a4c3b74a4|0,11,74,88,118",
        "client|extra_wrapper|ok|a|(Llm;)V|2|31|03dc846ab277dfd80910c9893c1e5c9f7f2d4e4c0a5de9e1ae5f0542067cab31|0,30",
        "client|main_wrapper|ok|b|(Llm;)V|2|42|a3589be66536f6a8b845340291e0032a9da13b1de756f7b556836e2e07df89f1|0,19,41",
        "client|save_enabled|ok|b|()Z|1|2|95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2|0",
        "client|maintenance|ok|a|()Z|1|131|8eba5425c0eb6c6b79064e2f71e6fc299e07809a631099e4d206c8db9a60a2d4|0,76,87,99,105",
        "client|world_time|fd|a|(J)V|1|9|b3d812a16755874725c695d086993aba96cdf4cd5354be43667f09f94c29be4a|0,8",
        "client|world_time_get|fd|t|()J|1|8|45dc4cb00dc68fb8b32bfbc0641b6657d794beec59ff2697e48bbe6b18c43929|0",
        "client|world_save|fd|a|(ZLyb;)V|1|54|154b5b7715c572e95e4f281c9c2b72478ef7a0981a0dc5347f89cfabfd993b07|0,26,47,53",
        "client|world_metadata|fd|D|()V|2|29|2b90d6a9bd0ddb01411ee1ed4660f627e6a86bbf88342587dab5c7fbba8d622a|0,28",
        "client|world_session|fd|r|()V|1|10|ead768d43e3392db5ed7e1e8c2f1d394e4ea44e780c8a682a2c69a59381e439d|0",
        "client|register_tiles|fd|a|(Ljava/util/Collection;)V|1|33|b0b9541c407fbf25bbbe9ab7b9a2df55b1b609d79b2d511d20cb5857a63e9ed8|0",
        "client|register_entities|fd|a|(Ljava/util/List;)V|1|44|348fb118fbb8d562b0e2ee9075a024a94ffc0c78e8bab4b36d1762d440a078a5|0",
        "client|defer_entities|fd|b|(Ljava/util/List;)V|1|12|6d76728429c671d89feb7e63b6a80f5e8729f56e93e956e42e4b26f8ce01f0bc|0",
        "client|eligibility|lm|a|(Z)Z|1|70|c3961431e40e3bbdffc097021568e173d29760c0abdaab920c1095983de2d1a4|0",
        "client|activate|lm|e|()V|1|52|954ec8fdd3b2c285473b09bebe03f27f3d61572326c2443d1253219f2ec9d2da|0,5,51",
        "client|detach|lm|f|()V|1|77|08e947c3912554abfe4142424fcfc1da231f6880ac2e1379fbbd75807efaea5c|0,5,67,76",
        "client|dirty|lm|g|()V|1|6|5a6d2b391d05c47e1f7024c99b8cd11b4e2fe138bd9b6506bdbfe1a9521e6c67|0",
        "client|initialize_chunk|lm|d|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "client|membership_add|lm|a|(Lsn;)V|1|155|5a77e52c47cd35d0cee3b4fa8fbc9e121825416bf904286f76de9d4b335abd3a|0,5",
        "client|membership_remove|lm|b|(Lsn;)V|1|10|6211195c3dd9366c7bcab5e65163bb466cbd74d67301f32f02c8e21f9a9f5795|0",
        "client|membership_remove_slice|lm|a|(Lsn;I)V|1|37|840e8625028124ce7218e4c5f7bca7c57e28d56ba81dd947d7ca0c959b1fcb99|0",
        "client|load|ld|a|(Lfd;II)Llm;|1|278|3349469872749baec1a3d455f296827315f1b81245397abef0fa54c3b521c38e|0,277",
        "client|write|ld|a|(Lfd;Llm;)V|1|106|010f2b7fb21f81dedaa4063990d235d9b16e25d1345a0369f095b612483f73c9|0,4,51,61,89,105",
        "client|loader_extra|ld|b|(Lfd;Llm;)V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "client|loader_maintenance|ld|a|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "client|loader_flush|ld|b|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "client|serialize_chunk|to|a|(Llm;Lfd;Lnu;)V|9|293|d2377ea0633eccf25973659878ba0173083fcdf35b3450ed280847d9609fe1c4|0,108",
        "client|save_info_wrapper|mx|a|(Lei;Ljava/util/List;)V|1|14|1b240f45898ef6dab8789a3106214ad060ecaf938eec698317e8ecfe80b20af6|0",
        "client|save_info|fm|a|(Lei;Ljava/util/List;)V|1|151|80de3665d10026935e6465156dfcd87671c2ed1649b375f89ed2d59453373311|0",
        "client|session_check|fm|b|()V|1|81|bb68a2319df7fd1b9304355ee68a495810957cf46bd7554ef6a760577b8f8260|0",
        "client|auxiliary|hc|a|()V|1|53|2d59fd34d8c4fd3c0b72619300ac333c7d2f235d51f4b6c2199106d04d2f0978|0",
        "client|provider_maintenance|yf|a|()Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0",
        "client|timestamp|qj|b|(III)V|2|41|3de743de9199f459f5655eaea989dbe45f0e7823a4df3ed0a79e8b9f1c1ff002|0,40",
        "client|region_write|qj|a|(II[BI)V|36|445|2143d53dce4ade104faea1d774cbc4b21e8bf72a45270282ae08dc60def18e14|0,420,444",
        "client|forbidden_generate|yf|b|(II)Llm;|1|130|b5de2724c479e108327aca1b3579c200de3d98c846fc45dbba251b187afdf882|0",
        "client|forbidden_populate|yf|a|(Lcl;II)V|1|2907|f09523f9e3ed68d6e27dbd8e0a8f28431a23c4497d5d824be1b2bfa0652b28dc|0",
        "client|forbidden_finish|lm|c|()V|1|222|b0af5c034ed6dd3e9df9b555c5ea6c50056124ceef3e22af8d3767cf470f24f8|0",
        "client|blank_eligibility|li|a|(Z)Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0",
        "client|client_incremental|fd|a|(I)Z|1|34|69a624fa3320b0539525f1b497edfcc319d806d35250a40ba9b69ed1e60c93a3|0",
        "client|handler_constructor|mx|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|8|5577af645a3cd2252842cf0a29d3b3fe6abfe2a342178b2a3c72b79e25f7f689|0",
        "client|handler_base_constructor|fm|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|91|2f3e854a1085603510285eb0c74e3f79e289df90b18e6d8b917c4a7f333c7c30|0",
        "client|handler_session_write|fm|d|()V|2|71|264a7f18b404f0812a5fc31baa93bb801d8179ca4b3b2561d847a00926934199|0",
        "base|acquire|jb|c|(II)Lhi;|1|394|521e6d404e5b1ed34ec56c2f4bfafbde125fde3e71efc280856542d3d9c3bac2|0,20,78,100,112,128,391",
        "base|save_pass|jb|a|(ZLpj;)Z|1|119|322a3b0a6b4375148d65f5f55822bca59e4965b67e7c758fc224cf0cc14e4b0b|0,11,74,88,118",
        "base|extra_wrapper|jb|a|(Lhi;)V|2|31|5727f024e5763a3066b8f7aef70e6f4d13b490b87081ed2c1f5896d0d20790cf|0,30",
        "base|main_wrapper|jb|b|(Lhi;)V|2|42|7aacbcc1a224e13392f39b8d4ebd8eff65f730b32b69e076972b4fdb6d1c72d2|0,19,41",
        "base|save_enabled|jb|b|()Z|1|2|95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2|0",
        "base|maintenance|jb|a|()Z|1|131|0f20d1a8571e044993e082858d77f9a7ec82e934fc4600941ce332955a1ca72a|0,76,87,99,105",
        "dedicated|acquire|he|c|(II)Lhi;|1|394|82a55898efef7c2b7184551f9ac64de16f942cd04592b560969a39348498ae7a|0,20,78,100,112,128,391",
        "dedicated|save_pass|he|a|(ZLpj;)Z|1|119|7b696be3c0e803173215fd72f344a9453efc6310a2e80e6bc0bb094c86ade8e8|0,11,74,88,118",
        "dedicated|extra_wrapper|he|a|(Lhi;)V|2|31|4fae4366297d9b0812e3d3ba14f2f7b8e74c357d7462c084ddb1803f78887d04|0,30",
        "dedicated|main_wrapper|he|b|(Lhi;)V|2|42|fac3041962c87746931e865c51f8d576ab46ecdb88fc96e43897b8086d34a617|0,19,41",
        "dedicated|save_enabled|he|b|()Z|1|16|50852624935189392b0b24a64bdc6ff3fb02675377825be22d8e2947cbbd42a9|0",
        "dedicated|maintenance|he|a|()Z|1|141|871595258368ffba04ad26a32d7c5d04a5809cc75421ab196b763cb19d8fb7bd|0,86,97,109,115",
        "dedicated|drop|he|d|(II)V|1|90|42deeff63db45c9e9f203ee2a138fd732a7f81d339a4e2915f6fca7fe113506c|0,89",
        "base,dedicated|world_time|dj|a|(J)V|1|9|290fe9fe23ef3b2fcac19e5bb34433e2fe0c3df2e1f2e52cc07e90a27b831682|0,8",
        "base,dedicated|world_time_get|dj|m|()J|1|8|740127b35e4a8179dba415c21e1219e2f8df40b83de974bfff7a666ae5fe796f|0",
        "base,dedicated|world_save|dj|a|(ZLpj;)V|1|54|44ea94e3f9cb4e8838a3f85f705f5aa1af6ceee9dc80a2e593adc2f1cdbe3333|0,26,47,53",
        "base,dedicated|world_metadata|dj|w|()V|2|29|f4bb9d08ba7118d97b178266cbf933c09199dee471d2960386d4b27cbbc28bf2|0,28",
        "base,dedicated|world_session|dj|k|()V|1|10|86b589be6f7f1e96583276c6c28a3c5dcbbd484253a67469bd8e81d6be4c2bf1|0",
        "base,dedicated|register_tiles|dj|a|(Ljava/util/Collection;)V|1|33|75eecaa52b5457c44c1c4a1bac00e5f697f70ed674dbaba81927a5c76d15a378|0",
        "base,dedicated|register_entities|dj|a|(Ljava/util/List;)V|1|44|2101e5c1197a000249ecca5834a4dcd4f7bc8bda62639cd28937b965c1749c90|0",
        "base,dedicated|defer_entities|dj|b|(Ljava/util/List;)V|1|12|6867d5cf5e5e3c87e081237a246ca5c514d2e9d7d7f1e4845154c05c8fd2b621|0",
        "base,dedicated|eligibility|hi|a|(Z)Z|1|70|71b620247e1396a927d20f398e7b85ba06af7b533fec224a1d2591deccd1ab6b|0",
        "base,dedicated|activate|hi|d|()V|1|52|4a07f07f0014ccd0b442a21b9b2fe1fddadda9cf293f44bd1465ec11bf4d3d22|0,5,51",
        "base,dedicated|detach|hi|e|()V|1|77|822fbf0f598d8b22a1d7a186e419bd944713681a663e5c8803361a593da64549|0,5,67,76",
        "base,dedicated|dirty|hi|f|()V|1|6|109bb7eea1ad46dd309cdba4b89229aecc23025e3eec652a3261742388a691f7|0",
        "base,dedicated|initialize_chunk|hi|c|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "base,dedicated|membership_add|hi|a|(Llq;)V|1|155|9a415745dc90deb09143fe0551ea38b5500b2079856e7e70f684ca59a3e01e47|0,5",
        "base,dedicated|membership_remove|hi|b|(Llq;)V|1|10|a6491dffd06fbc71bd6f1cb04e43dffdc2bd8f2d6dec8d419845faabf6d99158|0",
        "base,dedicated|membership_remove_slice|hi|a|(Llq;I)V|1|37|8d04a99ea0f993eaa413bad17ab5cb491308d187c87fcdc9c0b5d71b39ae3dca|0",
        "base,dedicated|load|hb|a|(Ldj;II)Lhi;|1|278|04af2e7d7021541054131879e77e901e013ba8ff15f9ce2e56d916f82d9a186e|0,277",
        "base,dedicated|write|hb|a|(Ldj;Lhi;)V|1|106|de8cbb5272fd6ba5e6a5d74e3985db5298a5e2b89fb4476bf0442dcc7d417146|0,4,51,61,89,105",
        "base,dedicated|loader_extra|hb|b|(Ldj;Lhi;)V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "base,dedicated|loader_maintenance|hb|a|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "base,dedicated|loader_flush|hb|b|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0",
        "base,dedicated|serialize_chunk|mg|a|(Lhi;Ldj;Liq;)V|9|293|228cdc8b09f273853918b0f6ede4d8a5f7f41c0d0bdf8093e738abb16740c4bc|0,108",
        "base,dedicated|save_info_wrapper|ie|a|(Lct;Ljava/util/List;)V|1|14|7b62d683f628803f4ab5d5e02e4a6a00eabac19b1e0a1e04405680e9e5aab246|0",
        "base,dedicated|save_info|dq|a|(Lct;Ljava/util/List;)V|1|151|7212372023608a58f6caada1e21bc28e989193bcb7aa61ac48088959ed90a9e8|0",
        "base,dedicated|session_check|dq|b|()V|1|81|5479dbd16e682f5e2f4606191e04c945bb9a4a32807e2b294532d2d0bad04bf7|0",
        "base,dedicated|auxiliary|es|a|()V|1|53|2d59fd34d8c4fd3c0b72619300ac333c7d2f235d51f4b6c2199106d04d2f0978|0",
        "base,dedicated|provider_maintenance|pk|a|()Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0",
        "base,dedicated|timestamp|jx|b|(III)V|2|41|3de743de9199f459f5655eaea989dbe45f0e7823a4df3ed0a79e8b9f1c1ff002|0,40",
        "base,dedicated|region_write|jx|a|(II[BI)V|36|445|2143d53dce4ade104faea1d774cbc4b21e8bf72a45270282ae08dc60def18e14|0,420,444",
        "base,dedicated|forbidden_generate|pk|b|(II)Lhi;|1|130|97eed3df6ca2056a265b15cf5ed4f8eb9eca856242bd449dad51ca93f6f640af|0",
        "base,dedicated|forbidden_populate|pk|a|(Lbl;II)V|1|2907|2002a2a4dead7af615b3a6db2fe9030654a244e688e40b8d0444e5b8b1be7a57|0",
        "base,dedicated|forbidden_finish|hi|b|()V|1|222|581a542efcc76d4056af780ec9868aaa47ba949628aaee64c560c5f9a836063c|0",
        "base,dedicated|blank_eligibility|hg|a|(Z)Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0",
        "base,dedicated|handler_constructor|ie|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|8|0a89ec8d2eb1c824548206cf5622efd57da63ee5432ca47400b7266a042fe111|0",
        "base,dedicated|handler_base_constructor|dq|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|91|7cf0421755a4a2bd940084b9618d591ad4a22c34f2e2079576eace602bdaa0fa|0",
        "base,dedicated|handler_session_write|dq|f|()V|2|71|ff8f68ff7a214e7c7e115771505091fb487c95ed8ac0f561e76f9ea011f008a7|0",
    };
    static void require(boolean value, String message) { ChunkLifecycleObservation.require(value, message); }
    static Map<String,Object> object(Object... values) { return ChunkLifecycleObservation.object(values); }
    static String hash(byte[] values) throws Exception { return ChunkLifecycleObservation.hash(values); }
    static long remaining(long deadline) {
        long value = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime());
        require(value > 0, "controller/debuggee deadline exceeded"); return Math.min(value, 10000);
    }
    static ObjectReference reference(Value value) {
        require(value instanceof ObjectReference, "nonnull object reference"); return (ObjectReference) value;
    }
    static int integer(Value value) { require(value instanceof IntegerValue, "Integer value"); return ((IntegerValue) value).value(); }
    static Value read(ObjectReference value, ReferenceType owner, String name) {
        Field member = owner.fieldByName(name);
        require(member != null && member.declaringType().equals(owner), "declared field " + owner.name() + "." + name);
        return value == null ? owner.getValue(member) : value.getValue(member);
    }
    static Value bootstrap(ObjectReference value, String name, String signature) {
        require(value.referenceType().classLoader() == null, "bootstrap object owner");
        Field member = value.referenceType().fieldByName(name);
        require(member != null && member.declaringType().classLoader() == null && member.signature().equals(signature), "bootstrap field " + name);
        return value.getValue(member);
    }
    static Object scalar(Value value) {
        if (value == null) return null;
        if (value instanceof VoidValue) return null;
        if (value instanceof BooleanValue item) return item.value();
        if (value instanceof IntegerValue item) return item.value();
        if (value instanceof LongValue item) return ChunkLifecycleObservation.hex(item.value());
        if (value instanceof StringReference item) return item.value();
        if (value instanceof ByteValue item) return item.value();
        if (value instanceof DoubleValue item) return ChunkLifecycleObservation.hex(Double.doubleToRawLongBits(item.value()));
        if (value instanceof FloatValue item) return String.format(java.util.Locale.ROOT, "%08x", Float.floatToRawIntBits(item.value()));
        if (value instanceof ObjectReference item) return object("object_id", item.uniqueID(), "class", item.referenceType().name());
        throw new IllegalStateException("unsupported JDI scalar");
    }
    static byte[] capture(InputStream source, CompletableFuture<String> startup, AtomicLong total) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream(), line = startup == null ? null : new ByteArrayOutputStream();
        byte[] block = new byte[8192];
        try {
            for (int count; (count = source.read(block)) >= 0;) {
                require(total.addAndGet(count) <= STREAM_LIMIT, "combined child stream limit"); out.write(block, 0, count);
                if (line != null) for (int i = 0; i < count; i++) {
                    if (block[i] == '\n') {
                        startup.complete(line.toString(StandardCharsets.US_ASCII).stripTrailing()); line = null; break;
                    }
                    require(line.size() < 1024, "JDWP startup line bound"); line.write(block[i]);
                }
            }
            if (line != null) startup.completeExceptionally(new IllegalStateException("missing JDWP startup"));
            return out.toByteArray();
        } catch (Throwable failure) { if (startup != null) startup.completeExceptionally(failure); throw failure; }
    }
    static void newOutput(Path path) {
        require(Files.isDirectory(path.getParent()) && !Files.exists(path, LinkOption.NOFOLLOW_LINKS)
                && !Files.isSymbolicLink(path), "create-new bounded output " + path);
    }
    static void text(Writer out, String value, int start, int length, long[] count, int limit) throws Exception {
        require(length >= 0 && count[0] <= limit - (long) length, "complete JSON output bound");
        out.write(value, start, length); count[0] += length;
    }
    static void text(Writer out, String value, long[] count, int limit) throws Exception {
        text(out, value, 0, value.length(), count, limit);
    }
    static void streamJson(Writer out, Object value, long[] count, int limit, int depth) throws Exception {
        require(depth <= 64, "bounded JSON depth");
        if (value == null) { text(out, "null", count, limit); return; }
        if (value instanceof String valueText) {
            text(out, "\"", count, limit); int start = 0;
            for (int i = 0; i < valueText.length(); i++) {
                char c = valueText.charAt(i);
                if (c == '"' || c == '\\' || c < 32 || c > 126) {
                    text(out, valueText, start, i - start, count, limit);
                    text(out, c == '"' || c == '\\' ? "\\" + c : String.format(java.util.Locale.ROOT, "\\u%04x", (int) c), count, limit);
                    start = i + 1;
                }
            }
            text(out, valueText, start, valueText.length() - start, count, limit); text(out, "\"", count, limit); return;
        }
        if (value instanceof Number || value instanceof Boolean) { text(out, value.toString(), count, limit); return; }
        if (value instanceof Map<?,?> map) {
            text(out, "{", count, limit); boolean comma = false;
            for (var entry : map.entrySet()) {
                require(entry.getKey() instanceof String, "JSON string key");
                if (comma) text(out, ",", count, limit); comma = true;
                streamJson(out, entry.getKey(), count, limit, depth + 1); text(out, ":", count, limit);
                streamJson(out, entry.getValue(), count, limit, depth + 1);
            }
            text(out, "}", count, limit); return;
        }
        if (value instanceof Iterable<?> values) {
            text(out, "[", count, limit); boolean comma = false;
            for (Object item : values) {
                if (comma) text(out, ",", count, limit); comma = true;
                streamJson(out, item, count, limit, depth + 1);
            }
            text(out, "]", count, limit); return;
        }
        throw new IllegalStateException("JSON value " + value.getClass());
    }
    static void write(Path path, Object value, int limit) throws Exception {
        Path partial = Path.of(path + ".partial"); newOutput(partial);
        try (Writer out = new BufferedWriter(Files.newBufferedWriter(partial, StandardCharsets.US_ASCII, StandardOpenOption.CREATE_NEW), 65536)) {
            long[] count = {0}; streamJson(out, value, count, limit, 0); text(out, "\n", count, limit);
        }
        newOutput(path); Files.move(partial, path, StandardCopyOption.ATOMIC_MOVE);
    }
    static final class Session {
        final VirtualMachine vm;
        final EventRequestManager manager;
        final String context, scenario;
        final boolean client, dedicated;
        final Map<String,ReferenceType> types = new LinkedHashMap<>();
        final Map<Method,String> watched = new LinkedHashMap<>();
        final Map<String,Object> bindings = new LinkedHashMap<>(), fields = new LinkedHashMap<>(), blobs = new LinkedHashMap<>();
        final Map<String,byte[]> blobBytes = new LinkedHashMap<>();
        final Map<Long,String> arrayHashes = new LinkedHashMap<>();
        final Map<Integer,Long> identityIDs = new LinkedHashMap<>();
        final Map<Long,ObjectReference> chunkObjects = new LinkedHashMap<>();
        final List<Object> operations = new ArrayList<>(), setupInventories = new ArrayList<>();
        final List<MethodExitRequest> exits = new ArrayList<>();
        final Map<String,Integer> unwatchedReturns = new LinkedHashMap<>();
        final Map<String,Integer> headerIndices = new LinkedHashMap<>();
        final List<Object> chunkHeaders = new ArrayList<>();
        final Map<String,Integer> chunkStateIndices = new LinkedHashMap<>();
        final List<Object> chunkStates = new ArrayList<>();
        final Map<Long,ObjectReference> externalObjects = new LinkedHashMap<>();
        final Map<Long,Object> externalRandomStates = new LinkedHashMap<>();
        // Values may be reused only while one suspended boundary is being read.
        // The next boundary always creates new tables and rereads the VM.
        Map<Long,Map<ReferenceType,Map<Field,Value>>> boundaryValues;
        Map<Long,Object> boundaryHeaders;
        ObjectReference world, cache, storage, owner, competingStorage;
        ClassLoaderReference gameLoader;
        ReferenceType worldType, cacheType, chunkType;
        ThreadReference measuredThread;
        MethodExitRequest iteratorExits;
        ExceptionRequest sessionExceptions;
        final List<Long> exceptionIDs = new ArrayList<>();
        int exceptionEventCount;
        List<Object> events;
        Object before;
        int active = -1, beginCount, endCount, identityMarkers, deliveredEvents, originalRequests, exitEnableCount, exitDisableCount;
        boolean setupSeen, installed, maintenance, exitsEnabled;
        String action;
        long decodedBlobBytes;
        Session(VirtualMachine vm, String context, String scenario) {
            this.vm = vm; this.context = context; this.scenario = scenario;
            client = context.equals("client"); dedicated = context.equals("dedicated"); manager = vm.eventRequestManager();
            ClassPrepareRequest prepare = manager.createClassPrepareRequest(); prepare.addClassFilter("ChunkLifecycleObservation");
            prepare.setSuspendPolicy(EventRequest.SUSPEND_ALL); prepare.enable();
        }
        ReferenceType type(String name) {
            ReferenceType saved = types.get(name); if (saved != null) return saved;
            List<ReferenceType> found = new ArrayList<>();
            for (ReferenceType value : vm.classesByName(name)) if (gameLoader.equals(value.classLoader())) found.add(value);
            require(found.size() == 1, "unique original-loader type " + name);
            types.put(name, found.get(0)); return found.get(0);
        }
        Value get(ObjectReference value, ReferenceType declaring, String name) {
            Field member = declaring.fieldByName(name);
            require(member != null && member.declaringType().equals(declaring) && gameLoader.equals(declaring.classLoader()), "original field binding");
            fields.put(declaring.name() + "." + name, object("class", declaring.name(), "name", name,
                       "signature", member.signature(), "modifiers", member.modifiers()));
            return boundaryValue(value, declaring, member);
        }
        Value boundaryValue(ObjectReference value, ReferenceType declaring, Field member) {
            if (boundaryValues == null) return value.getValue(member);
            Map<ReferenceType,Map<Field,Value>> owners = boundaryValues.computeIfAbsent(value.uniqueID(), key -> new HashMap<>());
            Map<Field,Value> values = owners.get(declaring);
            if (values == null) {
                List<Field> members = declaring.fields().stream().filter(field -> !field.isStatic()).toList();
                require(members.size() <= 256 && members.contains(member), "bounded declared instance fields");
                values = value.getValues(members); owners.put(declaring, values);
            }
            require(values.containsKey(member), "complete boundary field read"); return values.get(member);
        }
        Value boot(ObjectReference value, String name, String signature) {
            require(value.referenceType().classLoader() == null, "bootstrap object owner");
            Field member = value.referenceType().fieldByName(name);
            require(member != null && member.declaringType().classLoader() == null && member.signature().equals(signature), "bootstrap field " + name);
            return boundaryValue(value, member.declaringType(), member);
        }
        ObjectReference cacheObject(String name) { return reference(get(cache, cacheType, name)); }
        BreakpointRequest stop(Method method, long pc, String role, int policy) {
            Location site = method.locationOfCodeIndex(pc); require(site != null && site.codeIndex() == pc, "actual instruction location");
            BreakpointRequest request = manager.createBreakpointRequest(site); request.putProperty("role", role);
            request.setSuspendPolicy(policy); request.enable(); return request;
        }
        void prepared(ClassPrepareEvent event) {
            ReferenceType helper = event.referenceType(); require(helper.classLoader() != null, "helper application loader");
            for (String name : new String[] {"setup","begin","end","identity"}) {
                List<Method> values = helper.methodsByName(name); require(values.size() == 1, "unique helper marker");
                stop(values.get(0), 0, name, name.equals("identity") ? EventRequest.SUSPEND_EVENT_THREAD : EventRequest.SUSPEND_ALL);
            }
        }
        Method bind(String role, ReferenceType declaring, String name, String signature, int modifiers, int length, String digest) throws Exception {
            List<Method> methods = declaring.methodsByName(name, signature);
            require(methods.size() == 1 && methods.get(0).declaringType().equals(declaring), "declared original method " + role);
            Method method = methods.get(0); byte[] code = method.bytecodes();
            require(method.modifiers() == modifiers && code.length == length && hash(code).equals(digest), "original method fingerprint " + role);
            bindings.put(role, object("class", declaring.name(), "method", name, "signature", signature,
                "modifiers", modifiers, "bytecode_length", length, "bytecode_sha256", digest));
            return method;
        }
        void install() throws Exception {
            worldType = type(client ? "fd" : "dj"); cacheType = type(dedicated ? "he" : client ? "ok" : "jb");
            chunkType = type(client ? "lm" : "hi");
            require(world.referenceType().equals(type(dedicated ? "dp" : worldType.name())) && cache.referenceType().equals(cacheType), "actual context classes");
            Set<ReferenceType> exitTypes = new HashSet<>();
            for (String spec : SPECS) {
                String[] row = spec.split("\\|", -1); require(row.length == 9, "method specification");
                if (!Arrays.asList(row[0].split(",")).contains(context)) continue;
                if (row[1].startsWith("external_") && !scenario.equals("detach-owned")) continue;
                ReferenceType declaring = type(row[2]);
                Method method = bind(row[1], declaring, row[3], row[4], Integer.parseInt(row[5]), Integer.parseInt(row[6]), row[7]);
                watched.put(method, row[1]); exitTypes.add(declaring);
                for (String pc : row[8].split(",")) stop(method, Long.parseLong(pc), row[1], EventRequest.SUSPEND_EVENT_THREAD);
                originalRequests++;
            }
            for (ReferenceType declaring : exitTypes) {
                MethodExitRequest request = manager.createMethodExitRequest(); request.addClassFilter(declaring.name()); request.addThreadFilter(measuredThread);
                request.putProperty("declaring_name", declaring.name());
                request.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD); exits.add(request);
            }
            List<ReferenceType> iterators = vm.classesByName("java.util.HashMap$KeyIterator");
            require(iterators.size() == 1 && iterators.get(0).classLoader() == null, "pinned bootstrap key iterator");
            Method iterator = iterators.get(0).methodsByName("next", "()Ljava/lang/Object;").get(0); byte[] code = iterator.bytecodes();
            String digest = hash(code);
            require(code.length == 8 && (digest.equals("c854630810f7ba488ba0109df5f49e63ae87fcf2ee2b62e19cf0e1d08ae853de")
                    || digest.equals("3f8e40d2efb5d40dee4192240f43d9089e3e4bc335ce05b22b9a314a1346dd2a")), "supported actual iterator bytecodes");
            bindings.put("pending_next", object("class", iterators.get(0).name(), "method", "next", "signature", "()Ljava/lang/Object;",
                "modifiers", iterator.modifiers(), "bytecode_length", code.length, "bytecode_sha256", digest));
            Method constructor = iterators.get(0).methodsByName("<init>", "(Ljava/util/HashMap;)V").get(0);
            byte[] constructorCode = constructor.bytecodes(); String constructorDigest = hash(constructorCode);
            require(constructor.modifiers() == 0 && (constructorCode.length == 11 && constructorDigest.equals("8630cf4bac8c6df428ece4ecb6d9c7d1a4c46044b80718e5ec67bf2bf8014618")
                    || constructorCode.length == 6 && constructorDigest.equals("405f81f0b51683db4ab14a729fb164a2c889439967d0bfe6248ebe8183a4873d")), "supported actual iterator constructor bytecodes");
            bindings.put("pending_iterator_construct", object("class", iterators.get(0).name(), "method", "<init>", "signature", constructor.signature(),
                "modifiers", 0, "bytecode_length", constructorCode.length, "bytecode_sha256", constructorDigest));
            iteratorExits = manager.createMethodExitRequest(); iteratorExits.addClassFilter(iterators.get(0).name()); iteratorExits.addThreadFilter(measuredThread);
            iteratorExits.putProperty("declaring_name", iterators.get(0).name());
            iteratorExits.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD); installed = true;
            sessionExceptions = manager.createExceptionRequest(type(client ? "us" : "my"), true, true);
            sessionExceptions.addThreadFilter(measuredThread); sessionExceptions.addClassFilter(client ? "fm" : "dq");
            sessionExceptions.setSuspendPolicy(EventRequest.SUSPEND_EVENT_THREAD);
        }
        void originalExits(boolean enabled) {
            require(exitsEnabled != enabled, "balanced original exit observation intervals");
            for (MethodExitRequest request : exits) if (enabled) request.enable(); else request.disable();
            if (enabled) sessionExceptions.enable(); else sessionExceptions.disable();
            exitsEnabled = enabled; if (enabled) exitEnableCount++; else exitDisableCount++;
        }
        List<Object> map(ObjectReference value, boolean set) {
            require(value.referenceType().name().equals("java.util.HashMap"), "actual HashMap");
            int size = integer(boot(value, "size", "I")); require(size >= 0 && size <= 256, "map bound");
            ArrayReference table = (ArrayReference) boot(value, "table", "[Ljava/util/HashMap$Node;");
            List<Object> result = new ArrayList<>(); if (table == null) { require(size == 0, "null empty table"); return result; }
            require(table.length() <= 1024, "map table bound"); Set<Long> seen = new HashSet<>();
            for (Value head : table.getValues()) for (ObjectReference node = (ObjectReference) head; node != null;
                 node = (ObjectReference) boot(node, "next", "Ljava/util/HashMap$Node;")) {
                require(seen.add(node.uniqueID()) && seen.size() <= 256, "complete acyclic map chains");
                ObjectReference key = reference(boot(node, "key", "Ljava/lang/Object;"));
                require(key.referenceType().name().equals("java.lang.Integer"), "actual Integer map key");
                int number = integer(boot(key, "value", "I"));
                result.add(set ? number : object("key", number, "chunk", header((ObjectReference) boot(node, "value", "Ljava/lang/Object;"))));
            }
            require(result.size() == size, "complete map count"); return result;
        }
        List<Object> list(ObjectReference value, boolean chunks) {
            require(value.referenceType().name().equals("java.util.ArrayList"), "actual ArrayList");
            int size = integer(boot(value, "size", "I")); require(size >= 0 && size <= 256, "list bound");
            ArrayReference data = (ArrayReference) boot(value, "elementData", "[Ljava/lang/Object;");
            require(data != null && size <= data.length() && data.length() <= 1024, "list backing owner");
            List<Object> result = new ArrayList<>();
            for (Value item : data.getValues(0, size)) result.add(chunks ? header((ObjectReference) item) : scalar(item));
            return result;
        }
        Object header(ObjectReference value) {
            if (value == null) return null;
            require(value.referenceType().classLoader().equals(gameLoader), "original chunk loader");
            require(chunkObjects.containsKey(value.uniqueID()) || chunkObjects.size() < 128, "retained chunk bound");
            chunkObjects.putIfAbsent(value.uniqueID(), value); Map<String,Object> flags = object();
            if (boundaryHeaders != null && boundaryHeaders.containsKey(value.uniqueID())) return boundaryHeaders.get(value.uniqueID());
            for (String name : new String[] {"c","n","o","p","q"}) flags.put(name, scalar(get(value, chunkType, name)));
            Object result = object("id", value.uniqueID(), "class", value.referenceType().name(), "world", reference(get(value, chunkType, "d")).uniqueID(),
                "coordinates", Arrays.asList(integer(get(value, chunkType, "j")), integer(get(value, chunkType, "k"))),
                "flags", flags, "minimum", scalar(get(value, chunkType, "i")), "last_save", scalar(get(value, chunkType, "r")));
            String key = new String(ChunkLifecycleObservation.encoded(result), StandardCharsets.US_ASCII);
            Integer index = headerIndices.get(key);
            if (index == null) {
                require(chunkHeaders.size() < 4096, "bounded complete chunk header dictionary");
                index = chunkHeaders.size(); headerIndices.put(key, index); chunkHeaders.add(result);
            }
            Object retained = object("header_reference", index);
            if (boundaryHeaders != null) boundaryHeaders.put(value.uniqueID(), retained); return retained;
        }
        Object blob(ArrayReference array, int length) throws Exception {
            require(array != null && array.length() == length, "complete original array length"); byte[] raw = new byte[length];
            List<Value> values = array.getValues();
            for (int i = 0; i < length; i++) { require(values.get(i) instanceof ByteValue, "actual byte array"); raw[i] = ((ByteValue) values.get(i)).value(); }
            String known = arrayHashes.get(array.uniqueID());
            if (known != null && Arrays.equals(blobBytes.get(known), raw)) return object("id", array.uniqueID(), "bytes", length, "sha256", known);
            String sha = hash(raw); byte[] old = blobBytes.get(sha);
            if (old == null) {
                require(blobs.size() < 4096 && decodedBlobBytes <= 64L * 1024 * 1024 - raw.length, "trace blob/decoded bytes bound");
                decodedBlobBytes += raw.length; blobBytes.put(sha, raw.clone());
                blobs.put(sha, object("bytes", length, "hex", HexFormat.of().formatHex(raw)));
            } else require(Arrays.equals(old, raw), "trace blob collision");
            arrayHashes.put(array.uniqueID(), sha);
            return object("id", array.uniqueID(), "bytes", length, "sha256", sha);
        }
        Object fullChunk(ObjectReference value) throws Exception {
            String[] names = {"b","e","f","g","h"}, roles = {"blocks","metadata","skylight","block_light","heightmap"};
            int[] lengths = {32768,16384,16384,16384,256}; Map<String,Object> arrays = object();
            for (int i = 0; i < 5; i++) {
                ObjectReference field = reference(get(value, chunkType, names[i]));
                ArrayReference array = i > 0 && i < 4 ? (ArrayReference) get(field, type(client ? "wi" : "ob"), "a") : (ArrayReference) field;
                arrays.put(roles[i], object("owner", field.uniqueID(), "data", blob(array, lengths[i])));
            }
            ObjectReference tiles = reference(get(value, chunkType, "l"));
            int tileCount = integer(boot(tiles, "size", "I"));
            require(scenario.equals("detach-owned") ? tileCount <= 1 : tileCount == 0, "finite tile map");
            List<Object> tileEntries = new ArrayList<>();
            if (scenario.equals("detach-owned")) for (ObjectReference node : hashNodes(tiles)) {
                ObjectReference key = reference(boot(node, "key", "Ljava/lang/Object;"));
                ObjectReference tile = reference(boot(node, "value", "Ljava/lang/Object;")); retainExternal(tile);
                Map<String,Object> keyFields = new TreeMap<>();
                for (Field member : key.referenceType().fields()) if (!member.isStatic()) {
                    require(member.signature().equals("I"), "finite integer tile key"); keyFields.put(member.name(), scalar(get(key, key.referenceType(), member.name())));
                }
                tileEntries.add(object("key", key.uniqueID(), "key_class", key.referenceType().name(), "key_fields", keyFields, "value", tile.uniqueID()));
            }
            ArrayReference slices = (ArrayReference) get(value, chunkType, "m"); require(slices.length() == 8, "eight entity slices");
            List<Object> entities = new ArrayList<>(); for (Value slice : slices.getValues()) {
                ObjectReference actual = reference(slice);
                if (scenario.equals("detach-owned")) for (ObjectReference member : listObjects(actual)) retainExternal(member);
                entities.add(object("id", actual.uniqueID(), "members", list(actual, false)));
            }
            Object result = object("chunk", header(value), "arrays", arrays, "tiles", scenario.equals("detach-owned") ? object("id", tiles.uniqueID(), "size", tileCount, "entries", tileEntries) : object("id", tiles.uniqueID(), "size", 0),
                          "slices_owner", slices.uniqueID(), "entities", entities);
            String key = new String(ChunkLifecycleObservation.encoded(result), StandardCharsets.US_ASCII);
            Integer index = chunkStateIndices.get(key);
            if (index == null) {
                require(chunkStates.size() < 4096, "bounded complete chunk-state dictionary");
                index = chunkStates.size(); chunkStateIndices.put(key, index); chunkStates.add(result);
            }
            return object("chunk_reference", index);
        }
        Object state(boolean full, ObjectReference target) throws Exception {
            require(boundaryValues == null && boundaryHeaders == null, "nonrecursive boundary read");
            boundaryValues = new HashMap<>(); boundaryHeaders = new HashMap<>();
            try { return stateValues(full || scenario.equals("detach-owned"), target); }
            finally { boundaryValues = null; boundaryHeaders = null; }
        }
        Object stateValues(boolean full, ObjectReference target) throws Exception {
            ObjectReference values = cacheObject(dedicated ? "f" : "e"), ordered = cacheObject(dedicated ? "g" : "f");
            ObjectReference pending = cacheObject(dedicated ? "b" : "a");
            ObjectReference underlying = reference(boot(pending, "map", "Ljava/util/HashMap;"));
            List<Object> entries = map(values, false), insertions = list(ordered, true), pendingKeys = map(underlying, true);
            header(cacheObject(dedicated ? "c" : "b"));
            List<Object> locals = new ArrayList<>();
            for (ObjectReference value : new ArrayList<>(chunkObjects.values())) locals.add(full || value.equals(target) ? fullChunk(value) : header(value));
            Map<String,Object> metadata = object(); ObjectReference info = reference(get(world, worldType, "x"));
            ReferenceType infoType = type(client ? "ei" : "ct");
            for (int i = 0; i < 15; i++) metadata.put(String.valueOf((char) ('a' + i)), scalar(get(info, infoType, String.valueOf((char) ('a' + i)))));
            Map<String,Object> registrations = object();
            for (String name : new String[] {"C","b","c","d","e","D","G"}) {
                ObjectReference actual = reference(get(world, worldType, name));
                if (scenario.equals("detach-owned") && !name.equals("C")) for (ObjectReference member : listObjects(actual)) retainExternal(member);
                registrations.put(name, object("id", actual.uniqueID(), "values", list(actual, false)));
            }
            Map<String,Object> result = object("snapshot_kind", full ? "all-retained-chunks-full" : "all-headers-and-target-full",
                "world_id", world.uniqueID(), "cache_id", cache.uniqueID(), "storage_id", storage.uniqueID(), "info_id", info.uniqueID(),
                "map", object("id", values.uniqueID(), "entries", entries), "insertions", object("id", ordered.uniqueID(), "values", insertions),
                "pending", object("id", pending.uniqueID(), "map_id", underlying.uniqueID(), "values", pendingKeys),
                "chunks", locals, "metadata", metadata, "registrations", registrations,
                "handler", handler(storage), "competing_handler", competingStorage == null ? null : handler(competingStorage),
                "world_force_load", scalar(get(world, worldType, "y")), "save_off", dedicated ? scalar(get(world, type("dp"), "E")) : false,
                "provider_force_load", dedicated ? scalar(get(cache, cacheType, "a")) : null);
            if (scenario.equals("detach-owned")) result.put("external_state", externalState()); return result;
        }
        List<ObjectReference> hashNodes(ObjectReference value) {
            require(value.referenceType().name().equals("java.util.HashMap"), "finite external HashMap");
            int size = integer(boot(value, "size", "I")); require(size >= 0 && size <= 16, "external map bound");
            ArrayReference table = (ArrayReference) boot(value, "table", "[Ljava/util/HashMap$Node;");
            List<ObjectReference> result = new ArrayList<>(); Set<Long> seen = new HashSet<>();
            if (table != null) {
                require(table.length() <= 256, "external map owner bound");
                for (Value head : table.getValues()) for (ObjectReference node = (ObjectReference) head; node != null;
                    node = (ObjectReference) boot(node, "next", "Ljava/util/HashMap$Node;")) {
                    require(seen.add(node.uniqueID()) && seen.size() <= 16, "complete external map chains"); result.add(node);
                }
            }
            require(result.size() == size, "external complete map count"); return result;
        }
        List<ObjectReference> listObjects(ObjectReference value) {
            require(value.referenceType().name().equals("java.util.ArrayList"), "finite external ArrayList");
            int size = integer(boot(value, "size", "I")); require(size >= 0 && size <= 16, "external list bound");
            ArrayReference array = (ArrayReference) boot(value, "elementData", "[Ljava/lang/Object;");
            require(array != null && array.length() <= 256 && size <= array.length(), "external list owner");
            List<ObjectReference> result = new ArrayList<>(); for (Value item : array.getValues(0, size)) result.add(reference(item)); return result;
        }
        void retainExternal(ObjectReference value) {
            require(value.referenceType().classLoader().equals(gameLoader)
                && (value.referenceType().name().equals(client ? "fz" : "eb") || value.referenceType().name().equals(client ? "yk" : "pq")), "finite actual Boat/Sign");
            require(externalObjects.containsKey(value.uniqueID()) || externalObjects.size() < 8, "retained external object bound");
            externalObjects.putIfAbsent(value.uniqueID(), value);
        }
        Object externalValue(Value value, Set<Long> seen, int depth) throws Exception {
            if (value == null) return null;
            if (value instanceof BooleanValue item) return item.value();
            if (value instanceof IntegerValue item) return item.value();
            if (value instanceof ByteValue item) return object("byte", (int) item.value());
            if (value instanceof ShortValue item) return object("short", (int) item.value());
            if (value instanceof CharValue item) return object("char", (int) item.value());
            if (value instanceof LongValue item) return object("long", ChunkLifecycleObservation.hex(item.value()));
            if (value instanceof DoubleValue item) return object("binary64", ChunkLifecycleObservation.hex(Double.doubleToRawLongBits(item.value())));
            if (value instanceof FloatValue item) return object("binary32", String.format(java.util.Locale.ROOT, "%08x", Float.floatToRawIntBits(item.value())));
            if (value instanceof StringReference item) return item.value();
            ObjectReference item = reference(value); String name = item.referenceType().name();
            if (item.equals(world) || item.equals(cache) || item.equals(storage)) return object("borrowed", item.uniqueID());
            if (item.referenceType().classLoader() == null && Arrays.asList("java.lang.Byte", "java.lang.Short", "java.lang.Character", "java.lang.Integer", "java.lang.Long", "java.lang.Float", "java.lang.Double", "java.lang.Boolean").contains(name)) {
                Field member = item.referenceType().fieldByName("value"); return externalValue(boundaryValue(item, member.declaringType(), member), seen, depth + 1);
            }
            require(depth <= 12 && (seen.contains(item.uniqueID()) || seen.size() < 128), "bounded complete external ownership graph");
            if (!seen.add(item.uniqueID())) return object("reference", item.uniqueID());
            Map<String,Object> result = object("id", item.uniqueID(), "class", name);
            if (item instanceof ArrayReference array) {
                require(array.length() <= 256, "external array bound"); List<Object> values = new ArrayList<>();
                for (Value element : array.getValues()) values.add(externalValue(element, seen, depth + 1)); result.put("values", values); return result;
            }
            if (name.equals("java.util.HashMap")) {
                List<Object> entries = new ArrayList<>(); for (ObjectReference node : hashNodes(item)) entries.add(object(
                    "key", externalValue(boot(node, "key", "Ljava/lang/Object;"), seen, depth + 1),
                    "value", externalValue(boot(node, "value", "Ljava/lang/Object;"), seen, depth + 1)));
                result.put("entries", entries); return result;
            }
            if (name.equals("java.util.ArrayList")) {
                List<Object> values = new ArrayList<>(); for (ObjectReference member : listObjects(item)) values.add(externalValue(member, seen, depth + 1));
                result.put("values", values); return result;
            }
            if (name.equals("java.util.Random")) {
                ObjectReference seed = reference(boot(item, "seed", "Ljava/util/concurrent/atomic/AtomicLong;"));
                Object state = object("seed_owner", seed.uniqueID(), "seed", externalValue(boot(seed, "value", "J"), seen, depth + 1),
                    "have_next_gaussian", externalValue(boot(item, "haveNextNextGaussian", "Z"), seen, depth + 1),
                    "next_gaussian", externalValue(boot(item, "nextNextGaussian", "D"), seen, depth + 1));
                Object previous = externalRandomStates.putIfAbsent(item.uniqueID(), state);
                require(previous == null || previous.equals(state), "actual external Random unchanged within VM");
                result.put("raw_state", state); return result;
            }
            require(item.referenceType().classLoader().equals(gameLoader) && item.referenceType() instanceof ClassType, "original external object " + name);
            Map<String,Object> members = new TreeMap<>();
            for (ClassType declaring = (ClassType) item.referenceType(); declaring != null && gameLoader.equals(declaring.classLoader()); declaring = declaring.superclass())
                for (Field member : declaring.fields()) if (!member.isStatic()) members.put(declaring.name() + "." + member.name(),
                    externalValue(get(item, declaring, member.name()), seen, depth + 1));
            result.put("fields", members); return result;
        }
        Object externalState() throws Exception {
            Set<Long> seen = new HashSet<>(); List<Object> objects = new ArrayList<>();
            for (ObjectReference value : externalObjects.values()) objects.add(externalValue(value, seen, 0));
            Object registry = dedicated ? externalValue(get(world, type("dp"), "G"), seen, 0) : null;
            Object result = object("encoding", "owned-objects-v1", "objects", objects, "dedicated_registry", registry,
                "random_representation", "actual-JDI-seed-and-Gaussian-fields; raw per-VM state retained", "within_vm_random_unchanged", true);
            require(ChunkLifecycleObservation.encoded(result).length <= 256 * 1024, "bounded complete external graph bytes"); return result;
        }
        Object handler(ObjectReference value) {
            ReferenceType declaring = type(client ? "fm" : "dq"); Map<String,Object> result = object("id", value.uniqueID(), "class", value.referenceType().name(),
                "token", scalar(get(value, declaring, "e")));
            String[] names = {"b","c","d"}, roles = {"world_root","players_root","data_root"};
            for (int i = 0; i < names.length; i++) {
                ObjectReference file = reference(get(value, declaring, names[i])); require(file.referenceType().name().equals("java.io.File"), "actual handler File owner");
                Value path = boot(file, "path", "Ljava/lang/String;"); require(path instanceof StringReference, "actual handler path string");
                result.put(roles[i], ((StringReference) path).value());
            }
            return result;
        }
        boolean daemon(ThreadReference thread) {
            ReferenceType type = vm.classesByName("java.lang.Thread").get(0);
            Field direct = type.fieldByName("daemon");
            if (direct != null) return ((BooleanValue) thread.getValue(direct)).value();
            ObjectReference holder = reference(thread.getValue(type.fieldByName("holder")));
            return ((BooleanValue) bootstrap(holder, "daemon", "Z")).value();
        }
        Object inventory() throws Exception {
            List<ThreadReference> threads = vm.allThreads(); require(threads.size() <= 64, "thread count bound");
            List<Object> result = new ArrayList<>(); int inert = 0, mains = 0;
            for (ThreadReference thread : threads) {
                require(thread.isSuspended() && thread.frameCount() <= 128, "complete suspended thread frames");
                List<Object> frames = new ArrayList<>(); boolean original = false, sleeping = false;
                for (StackFrame frame : thread.frames()) {
                    Location location = frame.location(); boolean game = gameLoader.equals(location.declaringType().classLoader());
                    original |= game;
                    if (game) require(dedicated && location.declaringType().name().equals("bt") && location.method().name().equals("run"), "no concurrent original thread");
                    if (location.declaringType().name().equals("java.lang.Thread") && location.method().name().startsWith("sleep")) sleeping = true;
                    frames.add(object("class", location.declaringType().name(), "method", location.method().name(), "signature", location.method().signature(), "code_index", location.codeIndex(), "original_loader", game));
                }
                boolean isDaemon = daemon(thread);
                if (original) { require(isDaemon && sleeping, "inert dedicated daemon asleep"); inert++; }
                else if (!isDaemon) { require(thread.equals(measuredThread), "only measured main non-daemon"); mains++; }
                result.add(object("id", thread.uniqueID(), "name", thread.name(), "daemon", isDaemon, "status", thread.status(), "frames", frames));
            }
            require(mains == 1 && inert == (dedicated ? 1 : 0), "exact measured/inert thread inventory"); return result;
        }
        ObjectReference target(StackFrame frame) {
            if (frame.thisObject() != null && (frame.thisObject().referenceType().equals(chunkType)
                    || frame.thisObject().referenceType().name().equals(client ? "li" : "hg"))) return frame.thisObject();
            for (Value argument : frame.getArgumentValues()) if (argument instanceof ObjectReference value &&
                    (value.referenceType().equals(chunkType) || value.referenceType().name().equals(client ? "li" : "hg"))) return value;
            return null;
        }
        Object emptyCollection(ObjectReference value) {
            require(value.referenceType().classLoader() == null, "bootstrap empty collection proof");
            if (value.referenceType().name().equals("java.util.ArrayList")) {
                int size = integer(boot(value, "size", "I")); ArrayReference data = (ArrayReference) boot(value, "elementData", "[Ljava/lang/Object;");
                require(size == 0 && data != null && data.length() <= 1024, "fresh empty ArrayList proof");
                return object("id", value.uniqueID(), "class", value.referenceType().name(), "size", size,
                    "backing_id", data.uniqueID(), "backing_length", data.length());
            }
            require(value.referenceType().name().equals("java.util.HashMap$Values"), "finite empty map-values collection");
            ObjectReference map = reference(boot(value, "this$0", "Ljava/util/HashMap;"));
            int size = integer(boot(map, "size", "I")); require(size == 0, "fresh empty map-values proof");
            return object("id", value.uniqueID(), "class", value.referenceType().name(), "size", size, "map_id", map.uniqueID());
        }
        Object sparseState(String role, String edge, Method method, StackFrame frame, ObjectReference target) throws Exception {
            if (!scenario.equals("unload101")) return null;
            long pc = frame.location().codeIndex(); boolean entry = edge.equals("breakpoint"), exit = edge.equals("return");
            String reason = null; Map<String,Object> proof = object();
            if (role.equals("world_time_get") && (entry && pc == 0 || exit && pc == 7)) {
                reason = "world-time-getter"; ObjectReference info = reference(get(world, worldType, "x"));
                proof.put("world_id", world.uniqueID()); proof.put("info_id", info.uniqueID());
                proof.put("time", scalar(get(info, type(client ? "ei" : "ct"), "e")));
            } else if ((role.equals("register_tiles") && (entry && pc == 0 || exit && pc == 32))
                    || (role.equals("register_entities") && (entry && pc == 0 || exit && pc == 43))
                    || (role.equals("defer_entities") && (entry && pc == 0 || exit && pc == 11))) {
                reason = "empty-world-collection-call"; List<Value> args = frame.getArgumentValues();
                require(args.size() == 1, "one actual collection argument"); proof.put("collection", emptyCollection(reference(args.get(0))));
            } else if (role.equals("initialize_chunk") && pc == 0 && (entry || exit)) {
                reason = "one-byte-no-op-initializer"; require(target != null, "actual initializer target");
                proof.put("chunk_state", fullChunk(target));
            } else if (role.equals("detach") && entry && pc == 67) {
                reason = "empty-detach-slice-loop"; require(target != null, "actual detach target");
                ArrayReference slices = (ArrayReference) get(target, chunkType, "m"); require(slices.length() == 8, "eight actual detach slices");
                List<Object> empty = new ArrayList<>(); for (Value value : slices.getValues()) empty.add(emptyCollection(reference(value)));
                proof.put("slices_owner", slices.uniqueID()); proof.put("slices", empty);
            } else if (entry && ((role.equals("activate") && pc == 51) || (role.equals("main_wrapper") && pc == 41)
                    || (role.equals("extra_wrapper") && pc == 30) || (role.equals("load") && pc == 277)
                    || (role.equals("write") && pc == 105) || (role.equals("region_write") && pc == 444)
                    || (role.equals("drop") && pc == 89))) {
                reason = "pre-return-program-counter";
            }
            if (reason == null) return null;
            require(bindings.get(role) instanceof Map<?,?>, "bound omission source body");
            proof.put("body_sha256", ((Map<?,?>) bindings.get(role)).get("bytecode_sha256"));
            return object("snapshot_kind", "large101-omitted-graph-v1", "reason", reason, "proof", proof,
                "target_header", target == null ? null : header(target));
        }
        Map<String,Object> record(String role, String edge, Method method, Value result) throws Exception {
            require(events != null && events.size() < 32768, "bounded complete operation event stream");
            StackFrame frame = measuredThread.frame(0); ObjectReference target = target(frame);
            if (result instanceof ObjectReference returned && (returned.referenceType().equals(chunkType)
                    || returned.referenceType().name().equals(client ? "li" : "hg"))) { header(returned); target = returned; }
            if (target == null && measuredThread.frameCount() >= 2) {
                StackFrame caller = measuredThread.frame(1);
                if (caller.thisObject() != null && caller.location().declaringType().equals(chunkType)) target = caller.thisObject();
            }
            List<Object> args = new ArrayList<>(); for (Value value : frame.getArgumentValues()) args.add(scalar(value));
            List<Object> frames = new ArrayList<>(); require(measuredThread.frameCount() <= 128, "event frame bound");
            for (StackFrame entry : measuredThread.frames()) {
                Location site = entry.location(); if (gameLoader.equals(site.declaringType().classLoader()))
                    frames.add(object("class", site.declaringType().name(), "method", site.method().name(), "signature", site.method().signature(),
                                      "code_index", site.codeIndex(), "receiver", entry.thisObject() == null ? null : entry.thisObject().uniqueID()));
            }
            Object snapshot = sparseState(role, edge, method, frame, target);
            if (snapshot == null) snapshot = state(false, target);
            Object sessionHandler = role.equals("handler_session_write") || edge.equals("return")
                    && (role.equals("handler_constructor") || role.equals("handler_base_constructor")) ? handler(frame.thisObject()) : null;
            Object exceptionMessage = null;
            if (edge.equals("throw")) {
                ReferenceType throwable = vm.classesByName("java.lang.Throwable").stream().filter(value -> value.classLoader() == null).findFirst().orElseThrow();
                Field message = throwable.fieldByName("detailMessage");
                require(message.declaringType().equals(throwable) && message.signature().equals("Ljava/lang/String;"), "bootstrap Throwable message field");
                exceptionMessage = scalar(reference(result).getValue(message));
            }
            Map<String,Object> row = object("ordinal", events.size(), "role", role, "edge", edge, "class", method.declaringType().name(),
                "method", method.name(), "signature", method.signature(), "code_index", frame.location().codeIndex(),
                "thread_id", measuredThread.uniqueID(), "receiver", frame.thisObject() == null ? null : frame.thisObject().uniqueID(),
                "exit_request_class_filter", edge.equals("return") ? method.declaringType().name() : null,
                "arguments", args, "result", scalar(result), "clock_ms", System.currentTimeMillis(), "original_frames", frames,
                "session_handler", sessionHandler, "exception_message", exceptionMessage,
                "catch_location", null, "state", snapshot);
            events.add(row); return row;
        }
        void stopped(BreakpointEvent event) throws Exception {
            String role = (String) event.request().getProperty("role"); StackFrame frame = event.thread().frame(0);
            List<Value> args = frame.getArgumentValues();
            if (role.equals("setup")) {
                require(!setupSeen && active == -1 && beginCount == 0, "one post-constructor setup marker"); setupSeen = true;
                world = reference(args.get(0)); cache = reference(args.get(1)); storage = reference(args.get(2)); owner = (ObjectReference) args.get(3);
                gameLoader = world.referenceType().classLoader(); require(gameLoader != null, "original game loader"); measuredThread = event.thread();
                install(); setupInventories.add(inventory()); return;
            }
            if (role.equals("identity")) {
                require(setupSeen && event.thread().equals(measuredThread) && ++identityMarkers <= 16384, "bounded helper identity markers");
                ObjectReference value = reference(args.get(0)); int key = integer(args.get(1));
                require(key > 0 && !identityIDs.containsValue(value.uniqueID()) && identityIDs.putIfAbsent(key, value.uniqueID()) == null, "bijective helper identity label");
                return;
            }
            if (role.equals("begin")) {
                require(installed && active == -1 && event.thread().equals(measuredThread) && integer(args.get(2)) == operations.size(), "ordered complete begin marker");
                active = integer(args.get(2)); require(active < 512, "operation count bound"); beginCount++;
                require(args.get(3) instanceof StringReference, "explicit marker action"); action = ((StringReference) args.get(3)).value();
                require(exceptionIDs.isEmpty(), "fresh operation exception inventory");
                events = new ArrayList<>(); before = object("threads", inventory(), "state", state(true, null));
                originalExits(true); return;
            }
            if (role.equals("end")) {
                require(active >= 0 && event.thread().equals(measuredThread) && integer(args.get(1)) == active, "complete typed end marker");
                require(!maintenance, "complete maintenance method return");
                originalExits(false);
                Object exception = null;
                if (args.get(2) == null) require(exceptionIDs.isEmpty(), "successful operation has no session exception");
                else {
                    require(args.get(2) instanceof StringReference && ((StringReference) args.get(2)).value().equals(client ? "us" : "my")
                            && (action.equals("fatal_cache_save") || action.equals("fatal_world_save")) && !exceptionIDs.isEmpty()
                            && new HashSet<>(exceptionIDs).size() == 1 && args.get(0) == null, "actual bounded fatal session exception");
                    exception = object("class", client ? "us" : "my", "object_id", exceptionIDs.get(0), "events", exceptionIDs.size());
                }
                Value result = args.get(0);
                if (action.equals("competing_handler_constructor")) {
                    competingStorage = reference(result); require(!competingStorage.equals(storage) && competingStorage.referenceType().equals(type(client ? "mx" : "ie")), "actual separate competing handler");
                }
                if (result instanceof ObjectReference value && value.referenceType().classLoader() == null) {
                    if (value.referenceType().name().equals("java.lang.Boolean")) result = boot(value, "value", "Z");
                    else if (value.referenceType().name().equals("java.lang.Integer")) result = boot(value, "value", "I");
                }
                operations.add(object("index", active, "action", action, "before", before, "events", events, "result", scalar(result), "exception", exception,
                    "after", object("state", state(true, null), "threads", inventory())));
                endCount++; active = -1; action = null; events = null; exceptionIDs.clear(); return;
            }
            require(active >= 0 && event.thread().equals(measuredThread), "original event within measured operation");
            require(!role.startsWith("forbidden_"), "unexpected generation/population/finish/server execution " + role);
            if (role.equals("maintenance") && frame.location().codeIndex() == 0) { maintenance = true; iteratorExits.enable(); }
            record(role, "breakpoint", event.location().method(), null);
        }
        void returned(MethodExitEvent event) throws Exception {
            require(event.method().declaringType().name().equals(event.request().getProperty("declaring_name")), "exact declaring-name exit request ownership");
            if (event.method().declaringType().name().equals("java.util.HashMap$KeyIterator")) {
                require(active >= 0 && maintenance && event.thread().equals(measuredThread) && event.thread().frameCount() >= 2, "actual pending-next boundary");
                if (event.method().isConstructor()) {
                    require(event.method().signature().equals("(Ljava/util/HashMap;)V") && measuredThread.frameCount() >= 4, "actual pending iterator constructor");
                    Location source = measuredThread.frame(3).location();
                    require(source.declaringType().equals(cacheType) && source.method().name().equals("a") && source.method().signature().equals("()Z")
                            && source.codeIndex() == (dedicated ? 34 : 24), "original Set.iterator constructor caller: " + source);
                    record("pending_iterator_construct", "return", event.method(), event.returnValue()); return;
                }
                require(event.method().name().equals("next") && event.method().signature().equals("()Ljava/lang/Object;"), "only bound iterator methods");
                Location caller = event.thread().frame(1).location();
                require(caller.declaringType().equals(cacheType) && caller.method().name().equals("a") && caller.method().signature().equals("()Z")
                        && caller.codeIndex() == (dedicated ? 39 : 29), "actual original fresh iterator caller: " + caller);
                ObjectReference key = reference(event.returnValue()); require(key.referenceType().name().equals("java.lang.Integer"), "actual selected Integer");
                record("pending_next", "return", event.method(), bootstrap(key, "value", "I")); return;
            }
            require(active >= 0 && exitsEnabled && event.method().declaringType().classLoader().equals(gameLoader), "original return within measured interval/loader");
            require(event.thread().equals(measuredThread), "original return thread");
            String role = watched.get(event.method());
            if (role == null) { unwatchedReturns.merge(event.method().toString(), 1, Integer::sum); return; }
            record(role, "return", event.method(), event.returnValue());
            if (role.equals("maintenance")) { maintenance = false; iteratorExits.disable(); }
        }
        void thrown(ExceptionEvent event) throws Exception {
            require(active >= 0 && exitsEnabled && event.thread().equals(measuredThread)
                    && (action.equals("fatal_cache_save") || action.equals("fatal_world_save")), "session exception only in expected fatal API");
            Location site = event.location();
            require(site.declaringType().equals(type(client ? "fm" : "dq")) && site.method().name().equals("b") && site.method().signature().equals("()V")
                    && (site.codeIndex() == 51 || site.codeIndex() == 65) && event.exception().referenceType().equals(type(client ? "us" : "my")), "exact original session mismatch throw/rethrow");
            require(exceptionIDs.size() < 4, "bounded original session throws"); exceptionIDs.add(event.exception().uniqueID()); exceptionEventCount++;
            Map<String,Object> row = record("session_exception", "throw", site.method(), event.exception());
            Location caught = event.catchLocation();
            if (caught != null) {
                Method method = caught.method(); byte[] code = method.isNative() || method.isAbstract() ? new byte[0] : method.bytecodes();
                row.put("catch_location", object("class", caught.declaringType().name(), "method", method.name(), "signature", method.signature(),
                    "code_index", caught.codeIndex(), "modifiers", method.modifiers(), "bytecode_length", code.length, "bytecode_sha256", hash(code),
                    "original_loader", gameLoader.equals(caught.declaringType().classLoader()), "bootstrap_loader", caught.declaringType().classLoader() == null));
            }
        }
        boolean beforeMeasurement() {
            return !setupSeen && !installed && active == -1 && beginCount == 0 && endCount == 0 && identityMarkers == 0
                && operations.isEmpty() && bindings.isEmpty() && fields.isEmpty() && identityIDs.isEmpty() && chunkObjects.isEmpty()
                && setupInventories.isEmpty() && originalRequests == 0 && !maintenance && iteratorExits == null
                && !exitsEnabled && exitEnableCount == 0 && exitDisableCount == 0 && chunkHeaders.isEmpty() && headerIndices.isEmpty()
                && chunkStates.isEmpty() && chunkStateIndices.isEmpty() && action == null && competingStorage == null
                && sessionExceptions == null && exceptionIDs.isEmpty() && exceptionEventCount == 0;
        }
    }
    public static void main(String[] args) {
        Process child = null; VirtualMachine vm = null; Future<byte[]> stdout = null, stderr = null;
        Path outPath = null, errPath = null, failurePath = null; Session session = null;
        ExecutorService readers = Executors.newFixedThreadPool(2); long start = System.nanoTime();
        long observationSeconds =
            args.length == 7 && args[1].equals("dedicated") && args[2].equals("unload101") ? 600 : 300;
        long deadline = start + TimeUnit.SECONDS.toNanos(observationSeconds); int status = 1;
        try {
            require(args.length == 7 && Arrays.asList("client","base","dedicated").contains(args[1])
                && Arrays.asList("cohort","unload101","session-world","detach-owned").contains(args[2]) && args[3].equals("0000000000000017"), "explicit original lifecycle context/scenario");
            Path snapshot = Path.of(args[5]).toAbsolutePath().normalize(), trace = Path.of(args[6]).toAbsolutePath().normalize();
            outPath = Path.of(trace + ".child-stdout.txt"); errPath = Path.of(trace + ".child-stderr.txt");
            Path unavailable = Path.of(trace + ".unavailable.json"); failurePath = Path.of(trace + ".failure.json");
            List<Path> outputs = Arrays.asList(snapshot, trace, outPath, errPath, unavailable, failurePath);
            require(new HashSet<>(outputs).size() == outputs.size(), "distinct outputs"); for (Path output : outputs) newOutput(output);
            Path jar = Path.of(args[0]).toRealPath(); String expected = args[1].equals("client") ? ChunkLifecycleObservation.CLIENT : ChunkLifecycleObservation.SERVER;
            require(hash(Files.readAllBytes(jar)).equals(expected), "pinned original archive");
            Path java = Path.of(System.getProperty("java.home"), "bin", System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java");
            List<String> command = new ArrayList<>(Arrays.asList(java.toString(), "-Xint", "-Xmx512m",
                "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp", System.getProperty("java.class.path"), "ChunkLifecycleObservation"));
            command.addAll(Arrays.asList(Arrays.copyOf(args, 6))); child = new ProcessBuilder(command).start(); Process process = child;
            AtomicLong count = new AtomicLong(); CompletableFuture<String> startup = new CompletableFuture<>();
            stdout = readers.submit(() -> capture(process.getInputStream(), startup, count)); stderr = readers.submit(() -> capture(process.getErrorStream(), null, count));
            String line = startup.get(remaining(deadline), TimeUnit.MILLISECONDS);
            require(line.matches("Listening for transport dt_socket at address: [0-9]+"), "loopback JDWP startup");
            String port = line.substring(line.lastIndexOf(' ') + 1);
            AttachingConnector connector = Bootstrap.virtualMachineManager().attachingConnectors().stream()
                .filter(value -> value.name().equals("com.sun.jdi.SocketAttach")).findFirst().orElseThrow();
            Map<String,Connector.Argument> connection = connector.defaultArguments(); connection.get("hostname").setValue("127.0.0.1");
            connection.get("port").setValue(port); connection.get("timeout").setValue(Long.toString(remaining(deadline))); vm = connector.attach(connection);
            require(vm.canGetMethodReturnValues() && vm.canGetBytecodes(), "return values/bytecodes required");
            String vmName = vm.name(), vmVersion = vm.version(); session = new Session(vm, args[1], args[2]); vm.resume(); boolean alive = true;
            while (alive && System.nanoTime() < deadline) {
                EventSet events;
                try { events = vm.eventQueue().remove(Math.min(1000, remaining(deadline))); } catch (VMDisconnectedException closed) { break; }
                if (events == null) continue;
                for (Event event : events) {
                    require(++session.deliveredEvents <= 32768, "bounded total event delivery");
                    if (event instanceof ClassPrepareEvent value) session.prepared(value);
                    else if (event instanceof BreakpointEvent value) session.stopped(value);
                    else if (event instanceof MethodExitEvent value) session.returned(value);
                    else if (event instanceof ExceptionEvent value) session.thrown(value);
                    else if (event instanceof VMDeathEvent || event instanceof VMDisconnectEvent) alive = false;
                }
                try { events.resume(); } catch (VMDisconnectedException closed) { alive = false; }
            }
            require(child.waitFor(remaining(deadline), TimeUnit.MILLISECONDS), "bounded original child exit");
            byte[] out = stdout.get(remaining(deadline), TimeUnit.MILLISECONDS), err = stderr.get(remaining(deadline), TimeUnit.MILLISECONDS);
            Files.write(outPath, out, StandardOpenOption.CREATE_NEW); Files.write(errPath, err, StandardOpenOption.CREATE_NEW);
            require(hash(Files.readAllBytes(jar)).equals(expected), "original archive retained");
            if (child.exitValue() == 77) {
                require(session.beforeMeasurement() && !Files.exists(snapshot, LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(snapshot), "strict zero-evidence setup77");
                write(unavailable, object("schema", 1, "status", "unavailable", "stage", "original_setup", "child_exit", 77,
                    "context", args[1], "scenario", args[2], "jar_sha256", expected, "setup_markers", 0, "begin_markers", 0, "end_markers", 0,
                    "completed_operations", 0, "identity_markers", 0, "snapshot_started", false, "snapshot_exists", false, "child_command", command), STREAM_LIMIT);
                status = 77;
            } else {
                require(child.exitValue() == 0, "original child failed: " + new String(err, StandardCharsets.UTF_8));
                require(session.installed && session.active == -1 && !session.maintenance && !session.exitsEnabled && session.beginCount > 0
                    && session.exitEnableCount == session.beginCount && session.exitDisableCount == session.endCount
                    && session.beginCount == session.endCount && session.endCount == session.operations.size(), "all complete observed operations");
                require(Files.isRegularFile(snapshot, LinkOption.NOFOLLOW_LINKS) && Files.size(snapshot) <= STREAM_LIMIT, "complete bounded snapshot");
                write(trace, object("schema", 1, "scope", "original-chunk-lifecycle-read-only-JDI", "context", args[1], "scenario", args[2], "seed_hex", args[3],
                    "jar_sha256", expected, "vm_name", vmName, "vm_version", vmVersion, "child_command", command, "debuggee_mode", "interpreted",
                    "listen_address", "127.0.0.1", "port", Integer.parseInt(port), "child_exit", 0, "bindings", session.bindings, "fields", session.fields,
                    "identity_ids", session.identityIDs.entrySet().stream().map(entry -> object("reference", entry.getKey(), "object_id", entry.getValue())).toList(),
                    "setup_inventories", session.setupInventories, "operations", session.operations, "blobs", session.blobs,
                    "state_encoding", "chunk-headers-and-states-indexed-v1", "chunk_headers", session.chunkHeaders, "chunk_states", session.chunkStates,
                    "unwatched_original_returns", session.unwatchedReturns, "jdi_event_count", session.deliveredEvents,
                    "exception_event_count", session.exceptionEventCount,
                    "original_exit_intervals", object("enabled", session.exitEnableCount, "disabled", session.exitDisableCount,
                        "helper_snapshots_requests_disabled", true, "class_filter", "exact-declaring-name"),
                    "event_snapshot_policy", session.scenario.equals("unload101") ? "large101-checkpoints-v1" : "full-event-graph-v1",
                    "callback_snapshot_scope", session.scenario.equals("unload101")
                        ? "Full outer batch/API payloads and full graphs at lifecycle checkpoints; explicit allowlisted empty-call/getter/pre-return events retain fresh proof instead of a repeated graph."
                        : session.scenario.equals("detach-owned")
                        ? "All retained chunk payloads and complete finite external object fields reread at every event; collection entries preserve logical membership and owner identity, not all JDK backing internals."
                        : "All cache/chunk headers and complete affected target bytes; all retained chunk payloads reread at helper begin/end.",
                    "child_stdout", object("file", outPath.getFileName().toString(), "bytes", out.length, "sha256", hash(out)),
                    "child_stderr", object("file", errPath.getFileName().toString(), "bytes", err.length, "sha256", hash(err))), TRACE_LIMIT);
                status = 0;
            }
        } catch (Throwable failure) {
            System.err.println("FAILURE: chunk lifecycle controller"); failure.printStackTrace(System.err);
            if (failurePath != null && !Files.exists(failurePath)) try {
                write(failurePath, object("schema", 1, "status", "fail", "error", failure.toString(),
                    "setup_seen", session != null && session.setupSeen, "begin_markers", session == null ? 0 : session.beginCount,
                    "end_markers", session == null ? 0 : session.endCount, "active", session == null ? -1 : session.active,
                    "identity_markers", session == null ? 0 : session.identityMarkers, "jdi_events", session == null ? 0 : session.deliveredEvents,
                    "completed_operations", session == null ? new ArrayList<>() : session.operations,
                    "partial_events", session == null || session.events == null ? new ArrayList<>() : session.events,
                    "state_encoding", "chunk-headers-and-states-indexed-v1", "chunk_headers", session == null ? new ArrayList<>() : session.chunkHeaders,
                    "chunk_states", session == null ? new ArrayList<>() : session.chunkStates,
                    "event_snapshot_policy", session != null && session.scenario.equals("unload101") ? "large101-checkpoints-v1" : "full-event-graph-v1",
                    "blobs", session == null ? object() : session.blobs), TRACE_LIMIT);
            } catch (Throwable recording) {
                System.err.println("Failure evidence write: " + recording);
                try {
                    write(Path.of(failurePath + ".envelope.json"), object("schema", 1, "status", "fail",
                        "error", failure.toString(), "recording_error", recording.toString(),
                        "begin_markers", session == null ? 0 : session.beginCount,
                        "end_markers", session == null ? 0 : session.endCount,
                        "completed_operations", session == null ? 0 : session.operations.size(),
                        "jdi_events", session == null ? 0 : session.deliveredEvents,
                        "full_failure_partial", failurePath.getFileName() + ".partial"), 16384);
                } catch (Throwable envelopeFailure) { System.err.println("Failure envelope write: " + envelopeFailure); }
            }
        } finally {
            if (vm != null) try { vm.dispose(); } catch (RuntimeException ignored) { }
            if (child != null && child.isAlive()) { child.destroyForcibly(); try { child.waitFor(2, TimeUnit.SECONDS); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); } }
            for (int i = 0; i < 2; i++) {
                Path path = i == 0 ? outPath : errPath; Future<byte[]> bytes = i == 0 ? stdout : stderr;
                if (path != null && bytes != null && !Files.exists(path)) try { Files.write(path, bytes.get(2, TimeUnit.SECONDS), StandardOpenOption.CREATE_NEW); }
                catch (Exception recording) { System.err.println("Raw child stream retention: " + recording); }
            }
            readers.shutdownNow();
        }
        System.exit(status);
    }
}
