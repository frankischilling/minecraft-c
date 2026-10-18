#!/usr/bin/env python3
"""Validate bounded original lifecycle captures without executing any child.

Expectations come from original-only records. Indexed tables stay shared; no
expanded copy of every retained chunk payload is constructed. Historical inputs
can be diagnosed explicitly, but missing proof fails the default public API.
"""
from __future__ import annotations
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
import zlib
from collections.abc import Mapping
from types import MappingProxyType

MAX_RECORD = 64 * 1024 * 1024
MAX_NBT = 1024 * 1024
ARRAYS = {'blocks':32768, 'metadata':16384, 'skylight':16384, 'block_light':16384, 'heightmap':256}
INFO = ['seed','spawn_x','spawn_y','spawn_z','time','last_played','size_on_disk','player','player_dimension','level_name','version','raining','rain_time','thundering','thunder_time']
STATE_KEYS = set('world cache storage info provider generator loader fallback map insertions pending chunks queue registrations metadata world_force_load save_off provider_force_load handler_token files'.split())
JAR_HASHES = {'client':'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c',
              'server':'033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'}

# Original member fingerprints and watched instruction offsets, pinned by profile.
_SPECS = """
client|external_tile_invalidate|ow|i|()V|1|6|071d3f6a1e224612e34b37df9796156ae68f8bed718d422487e0c77a91971ee0|0,5
client|external_entity_register|fd|c|(Lsn;)V|4|41|a2e151e7d0801da7996e5cdb637d092b2cbe22890a181326cdd313ad44b74346|0
base,dedicated|external_tile_invalidate|jh|h|()V|1|6|111d5244a2b65d3ada4135e9b8e41819945016aeb3c327d789e06ed0362793ca|0,5
base,dedicated|external_entity_register|dj|c|(Llq;)V|4|41|34a3ffab959368744c5fbf2642f34ac7df85e9ccbdb50c4a30d469665b8a4548|0
dedicated|external_entity_registry|dp|c|(Llq;)V|4|18|78e4954cd1d797179312f5d4870c60a36ac18ad4bf723cd11b4634fe08f9be1e|0,17
client|acquire|ok|c|(II)Llm;|1|394|06e08e7b1b2956f8a581b21a56578b29d153e418e53d7ae7ed298d0d78ea7817|0,20,78,100,112,128,391
client|save_pass|ok|a|(ZLyb;)Z|1|119|db7438732819f36b38f8a1e124315976965e9a00f30936632abb784a4c3b74a4|0,11,74,88,118
client|extra_wrapper|ok|a|(Llm;)V|2|31|03dc846ab277dfd80910c9893c1e5c9f7f2d4e4c0a5de9e1ae5f0542067cab31|0,30
client|main_wrapper|ok|b|(Llm;)V|2|42|a3589be66536f6a8b845340291e0032a9da13b1de756f7b556836e2e07df89f1|0,19,41
client|save_enabled|ok|b|()Z|1|2|95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2|0
client|maintenance|ok|a|()Z|1|131|8eba5425c0eb6c6b79064e2f71e6fc299e07809a631099e4d206c8db9a60a2d4|0,76,87,99,105
client|world_time|fd|a|(J)V|1|9|b3d812a16755874725c695d086993aba96cdf4cd5354be43667f09f94c29be4a|0,8
client|world_time_get|fd|t|()J|1|8|45dc4cb00dc68fb8b32bfbc0641b6657d794beec59ff2697e48bbe6b18c43929|0
client|world_save|fd|a|(ZLyb;)V|1|54|154b5b7715c572e95e4f281c9c2b72478ef7a0981a0dc5347f89cfabfd993b07|0,26,47,53
client|world_metadata|fd|D|()V|2|29|2b90d6a9bd0ddb01411ee1ed4660f627e6a86bbf88342587dab5c7fbba8d622a|0,28
client|world_session|fd|r|()V|1|10|ead768d43e3392db5ed7e1e8c2f1d394e4ea44e780c8a682a2c69a59381e439d|0
client|register_tiles|fd|a|(Ljava/util/Collection;)V|1|33|b0b9541c407fbf25bbbe9ab7b9a2df55b1b609d79b2d511d20cb5857a63e9ed8|0
client|register_entities|fd|a|(Ljava/util/List;)V|1|44|348fb118fbb8d562b0e2ee9075a024a94ffc0c78e8bab4b36d1762d440a078a5|0
client|defer_entities|fd|b|(Ljava/util/List;)V|1|12|6d76728429c671d89feb7e63b6a80f5e8729f56e93e956e42e4b26f8ce01f0bc|0
client|eligibility|lm|a|(Z)Z|1|70|c3961431e40e3bbdffc097021568e173d29760c0abdaab920c1095983de2d1a4|0
client|activate|lm|e|()V|1|52|954ec8fdd3b2c285473b09bebe03f27f3d61572326c2443d1253219f2ec9d2da|0,5,51
client|detach|lm|f|()V|1|77|08e947c3912554abfe4142424fcfc1da231f6880ac2e1379fbbd75807efaea5c|0,5,67,76
client|dirty|lm|g|()V|1|6|5a6d2b391d05c47e1f7024c99b8cd11b4e2fe138bd9b6506bdbfe1a9521e6c67|0
client|initialize_chunk|lm|d|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
client|membership_add|lm|a|(Lsn;)V|1|155|5a77e52c47cd35d0cee3b4fa8fbc9e121825416bf904286f76de9d4b335abd3a|0,5
client|membership_remove|lm|b|(Lsn;)V|1|10|6211195c3dd9366c7bcab5e65163bb466cbd74d67301f32f02c8e21f9a9f5795|0
client|membership_remove_slice|lm|a|(Lsn;I)V|1|37|840e8625028124ce7218e4c5f7bca7c57e28d56ba81dd947d7ca0c959b1fcb99|0
client|load|ld|a|(Lfd;II)Llm;|1|278|3349469872749baec1a3d455f296827315f1b81245397abef0fa54c3b521c38e|0,277
client|write|ld|a|(Lfd;Llm;)V|1|106|010f2b7fb21f81dedaa4063990d235d9b16e25d1345a0369f095b612483f73c9|0,4,51,61,89,105
client|loader_extra|ld|b|(Lfd;Llm;)V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
client|loader_maintenance|ld|a|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
client|loader_flush|ld|b|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
client|serialize_chunk|to|a|(Llm;Lfd;Lnu;)V|9|293|d2377ea0633eccf25973659878ba0173083fcdf35b3450ed280847d9609fe1c4|0,108
client|save_info_wrapper|mx|a|(Lei;Ljava/util/List;)V|1|14|1b240f45898ef6dab8789a3106214ad060ecaf938eec698317e8ecfe80b20af6|0
client|save_info|fm|a|(Lei;Ljava/util/List;)V|1|151|80de3665d10026935e6465156dfcd87671c2ed1649b375f89ed2d59453373311|0
client|session_check|fm|b|()V|1|81|bb68a2319df7fd1b9304355ee68a495810957cf46bd7554ef6a760577b8f8260|0
client|auxiliary|hc|a|()V|1|53|2d59fd34d8c4fd3c0b72619300ac333c7d2f235d51f4b6c2199106d04d2f0978|0
client|provider_maintenance|yf|a|()Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0
client|timestamp|qj|b|(III)V|2|41|3de743de9199f459f5655eaea989dbe45f0e7823a4df3ed0a79e8b9f1c1ff002|0,40
client|region_write|qj|a|(II[BI)V|36|445|2143d53dce4ade104faea1d774cbc4b21e8bf72a45270282ae08dc60def18e14|0,420,444
client|forbidden_generate|yf|b|(II)Llm;|1|130|b5de2724c479e108327aca1b3579c200de3d98c846fc45dbba251b187afdf882|0
client|forbidden_populate|yf|a|(Lcl;II)V|1|2907|f09523f9e3ed68d6e27dbd8e0a8f28431a23c4497d5d824be1b2bfa0652b28dc|0
client|forbidden_finish|lm|c|()V|1|222|b0af5c034ed6dd3e9df9b555c5ea6c50056124ceef3e22af8d3767cf470f24f8|0
client|blank_eligibility|li|a|(Z)Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0
client|client_incremental|fd|a|(I)Z|1|34|69a624fa3320b0539525f1b497edfcc319d806d35250a40ba9b69ed1e60c93a3|0
client|handler_constructor|mx|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|8|5577af645a3cd2252842cf0a29d3b3fe6abfe2a342178b2a3c72b79e25f7f689|0
client|handler_base_constructor|fm|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|91|2f3e854a1085603510285eb0c74e3f79e289df90b18e6d8b917c4a7f333c7c30|0
client|handler_session_write|fm|d|()V|2|71|264a7f18b404f0812a5fc31baa93bb801d8179ca4b3b2561d847a00926934199|0
base|acquire|jb|c|(II)Lhi;|1|394|521e6d404e5b1ed34ec56c2f4bfafbde125fde3e71efc280856542d3d9c3bac2|0,20,78,100,112,128,391
base|save_pass|jb|a|(ZLpj;)Z|1|119|322a3b0a6b4375148d65f5f55822bca59e4965b67e7c758fc224cf0cc14e4b0b|0,11,74,88,118
base|extra_wrapper|jb|a|(Lhi;)V|2|31|5727f024e5763a3066b8f7aef70e6f4d13b490b87081ed2c1f5896d0d20790cf|0,30
base|main_wrapper|jb|b|(Lhi;)V|2|42|7aacbcc1a224e13392f39b8d4ebd8eff65f730b32b69e076972b4fdb6d1c72d2|0,19,41
base|save_enabled|jb|b|()Z|1|2|95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2|0
base|maintenance|jb|a|()Z|1|131|0f20d1a8571e044993e082858d77f9a7ec82e934fc4600941ce332955a1ca72a|0,76,87,99,105
dedicated|acquire|he|c|(II)Lhi;|1|394|82a55898efef7c2b7184551f9ac64de16f942cd04592b560969a39348498ae7a|0,20,78,100,112,128,391
dedicated|save_pass|he|a|(ZLpj;)Z|1|119|7b696be3c0e803173215fd72f344a9453efc6310a2e80e6bc0bb094c86ade8e8|0,11,74,88,118
dedicated|extra_wrapper|he|a|(Lhi;)V|2|31|4fae4366297d9b0812e3d3ba14f2f7b8e74c357d7462c084ddb1803f78887d04|0,30
dedicated|main_wrapper|he|b|(Lhi;)V|2|42|fac3041962c87746931e865c51f8d576ab46ecdb88fc96e43897b8086d34a617|0,19,41
dedicated|save_enabled|he|b|()Z|1|16|50852624935189392b0b24a64bdc6ff3fb02675377825be22d8e2947cbbd42a9|0
dedicated|maintenance|he|a|()Z|1|141|871595258368ffba04ad26a32d7c5d04a5809cc75421ab196b763cb19d8fb7bd|0,86,97,109,115
dedicated|drop|he|d|(II)V|1|90|42deeff63db45c9e9f203ee2a138fd732a7f81d339a4e2915f6fca7fe113506c|0,89
base,dedicated|world_time|dj|a|(J)V|1|9|290fe9fe23ef3b2fcac19e5bb34433e2fe0c3df2e1f2e52cc07e90a27b831682|0,8
base,dedicated|world_time_get|dj|m|()J|1|8|740127b35e4a8179dba415c21e1219e2f8df40b83de974bfff7a666ae5fe796f|0
base,dedicated|world_save|dj|a|(ZLpj;)V|1|54|44ea94e3f9cb4e8838a3f85f705f5aa1af6ceee9dc80a2e593adc2f1cdbe3333|0,26,47,53
base,dedicated|world_metadata|dj|w|()V|2|29|f4bb9d08ba7118d97b178266cbf933c09199dee471d2960386d4b27cbbc28bf2|0,28
base,dedicated|world_session|dj|k|()V|1|10|86b589be6f7f1e96583276c6c28a3c5dcbbd484253a67469bd8e81d6be4c2bf1|0
base,dedicated|register_tiles|dj|a|(Ljava/util/Collection;)V|1|33|75eecaa52b5457c44c1c4a1bac00e5f697f70ed674dbaba81927a5c76d15a378|0
base,dedicated|register_entities|dj|a|(Ljava/util/List;)V|1|44|2101e5c1197a000249ecca5834a4dcd4f7bc8bda62639cd28937b965c1749c90|0
base,dedicated|defer_entities|dj|b|(Ljava/util/List;)V|1|12|6867d5cf5e5e3c87e081237a246ca5c514d2e9d7d7f1e4845154c05c8fd2b621|0
base,dedicated|eligibility|hi|a|(Z)Z|1|70|71b620247e1396a927d20f398e7b85ba06af7b533fec224a1d2591deccd1ab6b|0
base,dedicated|activate|hi|d|()V|1|52|4a07f07f0014ccd0b442a21b9b2fe1fddadda9cf293f44bd1465ec11bf4d3d22|0,5,51
base,dedicated|detach|hi|e|()V|1|77|822fbf0f598d8b22a1d7a186e419bd944713681a663e5c8803361a593da64549|0,5,67,76
base,dedicated|dirty|hi|f|()V|1|6|109bb7eea1ad46dd309cdba4b89229aecc23025e3eec652a3261742388a691f7|0
base,dedicated|initialize_chunk|hi|c|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
base,dedicated|membership_add|hi|a|(Llq;)V|1|155|9a415745dc90deb09143fe0551ea38b5500b2079856e7e70f684ca59a3e01e47|0,5
base,dedicated|membership_remove|hi|b|(Llq;)V|1|10|a6491dffd06fbc71bd6f1cb04e43dffdc2bd8f2d6dec8d419845faabf6d99158|0
base,dedicated|membership_remove_slice|hi|a|(Llq;I)V|1|37|8d04a99ea0f993eaa413bad17ab5cb491308d187c87fcdc9c0b5d71b39ae3dca|0
base,dedicated|load|hb|a|(Ldj;II)Lhi;|1|278|04af2e7d7021541054131879e77e901e013ba8ff15f9ce2e56d916f82d9a186e|0,277
base,dedicated|write|hb|a|(Ldj;Lhi;)V|1|106|de8cbb5272fd6ba5e6a5d74e3985db5298a5e2b89fb4476bf0442dcc7d417146|0,4,51,61,89,105
base,dedicated|loader_extra|hb|b|(Ldj;Lhi;)V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
base,dedicated|loader_maintenance|hb|a|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
base,dedicated|loader_flush|hb|b|()V|1|1|149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d|0
base,dedicated|serialize_chunk|mg|a|(Lhi;Ldj;Liq;)V|9|293|228cdc8b09f273853918b0f6ede4d8a5f7f41c0d0bdf8093e738abb16740c4bc|0,108
base,dedicated|save_info_wrapper|ie|a|(Lct;Ljava/util/List;)V|1|14|7b62d683f628803f4ab5d5e02e4a6a00eabac19b1e0a1e04405680e9e5aab246|0
base,dedicated|save_info|dq|a|(Lct;Ljava/util/List;)V|1|151|7212372023608a58f6caada1e21bc28e989193bcb7aa61ac48088959ed90a9e8|0
base,dedicated|session_check|dq|b|()V|1|81|5479dbd16e682f5e2f4606191e04c945bb9a4a32807e2b294532d2d0bad04bf7|0
base,dedicated|auxiliary|es|a|()V|1|53|2d59fd34d8c4fd3c0b72619300ac333c7d2f235d51f4b6c2199106d04d2f0978|0
base,dedicated|provider_maintenance|pk|a|()Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0
base,dedicated|timestamp|jx|b|(III)V|2|41|3de743de9199f459f5655eaea989dbe45f0e7823a4df3ed0a79e8b9f1c1ff002|0,40
base,dedicated|region_write|jx|a|(II[BI)V|36|445|2143d53dce4ade104faea1d774cbc4b21e8bf72a45270282ae08dc60def18e14|0,420,444
base,dedicated|forbidden_generate|pk|b|(II)Lhi;|1|130|97eed3df6ca2056a265b15cf5ed4f8eb9eca856242bd449dad51ca93f6f640af|0
base,dedicated|forbidden_populate|pk|a|(Lbl;II)V|1|2907|2002a2a4dead7af615b3a6db2fe9030654a244e688e40b8d0444e5b8b1be7a57|0
base,dedicated|forbidden_finish|hi|b|()V|1|222|581a542efcc76d4056af780ec9868aaa47ba949628aaee64c560c5f9a836063c|0
base,dedicated|blank_eligibility|hg|a|(Z)Z|1|2|42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e|0
base,dedicated|handler_constructor|ie|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|8|0a89ec8d2eb1c824548206cf5622efd57da63ee5432ca47400b7266a042fe111|0
base,dedicated|handler_base_constructor|dq|<init>|(Ljava/io/File;Ljava/lang/String;Z)V|1|91|7cf0421755a4a2bd940084b9618d591ad4a22c34f2e2079576eace602bdaa0fa|0
base,dedicated|handler_session_write|dq|f|()V|2|71|ff8f68ff7a214e7c7e115771505091fb487c95ed8ac0f561e76f9ea011f008a7|0
""".strip().splitlines()

# Exact finite original-member event schedules from the three original-only detach runs.
# Bootstrap iterator implementation offsets are validated against the selected JDK separately.
_DETACH_EVENTS = {'client': ['world_time:breakpoint:0 world_time:breakpoint:8 world_time:return:8', 'acquire:breakpoint:0 acquire:breakpoint:20 load:breakpoint:0 membership_add:breakpoint:0 membership_add:breakpoint:5 membership_add:return:154 load:breakpoint:277 load:return:277 world_time_get:breakpoint:0 world_time_get:return:7 acquire:breakpoint:100 acquire:breakpoint:112 initialize_chunk:breakpoint:0 initialize_chunk:return:0 activate:breakpoint:0 activate:breakpoint:5 register_tiles:breakpoint:0 register_tiles:return:32 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 external_entity_register:breakpoint:0 external_entity_register:return:40 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 activate:breakpoint:51 activate:return:51 acquire:breakpoint:128 acquire:breakpoint:391 acquire:return:393', 'detach:breakpoint:0 detach:breakpoint:5 external_tile_invalidate:breakpoint:0 external_tile_invalidate:breakpoint:5 external_tile_invalidate:return:5 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:76 detach:return:76'], 'base': 'client', 'dedicated': ['world_time:breakpoint:0 world_time:breakpoint:8 world_time:return:8', 'acquire:breakpoint:0 acquire:breakpoint:20 load:breakpoint:0 membership_add:breakpoint:0 membership_add:breakpoint:5 membership_add:return:154 load:breakpoint:277 load:return:277 world_time_get:breakpoint:0 world_time_get:return:7 acquire:breakpoint:100 acquire:breakpoint:112 initialize_chunk:breakpoint:0 initialize_chunk:return:0 activate:breakpoint:0 activate:breakpoint:5 register_tiles:breakpoint:0 register_tiles:return:32 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 external_entity_registry:breakpoint:0 external_entity_register:breakpoint:0 external_entity_register:return:40 external_entity_registry:breakpoint:17 external_entity_registry:return:17 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 register_entities:breakpoint:0 register_entities:return:43 activate:breakpoint:51 activate:return:51 acquire:breakpoint:128 acquire:breakpoint:391 acquire:return:393', 'drop:breakpoint:0 drop:breakpoint:89 drop:return:89', 'maintenance:breakpoint:0 detach:breakpoint:0 detach:breakpoint:5 external_tile_invalidate:breakpoint:0 external_tile_invalidate:breakpoint:5 external_tile_invalidate:return:5 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:67 defer_entities:breakpoint:0 defer_entities:return:11 detach:breakpoint:76 detach:return:76 main_wrapper:breakpoint:0 world_time_get:breakpoint:0 world_time_get:return:7 main_wrapper:breakpoint:19 write:breakpoint:0 world_session:breakpoint:0 session_check:breakpoint:0 session_check:return:80 world_session:return:9 write:breakpoint:4 write:breakpoint:51 serialize_chunk:breakpoint:0 world_session:breakpoint:0 session_check:breakpoint:0 session_check:return:80 world_session:return:9 world_time_get:breakpoint:0 world_time_get:return:7 serialize_chunk:breakpoint:108 serialize_chunk:return:292 write:breakpoint:61 region_write:breakpoint:0 region_write:breakpoint:420 timestamp:breakpoint:0 timestamp:breakpoint:40 timestamp:return:40 region_write:breakpoint:444 region_write:return:444 write:breakpoint:89 write:breakpoint:105 write:return:105 main_wrapper:breakpoint:41 main_wrapper:return:41 extra_wrapper:breakpoint:0 loader_extra:breakpoint:0 loader_extra:return:0 extra_wrapper:breakpoint:30 extra_wrapper:return:30 maintenance:breakpoint:86 maintenance:breakpoint:97 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:109 maintenance:breakpoint:115 loader_maintenance:breakpoint:0 loader_maintenance:return:0 provider_maintenance:breakpoint:0 provider_maintenance:return:1 maintenance:return:140']}

class ValidationError(ValueError): pass
def need(value, message):
    if not value: raise ValidationError(message)
def exact(a,b):
    if isinstance(a,Mapping) and isinstance(b,Mapping):return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if isinstance(a,(list,tuple)) and isinstance(b,(list,tuple)):return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    if type(a) is not type(b): return False
    if isinstance(a,Mapping): return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if isinstance(a,(list,tuple)): return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    return a==b
def unique(pairs):
    out={}
    for k,v in pairs:
        need(k not in out,'duplicate JSON key '+k);out[k]=v
    return out
def bad_constant(value): raise ValidationError('nonfinite JSON '+value)
def read_json(path):
    path=Path(path);need(path.stat().st_size<=MAX_RECORD,'JSON bound '+str(path));raw=path.read_bytes()
    return json.loads(raw,object_pairs_hook=unique,parse_constant=bad_constant)
def digest(raw): return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def file_digest(path,limit=512*1024*1024):
    h=hashlib.sha256();size=0
    need(Path(path).stat().st_size<=limit,'file byte bound '+str(path))
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):
            size+=len(block);need(size<=limit,'file bound');h.update(block)
    return {'bytes':size,'sha256':h.hexdigest()}
def integer(value,lo=-(1<<31),hi=(1<<31)-1): return type(value) is int and lo<=value<=hi
def validate_process_tree(tree):
    need(isinstance(tree,Mapping),'owned process accounting object')
    if tree.get('kind')=='windows-job':
        # Windows can account for its console host beside the controller/helper JVMs.
        need(set(tree)=={'kind','processes','active_after_cleanup','remaining_processes_terminated'} and integer(tree['processes'],1,3),'bounded Windows job accounting (including optional console host)')
        need(type(tree['active_after_cleanup']) is int and tree['active_after_cleanup']==0 and type(tree['remaining_processes_terminated']) is int and tree['remaining_processes_terminated']==0,'successful owned Windows processes exited without emergency termination')
    else:
        need(tree.get('kind')=='process-group' and set(tree)=={'kind','direct_child_reaped','remaining_group_terminated','adopted_descendants_reaped','prior_subreaper'},'complete native process-group accounting')
        need(tree['direct_child_reaped'] is True and tree['remaining_group_terminated'] is False and exact(tree['adopted_descendants_reaped'],[]) and integer(tree['prior_subreaper'],0,1),'successful native group exited and reaped without emergency cleanup')
def u16(text): return text.encode('utf-16-be','surrogatepass')
def h64(value):
    need(type(value) is str and re.fullmatch('[0-9a-f]{16}',value),'hex64')
    n=int(value,16);return n-(1<<64) if n>>63 else n
def wrap64(value):return ((value+(1<<63))%(1<<64))-(1<<63)
def path_inventory(root):
    result={};total=0
    need(root.is_dir() and not root.is_symlink() and not(getattr(root.lstat(),'st_file_attributes',0)&0x400),'owned complete world root')
    for p in sorted(root.rglob('*')):
        need(not p.is_symlink() and not getattr(p,'is_junction',lambda:False)() and not(getattr(p.lstat(),'st_file_attributes',0)&0x400),'no world links/reparse aliases')
        if p.is_dir():continue
        need(p.is_file() and p.stat().st_nlink==1 and p.stat().st_size<=8*1024*1024,'regular bounded world file')
        total+=p.stat().st_size;need(len(result)<256 and total<=64*1024*1024,'complete world file budget')
        result[p.relative_to(root).as_posix()]=file_digest(p)
    return result

def _freeze(value):
    if isinstance(value,dict):return MappingProxyType({k:_freeze(v) for k,v in value.items()})
    if isinstance(value,list):return tuple(_freeze(v) for v in value)
    return value

class _View(Mapping):
    """Shallow lazy views retain shared immutable table rows and payload strings."""
    def __init__(self,record,data,kind):self.record=record;self.data=data;self.kind=kind;self.cache={}
    def __iter__(self):return iter(self.data)
    def __len__(self):return len(self.data)
    def __getitem__(self,key):
        if key in self.cache:return self.cache[key]
        value=self.data[key]
        if self.kind=='state' and key=='chunks':value=tuple(self.record.chunk(v) for v in value)
        elif self.kind=='boundary' and key=='state':value=_View(self.record,value,'state')
        elif self.kind=='event' and key=='state':value=_View(self.record,value,'state')
        elif self.kind=='operation' and key in ('before','after'):value=_View(self.record,value,'boundary' if self.record.trace else 'state')
        elif self.kind=='operation' and key=='events':value=tuple(_View(self.record,v,'event') for v in value)
        elif self.kind=='record' and key in ('initial','final'):value=_View(self.record,value,'state')
        elif self.kind=='record' and key=='operations':value=tuple(_View(self.record,v,'operation') for v in value)
        self.cache[key]=value;return value

class _Record(_View):
    def __init__(self,data,trace=False,development=False):
        self.trace=trace;self.development=development;self.used_chunks=set();self.used_headers=set()
        encoding=data.get('state_encoding')
        expected='chunk-headers-and-states-indexed-v1' if trace else 'chunks-indexed-v1'
        need(encoding==expected or development and encoding in (None,'chunk-headers-indexed-v1'),'explicit supported lossless state encoding')
        self.indexed=encoding==expected
        if self.indexed:need(type(data.get('chunk_states')) is list and 0<len(data['chunk_states'])<=4096,'bounded chunk-state table')
        if trace:need(type(data.get('chunk_headers')) is list and 0<len(data['chunk_headers'])<=4096,'bounded header table')
        frozen=_freeze(data);super().__init__(self,frozen,'record')
    def chunk(self,row):
        need(isinstance(row,Mapping),'chunk reference object')
        if set(row)=={'chunk_reference'}:
            index=row['chunk_reference'];need(self.indexed and integer(index,0,len(self.data['chunk_states'])-1),'bounded exact chunk reference')
            self.used_chunks.add(index);return self.data['chunk_states'][index]
        need(not self.indexed or self.trace and set(row)=={'header_reference'},'inline chunk outside declared indexed schema')
        return row
    def header(self,row):
        if isinstance(row,Mapping) and set(row)=={'header_reference'}:
            index=row['header_reference'];need(integer(index,0,len(self.data['chunk_headers'])-1),'bounded exact header reference')
            self.used_headers.add(index);return self.data['chunk_headers'][index]
        need(self.development,'inline header outside declared indexed schema');return row

class Reader:
    def __init__(self,raw):self.raw=raw;self.pos=0
    def take(self,n):
        need(type(n) is int and 0<=n<=len(self.raw)-self.pos,'truncated/bounded binary field')
        p=self.pos;self.pos+=n;return self.raw[p:self.pos]
    def number(self,fmt):return struct.unpack(fmt,self.take(struct.calcsize(fmt)))[0]
    def u1(self):return self.number('>B')
    def u2(self):return self.number('>H')
    def u4(self):return self.number('>I')

class NBT:
    def __init__(self,raw):
        need(len(raw)<=MAX_NBT,'NBT byte bound');self.r=Reader(raw);self.nodes=0;self.spans={}
        self.root=self.tag(0,());need(self.r.pos==len(raw),'NBT trailing bytes');self.raw=raw
    def name(self):return self.r.take(self.r.u2())
    def tag(self,depth,path):
        kind=self.r.u1();need(1<=kind<=11,'NBT named tag kind');name=self.name()
        return (kind,name,self.value(kind,depth+1,path+(name,)))
    def value(self,kind,depth,path):
        self.nodes+=1;need(depth<=64 and self.nodes<=65536,'NBT depth/node bound');start=self.r.pos
        if kind==1:v=self.r.number('>b')
        elif kind==2:v=self.r.number('>h')
        elif kind==3:v=self.r.number('>i')
        elif kind==4:v=self.r.number('>q')
        elif kind in (5,6):v=self.r.take(4 if kind==5 else 8)
        elif kind in (7,11):
            n=self.r.number('>i');need(0<=n<=MAX_NBT,'NBT array length');v=self.r.take(n*(4 if kind==11 else 1))
        elif kind==8:v=self.name()
        elif kind==9:
            element=self.r.u1();n=self.r.number('>i');need(0<=element<=11 and 0<=n<=65536 and (n==0 or element!=0),'NBT list shape')
            v=(element,tuple(self.value(element,depth+1,path+(i,)) for i in range(n)))
        elif kind==10:
            tags=[];names=set()
            while True:
                need(self.r.pos<len(self.r.raw),'unterminated compound')
                if self.r.raw[self.r.pos]==0:self.r.pos+=1;break
                child=self.tag(depth+1,path);need(child[1] not in names,'duplicate NBT child');names.add(child[1]);tags.append(child)
            v=tuple(tags)
        else:raise ValidationError('NBT kind')
        if kind in (1,2,3,4,5,6):self.spans[path]=(start,self.r.pos)
        return v
    def child(self,parent,name,kind=None):
        need(parent[0]==10,'compound required');rows=[x for x in parent[2] if x[1]==name.encode('ascii')]
        need(len(rows)==1,'required NBT '+name);value=rows[0]
        if kind is not None:need(value[0]==kind,'exact NBT type '+name)
        return value

def inflate(raw,gzip=False):
    d=zlib.decompressobj(31 if gzip else 15);out=d.decompress(raw,MAX_NBT+1)
    need(len(out)<=MAX_NBT and d.eof and not d.unused_data and not d.unconsumed_tail,'complete bounded compressed member')
    return out

class Blobs:
    def __init__(self,record):
        self.raw={};self.referenced=set();total=0
        need(isinstance(record['blobs'],Mapping) and len(record['blobs'])<=4096,'blob count')
        for key,row in record['blobs'].items():
            need(set(row)=={'bytes','hex'} and type(row['bytes']) is int and type(row['hex']) is str,'blob fields')
            need(re.fullmatch('[0-9a-f]{64}',key) and 0<=row['bytes']<=8*1024*1024 and len(row['hex'])==row['bytes']*2 and re.fullmatch('[0-9a-f]*',row['hex']),'blob framing')
            total+=row['bytes'];need(total<=64*1024*1024,'aggregate decoded blob budget')
            raw=bytes.fromhex(row['hex']);need(digest(raw)=={'bytes':row['bytes'],'sha256':key},'blob fingerprint');self.raw[key]=raw
    def get(self,row):
        need(isinstance(row,Mapping) and set(row) in ({'bytes','sha256'},{'id','bytes','sha256'}),'blob reference fields')
        if 'id' in row:need(integer(row['id'],1,(1<<63)-1),'typed backing-array identity')
        key=row['sha256'];need(key in self.raw and type(row['bytes']) is int and len(self.raw[key])==row['bytes'],'resolved blob')
        self.referenced.add(key);return self.raw[key]

class ClassFile:
    def __init__(self,raw):
        r=Reader(raw);need(r.u4()==0xcafebabe,'class magic');self.minor=r.u2();self.major=r.u2();cp=[None]*r.u2();i=1
        while i<len(cp):
            tag=r.u1()
            if tag==1:cp[i]=r.take(r.u2())
            elif tag in (3,4):r.take(4)
            elif tag in (5,6):r.take(8);i+=1
            elif tag in (7,8,16,19,20):cp[i]=r.u2()
            elif tag in (9,10,11,12,17,18):r.take(4)
            elif tag==15:r.take(3)
            else:raise ValidationError('constant pool tag')
            i+=1
        def text(index):return cp[index].decode('utf-8')
        self.modifiers=r.u2();self.name=text(cp[r.u2()]);super_index=r.u2();self.super_name=text(cp[super_index]).replace('/','.') if super_index else None;r.take(2*r.u2());self.fields={};self.methods={}
        for destination,is_method in ((self.fields,False),(self.methods,True)):
            for _ in range(r.u2()):
                flags,name,signature=r.u2(),text(r.u2()),text(r.u2());code=None;handlers=[]
                for _ in range(r.u2()):
                    attr=text(r.u2());data=r.take(r.u4())
                    if attr=='Code':
                        cr=Reader(data);cr.u2();cr.u2();code=cr.take(cr.u4())
                        for _ in range(cr.u2()):
                            start,end,handler,catch=cr.u2(),cr.u2(),cr.u2(),cr.u2()
                            handlers.append((start,end,handler,text(cp[catch]).replace('/','.') if catch else None))
                destination[(name,signature)]={'modifiers':flags,'code':code,'handlers':handlers}
        for _ in range(r.u2()):r.u2();r.take(r.u4())
        need(r.pos==len(raw),'class trailing bytes')

def instruction_starts(code):
    starts=set();p=0
    while p<len(code):
        starts.add(p);op=code[p];n=1
        if op in (0x10,0x12,0xa9,0xbc) or 0x15<=op<=0x19 or 0x36<=op<=0x3a:n=2
        elif op in (0x11,0x13,0x14,0x84,0xbb,0xbd,0xc0,0xc1,0xc6,0xc7) or 0x99<=op<=0xa8 or 0xb2<=op<=0xb8:n=3
        elif op in (0xb9,0xba,0xc8,0xc9):n=5
        elif op==0xc5:n=4
        elif op==0xc4:n=6 if code[p+1]==0x84 else 4
        elif op in (0xaa,0xab):
            q=(p+4)&~3
            if op==0xaa:
                low,high=struct.unpack('>ii',code[q+4:q+12]);need(0<=high-low<65536,'tableswitch');n=q+12+4*(high-low+1)-p
            else:
                pairs=struct.unpack('>i',code[q+4:q+8])[0];need(0<=pairs<=65536,'lookupswitch');n=q+8+8*pairs-p
        need(0<=op<=201 and p+n<=len(code),'instruction framing');p+=n
    return starts

class _CaptureValidator:
    def __init__(self,capture,development=False):
        self.development=development;self.nonempty=False;self.capture=capture;self.count=collections.Counter();self.gaps=[];self.findings=[];self.sections={};self.region_cache={};self.metadata_cache={};self.class_cache={};self.methods={};self.blobs={};self.records={};self.timestamp_inputs={};self.writes={};self.metadata_saves={}
    def gap(self,text):
        if text not in self.gaps:self.gaps.append(text)
    def section(self,name,fn):
        try:fn();self.sections[name]='pass'
        except Exception as e:self.sections[name]='fail';self.findings.append(name+': '+type(e).__name__+': '+str(e))
    def class_(self,name):
        if name not in self.class_cache:
            zip_=self.boot if name.startswith('java.') else self.jar
            raw=zip_.read(('classes/' if zip_ is self.boot else '')+name.replace('.','/')+'.class')
            self.class_cache[name]=ClassFile(raw);self.count['raw_class_files']+=1
        return self.class_cache[name]
    def provenance(self):
        p=self.capture;self.manifest=read_json(p/'manifest.json');m=self.manifest
        need(m['C_executed'] is False and (m['tested_commit'] is None or type(m['tested_commit']) is str and re.fullmatch('[0-9a-f]{40}',m['tested_commit'])) and m['errors']==[],'original-only provenance')
        need(exact(m['inputs_before'],m['inputs_after']) and exact(m['classes_before'],m['classes_after']),'stable input/class records')
        for path,expected in m['inputs_before'].items():
            need(type(expected['bytes']) is int and set(expected)=={'bytes','sha256'},'typed input fingerprint')
            archived=p/'source'/Path(path).name
            selected=archived if Path(path).suffix in ('.java','.py') and Path(path).parent.name!='source' and archived.is_file() else Path(path)
            need(exact(file_digest(selected),expected),'retained source/tool/JAR fingerprint '+str(selected));self.count['input_fingerprints']+=1
        self.python_provenance()
        for name,expected in m['classes_before'].items():
            need(exact(file_digest(p/'classes'/name),expected),'compiled class fingerprint');self.count['helper_classes']+=1
        need({x.name for x in (p/'classes').iterdir()}==set(m['classes_before']) and 3<=len(m['classes_before'])<=16,'complete generated helper class inventory')
        need(len(m['processes'])==5 and [x['name'] for x in m['processes']]==['java-version','compile','prepare','observed','control'],'complete ordered process inventory')
        for process in m['processes']:
            need(type(process['returncode']) is int and process['returncode']==0 and process['stopped'] is None,'actual process success')
            need(integer(process['seconds_limit'],1,360 if process['name']=='observed' else 180) and integer(process['per_stream_limit'],1,MAX_RECORD),'process budgets')
            need(Path(process['cwd']).resolve()==p and type(process['command']) is list and 1<=len(process['command'])<=64 and all(type(x) is str for x in process['command']),'bounded command/cwd')
            need(type(process['seconds']) in (int,float) and 0<=process['seconds']<=process['seconds_limit']+2,'bounded process duration')
            if 'reaped' not in process:need(self.development,'missing owned child reaping proof');self.gap('Historical process records lack explicit owned-tree/reaping accounting.')
            else:
                need(process['reaped'] is True,'actual child reaped')
                validate_process_tree(process['process_tree'])
            for role in ('stdout','stderr'):
                expected=process[role];actual=Path(expected['path']);need(actual==p/(process['name']+'.'+role),'canonical process stream')
                need(integer(expected['bytes'],0,process['per_stream_limit']) and exact(file_digest(actual,process['per_stream_limit']),{k:expected[k] for k in ('bytes','sha256')}),'process stream bytes');self.count['process_streams']+=1
                if 'reaped' in process:need(type(process[role+'_total_bytes']) is int and process[role+'_total_bytes']==expected['bytes'],'complete successful stream without truncation')
            self.count['processes']+=1
        for label,expected in m['retained_worlds'].items():
            need(exact(path_inventory(p/label),expected),'retained complete world inventory '+label);self.count['retained_files']+=len(expected)
        need(set(m['retained_worlds'])=={'preparation','observed-world','control-world'} and exact(m['retained_worlds']['preparation'],m['preparation']['files']) and all(exact(m['retained_worlds'][role+'-world'],m[role+'_after']) for role in ('observed','control')),'all declared before/after world inventories bind retained bytes')
        need(exact(m['preparation']['files'],m['observed_before']) and exact(m['observed_before'],m['control_before']),'all pristine baseline copies')
        for role in ('preparation','observed','control'):
            need((p/(role+'.json')).stat().st_size<=32*1024*1024,'bounded complete helper snapshot')
            self.records[role]=_Record(read_json(p/(role+'.json')),development=self.development);self.blobs[role]=Blobs(self.records[role]);self.count['blob_entries']+=len(self.blobs[role].raw)
            expected=m['preparation']['snapshot'] if role=='preparation' else m[role+'_snapshot']
            need(exact(file_digest(p/(role+'.json')),expected),'snapshot raw fingerprint')
        self.trace=_Record(read_json(p/'trace.json'),trace=True,development=self.development);self.blobs['trace']=Blobs(self.trace);self.count['blob_entries']+=len(self.blobs['trace'].raw)
        if 'trace_snapshot' not in m:need(self.development,'missing full raw trace fingerprint');self.gap('Historical manifest lacks complete raw trace fingerprint.')
        else:need(exact(file_digest(p/'trace.json'),m['trace_snapshot']),'complete raw trace fingerprint')
        self.context=self.trace['context'];self.scenario=self.trace['scenario']
        need(type(self.trace['schema']) is int and self.trace['schema']==1 and type(self.trace['child_exit']) is int and self.trace['child_exit']==0 and self.context in ('client','base','dedicated') and self.scenario in ('cohort','unload101','session-world','detach-owned'),'supported original lifecycle profile')
        need(self.scenario!='unload101' or self.context=='dedicated','natural dedicated pending profile')
        self.nonempty=self.scenario=='detach-owned';self.client=self.context=='client';self.chunk_type='lm' if self.client else 'hi';self.blank_type='li' if self.client else 'hg'
        self.world_type='fd' if self.client else 'dj';self.cache_type='ok' if self.client else 'he' if self.context=='dedicated' else 'jb'
        self.info_type='ei' if self.client else 'ct';self.jar_name='b1.7.3.jar' if self.client else 'b1.7.3_server.jar'
        need(self.trace['scope']=='original-chunk-lifecycle-read-only-JDI' and self.trace['seed_hex']=='0000000000000017' and self.trace['jar_sha256']==JAR_HASHES['client' if self.client else 'server'] and self.trace['debuggee_mode']=='interpreted' and self.trace['listen_address']=='127.0.0.1' and integer(self.trace['port'],1,65535),'exact original profile/loopback transport metadata')
        policy='large101-checkpoints-v1' if self.scenario=='unload101' else 'full-event-graph-v1'
        need(self.trace.get('event_snapshot_policy',policy if self.development else None)==policy,'explicit event graph scope policy')
        for role in ('stdout','stderr'):
            rec=self.trace['child_'+role];need(rec['file']=='trace.json.child-'+role+'.txt','canonical child stream')
            need(exact(file_digest(p/rec['file']),{k:rec[k] for k in ('bytes','sha256')}),'raw child stream');self.count['process_streams']+=1
        client=next(Path(x) for x in m['inputs_before'] if Path(x).name==self.jar_name)
        need(file_digest(client)['sha256']==JAR_HASHES['client' if self.client else 'server'],'pinned original archive bytes')
        self.jar=zipfile.ZipFile(client);self.boot=zipfile.ZipFile(next(Path(x) for x in m['inputs_before'] if Path(x).name=='java.base.jmod'))
        self.jar_url=client.resolve().as_uri().replace('file:///','file:/')
        self.original_names={x[:-6].replace('/','.') for x in self.jar.namelist() if x.endswith('.class')}
        for role,d in self.records.items():
            if not d.indexed:self.gap('Historical unindexed helper state encoding is diagnostic only.')
            need(d['schema']==1 and type(d['schema']) is int and d['context']==('client' if role=='preparation' and self.client else 'base' if role=='preparation' else self.context) and d['mode']==(('prepare-detach' if self.nonempty else 'prepare') if role=='preparation' else self.scenario) and d['seed_hex']=='0000000000000017','record profile')
            need(d['jar_sha256']==file_digest(client)['sha256'],'selected actual archive')
            need(1<=len(d['class_origins'])<=4096,'bounded complete original origin inventory')
            for name,origin in d['class_origins'].items():need(origin==self.jar_url and name in self.original_names,'exact selected class URL')
            for rows,kind in ((d['bindings'],'method'),(d['fields'],'field')):
                need(0<len(rows)<=1024,'complete bounded member inventory')
                for key,row in rows.items():
                    need(key==row['class']+'.'+row['method']+row['signature'] if kind=='method' else key==row['class']+'.'+row['name'],'canonical member inventory key');self.binding(row,kind)
            self.count['helper_binding_rows']+=len(d['bindings'])+len(d['fields'])
            self.helper_threads(d['threads_before'],d['context']);self.helper_threads(d['threads_after'],d['context'])
            need(d['provider_state_initial']['id']==d['initial']['generator'] and d['provider_state_initial']['class']==('yf' if self.client else 'pk') and exact(d['provider_state_initial'],d['provider_state_final']) and d['provider_state_unchanged'] is True,'complete untouched original provider graph')
            self.numeric_graph(role,d['provider_state_initial'])
        specs={};self.breakpoints={}
        for spec in _SPECS:
            contexts,role,cls,method,signature,flags,length,sha,offsets=spec.split('|')
            if self.context in contexts.split(',') and (not role.startswith('external_') or self.nonempty):
                self.breakpoints[role]={int(x) for x in offsets.split(',')};specs[role]={'class':cls,'method':method,'signature':signature,'modifiers':int(flags),'bytecode_length':int(length),'bytecode_sha256':sha}
        required=set(specs)|{'pending_next','pending_iterator_construct'}
        if self.development:
            missing={'handler_constructor','handler_base_constructor','handler_session_write'}-set(self.trace['bindings'])
            if missing:self.gap('Historical capture predates measured session-constructor bindings.');required-=missing
        if self.development and 'pending_iterator_construct' not in self.trace['bindings']:
            required.remove('pending_iterator_construct');self.gap('Historical capture lacks pending iterator-constructor role.')
        need(set(self.trace['bindings'])==required,'complete exact watched method roles')
        for role,row in self.trace['bindings'].items():
            self.binding(row,'method');self.methods[role]=row
            if role in specs:need(exact(row,specs[role]),'pinned original role '+role)
            else:
                need(row['class']=='java.util.HashMap$KeyIterator' and (row['method'],row['signature'],row['modifiers'])==(('next','()Ljava/lang/Object;',17) if role=='pending_next' else ('<init>','(Ljava/util/HashMap;)V',0)),'actual bootstrap pending role')
        field_names=self.trace_field_names()
        if self.development:
            missing={handler_class+'.'+name for name in ('e','b','c','d')}-set(self.trace['fields'])
            if missing:self.gap('Historical JDI field table lacks handler path/token fields.');field_names-=missing
        need(set(self.trace['fields'])==field_names,'complete exact original JDI field inventory')
        for key,row in self.trace['fields'].items():need(key==row['class']+'.'+row['name'],'canonical JDI field key');self.binding(row,'field')
        n=len(self.trace['operations'])
        need(0<n<=512 and n==len(self.records['observed']['operations'])==len(self.records['control']['operations']), 'complete operation count')
        need(exact(self.trace['original_exit_intervals'],{'enabled':n,'disabled':n,'helper_snapshots_requests_disabled':True,**({} if self.development and 'class_filter' not in self.trace['original_exit_intervals'] else {'class_filter':'exact-declaring-name'})}),'measured exit intervals')
        self.construction_proof()
        for role,d in self.records.items():
            if 'block_map' not in d:
                need(self.development,'missing actual post-load normalization map');self.gap('Historical capture lacks actual post-load block map.')
                continue
            mapping=self.blobs[role].get(d['block_map']);need(len(mapping)==256 and all(v in (0,i) for i,v in enumerate(mapping)),'registry-derived exact post-load block map')
        self.process_commands(client)
        self.count['trace_method_bindings']=len(self.trace['bindings']);self.count['trace_field_bindings']=len(self.trace['fields'])

    def external_lists(self):
        if hasattr(self,'external_list_values'):return self.external_list_values
        d=self.records['preparation'];rows=d['constructed_nbt']
        need(len(rows)==2 and [r['stage'] for r in rows]==['constructed_level','factory_reserialized_level'] and all(set(r)=={'stage','raw'} for r in rows),'complete constructed/factory raw NBT evidence')
        trees=[NBT(self.blobs['preparation'].get(r['raw'])) for r in rows]
        need(exact(trees[0].root,trees[1].root),'complete original factory round-trip typed NBT')
        root=trees[0].root;need(root[:2]==(10,b''),'constructed Level root')
        fields={x[1]:x for x in root[2]}
        expected={b'xPos':3,b'zPos':3,b'LastUpdate':4,b'Blocks':7,b'Data':7,b'SkyLight':7,b'BlockLight':7,b'HeightMap':7,b'TerrainPopulated':1,b'Entities':9,b'TileEntities':9}
        need({k:v[0] for k,v in fields.items()}==expected and fields[b'xPos'][2]==16 and fields[b'zPos'][2]==0 and fields[b'LastUpdate'][2]==0 and fields[b'TerrainPopulated'][2]==0,'exact constructed chunk schema/scalars')
        actor=((9,b'Motion',(6,(bytes(8),)*3)),(5,b'FallDistance',bytes(4)),(9,b'Pos',(6,tuple(bytes.fromhex(x) for x in ('4070280000000000','4050400000000000','4004000000000000')))),(2,b'Fire',0),(8,b'id',b'Boat'),(2,b'Air',300),(1,b'OnGround',1),(9,b'Rotation',(5,(bytes(4),)*2)))
        sign=((3,b'x',257),(8,b'Text4',b''),(3,b'y',64),(8,b'Text3',b''),(3,b'z',1),(8,b'Text2',b'NUL\xc0\x80'),(8,b'id',b'Sign'),(8,b'Text1',b'owned detach'))
        # Compound order remains raw and is retained; compare the exact fixed setup.
        need(exact(fields[b'Entities'][2],(10,(actor,))) and exact(fields[b'TileEntities'][2],(10,(sign,))),'all typed finite Boat/Sign constructed values')
        template=next(c for c in d['initial']['chunks'] if not c['blank'] and exact(c['coordinates'],[0,0]))
        for name,field in zip(ARRAYS,(b'Blocks',b'Data',b'SkyLight',b'BlockLight',b'HeightMap')):
            raw=self.blobs['preparation'].get(template['arrays'][name]['data'])
            if name=='blocks':raw=raw[:2240]+bytes([63])+raw[2241:]
            need(fields[field][2]==raw,'entire constructed origin array plus one declared Sign block')
        self.external_list_values={k:fields[k.encode()][2] for k in ('Entities','TileEntities')};self.count['constructed_full_nbt_records']+=2
        return self.external_list_values

    def tile_entries(self,rows,role):
        need(isinstance(rows,(list,tuple)) and len(rows)<=1,'finite complete Sign tile entries')
        for row in rows:
            need(set(row)=={'key','key_class','key_fields','value'} and integer(row['key'],1,(1<<63)-1 if role=='trace' else 16384) and integer(row['value'],1,(1<<63)-1 if role=='trace' else 16384),'typed actual tile owners')
            name='wf' if self.client else 'oa';need(row['key_class']==name and exact(row['key_fields'],{'a':1,'b':64,'c':1}),'complete original local Sign key fields')
            fields=self.class_(name).fields;need({k for k,v in fields.items() if not v['modifiers']&8}=={('a','I'),('b','I'),('c','I')},'every actual nonstatic tile-key field')
    def match_tiles(self,left,right,target):
        need(len(left)==len(right),'all tile entries retained')
        for a,b in zip(left,right):
            for k in ('key','value'):
                if target=='jdi':need(self.identity[a[k]]==b[k],'complete JDI tile key/value identity')
                else:self.link(a[k],b[k])
            need(a['key_class']==b['key_class'] and exact(a['key_fields'],b['key_fields']),'complete tile key class/scalars')
    def python_provenance(self):
        m=self.manifest;present={k for k in ('python_modules_before','python_modules_after') if k in m}
        if not present:return # Prior captures retain only their explicitly declared input closure.
        need(present=={'python_modules_before','python_modules_after'} and exact(m['python_modules_before'],m['python_modules_after']),'paired unchanged measured Python module inventory')
        inventory=m['python_modules_before'];need(isinstance(inventory,Mapping) and set(inventory)=={'modules','interpreter_library','interpreter_linkage'},'complete Python inventory shape')
        modules=inventory['modules'];need(isinstance(modules,Mapping) and 0<len(modules)<=512,'bounded complete Python modules')
        inputs=m['inputs_before'];need(exact(inputs,m['inputs_after']),'complete stable Python input fingerprints')
        def declared(value):
            need(type(value) is str and 0<len(value)<=32768,'typed measured Python path');path=Path(value)
            need(path.is_absolute() and path==path.resolve() and path.is_file() and str(path) in inputs,'real resolved Python file declared before/after')
            return path
        if inventory['interpreter_linkage']=='shared':declared(inventory['interpreter_library'])
        else:need(inventory['interpreter_linkage']=='embedded' and inventory['interpreter_library'] is None,'embedded interpreter alone permits no mapped library')
        for name,row in modules.items():
            need(type(name) is str and 0<len(name)<=512 and re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*(?:\.[A-Za-z_][A-Za-z_0-9]*)*',name),'typed Python module name')
            need(isinstance(row,Mapping) and set(row)=={'origin','file','cache'},'complete measured Python module row')
            for key in ('file','cache'):
                if row[key] is not None:declared(row[key]);self.count['python_module_file_references']+=1
            origin=row['origin'];need(origin is None or type(origin) is str and 0<len(origin)<=32768,'typed measured Python origin')
            if origin is not None and origin not in ('built-in','frozen'):
                need(row['file'] is not None and Path(origin).is_absolute() and Path(origin).resolve()==Path(row['file']),'file-based Python origin binds actual measured file')
        py=m['python'];need(isinstance(py,Mapping) and set(py)=={'executable','version','path','flags','environment','scope'},'complete Python execution metadata')
        need(type(py['executable']) is str and 0<len(py['executable'])<=32768 and type(py['version']) is str and 0<len(py['version'])<=4096,'typed Python interpreter/version')
        need(Path(py['executable']).is_absolute(),'absolute measured Python executable');declared(str(Path(py['executable']).resolve()))
        need(isinstance(py['path'],(list,tuple)) and 0<len(py['path'])<=128 and all(type(x) is str and len(x)<=32768 for x in py['path']),'bounded actual Python search path')
        need(type(py['flags']) is str and len(py['flags'])<=4096 and re.fullmatch(r'sys\.flags\((?:[a-z_][a-z_0-9]*=(?:-?\d+|True|False), ?)*[a-z_][a-z_0-9]*=(?:-?\d+|True|False)\)',py['flags']),'typed actual Python flags representation')
        flag_pairs=[tuple(v.strip() for v in x.split('=')) for x in py['flags'][10:-1].split(',')];flag_names=[x[0] for x in flag_pairs]
        need(len(flag_names)==len(set(flag_names)),'unique actual Python flag names')
        need({'debug','inspect','interactive','optimize','dont_write_bytecode','no_user_site','no_site','ignore_environment','verbose','bytes_warning','quiet','hash_randomization','isolated','dev_mode','utf8_mode'}<=set(flag_names),'complete common Python flags')
        need(all(value in ('True','False') if name in ('dev_mode','safe_path') else re.fullmatch('-?\\d+',value) for name,value in flag_pairs),'exact Python flag value kinds')
        environment=py['environment'];names={'PYTHONHOME','PYTHONPATH','PYTHONSAFEPATH','PYTHONNOUSERSITE','PYTHONDONTWRITEBYTECODE'}
        need(isinstance(environment,Mapping) and set(environment)==names and all(v is None or type(v) is str and len(v)<=32768 for v in environment.values()),'complete typed declared Python environment')
        need(py['scope']=='loaded Python files/caches/extensions and interpreter library; no OS dependency closure','precise Python dependency proof scope')
        self.count['measured_python_modules']=len(modules)

    def trace_field_names(self):
        field_names={self.cache_type+'.'+name for name in (('f','g','b','c') if self.context=='dedicated' else ('e','f','a','b'))}
        field_names|={self.chunk_type+'.'+name for name in ('c','n','o','p','q','d','j','k','i','r','b','e','f','g','h','l','m')}
        field_names|={('wi' if self.client else 'ob')+'.a',self.world_type+'.x',self.world_type+'.y'}
        field_names|={self.info_type+'.'+name for name in 'abcdefghijklmno'}
        field_names|={self.world_type+'.'+name for name in ('C','b','c','d','e','D','G')}
        handler_class='fm' if self.client else 'dq';field_names|={handler_class+'.'+name for name in ('e','b','c','d')}
        if self.context=='dedicated':field_names|={'dp.E','he.a'}
        if self.nonempty:
            field_names|={('wf' if self.client else 'oa')+'.'+name for name in 'abc'}
            if self.context=='dedicated':field_names.add('dp.G')
            def graph_fields(value):
                if isinstance(value,Mapping):
                    if 'id' in value and 'class' in value and 'fields' in value:field_names.update(self.external_fields(value['class']))
                    for child in value.values():graph_fields(child)
                elif isinstance(value,(list,tuple)):
                    for child in value:graph_fields(child)
            graph_fields(self.trace['operations'][1]['after']['state']['external_state'])
        return field_names

    def external_fields(self,name):
        result={};seen=set()
        while name in self.original_names:
            need(name not in seen,'acyclic actual original superclass chain');seen.add(name);body=self.class_(name)
            for (field,signature),row in body.fields.items():
                if not row['modifiers']&8:result[name+'.'+field]=signature
            name=body.super_name
        return result
    def array_name(self,name):
        if not name.startswith('['):return name
        depth=len(name)-len(name.lstrip('['));base=name[depth:]
        primitive={'Z':'boolean','B':'byte','C':'char','S':'short','I':'int','J':'long','F':'float','D':'double'}
        need(base in primitive or base.startswith('L') and base.endswith(';'),'actual array class descriptor')
        return (primitive[base] if base in primitive else base[1:-1].replace('/','.'))+'[]'*depth
    def external_graph(self,role,state):
        graph=state['external_state'];need(set(graph)=={'encoding','objects','dedicated_registry','random_representation','within_vm_random_unchanged'} and graph['encoding']=='owned-objects-v1' and graph['within_vm_random_unchanged'] is True,'complete mode-specific external graph')
        need(graph['random_representation']==('actual-JDI-seed-and-Gaussian-fields; raw per-VM state retained' if role=='trace' else 'actual-java-serialization; raw per-VM state retained'),'explicit actual Random representation')
        need(isinstance(graph['objects'],(list,tuple)) and len(graph['objects']) in (0,2),'complete finite Sign/Boat roots')
        expected=('yk','fz') if self.client else ('pq','eb')
        need(tuple(x['class'] for x in graph['objects']) in ((),expected),'exact retained Sign then Boat graph roots')
        need((graph['dedicated_registry'] is not None)==(self.context=='dedicated' and role!='preparation'),'actual dedicated entity registry scope')
        seen={};nodes=0;limit=(1<<63)-1 if role=='trace' else 16384
        known=getattr(self,'external_classes',{});self.external_classes=known;classes=known.setdefault(role,{})
        randoms=getattr(self,'external_randoms',{});self.external_randoms=randoms
        def assignable(name,descriptor):
            if descriptor=='Ljava/lang/Object;':return True
            if descriptor.startswith('['):return self.array_name(name)==self.array_name(descriptor)
            if not descriptor.startswith('L'):return False
            expected=descriptor[1:-1].replace('/','.')
            if name==expected:return True
            if name=='java.util.ArrayList' and expected in ('java.util.List','java.util.Collection','java.lang.Iterable'):return True
            if name=='java.util.HashMap' and expected=='java.util.Map':return True
            while name in self.original_names:
                name=self.class_(name).super_name
                if name==expected:return True
            return False
        def visit(value,depth=0,descriptor=None):
            nonlocal nodes
            nodes+=1;need(nodes<=16384 and depth<=12,'bounded complete external graph')
            if descriptor in ('Z','B','C','S','I','J','F','D'):
                wrappers={'B':('byte',-128,127),'C':('char',0,65535),'S':('short',-32768,32767)}
                if descriptor=='Z':need(type(value) is bool,'exact external boolean field')
                elif descriptor=='I':need(integer(value),'exact external integer field')
                elif descriptor in wrappers:
                    k,lo,hi=wrappers[descriptor];need(isinstance(value,Mapping) and set(value)=={k} and integer(value[k],lo,hi),'exact external small numeric field')
                else:
                    k={'J':'long','F':'binary32','D':'binary64'}[descriptor];need(isinstance(value,Mapping) and set(value)=={k},'exact external numeric wrapper');need(type(value[k]) is str and re.fullmatch('[0-9a-f]{'+('8' if descriptor=='F' else '16')+'}',value[k]),'exact external numeric bits')
                return
            if value is None:return
            if descriptor and isinstance(value,Mapping) and set(value) in ({'reference'},{'borrowed'}):
                if 'reference' in value:need(value['reference'] in seen and assignable(seen[value['reference']],descriptor),'actual field alias assignability')
                else:
                    base=self.records['observed']['initial'] if role=='trace' else self.records[role]['initial'];owners={self.identity[base[k]] if role=='trace' else base[k]:('dp' if self.context=='dedicated' and role!='preparation' else self.world_type) if k=='world' else self.cache_type if k=='cache' else 'mx' if self.client else 'ie' for k in ('world','cache','storage')};need(value['borrowed'] in owners and assignable(owners[value['borrowed']],descriptor),'actual borrowed field assignability')
            if descriptor and isinstance(value,Mapping) and 'class' in value:need(assignable(value['class'],descriptor),'actual complete field owner assignability')
            if type(value) in (bool,int,str):
                if type(value) is int:need(integer(value),'exact unboxed external Integer range')
                need(descriptor is None or descriptor=='Ljava/lang/Object;' or descriptor=='Ljava/lang/String;' and type(value) is str,'external reference field primitive type');return
            need(isinstance(value,Mapping),'typed complete external value');keys=set(value)
            if len(keys)==1:
                key=next(iter(keys))
                if key=='reference':need(integer(value[key],1,limit) and value[key] in seen,'prior external alias')
                elif key=='borrowed':
                    base=self.records['observed']['initial'] if role=='trace' else self.records[role]['initial'];ids={self.identity[base[k]] if role=='trace' else base[k] for k in ('world','cache','storage')};need(value[key] in ids and integer(value[key],1,limit),'actual borrowed context identity')
                elif key in ('byte','short','char','long','binary32','binary64'):visit(value,depth,{'byte':'B','short':'S','char':'C','long':'J','binary32':'F','binary64':'D'}[key])
                else:raise ValidationError('unknown external scalar wrapper')
                return
            need({'id','class'}<=keys and integer(value['id'],1,limit) and value['id'] not in seen and type(value['class']) is str,'fresh actual external owner')
            identity,name=value['id'],value['class'];need(classes.get(identity,name)==name,'stable within-VM external owner class');classes[identity]=name;seen[identity]=name
            body=keys-{'id','class'};need(len(body)==1,'one complete external body');kind=next(iter(body))
            if descriptor and descriptor.startswith('['):need(self.array_name(name)==self.array_name(descriptor),'exact external array field class')
            if name=='java.util.Random':
                if role=='trace':
                    need(kind=='raw_state' and set(value[kind])=={'seed_owner','seed','have_next_gaussian','next_gaussian'},'complete actual JDI Random state');raw=value[kind]
                    need(integer(raw['seed_owner'],1,(1<<63)-1) and raw['seed_owner'] not in self.identity.values() and type(raw['have_next_gaussian']) is bool and set(raw['seed'])=={'long'} and set(raw['next_gaussian'])=={'binary64'},'typed actual JDI seed/Gaussian owners');h64(raw['seed']['long']);h64(raw['next_gaussian']['binary64']);need(0<=h64(raw['seed']['long'])<(1<<48),'actual raw48 external Random seed')
                else:
                    need(kind=='serialized','actual serialized external Random');raw=self.blobs[role].get(value[kind]);self.random_format(raw)
                key=(role,identity);need(key not in randoms or exact(randoms[key],raw),'all actual external Random state unchanged within VM');randoms[key]=raw
            elif kind=='values':
                need(isinstance(value[kind],(list,tuple)) and len(value[kind])<=(256 if name.startswith('[') or name.endswith('[]') else 16),'complete bounded array/collection')
                need(name.startswith('[') or name.endswith('[]') or name=='java.util.ArrayList','actual external array/collection class')
                element=name[1:].replace('.','/') if name.startswith('[') else None
                for item in value[kind]:visit(item,depth+1,element)
            elif kind=='entries':
                need(name=='java.util.HashMap' and isinstance(value[kind],(list,tuple)) and len(value[kind])<=16,'complete bounded logical external map')
                for row in value[kind]:
                    need(set(row)=={'key','value'},'complete logical map entry');visit(row['key'],depth+1);visit(row['value'],depth+1)
            elif kind=='fields':
                fields=self.external_fields(name);need(name in self.original_names and isinstance(value[kind],Mapping) and set(value[kind])==set(fields),'every actual inherited nonstatic original external field')
                for field,desc in fields.items():visit(value[kind][field],depth+1,desc);self.count['external_original_fields']+=1
            else:raise ValidationError('unknown external object body')
        for root in graph['objects']:visit(root)
        visit(graph['dedicated_registry']);need(len(seen)<=128,'external graph owner bound')
        if graph['objects']:
            sign=graph['objects'][0];prefix='ow.' if self.client else 'jh.';text='yk.a' if self.client else 'pq.a'
            need(exact([sign['fields'][prefix+k] for k in 'efg'],[257,64,1]) and exact(sign['fields'][text]['values'],['owned detach','NUL\x00','','']),'entire Sign coordinates/UTF16 text from constructed typed NBT')
        roots={x['id']:x['class'] for x in graph['objects']}
        for key,row in state['registrations'].items():
            need(set(row)=={'id','values'} and isinstance(row['values'],(list,tuple)) and len(row['values'])<=2,'complete mode-specific registration values')
            for value in row['values']:
                if role=='trace':need(set(value)=={'object_id','class'} and value['object_id'] in roots and roots[value['object_id']]==value['class'],'actual full JDI registration value')
                else:need(integer(value,1,16384) and value in roots,'actual external registration root')
        sign_class,boat_class=expected
        for key,row in state['registrations'].items():
            allowed=boat_class if key in ('b','D') else sign_class if key=='c' else None
            values=[v['class'] if role=='trace' else roots[v] for v in row['values']]
            need(not values or allowed is not None and values==[allowed],'finite original registration roles/complete values')
        for value in state['chunks']:
            if role=='trace':
                chunk=self.header(value['chunk']);tiles=value['tiles'];slices=value['entities']
            else:chunk=value;tiles=value['tiles'];slices=value['entities']
            if exact(chunk['coordinates'],[16,0]) and chunk['class']==self.chunk_type:
                need(len(graph['objects'])==2 and len(tiles['entries'])==1,'every loaded external body has both complete graph roots/tile')
                sign,boat=graph['objects'];need(tiles['entries'][0]['value']==sign['id'],'actual Sign root bound to chunk tile map')
                for i,slice_ in enumerate(slices):
                    members=slice_['members'];need(len(members)==(1 if i==4 else 0),'complete Boat in actual fourth-height slice')
                    if members:
                        member=members[0];need(member['object_id' if role=='trace' else 'id']==boat['id'] and member['class']==boat['class'],'actual Boat root bound to slice member')
                        if role!='trace':
                            prefix='sn.' if self.client else 'lq.';positions=('aM','aN','aO') if self.client else ('aP','aQ','aR');membership=('bF','bG','bH','bI') if self.client else ('bG','bH','bI','bJ')
                            need(exact(member['position_bits'],[boat['fields'][prefix+x]['binary64'] for x in positions]) and exact(member['membership'],[boat['fields'][prefix+x] for x in membership]),'all represented Boat position/membership fields bound to whole inherited graph')
                            need(exact(member['position_bits'],['4070280000000000','4050400000000000','4004000000000000']),'full loaded Boat position from constructed typed NBT')
        self.count['external_graphs']+=1;self.count['external_graph_nodes']+=nodes
    def external_compare(self,left,right,target,allow_invalid=None):
        def compare(a,b,path=''):
            if not isinstance(a,Mapping):
                if isinstance(a,(list,tuple)):
                    need(isinstance(b,(list,tuple)) and len(a)==len(b),'complete external logical sequence')
                    for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'['+str(i)+']')
                else:need(exact(a,b),'all external scalar values '+path)
                return
            need(isinstance(b,Mapping),'paired complete external node')
            if set(a) in ({'reference'},{'borrowed'}):
                key=next(iter(a));need(set(b)=={key},'paired external alias kind')
                if target=='jdi':need(self.identity[a[key]]==b[key],'actual JDI external alias')
                else:self.link(a[key],b[key])
                return
            if 'id' in a:
                need('id' in b and self.array_name(a['class'])==self.array_name(b['class']),'actual external owner class')
                if target=='jdi':need(self.identity[a['id']]==b['id'],'actual external owner identity')
                else:self.link(a['id'],b['id'])
                if a['class']=='java.util.Random':
                    raw=self.blobs['observed'].get(a['serialized']);self.random_format(raw)
                    if target=='jdi':need(exact(b['raw_state']['seed'],{'long':raw[95:103].hex()}) and b['raw_state']['have_next_gaussian'] is bool(raw[86]) and exact(b['raw_state']['next_gaussian'],{'binary64':raw[87:95].hex()}),'entire actual Random serialization/JDI state binding');self.count['external_random_jdi_bindings']+=1
                    else:
                        other=self.blobs['control'].get(b['serialized']);self.random_format(other);need(raw[:95]==other[:95] and raw[103:]==other[103:],'only unconsumed per-VM external Random seed differs');self.count['external_random_control_pairs']+=1
                    return
            need(set(a)==set(b),'complete external graph keys')
            for key in a:
                if key in ('id','class','random_representation'):continue
                if allow_invalid is not None and key==('ow.h' if self.client else 'jh.h'):need(b[key] is allow_invalid,'actual event-bound Sign invalidation flag')
                else:compare(a[key],b[key],path+'.'+key)
        compare(left,right)
    def external_event(self,event):
        state=event['state'];graph=state['external_state'];op=self.records['observed']['operations'][1];anchor=op['after']['external_state']
        if event['role']=='external_tile_invalidate' and event['code_index']==5:self.external_invalid=True
        if event['role']=='external_entity_registry' and event['code_index']==17:self.external_registered=True
        need(len(graph['objects'])>=self.external_root_count,'retained event external roots never omitted');self.external_root_count=len(graph['objects'])
        need(len(graph['objects']) in (0,2),'complete finite event roots')
        for a,b in zip(anchor['objects'],graph['objects']):self.external_compare(a,b,'jdi',self.external_invalid)
        if self.context=='dedicated':
            expected=anchor['dedicated_registry'] if self.external_registered else self.records['observed']['initial']['external_state']['dedicated_registry']
            self.external_compare(expected,graph['dedicated_registry'],'jdi')
        self.count['external_full_event_graphs']+=1

    def numeric_graph(self,role,root):
        seen=set();nodes=0
        def visit(value,depth):
            nonlocal nodes
            nodes+=1;need(nodes<=1024*1024 and depth<=32,'bounded complete numerical graph')
            if value is None or type(value) in (bool,int,str):return
            if isinstance(value,(list,tuple)):
                need(len(value)<=32768,'bounded complete numerical array')
                for child in value:visit(child,depth+1)
                return
            need(isinstance(value,Mapping),'typed numerical graph value');keys=set(value)
            if len(keys)==1:
                key=next(iter(keys));child=value[key]
                if key in ('long','binary64'):h64(child)
                elif key=='binary32':need(type(child) is str and re.fullmatch('[0-9a-f]{8}',child),'exact float bits')
                elif key in ('byte','short','char'):need(integer(child,{'byte':-128,'short':-32768,'char':0}[key],{'byte':127,'short':32767,'char':65535}[key]),'exact small numeric type')
                elif key=='reference':need(integer(child,1,16384) and child in seen,'numerical prior-object alias')
                elif key=='borrowed':need(integer(child,1,16384) and child in tuple(self.records[role]['initial'][k] for k in ('world','cache','storage')),'actual borrowed context identity')
                else:raise ValidationError('unknown numerical scalar wrapper')
                return
            need({'id','class'}<=keys and integer(value['id'],1,16384) and value['id'] not in seen and type(value['class']) is str,'fresh numerical owned-object identity');seen.add(value['id'])
            body=keys-{'id','class'};need(len(body)==1,'one complete numerical body');kind=next(iter(body))
            if kind=='serialized':need(value['class']=='java.util.Random','actual numerical Random type');self.random_format(self.blobs[role].get(value[kind]))
            elif kind=='values':need(value['class'].startswith('[') or value['class'].startswith('java.util.'),'actual numerical array/collection class');visit(value[kind],depth+1)
            elif kind=='class_literal':need(value['class']=='java.lang.Class' and value[kind] in self.records[role]['class_origins'],'actual selected original class literal')
            elif kind=='fields':
                need(value['class'] in self.records[role]['class_origins'] and isinstance(value[kind],Mapping),'actual numerical original owner')
                for name,child in value[kind].items():need(name in self.records[role]['fields'],'complete bound numerical field');visit(child,depth+1)
            else:raise ValidationError('unknown numerical object body')
        visit(root,0);self.count['numeric_graph_nodes']+=nodes

    def tables(self):
        for role,record in (*self.records.items(),('trace',self.trace)):
            if record.indexed:need(record.used_chunks==set(range(len(record['chunk_states']))),'every interned full chunk state referenced: '+role)
            if role=='trace':need(record.used_headers==set(range(len(record['chunk_headers']))),'every interned header referenced')
            need(self.blobs[role].referenced==set(self.blobs[role].raw),'every retained blob has an actual semantic reference: '+role)
            self.count['referenced_chunk_table_rows']+=len(record.used_chunks)

    def process_commands(self,jar):
        p=self.capture;m=self.manifest
        java=next(str(Path(x)) for x in m['inputs_before'] if Path(x).name in ('java','java.exe'))
        javac=next(str(Path(x)) for x in m['inputs_before'] if Path(x).name in ('javac','javac.exe'))
        commands={r['name']:r['command'] for r in m['processes']}
        need(commands['java-version']==[java,'-Xmx128m','-version'],'actual version command')
        compile_command=[javac,'-J-Xmx512m','-proc:none','-implicit:none','-Xlint:all','-Werror','--release','17','--add-modules','jdk.jdi','-encoding','UTF-8','-classpath',str(p/'empty'),'-sourcepath',str(p/'empty'),'--module-path',str(p/'empty'),'-d',str(p/'classes'),str(p/'source/ChunkLifecycleObservation.java'),str(p/'source/ChunkLifecycleController.java')]
        need(commands['compile']==compile_command and not any((p/'empty').iterdir()),'isolated exact helper compile')
        for role,parent in (('prepare','preparation'),('control','control-world')):
            context=('client' if self.client else 'base') if role=='prepare' else self.context
            mode=('prepare-detach' if self.nonempty else 'prepare') if role=='prepare' else self.scenario;output='preparation.json' if role=='prepare' else 'control.json'
            need(commands[role]==[java,'-Xint','-Xmx512m','-cp',str(p/'classes'),'ChunkLifecycleObservation',str(jar),context,mode,'0000000000000017',str(p/parent),str(p/output)],'exact original helper command '+role)
        args=[str(jar),self.context,self.scenario,'0000000000000017',str(p/'observed-world'),str(p/'observed.json')]
        need(commands['observed']==[java,'--add-modules','jdk.jdi','-Xmx512m','-cp',str(p/'classes'),'ChunkLifecycleController',*args,str(p/'trace.json')],'exact observed controller command')
        need(exact(self.trace['child_command'],[java,'-Xint','-Xmx512m','-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0','-cp',str(p/'classes'),'ChunkLifecycleObservation',*args]),'exact original debuggee command')

    def construction_proof(self):
        self.handlers={};self.competitors={}
        for role,d in self.records.items():
            if 'construction_trace' not in d:
                need(self.development,'missing constructor epoch evidence');self.gap('Historical capture lacks constructor epochs/handler path proof.');continue
            rows=d['construction_trace'];need(1<=len(rows)<=256,'bounded complete constructor trace')
            expected_context=d['context'];dedicated=expected_context=='dedicated'
            route='direct-public-mx-and-fd; real-hi-close' if self.client else 'direct-public-owner-ie-dp; no-server-start' if dedicated else 'direct-public-ie-and-dj'
            need(d['constructor_route']==route,'exact genuine constructor route')
            result_ids=set();last=0
            for row in rows:
                need(set(row)=={'kind','class','method','signature','arguments','interval_ms','result'} and row['kind']=='constructor' and row['method']=='<init>','complete constructor event')
                self.binding({'class':row['class'],'method':'<init>','signature':row['signature'],'modifiers':1},'method')
                low,high=row['interval_ms'];need(integer(low,last,(1<<63)-1) and integer(high,low,low+180000),'ordered bounded constructor epoch');last=high
                need(set(row['result'])=={'reference','class'} and integer(row['result']['reference'],1,16384) and row['result']['reference'] not in result_ids and row['result']['class']==row['class'],'fresh exact constructor result');result_ids.add(row['result']['reference'])
            handler_class='mx' if self.client else 'ie';storage=[r for r in rows if r['class']==handler_class]
            has_session=role!='preparation' and self.scenario not in ('unload101','detach-owned') and 'competing_handler' in d['initial']
            need(len(storage)==(2 if has_session else 1),'one primary and exactly scheduled competing save-handler constructor');ctor=storage[0];h=d['initial']['handler']
            need(ctor['signature']=='(Ljava/io/File;Ljava/lang/String;Z)V' and len(ctor['arguments'])==3,'handler exact public constructor binding')
            arg=ctor['arguments'][0];parent=self.capture/('preparation' if role=='preparation' else role+'-world')
            need(set(arg)=={'reference','path'} and integer(arg['reference'],1,16384) and Path(arg['path'])==parent and exact(ctor['arguments'][1:],['world',False]),'handler exact constructor arguments')
            need(ctor['result']['reference']==d['initial']['storage']==h['id'] and h['class']==handler_class,'handler result actual retained identity')
            token=h64(h['token']);need(ctor['interval_ms'][0]<=token<=ctor['interval_ms'][1],'session token inside actual handler constructor epoch')
            self.handlers[role]=h
            worldclass='fd' if self.client else 'dp' if dedicated else 'dj';world=[r for r in rows if r['class']==worldclass]
            need(len(world)==1 and world[0]['result']['reference']==d['initial']['world'],'actual constructor world identity')
            wr=world[0];args=wr['arguments']
            if dedicated:
                need(wr['signature']=='(Lnet/minecraft/server/MinecraftServer;Lom;Ljava/lang/String;IJ)V' and len(args)==5 and args[1]=={'reference':h['id']} and exact(args[2:],['chunk-lifecycle-reference',0,{'long':'0000000000000017'}]),'dedicated world original arguments')
                owner=[r for r in rows if r['class']=='net.minecraft.server.MinecraftServer'];need(len(owner)==1 and owner[0]['signature']=='()V' and exact(owner[0]['arguments'],[]) and exact(args[0],{'reference':owner[0]['result']['reference']}),'genuine unstarted dedicated owner')
            else:
                need(wr['signature']==('(Lwt;Ljava/lang/String;JLxa;)V' if self.client else '(Lom;Ljava/lang/String;JLos;)V') and len(args)==4 and exact(args[:3],[{'reference':h['id']},'chunk-lifecycle-reference',{'long':'0000000000000017'}]),'base world original arguments')
                provider=[r for r in rows if r['class']==('rh' if self.client else 'kp')];need(len(provider)==1 and provider[0]['signature']=='()V' and exact(provider[0]['arguments'],[]) and exact(args[3],{'reference':provider[0]['result']['reference']}),'genuine surface provider constructor')
            prefix=['hi','mx','rh','fd'] if self.client else ['ie','net.minecraft.server.MinecraftServer','dp'] if dedicated else ['ie','kp','dj']
            suffix=([('nu' if self.client else 'iq')]*5+[('sp' if self.client else 'lr')]+[('nu' if self.client else 'iq')]*3+[('sp' if self.client else 'lr')]+[('nu' if self.client else 'iq')]*2+[('sp' if self.client else 'lr')]*2+[('nu' if self.client else 'iq')]+[('sp' if self.client else 'lr')]+[('nu' if self.client else 'iq')]) if role=='preparation' and self.nonempty else [('nu' if self.client else 'iq')]*102 if role=='preparation' else ([('fz' if self.client else 'eb')] if self.scenario=='cohort' else [])+([handler_class] if has_session else [])
            need([r['class'] for r in rows]==prefix+suffix,'complete finite constructor schedule')
            if self.client:
                format_args=rows[0]['arguments'];need(rows[0]['signature']=='(Ljava/io/File;)V' and len(format_args)==1 and set(format_args[0])=={'reference','path'} and integer(format_args[0]['reference'],1,16384) and Path(format_args[0]['path'])==parent,'real client format owner constructor')
            for row in rows[len(prefix):]:
                if role=='preparation':need(row['signature']=='()V' and exact(row['arguments'],[]),'genuine empty compound clone container')
                elif row['class']!=handler_class:need(row['signature']==('(Lfd;DDD)V' if self.client else '(Ldj;DDD)V') and exact(row['arguments'],[{'reference':d['initial']['world']},{'binary64':'3fe0000000000000'},{'binary64':'4050000000000000'},{'binary64':'3fe0000000000000'}]),'exact API-owned temporary Boat setup')
            if has_session:
                competing=d['final']['competing_handler'];cc=storage[1]
                need(d['initial']['competing_handler'] is None and competing['id']==cc['result']['reference'] and competing['id']!=h['id'] and cc['signature']==ctor['signature'] and len(cc['arguments'])==3 and set(cc['arguments'][0])=={'reference','path'} and Path(cc['arguments'][0]['path'])==parent and exact(cc['arguments'][1:],['world',False]),'exact genuine competing-handler construction')
                token=h64(competing['token']);need(competing['token']!=h['token'] and cc['interval_ms'][0]<=token<=cc['interval_ms'][1],'distinct competing token in its own constructor epoch')
                need(all(exact(competing[k],h[k]) for k in ('class','world_root','players_root','data_root')),'competing handler owns exact same saved-world paths')
                self.competitors[role]=competing
            self.count['constructor_epoch_bindings']+=len(rows)
    def binding(self,row,kind):
        keys={'class','method' if kind=='method' else 'name','signature','modifiers'}
        need(set(row)==keys or kind=='method' and set(row)==keys|{'bytecode_length','bytecode_sha256'},'complete exact member descriptor')
        need(all(type(row[k]) is str for k in keys-{'modifiers'}) and integer(row['modifiers'],0,65535),'typed member descriptor')
        cls=self.class_(row['class']);key=(row['method' if kind=='method' else 'name'],row['signature'])
        table=cls.methods if kind=='method' else cls.fields;need(key in table and table[key]['modifiers']==row['modifiers'],'actual class member binding '+str(key))
        if 'bytecode_sha256' in row:
            code=table[key]['code'];need(integer(row['bytecode_length'],1,65535) and type(row['bytecode_sha256']) is str and re.fullmatch('[0-9a-f]{64}',row['bytecode_sha256']) and code is not None and len(code)==row['bytecode_length'] and hashlib.sha256(code).hexdigest()==row['bytecode_sha256'],'actual selected method body')
    def metadata_file(self,raw):
        key=hashlib.sha256(raw).hexdigest()
        if key in self.metadata_cache:return self.metadata_cache[key]
        need(len(raw)>=18 and raw[:4]==b'\x1f\x8b\x08\x00','finite original gzip framing')
        tree=NBT(inflate(raw,True));need(tree.root[0]==10,'metadata root');data=tree.child(tree.root,'Data',10)
        need({x[1] for x in tree.root[2]}=={b'Data'},'complete finite metadata root')
        expected={'RandomSeed':4,'SpawnX':3,'SpawnY':3,'SpawnZ':3,'Time':4,'LastPlayed':4,'SizeOnDisk':4,'LevelName':8,'version':3,'raining':1,'rainTime':3,'thundering':1,'thunderTime':3}
        need({x[1].decode('ascii'):x[0] for x in data[2]}==expected,'full exact typed metadata schema')
        values={name:tree.child(data,name,kind)[2] for name,kind in expected.items()}
        need(values['raining'] in (0,1) and values['thundering'] in (0,1),'canonical bool bytes')
        start,end=tree.spans[(tree.root[1],b'Data',b'LastPlayed')];need(end-start==8,'LastPlayed payload span')
        normal=tree.raw[:start]+bytes(8)+tree.raw[end:]
        result={'tree':tree,'values':values,'normal':normal,'gzip_header':raw[:10]};self.metadata_cache[key]=result;self.count['metadata_files_decoded']+=1;return result
    def region(self,name,raw):
        cachekey=(name,hashlib.sha256(raw).hexdigest())
        if cachekey in self.region_cache:return self.region_cache[cachekey]
        match=re.fullmatch(r'region/r\.(-?\d+)\.(-?\d+)\.mcr',name);need(match and 8192<=len(raw)<=8*1024*1024 and len(raw)%4096==0,'complete region geometry')
        rx,rz=map(int,match.groups());records={};used=set()
        for slot in range(1024):
            location=struct.unpack_from('>I',raw,slot*4)[0];stamp=struct.unpack_from('>I',raw,4096+slot*4)[0]
            if not location:continue
            sector,count=location>>8,location&255;need(sector>=2 and count>0 and (sector+count)*4096<=len(raw),'allocated record bounds')
            sectors=set(range(sector,sector+count));need(not(used&sectors),'no overlapping live sectors');used|=sectors
            offset=sector*4096;length=struct.unpack_from('>I',raw,offset)[0];need(1<length<=count*4096-4,'record length')
            selector=raw[offset+4];need(selector in (1,2),'region compression selector')
            nbt=NBT(inflate(raw[offset+5:offset+4+length],selector==1));level=nbt.child(nbt.root,'Level',10)
            expected={'xPos':3,'zPos':3,'LastUpdate':4,'Blocks':7,'Data':7,'SkyLight':7,'BlockLight':7,'HeightMap':7,'TerrainPopulated':1,'Entities':9,'TileEntities':9}
            need({x[1].decode('ascii'):x[0] for x in level[2]}==expected and {x[1] for x in nbt.root[2]}=={b'Level'},'full original chunk schema')
            fields={k:nbt.child(level,k,t)[2] for k,t in expected.items()}
            for role,field in zip(ARRAYS,('Blocks','Data','SkyLight','BlockLight','HeightMap')):need(len(fields[field])==ARRAYS[role],'full disk array '+field)
            need(fields['TerrainPopulated'] in (0,1),'canonical TerrainPopulated byte')
            if self.nonempty and (fields['xPos'],fields['zPos'])==(16,0):
                for field,expected_list in self.external_lists().items():need(exact(fields[field],expected_list),'complete typed original Boat/Sign disk NBT '+field)
            else:need(fields['Entities']==fields['TileEntities']==(1,()),'finite exact empty-list runtime records')
            x,z=fields['xPos'],fields['zPos'];need(x//32==rx and z//32==rz and (x&31)+(z&31)*32==slot,'clone slot/coordinates')
            records[slot]={'fields':fields,'nbt':nbt.raw,'offset':offset,'end':offset+4+length,'stamp':stamp,'selector':selector}
            self.count['disk_chunk_records_decoded']+=1
        result={'records':records,'raw':raw};self.region_cache[cachekey]=result;self.count['region_images_decoded']+=1;return result
    def blob_files(self,role,state):return {name:self.blobs[role].get(row) for name,row in state['files'].items()}
    def header(self,row):
        if isinstance(row,Mapping) and set(row)=={'header_reference'}:self.count['indexed_header_references']+=1
        return self.trace.header(row)
    def helper_state(self,role,state):
        need(set(state)==STATE_KEYS|{'handler','competing_handler'}|({'external_state'} if self.nonempty else set()) or self.development and set(state) in (STATE_KEYS,STATE_KEYS|{'handler'}),'complete helper state fields');chunks=state['chunks'];need(0<len(chunks)<=128,'complete retained chunks')
        for key in ('world','cache','storage','info','provider','generator','loader','fallback'):need(integer(state[key],1,16384),'typed complete context identity')
        if 'handler' in state:
            h=state['handler'];parent=self.capture/('preparation' if role=='preparation' else role+'-world')/'world'
            need(set(h)=={'id','class','token','world_root','players_root','data_root'} and integer(h['id'],1,16384) and h['id']==state['storage'] and h['class']==('mx' if self.client else 'ie') and h['token']==state['handler_token'],'complete retained handler identity/token')
            for key,suffix in (('world_root',''),('players_root','players'),('data_root','data')):need(Path(h[key])==parent/suffix,'actual handler path '+key)
            need(exact(h,self.handlers[role]),'stable primary handler/token/paths')
        need(len({c['id'] for c in chunks})==len(chunks),'distinct retained chunk identities')
        for c in chunks:
            need(set(c)=={'id','class','world','coordinates','blank','flags','last_save','minimum','arrays','tiles','slices_owner','entities'},'complete helper chunk')
            need(integer(c['id'],1,16384) and type(c['blank']) is bool and integer(c['world'],1,16384) and c['world']==state['world'],'chunk owner')
            need(len(c['coordinates'])==2 and all(integer(x) for x in c['coordinates']) and integer(c['minimum']),'chunk coordinates/minimum')
            need(set(c['flags'])==set('cnopq') and all(type(x) is bool for x in c['flags'].values()),'all typed flags');h64(c['last_save'])
            need(set(c['arrays'])==set(ARRAYS),'all five array roles')
            for array,length in ARRAYS.items():
                row=c['arrays'][array];need(set(row)=={'owner','array','data'} and integer(row['owner'],1,16384) and integer(row['array'],1,16384) and len(self.blobs[role].get(row['data']))==length,'full typed array owner/backing/bytes')
            need(set(c['tiles'])=={'id','entries'} and integer(c['tiles']['id'],1,16384) and integer(c['slices_owner'],1,16384),'typed chunk collection owners')
            need(len(c['entities'])==8,'complete eight slices')
            if self.nonempty:self.tile_entries(c['tiles']['entries'],role)
            else:need(exact(c['tiles']['entries'],[]),'empty supported-profile tiles')
            for entity in c['entities']:
                need(set(entity)=={'id','members'} and integer(entity['id'],1,16384) and isinstance(entity['members'],(list,tuple)) and len(entity['members'])<=1,'finite typed entity setup')
                for member in entity['members']:
                    need(set(member)=={'id','class','position_bits','membership'} and integer(member['id'],1,16384) and member['class']==('fz' if self.client else 'eb'),'complete represented entity')
                    need(len(member['position_bits'])==3 and len(member['membership'])==4,'entity represented shape')
                    need(type(member['membership'][0]) is bool and all(integer(v) for v in member['membership'][1:]),'typed complete entity membership fields')
                    for value in member['position_bits']:h64(value)
            if c['blank']:
                need(c['id']==state['fallback'] and c['class']==self.blank_type,'actual blank fallback')
                need(all(not any(self.blobs[role].get(a['data'])) for a in c['arrays'].values()),'all blank backing bytes zero')
            self.count['helper_full_chunks']+=1
        ids={x['id'] for x in chunks};need(state['fallback'] in ids and sum(x['blank'] for x in chunks)==1,'retained exact fallback present')
        for key in ('map','insertions','pending','queue'):need(integer(state[key]['id'],1,16384),'typed helper collection owner')
        need(len(state['map']['entries'])<=128 and all(integer(k) and integer(v,1,16384) and v in ids for k,v in state['map']['entries']),'complete map members')
        need(len({k for k,v in state['map']['entries']})==len(state['map']['entries']),'unique packed map keys')
        need(len(state['insertions']['values'])<=256 and all(integer(v,1,16384) and v in ids for v in state['insertions']['values']),'insertion members');need(len(state['pending']['values'])<=256 and all(integer(v) for v in state['pending']['values']) and len(set(state['pending']['values']))==len(state['pending']['values']),'unique typed pending')
        need(exact(state['queue']['values'],[]) and set(state['registrations'])==set(('b','c','d','e','D','G')),'complete registrations/empty queue')
        if self.nonempty:self.external_graph(role,state)
        else:need(all(exact(v['values'],[]) for v in state['registrations'].values()),'empty supported-profile registrations')
        need(all(integer(v['id'],1,16384) for v in state['registrations'].values()),'typed helper registration owners')
        need(list(state['metadata'])==INFO,'all known Info fields')
        for k in ('seed','time','last_played','size_on_disk'):h64(state['metadata'][k])
        for k in ('spawn_x','spawn_y','spawn_z','player_dimension','version','rain_time','thunder_time'):need(integer(state['metadata'][k]),'typed Info int')
        need(state['metadata']['player'] is None and type(state['metadata']['level_name']) is str and all(type(state['metadata'][k]) is bool for k in ('raining','thundering')),'Info Player/name/bools')
        for key in ('world_force_load','save_off'):need(type(state[key]) is bool,'typed world policy')
        need(type(state['provider_force_load']) is bool if self.context=='dedicated' and role!='preparation' else state['provider_force_load'] is None,'typed provider force policy')
        competing=state.get('competing_handler')
        if competing is not None:need(role in self.competitors and exact(competing,self.competitors[role]),'exact stable genuine competing handler')
        files=self.blob_files(role,state);need(files['session.lock']==bytes.fromhex(competing['token'] if competing is not None else state['handler_token']),'exact current lock/writer-token bytes')
        need(0<len(files)<=256 and 'level.dat_new' not in files,'complete finite file set')
        for name,raw in files.items():
            if name.startswith('region/'):self.region(name,raw)
            elif name in ('level.dat','level.dat_old'):self.metadata_file(raw)
            else:need(name=='session.lock','unexpected file')
        self.count['helper_states']+=1
    def trace_state(self,state,helper=None):
        keys=set(('snapshot_kind','world_id','cache_id','storage_id','info_id','map','insertions','pending','chunks','metadata','registrations','world_force_load','save_off','provider_force_load'))
        need(set(state)==keys|{'handler','competing_handler'}|({'external_state'} if self.nonempty else set()) or self.development and set(state) in (keys,keys|{'handler'}),'complete JDI state fields')
        base=self.records['observed']['initial']
        for key in ('world','cache','storage','info'):need(integer(state[key+'_id'],1,(1<<63)-1) and state[key+'_id']==self.identity[base[key]],'stable actual JDI context identity')
        for key in ('map','insertions','pending'):need(integer(state[key]['id'],1,(1<<63)-1) and state[key]['id']==self.identity[base[key]['id']],'stable actual JDI collection owner')
        pending_map=state['pending']['map_id'];need(integer(pending_map,1,(1<<63)-1),'typed JDI pending backing map')
        if hasattr(self,'pending_map_id'):need(pending_map==self.pending_map_id,'stable pending backing-map owner')
        else:self.pending_map_id=pending_map
        need(len(state['chunks'])<=128 and len(state['map']['entries'])<=256 and len(state['insertions']['values'])<=256 and len(state['pending']['values'])<=256,'complete bounded JDI collections')
        need(all(integer(v) for v in state['pending']['values']) and len(set(state['pending']['values']))==len(state['pending']['values']),'typed unique pending values')
        need(set(state['metadata'])==set('abcdefghijklmno'),'complete JDI Info fields')
        for key in ('a','e','f','g'):h64(state['metadata'][key])
        for key in ('b','c','d','i','k','m','o'):need(integer(state['metadata'][key]),'typed exact JDI Info int')
        need(state['metadata']['h'] is None and type(state['metadata']['j']) is str and all(type(state['metadata'][k]) is bool for k in ('l','n')),'typed JDI Info values')
        need(set(state['registrations'])==set(('C','b','c','d','e','D','G')) and exact(state['registrations']['C']['values'],[]),'complete JDI registrations/empty queue')
        if self.nonempty:self.external_graph('trace',state)
        else:need(all(exact(v['values'],[]) for v in state['registrations'].values()),'empty supported-profile JDI registrations')
        for key,row in state['registrations'].items():
            owner=base['queue'] if key=='C' else base['registrations'][key];need(integer(row['id'],1,(1<<63)-1) and row['id']==self.identity[owner['id']],'stable registration/queue owner')
        need(type(state['world_force_load']) is bool and type(state['save_off']) is bool and (type(state['provider_force_load']) is bool if self.context=='dedicated' else state['provider_force_load'] is None),'typed JDI world policies')
        if 'handler' in state:
            h=base['handler'];th=state['handler'];need(set(th)==set(h) and integer(th['id'],1,(1<<63)-1) and th['id']==self.identity[h['id']] and all(exact(th[k],h[k]) for k in h if k!='id'),'complete stable JDI handler/token/paths')
        if state.get('competing_handler') is not None:self.jdi_handler(state['competing_handler'],self.competitors['observed'])
        need(state['snapshot_kind'] in (('all-retained-chunks-full',) if self.nonempty else ('all-retained-chunks-full','all-headers-and-target-full')),'explicit full nonempty state scope')
        headers={}
        for row in state['chunks']:
            full='chunk' in row;h=self.header(row['chunk'] if full else row)
            need(set(h)=={'id','class','world','coordinates','flags','minimum','last_save'},'complete indexed header')
            need(integer(h['id'],1,1<<63) and integer(h['world'],1,1<<63) and h['class'] in (self.chunk_type,self.blank_type) and len(h['coordinates'])==2 and all(integer(x) for x in h['coordinates']) and integer(h['minimum']),'typed full indexed header')
            need(set(h['flags'])==set('cnopq') and all(type(x) is bool for x in h['flags'].values()),'typed indexed flags');h64(h['last_save'])
            need(h['id'] not in headers and h['world']==state['world_id'],'distinct complete chunk header/world');headers[h['id']]=h
            if state['snapshot_kind']=='all-retained-chunks-full':need(full,'full API boundary chunk omitted')
            if full:
                self.trace_payload(row,h)
        for row in state['map']['entries']:
            need(set(row)=={'key','chunk'} and integer(row['key']),'complete typed JDI map entry');h=self.header(row['chunk']);need(h['id'] in headers and exact(h,headers[h['id']]),'map points at exact current retained header')
        need(len({row['key'] for row in state['map']['entries']})==len(state['map']['entries']),'distinct JDI map keys')
        for row in state['insertions']['values']:
            h=self.header(row);need(h['id'] in headers and exact(h,headers[h['id']]),'insertion points at exact current retained header')
        if helper is not None:self.match_jdi(helper,state)
        self.count['trace_states']+=1
    def trace_payload(self,row,h):
        need(set(row)=={'chunk','arrays','tiles','slices_owner','entities'} and set(row['arrays'])==set(ARRAYS) and len(row['entities'])==8 and type(row['tiles']['size']) is int,'full JDI chunk shape')
        if self.nonempty:self.tile_entries(row['tiles']['entries'],'trace');need(row['tiles']['size']==len(row['tiles']['entries']),'all JDI tile entries')
        else:need(row['tiles']['size']==0,'empty supported-profile JDI tiles')
        anchor=self.payload_anchors[h['id']]
        if self.nonempty:self.match_tiles(anchor['tiles']['entries'],row['tiles']['entries'],'jdi')
        need(row['tiles']['id']==self.identity[anchor['tiles']['id']] and row['slices_owner']==self.identity[anchor['slices_owner']],'stable actual callback tile/slices owners')
        for name,length in ARRAYS.items():
            a=row['arrays'][name];expected=anchor['arrays'][name]
            need(set(a)=={'owner','data'} and a['owner']==self.identity[expected['owner']] and a['data']['id']==self.identity[expected['array']],'stable actual callback array/wrapper identity')
            raw=self.blobs['trace'].get(a['data']);need(len(raw)==length and raw==self.blobs['observed'].get(expected['data']),'full immutable load-only callback array bytes')
        for actual,expected in zip(row['entities'],anchor['entities']):
            need(set(actual)=={'id','members'} and actual['id']==self.identity[expected['id']] and len(actual['members'])<=1,'stable callback entity slice owner')
            for member in actual['members']:need(set(member)=={'object_id','class'} and integer(member['object_id'],1,(1<<63)-1) and member['class']==('fz' if self.client else 'eb') and member['object_id'] in self.identity.values(),'actual finite API-owned Boat member identity')
        if self.nonempty:
            for actual,expected in zip(row['entities'],anchor['entities']):need(exact(actual['members'],[{'object_id':self.identity[x['id']],'class':x['class']} for x in expected['members']]),'complete immutable nonempty callback slice members')
        self.count['trace_full_chunks']+=1
    def match_jdi(self,h,t,count=True):
        ids=self.identity
        for role in ('world','cache','storage','info'):need(ids[h[role]]==t[role+'_id'],'helper/JDI global identity')
        for role in ('map','insertions','pending'):need(ids[h[role]['id']]==t[role]['id'],'collection owner identity')
        need([(k,ids[v]) for k,v in h['map']['entries']]==[(x['key'],self.header(x['chunk'])['id']) for x in t['map']['entries']],'complete ordered map links')
        need([ids[x] for x in h['insertions']['values']]==[self.header(x)['id'] for x in t['insertions']['values']],'complete insertion links')
        need(exact(h['pending']['values'],t['pending']['values']),'complete pending order')
        actual={self.header(x['chunk'])['id']:x for x in t['chunks']};need(set(actual)=={ids[x['id']] for x in h['chunks']},'every retained chunk linked')
        for c in h['chunks']:
            value=actual[ids[c['id']]];header=self.header(value['chunk'])
            for field in ('class','coordinates','flags','minimum','last_save'):need(exact(c[field],header[field]),'full helper/JDI header '+field)
            need(ids[c['world']]==header['world'],'chunk world identity')
            for name in ARRAYS:
                a,b=c['arrays'][name],value['arrays'][name];need(ids[a['owner']]==b['owner'] and ids[a['array']]==b['data']['id'],'wrapper/backing identity')
                need(self.blobs['observed'].get(a['data'])==self.blobs['trace'].get(b['data']),'all helper/JDI bytes')
            need(ids[c['tiles']['id']]==value['tiles']['id'] and ids[c['slices_owner']]==value['slices_owner'],'tile/slices owner')
            if self.nonempty:self.match_tiles(c['tiles']['entries'],value['tiles']['entries'],'jdi')
            for a,b in zip(c['entities'],value['entities']):
                need(ids[a['id']]==b['id'] and [(ids[x['id']],x['class']) for x in a['members']]==[(x['object_id'],x['class']) for x in b['members']],'entity slices/members')
        need(exact(dict(zip('abcdefghijklmno',(h['metadata'][k] for k in INFO))),t['metadata']),'all helper/JDI Info')
        if 'handler' in h:
            th=t['handler'];need(set(th)==set(h['handler']) and th['id']==ids[h['handler']['id']],'actual JDI handler identity')
            need(all(exact(h['handler'][k],th[k]) for k in h['handler'] if k!='id'),'all JDI handler/token/path fields')
        if 'competing_handler' in h:
            need((h['competing_handler'] is None)==(t['competing_handler'] is None),'complete helper/JDI competing-handler presence')
            if h['competing_handler'] is not None:self.jdi_handler(t['competing_handler'],h['competing_handler'])
        for k in ('world_force_load','save_off','provider_force_load'):need(exact(h[k],t[k]),'all policy flags')
        for k,row in h['registrations'].items():
            need(ids[row['id']]==t['registrations'][k]['id'],'registration owner')
            expected=[{'object_id':ids[v],'class':self.external_classes['observed'][v]} for v in row['values']] if self.nonempty else row['values']
            need(exact(expected,t['registrations'][k]['values']),'full registration membership/identity')
        if self.nonempty:self.external_compare(h['external_state'],t['external_state'],'jdi')
        need(ids[h['queue']['id']]==t['registrations']['C']['id'] and exact(t['registrations']['C']['values'],[]),'queue owner/contents')
        if count:self.count['full_helper_jdi_boundaries']+=1
    def trace_phase(self):
        t=self.trace;self.identity={r['reference']:r['object_id'] for r in t['identity_ids']}
        self.methods['session_exception']=self.methods['session_check']
        need(0<len(t['identity_ids'])<=16384 and all(set(r)=={'reference','object_id'} and integer(r['reference'],1,16384) and integer(r['object_id'],1,(1<<63)-1) for r in t['identity_ids']),'complete typed identity tags')
        need(len(self.identity)==len(t['identity_ids'])==len(set(self.identity.values())),'injective helper/JDI identity tags')
        self.payload_anchors={};self.external_invalid=False;self.external_registered=False;self.external_root_count=0
        d=self.records['observed']
        for state in (d['initial'],d['final'],*(x[p] for x in d['operations'] for p in ('before','after'))):
            if self.nonempty:self.external_graph('observed',state)
            for chunk in state['chunks']:
                identity=self.identity[chunk['id']]
                if identity not in self.payload_anchors:self.payload_anchors[identity]=chunk
                else:
                    old=self.payload_anchors[identity]
                    need(exact(old['arrays'],chunk['arrays']) and old['slices_owner']==chunk['slices_owner'] and old['tiles']['id']==chunk['tiles']['id'] and [x['id'] for x in old['entities']]==[x['id'] for x in chunk['entities']],'retained load-only payload backing owners/bytes never replaced')
        for header in t['chunk_headers']:need(set(header)=={'id','class','world','coordinates','flags','minimum','last_save'},'header dictionary shape')
        self.count['indexed_headers']=len(t['chunk_headers']);self.count['identity_tag_pairs']=len(self.identity)
        for inventory in tuple(t['setup_inventories'])+tuple(v for x in t['operations'] for v in (x['before']['threads'],x['after']['threads'])):self.jdi_threads(inventory)
        for index,op in enumerate(t['operations']):
            helper=self.records['observed']['operations'][index];need(type(op['index']) is int and op['index']==index and helper['index']==index and op.get('action',helper['action'] if self.development else None)==helper['action'],'ordered action-bound trace operations')
            self.trace_state(op['before']['state'],helper['before']);self.trace_state(op['after']['state'],helper['after'])
            if self.nonempty:
                sequence=_DETACH_EVENTS['client' if self.context=='base' else self.context][index]
                need(' '.join(e['role']+':'+e['edge']+':'+str(e['code_index']) for e in op['events'] if not e['role'].startswith('pending_'))==sequence,'complete ordered finite original detach event delivery')
            if isinstance(helper['result'],Mapping):
                need(set(op['result'])=={'object_id','class'} and integer(op['result']['object_id'],1,(1<<63)-1) and op['result']['object_id']==self.identity[helper['result']['reference']] and op['result']['class'] in ((('mx' if self.client else 'ie'),) if helper['action']=='competing_handler_constructor' else (self.chunk_type,self.blank_type)),'actual typed helper/JDI object result')
            elif type(helper['result']) is bool and self.development and isinstance(op['result'],Mapping):
                need(set(op['result'])=={'object_id','class'} and op['result']['class']=='java.lang.Boolean','historical boxed Boolean marker shape');self.gap('Historical boxed result marker lacks direct primitive action binding.')
            else:need(exact(op['result'],helper['result']),'actual helper/JDI void result')
            fatal=helper['exception'] is not None;throws=[]
            active_write=None;active_region=None;pending_iterator=None;writes=[];metadata=0;top_entries=[];top_returns=[]
            low,high=helper['interval_ms'];need(integer(low,0,1<<63) and integer(high,low,low+180000),'bounded helper epoch interval')
            for ordinal,event in enumerate(op['events']):
                need(type(event['ordinal']) is int and event['ordinal']==ordinal and event['role'] in self.methods and type(event['thread_id']) is int and event['thread_id']==1,'exact event ordinal/role/thread')
                need(integer(event['clock_ms'],low,high),'event inside own helper interval')
                binding=self.methods[event['role']];need(all(event[k]==binding[k] for k in ('class','method','signature')),'event binding')
                code=self.class_(event['class']).methods[(event['method'],event['signature'])]['code']
                need(integer(event['code_index'],0,65535) and event['code_index'] in instruction_starts(code),'event actual typed instruction boundary')
                if event['edge']=='return':
                    need(code[event['code_index']] in range(0xac,0xb2),'return at actual return opcode')
                    need(event.get('exit_request_class_filter',event['class'] if self.development else None)==event['class'],'exact declaring-name MethodExit request filter')
                elif event['edge']=='throw':need(event['role']=='session_exception' and event['code_index'] in (51,65) and code[event['code_index']]==0xbf,'actual pinned session throw instruction');throws.append(event)
                else:need(event['edge']=='breakpoint' and event['code_index'] in self.breakpoints[event['role']],'actual watched breakpoint offset')
                need(not event['role'].startswith('forbidden_'),'no generation/population/finish call')
                self.event_binding(event)
                if all(event[k]==helper[k] for k in ('class','method','signature')) and event['receiver']==self.identity[helper['receiver']]:
                    if event['edge']=='return':top_returns.append(event)
                    elif event['code_index']==0:top_entries.append(event)
                self.sparse_state(event,op['events'],ordinal,helper) if event['state'].get('snapshot_kind')=='large101-omitted-graph-v1' else self.trace_state(event['state'])
                if 'competing_handler' in event['state']:need((event['state']['competing_handler'] is None)==(helper['before'].get('competing_handler') is None),'event competing-handler publication follows actual end marker')
                self.session_event(event,helper)
                if self.nonempty:self.external_event(event)
                need(0<len(event['original_frames'])<=128,'complete actual original caller frames')
                for frame in event['original_frames']:
                    need(set(frame)=={'class','method','signature','code_index','receiver'} and (frame['receiver'] is None or integer(frame['receiver'],1,(1<<63)-1)),'complete typed original frame')
                    need(frame['class'] in self.original_names,'actual original frame class')
                    method=self.class_(frame['class']).methods[(frame['method'],frame['signature'])]
                    need(integer(frame['code_index'],0,65535) and method['code'] is not None and frame['code_index'] in instruction_starts(method['code']),'original caller typed instruction')
                first=event['original_frames'][0]
                if event['class'] in self.original_names:need(all(exact(first[k],event[k]) for k in ('class','method','signature','code_index','receiver')),'current original event frame bound completely')
                if event['role'] in ('pending_next','pending_iterator_construct'):
                    need(first['class']==self.cache_type and first['method']=='a' and first['signature']=='()Z' and first['receiver']==self.identity[helper['before']['cache']] and first['code_index']==((39 if self.context=='dedicated' else 29) if event['role']=='pending_next' else (34 if self.context=='dedicated' else 24)),'exact original fresh-iterator caller binding')
                    if event['role']=='pending_iterator_construct':
                        need(pending_iterator is None and event['result'] is None and len(event['arguments'])==1 and event['arguments'][0]['object_id']==self.pending_map_id,'actual new pending iterator/backing-map constructor')
                        pending_iterator=event['receiver']
                    else:
                        need((pending_iterator==event['receiver'] or self.development and 'pending_iterator_construct' not in self.methods) and event['state']['pending']['values'] and exact(event['result'],event['state']['pending']['values'][0]),'actual fresh iterator returns first current pending key')
                        pending_iterator=None;self.count['actual_selected_pending_keys']+=1
                if event['role']=='write' and event['edge']=='breakpoint' and event['code_index']==0:
                    need(active_write is None,'nonrecursive write profile');chunk=event['arguments'][1]['object_id']
                    target=next(self.header(x['chunk'] if 'chunk' in x else x) for x in event['state']['chunks'] if self.header(x['chunk'] if 'chunk' in x else x)['id']==chunk)
                    active_write={'chunk_id':chunk,'coordinates':target['coordinates'],'start_ms':event['clock_ms']}
                if event['role']=='region_write' and event['edge']=='breakpoint' and event['code_index']==0:
                    need(active_write is not None and active_region is None,'region inside real write');active_region={'start_ms':event['clock_ms'],'args':event['arguments'],'receiver':event['receiver']}
                if event['role']=='timestamp' and event['edge']=='breakpoint' and event['code_index']==0:
                    need(active_region is not None and active_write is not None,'timestamp inside real region/write')
                    x,z,value=event['arguments'];need(exact([x,z],active_region['args'][:2]) and event['receiver']==active_region['receiver'],'timestamp receiver/local args')
                    need(any((second&0xffffffff)==(value&0xffffffff) for second in range(active_region['start_ms']//1000,event['clock_ms']//1000+1)),'setter value inside actual sampling interval')
                    need([x,z]==[v&31 for v in active_write['coordinates']],'timestamp cell from actual chunk')
                    active_write['timestamp']=value&0xffffffff;self.count['observed_timestamp_inputs']+=1
                if event['role']=='region_write' and event['edge']=='return':active_region=None
                if event['role']=='write' and event['edge']=='return':
                    need(active_write is not None and 'timestamp' in active_write,'complete actual write');writes.append(active_write);active_write=None
                if event['role']=='save_info' and event['edge']=='breakpoint' and event['code_index']==0:metadata+=1
                self.count['events']+=1
            if fatal:
                need(active_region is None and pending_iterator is None and not writes and metadata==0 and (active_write is None or helper['action']=='fatal_cache_save' and 'timestamp' not in active_write),'fatal session abort has no completed write/metadata/timestamp')
                need(len(throws)==2 and [x['code_index'] for x in throws]==[51,65] and exact(op['exception'],{'class':helper['exception']['class'],'object_id':self.identity[helper['exception']['id']],'events':2}) and all(x['result']['object_id']==op['exception']['object_id'] for x in throws),'complete same-object actual throw/rethrow and helper exception binding')
                self.count['exception_events']+=len(throws)
            else:need(active_write is None and active_region is None and pending_iterator is None and not throws and op.get('exception') is None,'balanced successful write/iterator scopes and exception absence')
            need(metadata in (0,1),'finite metadata saves per helper call')
            calls=helper.get('calls',({'arguments':helper['arguments'],'result':helper['result']},))
            need(len(top_entries)==len(calls) and (not top_returns if fatal else len(top_returns)>=len(calls)),'every actual top-level entry/return or abrupt throw bound to helper call')
            if len(top_returns)>len(calls):
                need(self.development,'ambiguous duplicate original return delivery');self.gap('Historical duplicate blank return delivery; diagnostic only.')
                need(helper['action']=='blank_eligibility' and len(top_returns)==2 and len(calls)==1 and exact(top_returns[0]['result'],top_returns[1]['result']),'only diagnosed historical identical duplicate delivery')
                top_returns=top_returns[:1];self.count['duplicate_blank_return_deliveries']+=1
            if self.nonempty:
                need(len(top_entries)==len(top_returns)==1,'one complete finite external API entry/return')
                self.match_jdi(helper['before'],top_entries[0]['state'],count=False);self.match_jdi(helper['after'],top_returns[0]['state'],count=False)
                self.count['external_top_event_endpoint_bindings']+=2
            for call,entry,exit_ in zip(calls,top_entries,(None,) if fatal else top_returns):
                if exit_ is not None:need(entry['ordinal']<exit_['ordinal'],'top-level call entry before return')
                need(len(entry['arguments'])==len(call['arguments']),'actual argument arity')
                for wanted,actual in zip(call['arguments'],entry['arguments']):
                    if isinstance(wanted,Mapping) and 'reference' in wanted:need(actual['object_id']==self.identity[wanted['reference']],'actual referenced argument')
                    elif isinstance(wanted,Mapping) and set(wanted)=={'long'}:need(exact(wanted['long'],actual),'actual long argument')
                    else:need(exact(wanted,actual),'actual primitive/null argument')
                if exit_ is None:continue
                returned=exit_['result']
                if helper['action']=='competing_handler_constructor':need(returned is None and call['result']['reference']==helper['receiver'],'actual void constructor exit and retained object result')
                elif isinstance(call['result'],Mapping):need(returned['object_id']==self.identity[call['result']['reference']],'actual returned chunk identity')
                else:need(exact(returned,call['result']),'actual primitive/void MethodExit return value')
                self.count['actual_top_level_return_bindings']+=1
            self.writes[index]=writes;self.metadata_saves[index]=metadata
        need(self.count['events']>0 and self.count['events']<=32768,'complete bounded retained events')
        unwatched=t['unwatched_original_returns'];need(isinstance(unwatched,Mapping) and len(unwatched)<=4096 and all(type(k) is str and 1<=len(k)<=2048 and integer(v,1,1024*1024) for k,v in unwatched.items()),'complete typed unwatched original-return accounting')
        need(integer(t['jdi_event_count'],self.count['events']+2*len(t['operations'])+len(t['identity_ids'])+sum(unwatched.values())+1,1024*1024),'raw JDI deliveries include every retained return/marker/identity event')
        exception_count=self.trace.get('exception_event_count',self.count['exception_events'] if self.development else None)
        need(type(exception_count) is int and exception_count==self.count['exception_events'],'complete recorded exception-event count')
        need(all(k in self.methods for k in ('forbidden_generate','forbidden_populate','forbidden_finish')),'complete no-generation binding inventory')
    def jdi_handler(self,actual,helper):
        need(set(actual)==set(helper) and integer(actual['id'],1,(1<<63)-1) and actual['id']==self.identity[helper['id']] and all(exact(actual[k],helper[k]) for k in helper if k!='id'),'complete same actual handler descriptor through identity correspondence')
    def session_event(self,event,helper):
        role=event['role'];edge=event['edge'];fatal=helper['exception'] is not None
        if self.development and 'session_handler' not in event:return
        need({'session_handler','exception_message','catch_location'}<=set(event),'complete actual session/catch observation fields')
        if role in ('handler_constructor','handler_base_constructor','handler_session_write'):
            need(helper['action']=='competing_handler_constructor' and event['receiver']==self.identity[helper['receiver']],'actual measured competing-handler constructor/writer owner')
            expected=self.competitors['observed']
            if role=='handler_session_write' or edge=='return':self.jdi_handler(event['session_handler'],expected)
            else:need(event['session_handler'] is None,'no uninitialized handler-field claim at constructor entry')
            if role in ('handler_constructor','handler_base_constructor'):
                need(len(event['arguments'])==3 and event['arguments'][0]['object_id']==self.identity[helper['arguments'][0]['reference']] and event['arguments'][0]['class']=='java.io.File' and exact(event['arguments'][1:],['world',False]),'actual derived/base constructor File/name/boolean binding')
        else:need(event['session_handler'] is None,'handler descriptor appears only at actual initialized handler methods')
        if edge!='throw':need(event['exception_message'] is None and event['catch_location'] is None,'no fabricated nonthrow exception/catch data');return
        need(fatal and role=='session_exception' and event['receiver']==self.identity[helper['before']['storage']] and event['result']['object_id']==self.identity[helper['exception']['id']] and event['result']['class']==helper['exception']['class'] and event['exception_message']==helper['exception']['message'],'actual original fatal object/message/handler binding')
        catch=event['catch_location'];need(set(catch)=={'class','method','signature','code_index','modifiers','bytecode_length','bytecode_sha256','original_loader','bootstrap_loader'} and type(catch['original_loader']) is bool and type(catch['bootstrap_loader']) is bool,'complete exact caught-location descriptor')
        if event['code_index']==51:
            need(catch['class']==('fm' if self.client else 'dq') and catch['method']=='b' and catch['signature']=='()V' and catch['code_index']==59 and catch['original_loader'] is True and catch['bootstrap_loader'] is False,'actual original finally catch location')
            body=self.class_(catch['class']).methods[(catch['method'],catch['signature'])]
            need(any(start<=51<end and handler==59 for start,end,handler,_ in body['handlers']),'actual original exception table protects throw PC')
        elif catch['bootstrap_loader'] is True:
            need(event['code_index']==65 and catch['class']=='jdk.internal.reflect.DirectMethodHandleAccessor' and catch['method']=='invoke' and catch['signature']=='(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;' and catch['code_index']==82 and catch['original_loader'] is False,'exact JDK21 reflection wrapping catch for original session rethrow')
            # The selected, before/after-fingerprinted runtime archive supplies
            # the actual Throwable handler around invokeImpl at PC23. This is
            # a single measured wrapping bridge, not a generic bootstrap catch.
            cls=ClassFile(self.boot.read('classes/jdk/internal/reflect/DirectMethodHandleAccessor.class'));body=cls.methods[(catch['method'],catch['signature'])]
            need(cls.name=='jdk/internal/reflect/DirectMethodHandleAccessor' and cls.major==65 and cls.minor==0 and body['modifiers']==1 and len(body['code'])==92 and hashlib.sha256(body['code']).hexdigest()=='0bbe29e90588c812db8ee77cccd86786921cb5ae826a3cdcb293da1d6e4ca39d','selected JDK21 reflection bridge method fingerprint')
            need(body['handlers']==[(20,26,27,'java.lang.ClassCastException'),(20,26,27,'java.lang.invoke.WrongMethodTypeException'),(20,26,55,'java.lang.NullPointerException'),(20,26,82,'java.lang.Throwable')],'exact selected runtime wrapping handler table protects invokeImpl call')
        else:
            need(event['code_index']==65 and catch['class']=='ChunkLifecycleObservation$Observer' and catch['method']=='invoke' and catch['signature']=='(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;' and catch['original_loader'] is False and catch['bootstrap_loader'] is False,'actual project-owned fatal recorder catch location')
            cls=ClassFile((self.capture/'classes/ChunkLifecycleObservation$Observer.class').read_bytes());body=cls.methods[(catch['method'],catch['signature'])]
            need(any(handler==catch['code_index'] and type_=='java.lang.RuntimeException' for _,_,handler,type_ in body['handlers']),'actual retained helper RuntimeException catch table')
        need(integer(catch['code_index'],0,65535) and catch['code_index'] in instruction_starts(body['code']) and type(catch['modifiers']) is int and catch['modifiers']==body['modifiers'] and type(catch['bytecode_length']) is int and catch['bytecode_length']==len(body['code']) and catch['bytecode_sha256']==hashlib.sha256(body['code']).hexdigest(),'exact actual catch method body/PC/modifier fingerprint')
    def event_binding(self,event):
        base=self.records['observed']['initial'];role=event['role'];receiver=event['receiver']
        need(receiver is None or integer(receiver,1,(1<<63)-1),'typed actual event receiver')
        groups={
            'world':{'world_time','world_time_get','world_save','world_metadata','world_session','register_tiles','register_entities','defer_entities','client_incremental'},
            'cache':{'acquire','save_pass','extra_wrapper','main_wrapper','save_enabled','maintenance','drop'},
            'loader':{'load','write','loader_extra','loader_maintenance','loader_flush'},
            'storage':{'save_info_wrapper','save_info','session_check','session_exception'},
            'generator':{'provider_maintenance'}}
        for owner,roles in groups.items():
            if role in roles:need(receiver==self.identity[base[owner]],'actual original '+role+' owner')
        if self.nonempty and role.startswith('external_'):
            roots=self.records['observed']['operations'][1]['after']['external_state']['objects'];sign,boat=roots
            need(receiver==self.identity[sign['id']] if role=='external_tile_invalidate' else receiver==self.identity[base['world']],'actual external callback receiver')
            need(len(event['original_frames'])>=2,'actual external callback caller');caller=event['original_frames'][1]
            if role=='external_tile_invalidate':wanted=(self.chunk_type,'f' if self.client else 'e','()V',40,self.identity[self.records['observed']['operations'][1]['result']['reference']])
            elif role=='external_entity_register' and self.context=='dedicated':wanted=('dp','c','(Llq;)V',2,self.identity[base['world']])
            else:wanted=(self.world_type,'a','(Ljava/util/List;)V',34,self.identity[base['world']])
            need(exact(tuple(caller[k] for k in ('class','method','signature','code_index','receiver')),wanted),'actual external callback source call/PC/receiver')
        if role=='serialize_chunk':need(receiver is None,'actual static schema receiver')
        elif role in {'eligibility','blank_eligibility','activate','detach','dirty','initialize_chunk','membership_add','membership_remove','membership_remove_slice'}:
            need(receiver in self.payload_anchors,'actual chunk receiver belongs to complete retained graph')
        signature=event['signature'];parameters=signature[1:signature.index(')')];types=re.findall(r'\[*L[^;]+;|\[*[ZBCSIJFD]',parameters)
        need(''.join(types)==parameters and len(types)==len(event['arguments']),'exact original argument descriptor arity')
        for descriptor,value in zip(types,event['arguments']):
            if descriptor=='Z':need(type(value) is bool,'actual Boolean argument')
            elif descriptor in ('B','C','S','I'):need(integer(value),'actual int-family argument')
            elif descriptor=='J':h64(value)
            elif descriptor in ('D','F'):need(type(value) is str and re.fullmatch('[0-9a-f]{'+('16' if descriptor=='D' else '8')+'}',value),'actual floating bit argument')
            else:need(value is None or descriptor=='Ljava/lang/String;' and type(value) is str or isinstance(value,Mapping) and set(value)=={'object_id','class'} and integer(value['object_id'],1,(1<<63)-1) and type(value['class']) is str,'actual reference argument')
        args=event['arguments']
        if self.nonempty and role in ('external_entity_register','external_entity_registry'):need(exact(args,[{'object_id':self.identity[boat['id']],'class':boat['class']}]),'actual loaded Boat registration argument')
        if role in ('load','write','loader_extra'):need(args[0]['object_id']==self.identity[base['world']],'actual loader world argument')
        if role in ('save_info','save_info_wrapper'):need(args[0]['object_id']==self.identity[base['info']] and args[1]['object_id']==self.identity[base['registrations']['d']['id']],'actual save Info and empty player-list owners')
        if role=='serialize_chunk':need(args[0]['object_id'] in self.payload_anchors and args[1]['object_id']==self.identity[base['world']] and args[2]['class']==('nu' if self.client else 'iq'),'actual serializer chunk/world/NBT arguments')
        if event['edge']=='throw':need(event['role']=='session_exception' and event.get('exit_request_class_filter') is None and set(event['result'])=={'object_id','class'} and integer(event['result']['object_id'],1,(1<<63)-1),'actual thrown exception object')
        elif event['edge']=='breakpoint':need(event['result'] is None and event.get('exit_request_class_filter') is None,'breakpoint has no fabricated return/filter')
        else:
            result=event['result'];descriptor=signature[signature.index(')')+1:]
            if role=='pending_next':need(integer(result),'actual decoded Integer selected key')
            elif descriptor=='V':need(result is None,'actual void return')
            elif descriptor=='Z':need(type(result) is bool,'actual Boolean return')
            elif descriptor=='I':need(integer(result),'actual Integer return')
            elif descriptor=='J':h64(result)
            else:need(result is None or isinstance(result,Mapping) and set(result)=={'object_id','class'} and integer(result['object_id'],1,(1<<63)-1) and type(result['class']) is str,'actual object return')
            if role in ('acquire','load') and result is not None:need(result['object_id'] in self.payload_anchors and result['class']==self.payload_anchors[result['object_id']]['class'],'actual returned chunk type/identity')
    def sparse_state(self,event,events,ordinal,helper):
        need(self.scenario=='unload101' and self.trace['event_snapshot_policy']=='large101-checkpoints-v1','graph omission only in explicit dedicated101 profile')
        state=event['state'];need(set(state)=={'snapshot_kind','reason','proof','target_header'},'complete explicit omitted-graph record')
        role,edge,pc=event['role'],event['edge'],event['code_index'];entry=edge=='breakpoint';exit_=edge=='return'
        expected=None
        if role=='world_time_get' and (entry and pc==0 or exit_ and pc==7):expected='world-time-getter'
        elif role in ('register_tiles','register_entities','defer_entities') and (entry and pc==0 or exit_ and pc=={'register_tiles':32,'register_entities':43,'defer_entities':11}[role]):expected='empty-world-collection-call'
        elif role=='initialize_chunk' and pc==0 and (entry or exit_):expected='one-byte-no-op-initializer'
        elif role=='detach' and entry and pc==67:expected='empty-detach-slice-loop'
        elif entry and pc=={'activate':51,'main_wrapper':41,'extra_wrapper':30,'load':277,'write':105,'region_write':444,'drop':89}.get(role,-1):expected='pre-return-program-counter'
        need(expected is not None and state['reason']==expected,'exact permitted graph-omission role/edge/PC')
        proof=state['proof'];need(isinstance(proof,Mapping) and proof.get('body_sha256')==self.methods[role]['bytecode_sha256'],'omission bound to actual pinned method body')
        target=None
        if event['class'] in (self.chunk_type,self.blank_type):target=event['receiver']
        else:
            for arg in event['arguments']:
                if isinstance(arg,Mapping) and arg.get('class') in (self.chunk_type,self.blank_type):target=arg['object_id'];break
        if target is None and len(event['original_frames'])>=2 and event['original_frames'][1]['class']==self.chunk_type:target=event['original_frames'][1]['receiver']
        h=self.header(state['target_header']) if state['target_header'] is not None else None
        need((h is None and target is None) or h is not None and h['id']==target and target in self.payload_anchors,'fresh omission target bound to actual receiver/argument/caller')
        if h is not None:
            need(set(h)=={'id','class','world','coordinates','flags','minimum','last_save'} and h['world']==self.identity[helper['before']['world']] and h['class'] in (self.chunk_type,self.blank_type) and len(h['coordinates'])==2 and all(integer(v) for v in h['coordinates']) and integer(h['minimum']) and set(h['flags'])==set('cnopq') and all(type(v) is bool for v in h['flags'].values()),'complete freshly read omission target header');h64(h['last_save'])
        if expected=='world-time-getter':
            need(set(proof)=={'body_sha256','world_id','info_id','time'} and integer(proof['world_id'],1,(1<<63)-1) and integer(proof['info_id'],1,(1<<63)-1) and proof['world_id']==self.identity[helper['before']['world']] and proof['info_id']==self.identity[helper['before']['info']] and proof['time']=='0000000000001770','fresh getter world/info/time proof')
            if exit_:need(exact(event['result'],proof['time']),'getter actual primitive return equals fresh field')
        elif expected=='empty-world-collection-call':
            need(set(proof)=={'body_sha256','collection'} and h is not None and len(event['arguments'])==1 and proof['collection']['id']==event['arguments'][0]['object_id'],'empty proof bound to actual world call argument')
            self.empty_collection(proof['collection'],self.payload_anchors[target])
        elif expected=='one-byte-no-op-initializer':
            need(set(proof)=={'body_sha256','chunk_state'} and h is not None,'complete no-op target proof')
            chunk=self.trace.chunk(proof['chunk_state']);need(exact(self.header(chunk['chunk']),h),'full no-op payload matches fresh header');self.trace_payload(chunk,h)
        elif expected=='empty-detach-slice-loop':
            need(set(proof)=={'body_sha256','slices_owner','slices'} and h is not None and len(proof['slices'])==8,'complete eight-slice detach proof')
            anchor=self.payload_anchors[target];need(integer(proof['slices_owner'],1,(1<<63)-1) and proof['slices_owner']==self.identity[anchor['slices_owner']],'actual detach slices-array owner')
            for row,slice_ in zip(proof['slices'],anchor['entities']):
                need(row['id']==self.identity[slice_['id']] and row['class']=='java.util.ArrayList','ordered actual detach slice identity');self.empty_collection(row,anchor)
        else:need(set(proof)=={'body_sha256'},'pre-return omission contains only exact body proof')
        if entry and expected in ('world-time-getter','empty-world-collection-call','one-byte-no-op-initializer','pre-return-program-counter'):
            need(ordinal+1<len(events),'omitted pure step retains actual return');next_=events[ordinal+1]
            need(next_['edge']=='return' and all(exact(next_[k],event[k]) for k in ('role','class','method','signature','receiver')),'omitted pure/pre-return step immediately followed by actual matching MethodExit')
            if expected=='pre-return-program-counter':need(next_['code_index']==pc,'pre-return checkpoint is actual same return instruction')
        self.count['explicit_graph_omissions']+=1
    def empty_collection(self,row,anchor):
        need(isinstance(row,Mapping) and integer(row.get('id'),1,(1<<63)-1) and type(row.get('size')) is int and row['size']==0,'fresh empty collection size/identity')
        if row.get('class')=='java.util.ArrayList':
            need(set(row)=={'id','class','size','backing_id','backing_length'} and row['id'] in {self.identity[x['id']] for x in anchor['entities']} and integer(row['backing_id'],1,(1<<63)-1) and integer(row['backing_length'],0,1024),'actual empty slice/backing proof')
        else:need(row.get('class')=='java.util.HashMap$Values' and set(row)=={'id','class','size','map_id'} and integer(row['map_id'],1,(1<<63)-1) and row['map_id']==self.identity[anchor['tiles']['id']],'actual empty tile-map values owner')
        if not hasattr(self,'collection_proofs'):self.collection_proofs={}
        need(row['id'] not in self.collection_proofs or exact(self.collection_proofs[row['id']],row),'unchanged empty collection backing owner')
        self.collection_proofs[row['id']]=row
    def jdi_threads(self,inventory):
        need(0<len(inventory)<=64 and len({x['id'] for x in inventory})==len(inventory),'complete finite JDI thread inventory')
        need(sum(not x['daemon'] for x in inventory)==1,'one non-daemon')
        for row in inventory:
            need(set(row)=={'id','name','daemon','status','frames'} and integer(row['id'],1,(1<<63)-1) and type(row['name']) is str and integer(row['status'],-1,5) and type(row['daemon']) is bool and len(row['frames'])<=128,'typed bounded thread')
            if not row['daemon']:need(row['name']=='main' and row['id']==1,'actual measured main')
            for frame in row['frames']:
                need(set(frame)=={'class','method','signature','code_index','original_loader'} and all(type(frame[k]) is str for k in ('class','method','signature')) and integer(frame['code_index'],-1,65535) and type(frame['original_loader']) is bool and frame['original_loader']==(frame['class'] in self.original_names),'whole-original-JAR thread classification')
            original=[x for x in row['frames'] if x['original_loader']]
            if original:
                need(self.context=='dedicated' and row['daemon'] and row['status']==2 and all(x['class']=='bt' and x['method']=='run' and x['signature']=='()V' and x['code_index'] in instruction_starts(self.class_('bt').methods[('run','()V')]['code']) for x in original),'only exact inert dedicated original daemon')
                need(any(x['class']=='java.lang.Thread' and x['method'].startswith('sleep') for x in row['frames']) and all(x['class']=='bt' or x['class']=='java.lang.Thread' and x['method'].startswith('sleep') for x in row['frames']),'whole dedicated daemon is sleep-only')
        need(sum(any(f['original_loader'] for f in row['frames']) for row in inventory)==(1 if self.context=='dedicated' else 0),'exact original daemon inventory cardinality')
        self.count['jdi_thread_inventories']+=1
    def helper_threads(self,inventory,context):
        need(0<len(inventory)<=64 and len({r['id'] for r in inventory})==len(inventory),'complete bounded helper thread inventory')
        original_count=0
        for row in inventory:
            need(set(row)=={'id','name','daemon','state','original_frame','frames'} and integer(row['id'],1,(1<<63)-1) and type(row['name']) is str and type(row['daemon']) is bool and type(row['original_frame']) is bool and row['state'] in ('NEW','RUNNABLE','BLOCKED','WAITING','TIMED_WAITING','TERMINATED') and len(row['frames'])<=128 and all(type(f) is str and len(f)<=4096 for f in row['frames']),'complete typed helper thread row')
            frames=[f for f in row['frames'] if any(f.startswith(name+'.') for name in self.original_names)]
            need(row['original_frame']==bool(frames),'whole-JAR helper thread frame classification')
            if frames:
                need(context=='dedicated' and row['daemon'] and all(f.startswith('bt.run(') or f.startswith('java.base@') and '/java.lang.Thread.sleep' in f or f.startswith('java.base/java.lang.Thread.sleep') or f.startswith('java.lang.Thread.sleep') for f in row['frames']),'exact inert dedicated sleep-only thread')
                original_count+=1
            if not row['daemon']:need(row['name']=='main','single measured main thread')
        need(sum(not r['daemon'] for r in inventory)==1 and original_count==(1 if context=='dedicated' else 0),'exact helper thread roles')
    def saved_info(self,parsed,live,interval):
        v=parsed['values'];expected={'RandomSeed':h64(live['seed']),'SpawnX':live['spawn_x'],'SpawnY':live['spawn_y'],'SpawnZ':live['spawn_z'],'Time':h64(live['time']),'SizeOnDisk':h64(live['size_on_disk']),'LevelName':mutf8(u16(live['level_name'])),'version':19132,'raining':int(live['raining']),'rainTime':live['rain_time'],'thundering':int(live['thundering']),'thunderTime':live['thunder_time']}
        need(all(exact(v[k],value) for k,value in expected.items()),'every serialized known Info field from pre-save live Info')
        need(interval[0]<=v['LastPlayed']<=interval[1],'LastPlayed inside its own save interval');self.count['saved_info_bindings']+=1
    def preparation(self):
        if self.nonempty:return self.preparation_detach()
        d=self.records['preparation'];ops=d['operations'];need(len(ops)==105,'complete preparation operation count')
        self.helper_state('preparation',d['initial'])
        for index,op in enumerate(ops):
            need(type(op['index']) is int and op['index']==index and op['exception'] is None and op['result'] is None,'complete preparation index/result')
            previous=d['initial'] if index==0 else ops[index-1]['after'];before=op['before']
            if 1<=index<=102:
                need(all(exact(previous[k],before[k]) for k in previous if k!='chunks') and len(before['chunks'])==len(previous['chunks'])+1 and exact(before['chunks'][:-1],previous['chunks']),'only explicitly constructed clone added between preparation calls')
                clone=before['chunks'][-1];template=next(c for c in d['initial']['chunks'] if not c['blank'])
                need(exact(clone['coordinates'],[16*(index-1),0]) and clone['class']==self.chunk_type and clone['world']==before['world'] and clone['minimum']==0 and clone['last_save']=='0000000000000000' and all(v is False for v in clone['flags'].values()),'exact static-decoder clone transients/coordinates')
                for name in ARRAYS:need(clone['arrays'][name]['array']==template['arrays'][name]['array'] and exact(clone['arrays'][name]['data'],template['arrays'][name]['data']),'actual original encoder/decoder backing-array alias')
                need(op['receiver']==before['loader'] and exact(op['arguments'],[{'reference':before['world']},{'reference':clone['id']}]) and (op['class'],op['method'],op['signature'])==(('ld','a','(Lfd;Llm;)V') if self.client else ('hb','a','(Ldj;Lhi;)V')),'exact original clone-write invocation')
            else:need(exact(before,previous),'complete preparation continuity')
            if index==0:
                need((op['class'],op['method'],op['signature'])==(self.world_type,'a','(J)V') and op['receiver']==before['world'] and exact(op['arguments'],[{'long':'00000000000003e8'}]),'exact preparation world-time invocation')
            elif index>=103:
                need((op['class'],op['method'],op['signature'])==(self.world_type,'a','(ZL'+('yb' if self.client else 'pj')+';)V') and op['receiver']==before['world'] and exact(op['arguments'],[True,None]),'exact preparation forced world-save invocation')
            low,high=op['interval_ms'];need(integer(low,0,(1<<63)-1) and integer(high,low,low+180000),'preparation sampled epoch interval')
            self.helper_state('preparation',op['before']);self.helper_state('preparation',op['after'])
        need(exact(d['final'],ops[-1]['after']),'complete preparation endpoint')
        need([x['action'] for x in ops]==['time']+['constructed_storage_clone_write']*102+['prepare_world_save_first','prepare_world_save_second'],'exact preparation schedule')
        template=next(c for c in d['initial']['chunks'] if not c['blank']);need(exact(template['coordinates'],[0,0]) and not template['flags']['n'],'genuine unpopulated origin template')
        files=self.blob_files('preparation',d['final']);seen=set()
        for name,raw in files.items():
            if not name.startswith('region/'):continue
            for record in self.region(name,raw)['records'].values():
                fields=record['fields'];coordinate=(fields['xPos'],fields['zPos']);need(coordinate not in seen,'unique prepared coordinates');seen.add(coordinate)
                need(fields['LastUpdate']==1000 and fields['TerrainPopulated']==0,'prepared clone time/population')
                for role,key in zip(ARRAYS,('Blocks','Data','SkyLight','BlockLight','HeightMap')):need(fields[key]==self.blobs['preparation'].get(template['arrays'][role]['data']),'every prepared clone array from actual origin')
                self.count['preparation_full_clone_bindings']+=1
        need(seen=={(i*16,0) for i in range(102)},'all102 fixed preparation slots')
        for ordinal,op in enumerate(ops[-2:]):
            before=self.blob_files('preparation',op['before']);after=self.blob_files('preparation',op['after'])
            self.saved_info(self.metadata_file(after['level.dat']),op['before']['metadata'],op['interval_ms'])
            if 'level.dat' in before:need(after['level.dat_old']==before['level.dat'],'preparation metadata exact rotation')
            for name in (n for n in before if n.startswith('region/')):
                if ordinal==0 and name=='region/r.0.0.mcr':
                    # The real constructor origin remains dirty after the clone writes.
                    # Its first forced save rewrites the same payload and samples its cell.
                    need(before[name][:4096]==after[name][:4096] and before[name][4100:]==after[name][4100:],'preparation origin overwrite changes only its timestamp cell')
                    stamp=struct.unpack_from('>I',after[name],4096)[0];need(op['interval_ms'][0]//1000<=stamp<=op['interval_ms'][1]//1000,'preparation origin timestamp clock')
                    old=next(x for x in op['before']['chunks'] if x['id']==template['id']);new=next(x for x in op['after']['chunks'] if x['id']==template['id'])
                    need(old['flags']['o'] is True and new['flags']['o'] is False and new['last_save']=='00000000000003e8','actual dirty constructor origin saved once')
                    self.count['preparation_written_timestamp_cells']+=1
                else:need(before[name]==after[name],'all other preparation region bytes preserved')
        need(exact({'world/'+k:digest(v) for k,v in files.items()},self.manifest['preparation']['files']),'complete preparation final source files')
    def preparation_detach(self):
        d=self.records['preparation'];ops=d['operations'];need(len(ops)==4 and [o['action'] for o in ops]==['time','constructed_external_storage_write','prepare_world_save_first','prepare_world_save_second'],'complete finite external preparation schedule')
        need(self.records['observed']['constructed_nbt']==self.records['control']['constructed_nbt']==(), 'only preparation constructs typed NBT')
        self.external_lists();self.helper_state('preparation',d['initial'])
        template=next(c for c in d['initial']['chunks'] if not c['blank']);setup=ops[0]['before'];target=setup['chunks'][-1]
        need(len(setup['chunks'])==len(d['initial']['chunks'])+1 and exact(setup['chunks'][:-1],d['initial']['chunks']) and all(exact(setup[k],d['initial'][k]) for k in setup if k not in ('chunks','external_state')),'only explicit static factory-owned body setup before first measured preparation call')
        need(exact(target['coordinates'],[16,0]) and target['class']==self.chunk_type and target['world']==setup['world'] and target['last_save']=='0000000000000000' and target['minimum']==0 and exact(target['flags'],{'c':False,'n':False,'o':False,'p':False,'q':True}),'all genuine static factory chunk transients')
        constructed=NBT(self.blobs['preparation'].get(d['constructed_nbt'][1]['raw']));f={x[1].decode():x[2] for x in constructed.root[2]}
        for name,field in zip(ARRAYS,('Blocks','Data','SkyLight','BlockLight','HeightMap')):need(self.blobs['preparation'].get(target['arrays'][name]['data'])==f[field],'entire factory chunk arrays from retained typed NBT')
        for i,op in enumerate(ops):
            need(type(op['index']) is int and op['index']==i and op['exception'] is None and op['result'] is None,'complete indexed external preparation result')
            before,after=op['before'],op['after'];need(i==0 or exact(before,ops[i-1]['after']),'full external preparation continuity')
            need(exact(before['external_state'],setup['external_state']) and exact(after['external_state'],setup['external_state']),'complete external preparation objects unchanged after actual factory setup')
            self.helper_state('preparation',before);self.helper_state('preparation',after)
            low,high=op['interval_ms'];need(integer(low,0,(1<<63)-1) and integer(high,low,low+180000),'bounded external preparation epoch')
            if i==0:binding=(self.world_type,'a','(J)V');args=[{'long':'00000000000003e8'}];owner=before['world']
            elif i==1:binding=('ld','a','(Lfd;Llm;)V') if self.client else ('hb','a','(Ldj;Lhi;)V');args=[{'reference':before['world']},{'reference':target['id']}];owner=before['loader']
            else:binding=(self.world_type,'a','(ZL'+('yb' if self.client else 'pj')+';)V');args=[True,None];owner=before['world']
            need(exact(tuple(op[k] for k in ('class','method','signature')),binding) and exact(op['arguments'],args) and op['receiver']==owner,'exact finite external preparation API/arguments/owner')
            old,new=self.blob_files('preparation',before),self.blob_files('preparation',after)
            need(old['session.lock']==new['session.lock'] and set(new)==set(old)|({'level.dat'} if i==2 else {'level.dat_old'} if i==3 else set()),'exact external preparation file creation/session')
            if i>=2:
                self.saved_info(self.metadata_file(new['level.dat']),before['metadata'],op['interval_ms'])
                if 'level.dat' in old:need(new['level.dat_old']==old['level.dat'],'exact external preparation metadata rotation')
            else:need(all(old[k]==new[k] for k in old if not k.startswith('region/')),'all nonregion setup bytes unchanged')
            for name in (n for n in new if n.startswith('region/')):
                raw,previous=new[name],old[name];parsed=self.region(name,raw);slot=16 if i==1 else 0 if i==2 else None
                if slot is None:need(raw==previous,'all region bytes unchanged outside actual preparation write');continue
                need(name=='region/r.0.0.mcr','single finite preparation region')
                record=parsed['records'][slot];need(low//1000<=record['stamp']<=high//1000,'external preparation actual write timestamp')
                chunk=next(c for c in after['chunks'] if c['id']==(target['id'] if i==1 else template['id']))
                self.disk_chunk_matches('preparation',record,chunk,1000)
                allowed=((slot*4,slot*4+4),(4096+slot*4,4100+slot*4),(record['offset'],record['end']));mask=bytearray(len(raw))
                for lo,hi in allowed:mask[lo:hi]=b'\x01'*(hi-lo)
                need(len(raw)>=len(previous) and all(mask[n] or v==(previous[n] if n<len(previous) else 0) for n,v in enumerate(raw)),'every unrelated header/payload/dead/padding byte preserved during external setup')
                self.count['preparation_written_timestamp_cells']+=1
            growth=sum(len(new[n])-len(old[n]) for n in new if n.startswith('region/'))
            if i==1:
                need(len(old['region/r.0.0.mcr'])==8192 and not any(old['region/r.0.0.mcr']),'new original region header awaiting first growth debit');growth+=8192
            need(h64(after['metadata']['size_on_disk'])==wrap64(h64(before['metadata']['size_on_disk'])+growth),'external preparation growth includes newly created header once')
            need(exact(next(c for c in before['chunks'] if c['id']==target['id']),next(c for c in after['chunks'] if c['id']==target['id'])),'unpublished factory chunk stays unchanged by direct write/world saves')
        final=self.blob_files('preparation',d['final']);need(exact(d['final'],ops[-1]['after']) and exact({'world/'+k:digest(v) for k,v in final.items()},self.manifest['preparation']['files']),'complete external preparation endpoint/retained files')
        records=self.region('region/r.0.0.mcr',final['region/r.0.0.mcr'])['records'];need(set(records)=={0,16},'exact origin and external baseline records')
        self.count['preparation_full_clone_bindings']+=1

    def schedule(self):
        expected=[]
        def add(action,args,receiver):expected.append((action,args,receiver))
        def clock(value):add('time',[{'long':format(value&((1<<64)-1),'016x')}],'world')
        def marks(n):
            for i in range(n):add('mark_dirty',[],('slot',i))
        if self.nonempty:
            clock(6000);add('acquire',[16,0],'cache')
            if self.context=='dedicated':add('drop_external',[16,0],'cache');add('unload_external',[],'cache')
            else:add('direct_detach_external',[],('slot',0))
        elif self.scenario=='session-world':
            clock(6000);add('acquire',[0,0],'cache');marks(1);clock(8000);add('competing_handler_constructor','constructor','competing');add('fatal_world_save',[True,None],'world')
        elif self.scenario=='unload101':
            clock(6000)
            for action in ('batch_acquire101','batch_drop101'):add(action,{'first':1,'last':101,'multiplier':16,'z':0},'cache')
            for action in ('unload100','unload_last'):add(action,[],'cache')
            add('acquire',[16,0],'cache');add('acquire',[1616,0],'cache')
        else:
            clock(1000)
            for i in range(24):add('acquire',[16*i,0],'cache')
            for value in (False,True):add('clean_eligibility',[value],('slot',0))
            for value in (False,True):add('blank_eligibility',[value],'fallback')
            marks(24);add('exact24_incremental',[False,None],'cache');add('acquire',[384,0],'cache');marks(25);clock(2000)
            for action,force in [('save25_cutoff',False),('save25_continuation',False),('clean_forced',True)]:add(action,[force,None],'cache')
            marks(1);clock(3000);add('world_forced',[True,None],'world');marks(25);clock(4000)
            add('world_incremental',[False,None],'world');add('world_continuation',[False,None],'cache')
            if self.client:
                marks(24);add('client_nonzero_pass',[1],'world');add('client_zero_pass',[0],'world')
            clock((1<<63)-301);marks(1);add('stamp_overflow_base',[False,None],'cache')
            add('api_owned_membership_add','entity',('slot',0));add('api_owned_membership_remove','entity',('slot',0))
            add('overflow_equal_forced',[True],('slot',0));add('overflow_equal_incremental',[False],('slot',0));clock(-(1<<63)+298)
            add('overflow_before_deadline',[False],('slot',0));clock(-(1<<63)+299);add('overflow_at_deadline',[False],('slot',0));add('forced_distinct',[True],('slot',0))
            clock(5000);add('refresh_empty_serializer_q',[True,None],'cache');add('empty_maintenance',[],'cache')
            if self.context=='dedicated':add('drop',[16,0],'cache');add('unload_one',[],'cache');add('acquire',[16,0],'cache')
            if 'competing_handler' in self.records['observed']['initial']:
                clock(8000);marks(1);add('competing_handler_constructor','constructor','competing');add('fatal_cache_save',[False,None],'cache')
        progress='yb' if self.client else 'pj';entity='sn' if self.client else 'lq'
        def binding(action):
            if action=='competing_handler_constructor':return ('mx' if self.client else 'ie'),'<init>','(Ljava/io/File;Ljava/lang/String;Z)V'
            if action=='time':return self.world_type,'a','(J)V'
            if action in ('acquire','batch_acquire101'):return self.cache_type,'c','(II)L'+self.chunk_type+';'
            if action in ('drop','batch_drop101','drop_external'):return self.cache_type,'d','(II)V'
            if action=='direct_detach_external':return self.chunk_type,('f' if self.client else 'e'),'()V'
            if action=='mark_dirty':return self.chunk_type,('g' if self.client else 'f'),'()V'
            if action=='blank_eligibility':return self.blank_type,'a','(Z)Z'
            if action in ('clean_eligibility','overflow_equal_forced','overflow_equal_incremental','overflow_before_deadline','overflow_at_deadline','forced_distinct'):return self.chunk_type,'a','(Z)Z'
            if action.startswith('api_owned_membership_'):return self.chunk_type,('a' if action.endswith('add') else 'b'),'(L'+entity+';)V'
            if action.startswith('client_'):return self.world_type,'a','(I)Z'
            if action in ('world_forced','world_incremental','fatal_world_save'):return self.world_type,'a','(ZL'+progress+';)V'
            if action in ('empty_maintenance','unload_one','unload100','unload_last','unload_external'):return self.cache_type,'a','()Z'
            return self.cache_type,'a','(ZL'+progress+';)Z'
        truths={'unload_external':False,'clean_eligibility':False,'blank_eligibility':False,'exact24_incremental':False,'save25_cutoff':False,'save25_continuation':True,'clean_forced':True,'world_continuation':True,'client_nonzero_pass':False,'client_zero_pass':True,'stamp_overflow_base':True,'overflow_equal_forced':False,'overflow_equal_incremental':True,'overflow_before_deadline':False,'overflow_at_deadline':True,'forced_distinct':True,'refresh_empty_serializer_q':True,'empty_maintenance':False,'unload_one':False,'unload100':False,'unload_last':False}
        for role in ('observed','control'):
            ops=self.records[role]['operations'];need(len(ops)==len(expected),'complete independent fixed schedule')
            boat=None
            for index,(op,(action,args,receiver)) in enumerate(zip(ops,expected)):
                need(type(op['index']) is int and op['index']==index and op['action']==action and (op['exception'] is None if not action.startswith('fatal_') else isinstance(op['exception'],Mapping)),'fixed independent indexed action schedule')
                need(exact(tuple(op[k] for k in ('class','method','signature')),binding(action)),'exact scheduled original API')
                if args=='constructor':
                    need(len(op['arguments'])==3 and set(op['arguments'][0])=={'reference','path'} and integer(op['arguments'][0]['reference'],1,16384) and Path(op['arguments'][0]['path'])==self.capture/(role+'-world') and exact(op['arguments'][1:],['world',False]),'actual constructor File/name/boolean arguments')
                elif args=='entity':
                    need(len(op['arguments'])==1 and set(op['arguments'][0])=={'reference'},'actual membership entity argument')
                    if boat is None:boat=op['arguments'][0]
                    else:need(exact(boat,op['arguments'][0]),'same retained add/remove entity')
                else:need(exact(op['arguments'],args),'fixed independent action arguments')
                wanted=op['after']['competing_handler']['id'] if receiver=='competing' else op['before']['insertions']['values'][receiver[1]] if isinstance(receiver,tuple) else op['before'][receiver]
                need(op['receiver']==wanted,'exact scheduled target identity')
                if action=='competing_handler_constructor':need(exact(op['result'],{'reference':wanted}),'actual constructed handler result')
                elif action in truths:need(op['result'] is truths[action],'actual fixed finite result '+action)
                elif action=='acquire':need(set(op['result'])=={'reference'} and integer(op['result']['reference'],1,16384),'actual retained acquisition result')
                else:need(op['result'] is None,'actual void/batch result')
                if action.startswith('fatal_'):
                    ex=op['exception'];need(set(ex)=={'id','class','message'} and integer(ex['id'],1,16384) and ex['class']==('us' if self.client else 'my') and ex['message']=='The save is being accessed from another location, aborting' and op['result'] is None,'exact observed fatal session exception')
                if action.startswith('batch_'):
                    need(len(op['calls'])==101,'all101 individual batch results')
                    for i,call in enumerate(op['calls']):
                        need(set(call)=={'index','arguments','result'} and type(call['index']) is int and call['index']==i and exact(call['arguments'],[16*(i+1),0]),'ordered actual batch call')
                        if action=='batch_acquire101':need(set(call['result'])=={'reference'} and integer(call['result']['reference'],1,16384),'actual individual batch acquisition')
                        else:need(call['result'] is None,'individual batch drop void return')
                self.count['fixed_schedule_operations']+=1
    def run_states(self,role):
        d=self.records[role];states=[d['initial']]
        initial_files=self.blob_files(role,d['initial'])
        for name,raw in initial_files.items():
            if name!='session.lock':need(exact(digest(raw),self.manifest[role+'_before']['world/'+name]),'initial snapshot complete immutable baseline except new handler lock')
        initial_info=self.metadata_file(initial_files['level.dat'])
        need(h64(d['initial']['metadata']['last_played'])==initial_info['values']['LastPlayed'],'loaded live LastPlayed from exact source file')
        self.saved_info(initial_info,d['initial']['metadata'],(initial_info['values']['LastPlayed'],)*2)
        for op in d['operations']:states.extend((op['before'],op['after']))
        states.append(d['final'])
        for state in states:self.helper_state(role,state)
        need(exact(d['initial'],d['operations'][0]['before']) and exact(d['final'],d['operations'][-1]['after']),'complete endpoints')
        for index,op in enumerate(d['operations']):
            need(op['index']==index and (index==0 or exact(op['before'],d['operations'][index-1]['after'])),'complete state continuity')
            low,high=op['interval_ms'];need(integer(low,0,1<<63) and integer(high,low,low+180000),'bounded exact integer epoch interval')
            before,after=self.blob_files(role,op['before']),self.blob_files(role,op['after'])
            need(before.keys()==after.keys() and (before['session.lock']==after['session.lock'] or op['action']=='competing_handler_constructor'),'full file paths/stable session')
            if self.metadata_saves[index]:
                self.saved_info(self.metadata_file(after['level.dat']),op['before']['metadata'],op['interval_ms'])
                need(after['level.dat_old']==before['level.dat'],'exact metadata rotation lineage')
                need(op['after']['metadata']['last_played']==op['before']['metadata']['last_played'],'loaded LastPlayed unchanged')
            else:need(all(before[k]==after[k] for k in ('level.dat','level.dat_old')),'metadata unchanged outside save')
            writes=self.writes[index];allowed=collections.defaultdict(dict)
            for write in writes:
                x,z=write['coordinates'];name=f'region/r.{x//32}.{z//32}.mcr';slot=(x&31)+32*(z&31);need(slot not in allowed[name],'one write per cell in this finite helper operation');allowed[name][slot]=write
            for name in (x for x in before if x.startswith('region/')):
                old,new=before[name],after[name]
                if name not in allowed:need(old==new,'complete unrelated region file preserved');continue
                parsed=self.region(name,new);permitted=[]
                for slot,write in allowed[name].items():
                    record=parsed['records'][slot];permitted.extend(((slot*4,slot*4+4),(4096+slot*4,4100+slot*4),(record['offset'],record['end'])))
                    if role=='observed':need(record['stamp']==write['timestamp'],'exact actual setter timestamp cell')
                    else:need(low//1000<=record['stamp']<=high//1000,'control timestamp inside enclosing write call')
                    candidates=[x for x in op['after']['chunks'] if exact(x['coordinates'],write['coordinates']) and not x['blank']]
                    need(len(candidates)==1,'unique current loaded identity per written clone slot');self.disk_chunk_matches(role,record,candidates[0],h64(op['after']['metadata']['time']))
                    self.count['written_timestamp_cells']+=1
                mark=bytearray(len(new))
                for start,end in permitted:mark[start:end]=b'\x01'*(end-start)
                need(all(mark[i] or value==(old[i] if i<len(old) else 0) for i,value in enumerate(new)),'all unwritten region/padding/dead bytes preserved')
            growth=sum(len(after[n])-len(before[n]) for n in after if n.startswith('region/'))
            need(h64(op['after']['metadata']['size_on_disk'])==wrap64(h64(op['before']['metadata']['size_on_disk'])+growth),'exact live physical growth accounting')
            calls=op['calls'] if op['action']=='batch_acquire101' else (op,) if op['action']=='acquire' else ()
            for call in calls:
                x,z=call['arguments'];record=self.region(f'region/r.{x//32}.{z//32}.mcr',before[f'region/r.{x//32}.{z//32}.mcr'])['records'][(x&31)+32*(z&31)]
                returned=next(x for x in op['after']['chunks'] if x['id']==call['result']['reference']);self.disk_chunk_matches(role,record,returned,None,loaded=True)
                need(returned['last_save']==op['after']['metadata']['time'] and returned['flags']=={'c':True,'n':False,'o':False,'p':False,'q':self.nonempty} and returned['minimum']==0,'actual clean loaded transients and callback-time stamp')
                self.count['actual_loaded_chunks']+=1
        physical=self.manifest['retained_worlds'][role+'-world'];need(exact({'world/'+k:digest(v) for k,v in self.blob_files(role,d['final']).items()},physical),'final full snapshot binds actual post-close world bytes')
        need(exact(d['provider_state_initial'],d['provider_state_final']) and d['provider_state_unchanged'] is True,'full provider unchanged within each run')
    def disk_chunk_matches(self,role,record,chunk,world_time,loaded=False):
        f=record['fields'];need(exact([f['xPos'],f['zPos']],chunk['coordinates']) and f['TerrainPopulated']==int(chunk['flags']['n']),'full disk scalar payload')
        if world_time is not None:need(f['LastUpdate']==world_time,'exact serialized world LastUpdate')
        for name,field in zip(ARRAYS,('Blocks','Data','SkyLight','BlockLight','HeightMap')):
            expected=f[field]
            if loaded and name=='blocks' and 'block_map' in self.records[role]:expected=expected.translate(self.blobs[role].get(self.records[role]['block_map']))
            need(expected==self.blobs[role].get(chunk['arrays'][name]['data']),'all five disk/retained arrays, with one actual load normalization')
        self.count['disk_full_chunk_bindings']+=1
    def pair(self):
        a,b=self.records['observed'],self.records['control'];self.pair_ids={};self.pair_back={}
        for key in ('schema','scope','context','mode','seed_hex','jar_sha256','class_origins','bindings','fields','construction','limitations'):need(exact(a[key],b[key]),'exact paired record '+key)
        pairs=[(a['initial'],b['initial'])]+[(x[phase],y[phase]) for x,y in zip(a['operations'],b['operations']) for phase in ('before','after')]+[(a['final'],b['final'])]
        for x,y in pairs:self.pair_state(x,y)
        for x,y in zip(a['operations'],b['operations']):
            for key in ('index','action','class','method','signature'):need(exact(x[key],y[key]),'paired operation '+key)
            need(self.pair_value(x['exception'],y['exception']),'paired full actual exception')
            self.link(x['receiver'],y['receiver'])
            if x['action']=='competing_handler_constructor':self.link(x['arguments'][0]['reference'],y['arguments'][0]['reference']);need(exact(x['arguments'][1:],y['arguments'][1:]),'paired exact constructor name/boolean arguments')
            else:need(self.pair_value(x['arguments'],y['arguments']),'paired all arguments')
            need(self.pair_value(x['result'],y['result']),'paired actual result')
            if 'calls' in x or 'calls' in y:need(self.pair_value(x['calls'],y['calls']),'complete ordered paired batch calls/results')
        if 'construction_trace' in a:
            need(a['constructor_route']==b['constructor_route'] and len(a['construction_trace'])==len(b['construction_trace']),'paired genuine constructor route/count')
            for x,y in zip(a['construction_trace'],b['construction_trace']):
                for key in ('kind','class','method','signature'):need(exact(x[key],y[key]),'exact constructor binding correspondence')
                self.link(x['result']['reference'],y['result']['reference'])
                for av,bv in zip(x['arguments'],y['arguments']):
                    if isinstance(av,Mapping) and 'path' in av:self.link(av['reference'],bv['reference']) # Each exact owned path was proved separately.
                    else:need(self.pair_value(av,bv),'paired full constructor argument')
        self.provider_pair(a['provider_state_initial'],b['provider_state_initial'],'provider')
        for phase in ('threads_before','threads_after'):
            x={r['name']:{k:v for k,v in r.items() if k!='id'} for r in a[phase]};y={r['name']:{k:v for k,v in r.items() if k!='id'} for r in b[phase]}
            need(len(x)==len(a[phase])==len(y) and exact(x,y),'full thread roles/states/frames after ID correspondence and unordered inventory')
            need(sum(not r['daemon'] for r in x.values())==1 and sum(r['original_frame'] for r in x.values())==(1 if self.context=='dedicated' else 0),'exact original thread roles')
        self.count['pair_identity_correspondence']=len(self.pair_ids)
    def link(self,a,b):
        need(integer(a,1,16384) and integer(b,1,16384),'typed helper identity')
        need(self.pair_ids.get(a,b)==b and self.pair_back.get(b,a)==a,'global identity bijection');self.pair_ids[a]=b;self.pair_back[b]=a
    def pair_value(self,a,b):
        if type(a) is not type(b):return False
        if isinstance(a,Mapping):
            if a.keys()!=b.keys():return False
            for k in a:
                if k in ('id','reference','borrowed') and type(a[k]) is int:self.link(a[k],b[k])
                elif not self.pair_value(a[k],b[k]):return False
            return True
        if isinstance(a,(list,tuple)):return len(a)==len(b) and all(self.pair_value(x,y) for x,y in zip(a,b))
        return exact(a,b)
    def pair_state(self,a,b):
        need(a.keys()==b.keys(),'paired complete state shape')
        for role in ('world','cache','storage','info','provider','generator','loader','fallback'):self.link(a[role],b[role])
        for role in ('map','insertions','pending','queue'):self.link(a[role]['id'],b[role]['id'])
        need(len(a['map']['entries'])==len(b['map']['entries']),'paired complete map size')
        for (ka,va),(kb,vb) in zip(a['map']['entries'],b['map']['entries']):need(exact(ka,kb),'paired raw map traversal');self.link(va,vb)
        need(len(a['insertions']['values'])==len(b['insertions']['values']),'paired insertion count')
        for x,y in zip(a['insertions']['values'],b['insertions']['values']):self.link(x,y)
        for role in ('pending','queue'):need(exact(a[role]['values'],b[role]['values']),'ordered pending/queue')
        need(a['registrations'].keys()==b['registrations'].keys(),'complete registration keys')
        for k in a['registrations']:
            self.link(a['registrations'][k]['id'],b['registrations'][k]['id']);av,bv=a['registrations'][k]['values'],b['registrations'][k]['values'];need(len(av)==len(bv),'complete registration size')
            if self.nonempty:
                for x,y in zip(av,bv):self.link(x,y)
            else:need(exact(av,bv),'empty registration contents')
        for k in ('metadata','world_force_load','save_off','provider_force_load'):need(exact(a[k],b[k]),'all paired typed scalar state '+k)
        if 'handler' in a:
            self.link(a['handler']['id'],b['handler']['id']);need(a['handler']['class']==b['handler']['class'],'paired exact handler type')
        if a.get('competing_handler') is not None or b.get('competing_handler') is not None:
            need(a.get('competing_handler') is not None and b.get('competing_handler') is not None,'paired competing-handler presence');self.link(a['competing_handler']['id'],b['competing_handler']['id']);need(a['competing_handler']['class']==b['competing_handler']['class'],'paired competing-handler type')
        need(len(a['chunks'])==len(b['chunks']),'no skipped retained chunks')
        for x,y in zip(a['chunks'],b['chunks']):
            self.link(x['id'],y['id']);self.link(x['world'],y['world']);self.link(x['slices_owner'],y['slices_owner']);self.link(x['tiles']['id'],y['tiles']['id'])
            for k in ('class','coordinates','blank','flags','last_save','minimum'):need(exact(x[k],y[k]),'all paired chunk scalar '+k)
            self.match_tiles(x['tiles']['entries'],y['tiles']['entries'],'control') if self.nonempty else need(exact(x['tiles']['entries'],y['tiles']['entries']),'tile entries')
            for name in ARRAYS:
                ar,br=x['arrays'][name],y['arrays'][name];self.link(ar['owner'],br['owner']);self.link(ar['array'],br['array']);need(self.blobs['observed'].get(ar['data'])==self.blobs['control'].get(br['data']),'paired full array bytes')
            for ar,br in zip(x['entities'],y['entities']):self.link(ar['id'],br['id']);need(self.pair_value(ar['members'],br['members']),'paired represented entity/member state')
        if self.nonempty:self.external_compare(a['external_state'],b['external_state'],'control')
        fa,fb=self.blob_files('observed',a),self.blob_files('control',b);need(fa.keys()==fb.keys(),'all paired file paths')
        for name,raw in fa.items():
            if name=='session.lock':continue # Each complete token/path/constructor bracket was validated independently.
            if name in ('level.dat','level.dat_old'):
                x,y=self.metadata_file(raw),self.metadata_file(fb[name]);need(x['normal']==y['normal'] and x['gzip_header']==y['gzip_header'],'all metadata NBT bytes except proved LastPlayed payload')
            elif name.startswith('region/'):
                need(raw[:4096]==fb[name][:4096] and raw[8192:]==fb[name][8192:],'whole paired region bytes except individually validated timestamp cells')
                # Each run's transitions above checked untouched cells and each changed cell's clock.
            else:need(raw==fb[name],'unrelated paired file bytes')
        self.count['paired_full_states']+=1
    def provider_pair(self,a,b,path):
        need(type(a) is type(b),'provider exact shape')
        if isinstance(a,Mapping):
            need(a.keys()==b.keys(),'provider fields')
            for key in a:
                if key in ('id','borrowed','reference'):self.link(a[key],b[key])
                elif key=='serialized':
                    x,y=self.blobs['observed'].get(a[key]),self.blobs['control'].get(b[key]);self.random_format(x);self.random_format(y)
                    if x!=y:
                        need(path==('provider.fields.yf.u.fields.fv.b' if self.client else 'provider.fields.pk.u.fields.dy.b'),'only specifically declared unseeded cave Random differs')
                        self.count['permitted_unseeded_random_identity']=1
                        need(x[:-9]==y[:-9] and x[-1:]==y[-1:],'Random difference is only serialized seed long')
                else:self.provider_pair(a[key],b[key],path+'.'+key)
        elif isinstance(a,(list,tuple)):
            need(len(a)==len(b),'provider array sizes')
            for i,(x,y) in enumerate(zip(a,b)):self.provider_pair(x,y,path+'['+str(i)+']')
        else:need(exact(a,b),'complete numerical provider graph '+path)
    def random_format(self,raw):
        need(len(raw)==104,'retained Random serialization length');r=Reader(raw)
        need(r.u2()==0xaced and r.u2()==5 and r.u1()==0x73 and r.u1()==0x72,'Random stream/object/class framing')
        need(r.take(r.u2())==b'java.util.Random' and r.number('>q')==3905348978240129619 and r.u1()==3 and r.u2()==3,'Random exact class descriptor')
        for kind,name in ((ord('Z'),b'haveNextNextGaussian'),(ord('D'),b'nextNextGaussian'),(ord('J'),b'seed')):
            need(r.u1()==kind and r.take(r.u2())==name,'Random exact field descriptors')
        need(r.u1()==0x78 and r.u1()==0x70 and r.u1()==0 and r.take(8)==bytes(8),'Random empty annotations/super/cached Gaussian')
        need(r.number('>Q')<(1<<48) and r.u1()==0x78 and r.pos==len(raw),'Random seed/end framing');self.count['serialized_random_records']+=1
    def report(self):
        return {'schema':1,'scope':'bounded original-only lifecycle/untouched-control proof; pure validation without C/JVM/child execution','capture':str(self.capture),'manifest':file_digest(self.capture/'manifest.json') if (self.capture/'manifest.json').is_file() else None,'records':{name:file_digest(self.capture/(name+'.json'),MAX_RECORD) for name in ('preparation','observed','control','trace') if (self.capture/(name+'.json')).is_file() and (self.capture/(name+'.json')).stat().st_size<=MAX_RECORD},'context':getattr(self,'context',None),'scenario':getattr(self,'scenario',None),'status':'fail' if self.findings else 'incomplete-proof' if self.gaps else 'pass','sections':self.sections,'counts':dict(self.count),'findings':self.findings,'proof_gaps':self.gaps,'limits':['Only fixed seed23 cohort/session-world, dedicated101 load-only, or finite Boat/Sign detach profiles are covered.','Full helper API-boundary payloads and indexed event headers/affected-target payloads are checked; large101 explicitly proved pure/empty/pre-return events omit repeated graphs.','Cross-control Random exclusions are limited to the exact untouched cave seed and, in finite detach mode, unchanged actor Random seeds; complete same-VM serialized/JDI state is bound.','Session conflicts terminate API work on that original world; no recovery or general I/O-failure claim.','No claim of Java GC, C retirement, entity ticks, running server/gameplay or provider raw48 JDI sampling. Detach collection graphs retain logical entries/owners, not all JDK internals; Random AtomicLong owners are checked for JDI retention, without a helper owner tag.','Source/tool provenance is bounded by the retained manifest inventory; this validator does not independently bind a Git tree.']}

def mutf8(units):
    out=bytearray()
    for i in range(0,len(units),2):
        v=int.from_bytes(units[i:i+2],'big')
        if 1<=v<=127:out.append(v)
        elif v<=2047:out.extend((0xc0|(v>>6),0x80|(v&63)))
        else:out.extend((0xe0|(v>>12),0x80|((v>>6)&63),0x80|(v&63)))
    return bytes(out)


def validate_capture(directory, *, development=False):
    """Return a bounded proof summary; raise ValidationError on failure or gap.

    development=True is reserved for diagnosing historical captures. Its result
    carries explicit gaps and must never be used as accepted original evidence.
    No file, child process, JVM or C probe is created or modified.
    """
    try:
        need(type(development) is bool,'explicit diagnostic mode type')
        validator = _CaptureValidator(Path(directory).resolve(), development)
        validator.section('provenance', validator.provenance)
        if not validator.findings:
            validator.section('preparation', validator.preparation)
            validator.section('schedule', validator.schedule)
            validator.section('trace', validator.trace_phase)
            if validator.sections.get('trace') == 'pass':
                validator.section('observed', lambda: validator.run_states('observed'))
                validator.section('control', lambda: validator.run_states('control'))
                validator.section('pair', validator.pair)
                validator.section('tables', validator.tables)
        result = validator.report()
        if validator.findings or (validator.gaps and not development):raise ValidationError('; '.join(validator.findings + validator.gaps))
        return result
    except (OSError,KeyError,TypeError,ValueError,RecursionError,zipfile.BadZipFile) as error:
        if isinstance(error,ValidationError):raise
        raise ValidationError(type(error).__name__+': '+str(error)) from error
    finally:
        if 'validator' in locals():
            for key in ('jar','boot'):
                archive=getattr(validator,key,None)
                if archive is not None:archive.close()
