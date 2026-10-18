"""Validate finite original cache callback traces and derive explicit MCC1 seams.

The external loader, generation and population bodies are opaque effects. This
module checks the coordinator's observations and identity boundaries; it neither
decodes saved chunks nor reproduces terrain-feature implementations.
"""

from __future__ import annotations

import copy
import hashlib
import re


# Exact unmodified method bodies captured from the two pinned original JARs.
BINDINGS = {'client': {'acquire': {'class': 'ok',
                        'method': 'c',
                        'signature': '(II)Llm;',
                        'modifiers': 1,
                        'bytecode_length': 394,
                        'bytecode_sha256': '06e08e7b1b2956f8a581b21a56578b29d153e418e53d7ae7ed298d0d78ea7817'},
            'get': {'class': 'ok',
                    'method': 'b',
                    'signature': '(II)Llm;',
                    'modifiers': 1,
                    'bytecode_length': 34,
                    'bytecode_sha256': 'ef1ce4e47bf2aa234367946b6f54b1bf549dcbd54cb8b215e2182223b14f66ec'},
            'load_wrapper': {'class': 'ok',
                             'method': 'd',
                             'signature': '(II)Llm;',
                             'modifiers': 2,
                             'bytecode_length': 49,
                             'bytecode_sha256': '4a1d90b4ccc940750988b8362542149033a123c301ccfb3846d0c0f158e5f3ba'},
            'load': {'class': 'ld',
                     'method': 'a',
                     'signature': '(Lfd;II)Llm;',
                     'modifiers': 1,
                     'bytecode_length': 278,
                     'bytecode_sha256': '3349469872749baec1a3d455f296827315f1b81245397abef0fa54c3b521c38e'},
            'generate': {'class': 'yf',
                         'method': 'b',
                         'signature': '(II)Llm;',
                         'modifiers': 1,
                         'bytecode_length': 130,
                         'bytecode_sha256': 'b5de2724c479e108327aca1b3579c200de3d98c846fc45dbba251b187afdf882'},
            'provider_populate': {'class': 'yf',
                                  'method': 'a',
                                  'signature': '(Lcl;II)V',
                                  'modifiers': 1,
                                  'bytecode_length': 2907,
                                  'bytecode_sha256': 'f09523f9e3ed68d6e27dbd8e0a8f28431a23c4497d5d824be1b2bfa0652b28dc'},
            'finish': {'class': 'lm',
                       'method': 'c',
                       'signature': '()V',
                       'modifiers': 1,
                       'bytecode_length': 222,
                       'bytecode_sha256': 'b0af5c034ed6dd3e9df9b555c5ea6c50056124ceef3e22af8d3767cf470f24f8'},
            'activate': {'class': 'lm',
                         'method': 'e',
                         'signature': '()V',
                         'modifiers': 1,
                         'bytecode_length': 52,
                         'bytecode_sha256': '954ec8fdd3b2c285473b09bebe03f27f3d61572326c2443d1253219f2ec9d2da'},
            'population_wrapper': {'class': 'ok',
                                   'method': 'a',
                                   'signature': '(Lcl;II)V',
                                   'modifiers': 1,
                                   'bytecode_length': 47,
                                   'bytecode_sha256': '08a06561ab0512db6c5f462f60a1dc47a394e42e7f7e0880c07a8da19df16723'},
            'mark_dirty': {'class': 'lm',
                           'method': 'g',
                           'signature': '()V',
                           'modifiers': 1,
                           'bytecode_length': 6,
                           'bytecode_sha256': '5a6d2b391d05c47e1f7024c99b8cd11b4e2fe138bd9b6506bdbfe1a9521e6c67'},
            'time_api': {'class': 'fd',
                         'method': 'a',
                         'signature': '(J)V',
                         'modifiers': 1,
                         'bytecode_length': 9,
                         'bytecode_sha256': 'b3d812a16755874725c695d086993aba96cdf4cd5354be43667f09f94c29be4a'}},
 'server': {'acquire': {'class': 'jb',
                        'method': 'c',
                        'signature': '(II)Lhi;',
                        'modifiers': 1,
                        'bytecode_length': 394,
                        'bytecode_sha256': '521e6d404e5b1ed34ec56c2f4bfafbde125fde3e71efc280856542d3d9c3bac2'},
            'get': {'class': 'jb',
                    'method': 'b',
                    'signature': '(II)Lhi;',
                    'modifiers': 1,
                    'bytecode_length': 34,
                    'bytecode_sha256': '3867af5663d3f5e6b24c1f36d666de060eb14bba26efc20a9d547c6bbaa46d24'},
            'load_wrapper': {'class': 'jb',
                             'method': 'd',
                             'signature': '(II)Lhi;',
                             'modifiers': 2,
                             'bytecode_length': 49,
                             'bytecode_sha256': 'c97a96b3f7d09bd609eb2b43739bde99164c08fc4c1adaaf05570d0f224e7f3a'},
            'load': {'class': 'hb',
                     'method': 'a',
                     'signature': '(Ldj;II)Lhi;',
                     'modifiers': 1,
                     'bytecode_length': 278,
                     'bytecode_sha256': '04af2e7d7021541054131879e77e901e013ba8ff15f9ce2e56d916f82d9a186e'},
            'generate': {'class': 'pk',
                         'method': 'b',
                         'signature': '(II)Lhi;',
                         'modifiers': 1,
                         'bytecode_length': 130,
                         'bytecode_sha256': '97eed3df6ca2056a265b15cf5ed4f8eb9eca856242bd449dad51ca93f6f640af'},
            'provider_populate': {'class': 'pk',
                                  'method': 'a',
                                  'signature': '(Lbl;II)V',
                                  'modifiers': 1,
                                  'bytecode_length': 2907,
                                  'bytecode_sha256': '2002a2a4dead7af615b3a6db2fe9030654a244e688e40b8d0444e5b8b1be7a57'},
            'finish': {'class': 'hi',
                       'method': 'b',
                       'signature': '()V',
                       'modifiers': 1,
                       'bytecode_length': 222,
                       'bytecode_sha256': '581a542efcc76d4056af780ec9868aaa47ba949628aaee64c560c5f9a836063c'},
            'activate': {'class': 'hi',
                         'method': 'd',
                         'signature': '()V',
                         'modifiers': 1,
                         'bytecode_length': 52,
                         'bytecode_sha256': '4a07f07f0014ccd0b442a21b9b2fe1fddadda9cf293f44bd1465ec11bf4d3d22'},
            'population_wrapper': {'class': 'jb',
                                   'method': 'a',
                                   'signature': '(Lbl;II)V',
                                   'modifiers': 1,
                                   'bytecode_length': 47,
                                   'bytecode_sha256': '67902c9f35b2e07da7cddd5ff8afd2d55d071672ab5a403f83173bb5ce05d107'},
            'mark_dirty': {'class': 'hi',
                           'method': 'f',
                           'signature': '()V',
                           'modifiers': 1,
                           'bytecode_length': 6,
                           'bytecode_sha256': '109bb7eea1ad46dd309cdba4b89229aecc23025e3eec652a3261742388a691f7'},
            'time_api': {'class': 'dj',
                         'method': 'a',
                         'signature': '(J)V',
                         'modifiers': 1,
                         'bytecode_length': 9,
                         'bytecode_sha256': '290fe9fe23ef3b2fcac19e5bb34433e2fe0c3df2e1f2e52cc07e90a27b831682'}}}


def _integer(value, low, high, require, context):
    require(type(value) is int and low <= value <= high, context + ": integer bounds")
    return value


def _keys(value, names, require, context):
    require(type(value) is dict and set(value) == set(names), context + ": complete keys")


def _inventory(value, operation, marker, loader, require, equal):
    _keys(value, ("marker", "index", "suspend_policy", "measured_thread_id", "original_loader_id",
        "thread_limit", "frames_per_thread_limit", "daemon_layout", "thread_count", "measured_main_thread",
        "all_threads_suspended", "all_other_threads_daemon", "no_other_original_frames", "threads"), require, "inventory")
    require(value["marker"] == marker and equal(value["index"], operation["index"]) and
        equal(value["suspend_policy"], 2) and equal(value["measured_thread_id"], operation["thread_id"]) and
        equal(value["original_loader_id"], loader) and equal(value["thread_limit"], 64) and
        equal(value["frames_per_thread_limit"], 64) and value["daemon_layout"] in
        ("thread-daemon", "thread-holder-daemon"), "strict SUSPEND_ALL base inventory identity")
    require(all(value[name] is True for name in ("measured_main_thread", "all_threads_suspended",
        "all_other_threads_daemon", "no_other_original_frames")), "strict base thread policy")
    require(type(value["threads"]) is list and 1 <= len(value["threads"]) <= 64 and
        equal(value["thread_count"], len(value["threads"])), "complete bounded thread inventory")
    seen, mains = set(), 0
    for ordinal, thread in enumerate(value["threads"]):
        _keys(thread, ("ordinal", "id", "name", "object_class", "status", "daemon", "measured", "suspended",
            "frame_count", "original_frame", "frames"), require, "inventoried thread")
        number = _integer(thread["id"], 1, (1 << 63) - 1, require, "thread identity")
        require(number not in seen and equal(thread["ordinal"], ordinal) and thread["suspended"] is True and
            type(thread["daemon"]) is bool and type(thread["measured"]) is bool and
            type(thread["original_frame"]) is bool and type(thread["name"]) is str and
            type(thread["object_class"]) is str, "typed unique inventoried thread")
        seen.add(number)
        _integer(thread["status"], -1, 5, require, "thread status")
        require(type(thread["frames"]) is list and len(thread["frames"]) <= 64 and
            equal(thread["frame_count"], len(thread["frames"])), "complete bounded stack inventory")
        original = False
        for index, frame in enumerate(thread["frames"]):
            _keys(frame, ("ordinal", "class", "method", "signature", "code_index", "class_loader_id", "original_frame"),
                require, "inventoried frame")
            require(equal(frame["ordinal"], index) and type(frame["original_frame"]) is bool and
                all(type(frame[name]) is str and frame[name] for name in ("class", "method", "signature")),
                "typed inventoried frame")
            _integer(frame["code_index"], -1, (1 << 31) - 1, require, "stack code index")
            if frame["class_loader_id"] is not None:
                _integer(frame["class_loader_id"], 1, (1 << 63) - 1, require, "frame loader")
            require(frame["original_frame"] is (frame["class_loader_id"] == loader), "actual original frame classification")
            original |= frame["original_frame"]
        require(thread["original_frame"] is original, "complete original frame classification")
        if thread["measured"]:
            mains += 1
            require(equal(thread["id"], operation["thread_id"]) and thread["name"] == "main" and
                thread["object_class"] == "java.lang.Thread" and thread["daemon"] is False,
                "actual measured non-daemon main")
            require(thread["original_frame"] is False and len(thread["frames"]) == 2,
                "measurement marker is outside every original frame with exactly two helper frames")
            top = thread["frames"][0]
            require(top["class"] == "CacheCallbacksObservation" and top["method"] == marker and
                top["signature"] == ("(Ljava/lang/Object;Ljava/lang/Object;I)V" if marker == "begin" else "(Ljava/lang/Object;I)V") and
                equal(top["code_index"], 0) and top["original_frame"] is False,
                "exact project-owned measurement marker frame")
            caller = thread["frames"][1]
            require(caller["class"] == "CacheCallbacksObservation" and caller["method"] == "main" and
                caller["signature"] == "([Ljava/lang/String;)V" and caller["code_index"] > 0 and
                caller["original_frame"] is False and type(top["class_loader_id"]) is int and
                top["class_loader_id"] > 0 and top["class_loader_id"] != loader and
                equal(caller["class_loader_id"], top["class_loader_id"]),
                "marker and direct main caller use the same project loader")
        else:
            require(thread["daemon"] is True and original is False, "every other thread daemon without original frames")
    require(mains == 1, "one measured main thread")
    return next(thread["frames"][0]["class_loader_id"] for thread in value["threads"] if thread["measured"])


class _Validation:
    def __init__(self, value, trace, expected, ids, acquire, finish, public_proof):
        self.value, self.trace, self.expected, self.ids = value, trace, expected, ids
        self.acquire, self.finish = acquire, finish
        self.public_proof = public_proof
        self.require, self.equal = acquire.require, acquire.same_json
        self.side, self.scenario = value["side"], value["scenario"]
        self.client = self.side == "client"
        self.classes = ("fd", "ok", "lm", "yf", "ld", "cl", "li", "wi") if self.client else (
            "dj", "jb", "hi", "pk", "hb", "bl", "hg", "ob")
        self.world_class, self.cache_class, self.chunk_class, self.generator_class, self.loader_class, self.provider_class, self.blank_class, self.nibble_class = self.classes
        self.links = acquire.trace_links(trace, [value["initial"]] +
            [stage for row in value["operations"] for stage in (row["before"], row["after"])])
        for row in self.links.values():
            self.require(type(row["class"]) is str and row["class"] and
                (row["loader_id"] is None or type(row["loader_id"]) is int and row["loader_id"] > 0), "typed global object link")
            _integer(row["object_id"], 1, (1 << 63) - 1, self.require, "global JDI identity")
        self.by_jdi = {self.link(identity): number for identity, number in ids.items()}
        self.by_number = {number: identity for identity, number in ids.items()}
        self.require(set(self.by_number) == set(range(len(ids))), "contiguous final helper identities")
        first = value["initial"]["world"]
        self.context_ids = {"world_id": self.link(first["world"]), "cache_id": self.link(first["cache"]),
            "generator_id": self.link(first["generator"]), "loader_id": self.link(first["loader"])}
        for role, actual_class in (("world", self.world_class), ("cache", self.cache_class),
                ("generator", self.generator_class), ("loader", self.loader_class)):
            self.require(first[role]["class"] == actual_class, "exact original context object class: " + role)
        self.loader = self.links[acquire.ref(first["world"])]["loader_id"]
        _integer(self.loader, 1, (1 << 63) - 1, self.require, "original archive loader")
        self.owners = {"cache_id": self.context_ids["cache_id"]}
        for key, role in (("map_id", "map"), ("list_id", "insertion_list"), ("pending_id", "pending_set"), ("queue_id", "queue")):
            self.owners[key] = self.link(first[role]["object"])
        fallback = [chunk for chunk in first["chunks"] if acquire.ref(chunk["object"]) == first["fallback"]]
        self.require(len(fallback) == 1, "one reflected blank fallback")
        self.eq([item["class"] for item in fallback[0]["array_references"]],
            ["[B", self.nibble_class, "[B", self.nibble_class, "[B", self.nibble_class, "[B", "[B"],
            "exact reflected blank array and packed-wrapper classes")
        self.blank_array_ids = [self.link(item) for item in fallback[0]["array_references"]]
        self.require(len(set(self.blank_array_ids)) == 8, "eight independently tagged blank backing owners")
        self.project_loader = None
        self.local_owners, self.header_classes, self.known_boundary_owners = {}, {}, {}
        for raw in [value["initial"]] + [stage for row in value["operations"] for stage in (row["before"], row["after"])]:
            for local in raw["cached_locals"]:
                identity = self.link(local["object"])
                self.eq([item["class"] for item in local["array_references"]],
                    ["[B", self.nibble_class, "[B", self.nibble_class, "[B", self.nibble_class, "[B", "[B"],
                    "exact primitive-array and packed-wrapper classes")
                self.require(local["tile_map"]["object"]["class"] == "java.util.HashMap" and
                    all(item["object"]["class"] == "java.util.ArrayList" for item in local["entity_slices"]),
                    "actual ordinary tile map and entity-slice classes")
                chain = {"world_id": self.link(local["world"]), "array_ids": [self.link(item) for item in local["array_references"]],
                    "tile_map": {"id": self.link(local["tile_map"]["object"]), "size": local["tile_map"]["size"]},
                    "entity_slices": [{"id": self.link(item["object"]), "size": item["size"]} for item in local["entity_slices"]]}
                self.require(chain["world_id"] == self.context_ids["world_id"] and len(chain["array_ids"]) == 8 and
                    len(set(chain["array_ids"])) == 8 and len(chain["entity_slices"]) == 8 and
                    all(item["size"] == 0 and type(item["size"]) is int for item in chain["entity_slices"]) and
                    chain["tile_map"]["size"] == 0 and type(chain["tile_map"]["size"]) is int, "complete ordinary owner chain")
                if identity in self.local_owners:
                    self.eq(chain, self.local_owners[identity], "ordinary payload owners retained")
                self.local_owners[identity] = chain
        self.scripts = []

    def eq(self, left, right, context):
        self.require(self.equal(left, right), context)

    def link(self, value):
        name = value if type(value) is str else self.acquire.ref(value)
        self.require(name in self.links, "explicit helper/JDI owner link: " + str(name))
        row = self.links[name]
        if type(value) is dict:
            self.require(row["class"] == value["class"], "reflection/JDI class name equality")
        return row["object_id"]

    def header(self, value):
        _keys(value, ("id", "class", "coordinates", "flags", "time"), self.require, "complete JDI chunk header")
        _integer(value["id"], 1, (1 << 63) - 1, self.require, "chunk JDI identity")
        self.require(value["id"] in self.by_jdi, "known helper-linked chunk")
        number = self.by_jdi[value["id"]]
        self.require(value["class"] == (self.blank_class if number == 0 else self.chunk_class), "actual normal/blank dispatch class")
        self.eq(value["class"], self.links[self.by_number[number]]["class"], "header class bound to global JDI identity")
        self.require(type(value["coordinates"]) is list and len(value["coordinates"]) == 2, "complete chunk coordinates")
        for coordinate in value["coordinates"]:
            _integer(coordinate, -(1 << 31), (1 << 31) - 1, self.require, "chunk coordinate")
        self.acquire.flags(value["flags"])
        self.acquire.raw_hex(value["time"], 8, "chunk time")
        return {"id": number, "coordinates": value["coordinates"], "blank": number == 0,
            "flags": value["flags"], "time": value["time"]}

    def graph(self, cache):
        _keys(cache, (*self.owners, "entries", "insertions", "pending", "queue", "fallback", "world_force_load", "provider_force_load"), self.require, "complete traced cache")
        for name, identity in self.owners.items():
            self.eq(cache[name], identity, "actual retained cache collection owner")
        self.require(cache["world_force_load"] is False and cache["provider_force_load"] is None, "actual base force flags")
        self.require(all(type(cache[name]) is list for name in ("entries", "insertions", "pending", "queue")) and
            len(cache["entries"]) <= 64 and len(cache["insertions"]) <= 64 and len(cache["pending"]) <= 64 and
            len(cache["queue"]) <= 32768, "bounded complete traced collections")
        headers, mappings = {}, []
        def chunk(raw):
            selected = self.header(raw)
            if selected["id"] in headers:
                self.eq(selected, headers[selected["id"]], "same object header across map/list/fallback")
            headers[selected["id"]] = selected
            return selected["id"]
        for row in cache["entries"]:
            _keys(row, ("key", "value"), self.require, "map row")
            key = _integer(row["key"], -(1 << 31), (1 << 31) - 1, self.require, "map key")
            mappings.append([key, chunk(row["value"])])
        self.require(len({row[0] for row in mappings}) == len(mappings), "unique traced map keys")
        insertions = [chunk(raw) for raw in cache["insertions"]]
        self.require(chunk(cache["fallback"]) == 0, "actual fallback identity0")
        for number in cache["pending"]:
            _integer(number, -(1 << 31), (1 << 31) - 1, self.require, "pending key")
        self.require(len(set(cache["pending"])) == len(cache["pending"]), "unique pending membership")
        queue, region_ids = [], set()
        for row in cache["queue"]:
            _keys(row, ("id", "region"), self.require, "queued light object")
            identity = _integer(row["id"], 1, (1 << 63) - 1, self.require, "queued object identity")
            self.require(identity not in region_ids and type(row["region"]) is list and len(row["region"]) == 7, "complete distinct queued regions")
            region_ids.add(identity)
            _integer(row["region"][0], 0, 1, self.require, "light kind")
            for number in row["region"][1:]:
                _integer(number, -(1 << 31), (1 << 31) - 1, self.require, "light bound")
            self.require(all(row["region"][axis] <= row["region"][axis + 3] for axis in (1, 2, 3)), "ordered queued bounds")
            queue.append(row["region"])
        return {"map": sorted(mappings), "insertions": insertions, "pending": sorted(cache["pending"]), "queue": queue}, headers

    def boundary(self, raw):
        _keys(raw, ("cache", "chunks", "world_time", "depth", "drain_depth", "no_sky", "spawn", "world_force_load", "provider_force_load"), self.require, "complete external callback boundary")
        graph, headers = self.graph(raw["cache"])
        self.acquire.raw_hex(raw["world_time"], 8, "live callback time")
        _integer(raw["depth"], -(1 << 31), (1 << 31) - 1, self.require, "enqueue depth")
        self.eq(raw["drain_depth"], 0, "finite profile does not drain lighting")
        self.require(raw["no_sky"] is False and raw["world_force_load"] is False and raw["provider_force_load"] is None, "actual ordinary base world context")
        self.eq(raw["spawn"], [0, 0], "actual finite spawn")
        self.require(type(raw["chunks"]) is list and 1 <= len(raw["chunks"]) <= 32, "bounded complete callback chunk inventory")
        chunks, owners = {}, {}
        for row in raw["chunks"]:
            _keys(row, ("chunk", "blank", "heightmap", "minimum_height", "data", "world_id", "array_ids", "tile_map", "entity_slices"), self.require, "complete callback local")
            selected = self.header(row["chunk"])
            number = selected["id"]
            self.require(number not in chunks and row["blank"] is selected["blank"], "unique actual callback dispatch")
            self.acquire.descriptor(row["heightmap"], 256, "full callback heightmap")
            _integer(row["minimum_height"], -(1 << 31), (1 << 31) - 1, self.require, "local minimum height")
            if selected["blank"]:
                self.require(row["data"] is None and row["heightmap"]["values"] == "00" * 256 and row["minimum_height"] == 0, "unused blank backing state")
            else:
                self.acquire.local_data(row["data"])
                self.eq(row["data"]["heightmap"], row["heightmap"], "complete heightmap descriptor equality")
                self.eq(row["data"]["minimum_height"], row["minimum_height"], "complete minimum equality")
            selected["data"] = row["data"]
            chain = {name: row[name] for name in ("world_id", "array_ids", "tile_map", "entity_slices")}
            self.eq(chain["world_id"], self.context_ids["world_id"], "callback chunk belongs to same world")
            self.require(type(chain["array_ids"]) is list and len(chain["array_ids"]) == 8 and len(set(chain["array_ids"])) == 8 and
                all(type(item) is int and item > 0 for item in chain["array_ids"]), "eight distinct actual array owners")
            _keys(chain["tile_map"], ("id", "size"), self.require, "tile map owner")
            _integer(chain["tile_map"]["id"], 1, (1 << 63) - 1, self.require, "tile map identity")
            self.eq(chain["tile_map"]["size"], 0, "finite profile has no tile objects")
            self.require(type(chain["entity_slices"]) is list and len(chain["entity_slices"]) == 8, "eight entity slices")
            for item in chain["entity_slices"]:
                _keys(item, ("id", "size"), self.require, "entity slice owner")
                _integer(item["id"], 1, (1 << 63) - 1, self.require, "entity slice identity")
                self.eq(item["size"], 0, "finite profile has empty entity slices")
            if not selected["blank"]:
                self.eq(chain, self.local_owners[row["chunk"]["id"]], "unpublished/cached local owner chain matches final reflected owners")
            else:
                self.eq(chain["array_ids"], self.blank_array_ids, "blank backing owner IDs match eight actual reflected helper tags")
            if number in self.known_boundary_owners:
                self.eq(chain, self.known_boundary_owners[number], "all JDI owner chains retained including blank backing")
            self.known_boundary_owners[number] = copy.deepcopy(chain)
            chunks[number], owners[number] = selected, chain
        self.require(set(chunks) == set(range(len(chunks))), "contiguous growing boundary identity inventory")
        for number, header in headers.items():
            self.eq(header, {name: item for name, item in chunks[number].items() if name != "data"}, "callback map/list/fallback header matches full local")
        return {"context": {"dedicated": False, "world_time": raw["world_time"], "world_force": False,
            "provider_force": None, "spawn": [0, 0], "no_sky": False},
            "world": dict(graph, depth=raw["depth"], chunks=[chunks[number] for number in range(len(chunks))])}

    def provenance(self):
        trace = self.trace
        _keys(trace, ("schema", "scope", "side", "scenario", "seed_hex", "jar_sha256", "can_get_method_return_values",
            "can_get_bytecodes", "vm_name", "vm_version", "child_command", "debuggee_mode", "listen_address", "port",
            "setup_proof", "bindings", "time_proof", "object_references", "operations", "jdi_event_count", "child_exit",
            "child_stdout", "child_stderr", "elapsed_ms"), self.require, "complete callback trace")
        self.eq({name: trace[name] for name in ("schema", "scope", "side", "scenario", "seed_hex", "jar_sha256")},
            {name: self.value[name] for name in ("schema", "scope", "side", "scenario", "seed_hex", "jar_sha256")}, "actual original trace provenance")
        self.require(trace["can_get_method_return_values"] is True and trace["can_get_bytecodes"] is True and
            trace["debuggee_mode"] == "interpreted" and trace["listen_address"] == "127.0.0.1" and
            all(type(trace[name]) is str and trace[name] for name in ("vm_name", "vm_version")), "read-only loopback JDI capabilities")
        self.eq(trace["child_exit"], 0, "successful original child")
        _integer(trace["port"], 1, 65535, self.require, "ephemeral port")
        _integer(trace["elapsed_ms"], 1, 90000, self.require, "bounded controller runtime")
        _integer(trace["jdi_event_count"], 1, 65536, self.require, "bounded total JDI delivery")
        command = trace["child_command"]
        self.require(type(command) is list and len(command) == 13 and all(type(item) is str and item for item in command), "complete bounded original child command")
        self.eq(command[1:5], ["-Xint", "-Xmx512m", "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:0", "-cp"], "exact read-only debuggee options")
        self.eq(command[6], "CacheCallbacksObservation", "actual project observer main")
        self.eq(command[8:11], [self.side, self.scenario, self.value["seed_hex"]], "actual observer profile arguments")
        for name in ("child_stdout", "child_stderr"):
            _keys(trace[name], ("file", "bytes", "sha256"), self.require, "complete child stream fingerprint")
            self.require(type(trace[name]["file"]) is str and re.fullmatch(r"[^/\\:]+", trace[name]["file"]), "relative child stream name")
            _integer(trace[name]["bytes"], 0, 32 * 1024 * 1024, self.require, "child stream bound")
            self.acquire.raw_hex(trace[name]["sha256"], 32, "child stream SHA256")
        proof = {"scope": "persistent-base-world-api", "spawn_entries": int(not self.client and self.scenario == "population"),
            "random_steps": 0, "same_world": True if not self.client and self.scenario == "population" else None,
            "same_thread": True if not self.client and self.scenario == "population" else None,
            "time_api_complete": True if self.scenario == "load" else None}
        self.eq(trace["setup_proof"], proof, "exact genuine constructor route")
        self.require(type(trace["bindings"]) is dict and set(trace["bindings"]) == set(BINDINGS[self.side]), "all eleven original method bindings")
        for name, pinned in BINDINGS[self.side].items():
            actual = trace["bindings"][name]
            _keys(actual, tuple(pinned) if self.public_proof else (*pinned, "bytecode_hex"), self.require, "complete original method binding")
            self.eq({key: actual[key] for key in pinned}, pinned, "pinned descriptor/modifiers/bytecode identity: " + name)
            if not self.public_proof:
                code = self.acquire.raw_hex(actual["bytecode_hex"], pinned["bytecode_length"], "complete original method bytes")
                self.eq(hashlib.sha256(code).hexdigest(), pinned["bytecode_sha256"], "original method body digest")
        for link in self.links.values():
            if link["class"] in self.value["class_origins"]:
                self.eq(link["loader_id"], self.loader, "all linked original objects use supplied-JAR loader")
            else:
                self.require(link["class"].startswith(("java.", "[")) and link["loader_id"] is None, "only bootstrap ordinary collection/array links")

    def time_proof(self, first):
        rows = self.trace["time_proof"]
        if self.scenario == "population":
            self.eq(rows, [], "population profile does not set world time")
            return
        self.require(type(rows) is list and len(rows) == 4, "complete four-boundary original time API proof")
        metadata = None
        for ordinal, row in enumerate(rows):
            _keys(row, ("kind", "class", "method", "signature", "code_index", "thread_id", "world_id", "metadata_id", "world_time", "value"), self.require, "time API observation")
            helper = ordinal in (0, 3)
            method = "timeBegin" if ordinal == 0 else "timeEnd" if ordinal == 3 else "a"
            expected = {"kind": method if helper else "time_api", "class": "CacheCallbacksObservation" if helper else self.world_class,
                "method": method, "signature": "(Ljava/lang/Object;J)V" if helper else "(J)V", "code_index": 8 if ordinal == 2 else 0,
                "thread_id": first["thread_id"], "world_id": first["world_id"], "world_time": "0000000000000000" if ordinal < 2 else "00000000000004d2", "value": 1234}
            self.eq({name: row[name] for name in expected}, expected, "actual public setter entry and return")
            _integer(row["metadata_id"], 1, (1 << 63) - 1, self.require, "world metadata identity")
            if metadata is None:
                metadata = row["metadata_id"]
            self.eq(row["metadata_id"], metadata, "retained original metadata time owner")

    def random(self, raw):
        _keys(raw, ("generator_id", "generator_random_id", "generator_seed", "cave_id", "cave_random_id", "cave_seed"), self.require, "actual retained Random observation")
        self.eq(raw["generator_id"], self.context_ids["generator_id"], "actual retained generator identity")
        for name in ("generator_random_id", "cave_id", "cave_random_id"):
            _integer(raw[name], 1, (1 << 63) - 1, self.require, "retained provider state identity")
        self.require(len({raw[name] for name in ("generator_id", "generator_random_id", "cave_id", "cave_random_id")}) == 4, "distinct retained provider objects")
        for name in ("generator_seed", "cave_seed"):
            self.acquire.raw_hex(raw[name], 6, "independent actual 48-bit Random state")

    def callback(self, kind, event, graph, target=None, script=None, slice_=None, coordinates=None):
        if coordinates is None:
            coordinates = event["arguments"][-2:]
        return {"kind": kind, "effect": "coordinator" if kind in ("tiles", "entities") else "observed-original-replay",
            "x": coordinates[0], "z": coordinates[1], "id": None if target is None else target["id"], "slice": slice_, "script": script,
            "owner_matched": True, "context_matched": True, "map_count": len(graph["map"]), "list_count": len(graph["insertions"]),
            "pending_count": len(graph["pending"]), "active": None if target is None else target["flags"]["c"],
            "dirty": None if target is None else target["flags"]["o"], "populated": None if target is None else target["flags"]["n"],
            "last_save_time": None if target is None else target["time"]}

    def schedule(self, index):
        fresh = index < (1 if self.scenario == "load" else 3)
        if not fresh:
            return [("acquire", 0), ("acquire", 20), ("acquire", 391), ("acquire_return", 393)]
        loaded = self.scenario == "load"
        schedule = [("acquire", 0), ("acquire", 20), ("acquire", 46), ("load_wrapper", 0), ("loader", 0),
            ("loader_ready", 277 if loaded else 27), ("loader_return", 277 if loaded else 27), ("load_wrapper", 25)]
        if loaded:
            schedule += [("load_wrapper", 37)]
        schedule += [("load_wrapper", 40), ("load_wrapper_return", 41)]
        if not loaded:
            schedule += [("acquire", 78), ("generate", 0), ("finish", 0), ("finish", 221), ("generator_ready", 129), ("generate_return", 129)]
        schedule += [("acquire", 85), ("acquire", 100), ("acquire", 112), ("activate", 0), ("activate", 5),
            ("register_tiles", 0), ("register_tiles_return", 32)]
        schedule += [(kind, offset) for _ in range(8) for kind, offset in (("register_entities", 0), ("register_entities_return", 43))]
        schedule += [("activate", 51), ("acquire", 128)]
        if not loaded:
            schedule += [("get", 0), ("get_return", 33)] * (3 if index == 2 else 1)
            if index == 2:
                schedule += [("population", 0), ("get", 0), ("get_return", 33), ("population", 22), ("population", 36),
                    ("provider_populate", 0), ("provider_ready", 2906), ("provider_populate_return", 2906), ("population", 43),
                    ("mark_dirty", 0), ("mark_dirty", 5), ("population", 46)]
        return schedule + [("acquire", 391), ("acquire_return", 393)]

    def operation(self, measured, row, output, before):
        index = output["index"]
        _keys(measured, ("index", "thread_id", "world_id", "cache_id", "generator_id", "loader_id", "result", "begin_threads", "end_threads",
            "begin_random", "end_random", "generated_count", "finish_count", "population_count", "events", "seams"), self.require, "complete measured operation")
        self.eq(measured["index"], index, "ordered measured operation")
        for name, identity in self.context_ids.items():
            self.eq(measured[name], identity, "actual retained operation context")
        _integer(measured["thread_id"], 1, (1 << 63) - 1, self.require, "measured main thread")
        for marker in ("begin", "end"):
            project_loader = _inventory(measured[marker + "_threads"], measured, marker, self.loader, self.require, self.equal)
            if self.project_loader is None:
                self.project_loader = project_loader
            self.eq(project_loader, self.project_loader, "same project marker loader across all operations")
        self.random(measured["begin_random"])
        self.random(measured["end_random"])
        self.eq({name: item for name, item in measured["begin_random"].items() if not name.endswith("_seed")},
            {name: item for name, item in measured["end_random"].items() if not name.endswith("_seed")},
            "same generator/cave/Random ownership within original operation")
        fresh = index < (1 if self.scenario == "load" else 3)
        generated = fresh and self.scenario == "population"
        populated = generated and index == 2
        self.eq([measured[name] for name in ("generated_count", "finish_count", "population_count")],
            [int(generated), int(generated), int(populated)], "complete zero-extra generation/finish/population counts")
        if not generated:
            self.eq(measured["begin_random"], measured["end_random"], "loaded/hit paths leave retained provider RNGs untouched")
        self.require(type(measured["events"]) is list and len(measured["events"]) <= 1024, "bounded lifecycle events")
        events = measured["events"]
        self.eq([[event["kind"], event["code_index"]] for event in events], [list(item) for item in self.schedule(index)],
            "complete exact callback lifecycle schedule; no omitted or opaque interior acquisitions")
        wanted = ["load", "generate", "populate"] if populated else ["load", "generate"] if generated else ["load"] if fresh else []
        self.require(type(measured["seams"]) is list, "complete ordered external seams")
        self.eq([seam["kind"] for seam in measured["seams"]], wanted, "exact outer callback seam schedule")
        seam_entries, seam_returns, ready_states = {}, {}, {}
        x, z = output["coordinates"]
        result_number = output["result"]
        result_jdi = self.link(self.by_number[result_number])
        origin_number = next((number for key, number in before["world"]["map"] if key == 0), None)
        for ordinal, seam in enumerate(measured["seams"]):
            _keys(seam, ("ordinal", "kind", "x", "z", "receiver_id", "provider_argument_id", "population_target_id", "opaque_interior", "before", "after", "result", "entry_event", "return_event"), self.require, "complete actual callback seam")
            kind = seam["kind"]
            self.eq(seam["ordinal"], ordinal, "ordered operation-local seams")
            self.eq([seam["x"], seam["z"]], [0, 0] if kind == "populate" else [x, z], "actual callback coordinates")
            self.require(seam["opaque_interior"] is False, "finite profile has zero nested external callbacks")
            self.eq(seam["receiver_id"], self.context_ids["loader_id" if kind == "load" else "generator_id"], "actual retained external callback receiver")
            self.eq(seam["provider_argument_id"], self.context_ids["cache_id"] if kind == "populate" else None, "same coordinator passed to population provider")
            self.eq(seam["population_target_id"], self.link(self.by_number[origin_number]) if kind == "populate" else None, "actual b-return population target identity")
            entry = _integer(seam["entry_event"], 0, len(events) - 1, self.require, "callback entry boundary")
            returned = _integer(seam["return_event"], entry + 1, len(events) - 1, self.require, "callback return boundary")
            entry_kind, return_kind = {"load": ("loader", "loader_return"), "generate": ("generate", "generate_return"), "populate": ("provider_populate", "provider_populate_return")}[kind]
            self.eq([events[entry]["kind"], events[returned]["kind"]], [entry_kind, return_kind], "paired exact original callback entry/return events")
            self.require(entry not in seam_entries and returned not in seam_returns, "one script per actual external invocation")
            pre, post = self.boundary(seam["before"]), self.boundary(seam["after"])
            self.eq(pre["context"], before["context"], "live context at callback entry")
            self.eq(post["context"], before["context"], "live context at callback return")
            returned_number = None if seam["result"] is None else self.header(seam["result"])["id"]
            new = kind == "generate" or kind == "load" and self.scenario == "load"
            self.eq(len(post["world"]["chunks"]), len(pre["world"]["chunks"]) + int(new), "fresh return grows visible identity inventory exactly once")
            for left, right in zip(pre["world"]["chunks"], post["world"]["chunks"]):
                self.eq([left["id"], left["coordinates"], left["blank"]], [right["id"], right["coordinates"], right["blank"]], "opaque body preserves existing identity/coordinates/dispatch")
            if new:
                self.eq(returned_number, result_number, "fresh callback returns actual outer result")
                self.eq(returned_number, len(pre["world"]["chunks"]), "fresh unpublished object is next stable ID")
                selected = post["world"]["chunks"][returned_number]
                self.eq(self.header(seam["result"]), {name: item for name, item in selected.items() if name != "data"}, "returned header equals complete unpublished payload")
                self.eq(selected["coordinates"], [x, z], "actual returned coordinates")
                self.eq(selected["flags"], {"c": False, "n": False, "o": kind == "generate", "p": False, "q": False}, "actual inactive unfinished-publication return flags")
                self.eq(selected["time"], "0000000000000000", "loader/generator result precedes live-time stamp")
                self.require(all(number != returned_number for _, number in post["world"]["map"]) and returned_number not in post["world"]["insertions"], "returned object remains unpublished at callback seam")
            else:
                self.require(returned_number is None, "no chunk returned by MISS/population")
            if kind == "load":
                self.eq(pre["world"]["map"], post["world"]["map"], "loader does not publish")
                self.eq(pre["world"]["insertions"], post["world"]["insertions"], "loader does not append")
                if not new:
                    self.eq(pre, post, "genuine missing-region loader has no world effects")
            if kind == "populate":
                self.require(pre["world"]["chunks"][origin_number]["flags"]["n"] is True, "populated flag precedes original provider call")
            script_index = len(self.scripts)
            script = {"kind": kind, "coordinates": [seam["x"], seam["z"]], "result": "loaded" if kind == "load" and new else "miss" if kind == "load" else "success",
                "returned_id": returned_number, "before": pre, "after": post}
            self.scripts.append(script)
            seam_entries[entry] = seam_returns[returned] = (script_index, script, seam)
            if kind in ("generate", "populate"):
                ready_states[returned - 1] = post
        phase = copy.deepcopy(before)
        current_chunk = None
        entity_slice, tiles_argument, gets = 0, None, 0
        callbacks = []
        cache_class, world_class, chunk_class = self.cache_class, self.world_class, self.chunk_class
        chunk_signature = "(II)L" + chunk_class + ";"
        def frame(owner, method, signature, offset, receiver):
            return {"class": owner, "method": method, "signature": signature, "code_index": offset, "receiver": receiver}
        cache_frame = lambda offset: frame(cache_class, "c", chunk_signature, offset, self.context_ids["cache_id"])
        wrapper_frame = lambda offset: frame(cache_class, "d", chunk_signature, offset, self.context_ids["cache_id"])
        generator_frame = lambda offset: frame(self.generator_class, "b", chunk_signature, offset, self.context_ids["generator_id"])
        activation_frame = lambda offset: frame(chunk_class, "e" if self.client else "d", "()V", offset, result_jdi)
        population_frame = lambda offset: frame(cache_class, "a", "(L" + self.provider_class + ";II)V", offset, self.context_ids["cache_id"])
        gen_after = next((script["after"] for _, script, _ in seam_returns.values() if script["kind"] == "generate"), None)
        pop_after = next((script["after"] for _, script, _ in seam_returns.values() if script["kind"] == "populate"), None)
        get_arguments = [([0, 0], 194 if index == 0 else 263, False)] if generated and index < 2 else [
            ([0, 1], 194, False), ([1, 0], 263, False), ([0, 0], 336, False), ([0, 0], 3, True)] if populated else []
        for ordinal, event in enumerate(events):
            _keys(event, ("ordinal", "kind", "opaque_interior", "class", "method", "signature", "code_index", "thread_id", "receiver", "arguments", "result", "chunk", "cache", "original_frames"), self.require, "complete lifecycle event")
            kind, offset = event["kind"], event["code_index"]
            self.eq(event["ordinal"], ordinal, "ordered lifecycle event")
            self.eq(event["thread_id"], measured["thread_id"], "same original measured thread")
            self.require(event["opaque_interior"] is (kind in ("provider_populate", "provider_ready")), "exactly classified provider body boundaries; no inner c/gen")
            arguments, callers = [], []
            get_result = None
            if kind in ("acquire", "acquire_return"):
                selected, arguments = cache_frame(offset), [x, z]
            elif kind.startswith("load_wrapper"):
                selected, callers, arguments = wrapper_frame(offset), [cache_frame(46)], [x, z]
            elif kind in ("loader", "loader_ready", "loader_return"):
                selected = frame(self.loader_class, "a", "(L" + world_class + ";II)L" + chunk_class + ";", offset, self.context_ids["loader_id"])
                callers, arguments = [wrapper_frame(19), cache_frame(46)], [{"id": self.context_ids["world_id"], "class": world_class}, x, z]
            elif kind in ("generate", "generator_ready", "generate_return"):
                selected, callers, arguments = generator_frame(offset), [cache_frame(78)], [x, z]
            elif kind == "finish":
                selected = frame(chunk_class, "c" if self.client else "b", "()V", offset, result_jdi)
                callers = [generator_frame(124), cache_frame(78)]
            elif kind == "activate":
                selected, callers = activation_frame(offset), [cache_frame(125)]
            elif kind.startswith("register_"):
                entities = kind.startswith("register_entities")
                selected = frame(world_class, "a", "(Ljava/util/List;)V" if entities else "(Ljava/util/Collection;)V", offset, self.context_ids["world_id"])
                callers = [activation_frame(42 if entities else 18), cache_frame(125)]
                if entities:
                    arguments = [{"id": self.local_owners[result_jdi]["entity_slices"][entity_slice]["id"], "class": "java.util.ArrayList"}]
                else:
                    self.require(type(event["arguments"]) is list and len(event["arguments"]) == 1, "actual tile collection argument")
                    _keys(event["arguments"][0], ("id", "class"), self.require, "tile values view")
                    self.require(event["arguments"][0]["class"] == "java.util.HashMap$Values", "original HashMap values view")
                    _integer(event["arguments"][0]["id"], 1, (1 << 63) - 1, self.require, "tile values view identity")
                    if tiles_argument is None:
                        tiles_argument = event["arguments"]
                    arguments = tiles_argument
            elif kind in ("get", "get_return"):
                coordinates, caller_offset, population_get = get_arguments[gets]
                selected = frame(cache_class, "b", chunk_signature, offset, self.context_ids["cache_id"])
                callers = [population_frame(3), cache_frame(388)] if population_get else [cache_frame(caller_offset)]
                arguments = coordinates
                key = (coordinates[0] << 16) | coordinates[1]
                get_result = next(number for packed, number in phase["world"]["map"] if packed == key)
            elif kind == "population":
                selected, callers = population_frame(offset), [cache_frame(388)]
                arguments = [{"id": self.context_ids["cache_id"], "class": cache_class}, 0, 0]
            elif kind in ("provider_populate", "provider_ready", "provider_populate_return"):
                selected = frame(self.generator_class, "a", "(L" + self.provider_class + ";II)V", offset, self.context_ids["generator_id"])
                callers, arguments = [population_frame(36), cache_frame(388)], [{"id": self.context_ids["cache_id"], "class": cache_class}, 0, 0]
            else:
                self.require(kind == "mark_dirty", "known original lifecycle method")
                selected = frame(chunk_class, "g" if self.client else "f", "()V", offset, self.link(self.by_number[origin_number]))
                callers = [population_frame(43), cache_frame(388)]
            self.eq({name: event[name] for name in selected}, selected, "exact event method/descriptor/receiver/code index")
            self.eq(event["arguments"], arguments, "exact original call arguments and owners")
            self.eq(event["original_frames"], [selected, *callers], "complete exact original direct caller stack")
            if ordinal in ready_states:
                phase = copy.deepcopy(ready_states[ordinal])
            if kind == "finish" and offset == 221:
                phase = copy.deepcopy(gen_after)
            if ordinal in seam_entries:
                script_index, script, seam = seam_entries[ordinal]
                self.eq(phase, script["before"], "complete actual callback input equals live coordinator state")
                target = phase["world"]["chunks"][origin_number] if script["kind"] == "populate" else None
                callbacks.append(self.callback(script["kind"] + "_enter", event, phase["world"], target, script_index, coordinates=script["coordinates"]))
            if ordinal in seam_returns:
                script_index, script, seam = seam_returns[ordinal]
                phase = copy.deepcopy(script["after"])
                target_id = origin_number if script["kind"] == "populate" else script["returned_id"]
                target = None if target_id is None else phase["world"]["chunks"][target_id]
                callbacks.append(self.callback(script["kind"] + "_return", event, phase["world"], target, script_index, coordinates=script["coordinates"]))
                self.eq(event["result"], seam["result"], "actual event and seam return value agree")
            if kind == "acquire" and offset == 20:
                key = (x << 16) | z
                phase["world"]["pending"] = [number for number in phase["world"]["pending"] if number != key]
            if kind == "load_wrapper" and offset == 40 and self.scenario == "load":
                phase["world"]["chunks"][result_number]["time"] = phase["context"]["world_time"]
            if kind == "acquire" and offset == 100:
                key = (x << 16) | z
                phase["world"]["map"] = sorted([pair for pair in phase["world"]["map"] if pair[0] != key] + [[key, result_number]])
            if kind == "acquire" and offset == 112:
                phase["world"]["insertions"].append(result_number)
            if kind == "activate" and offset == 5:
                phase["world"]["chunks"][result_number]["flags"]["c"] = True
            if kind == "population" and offset == 22:
                phase["world"]["chunks"][origin_number]["flags"]["n"] = True
            if kind == "mark_dirty" and offset == 5:
                phase["world"]["chunks"][origin_number]["flags"]["o"] = True
            graph, headers = self.graph(event["cache"])
            self.eq(graph, {name: phase["world"][name] for name in graph}, "exact intermediate map/list/pending/ordered queue contents")
            for number, header in headers.items():
                self.eq(header, {name: item for name, item in phase["world"]["chunks"][number].items() if name != "data"}, "every intermediate cached header and flag/time")
            if kind == "finish" and offset == 0:
                current_chunk = copy.deepcopy(gen_after["world"]["chunks"][result_number])
                current_chunk["flags"]["o"] = False
            elif kind == "finish" and offset == 221 or kind == "load_wrapper_return" and self.scenario == "load" or kind == "activate":
                current_chunk = phase["world"]["chunks"][result_number]
            elif current_chunk is not None and len(phase["world"]["chunks"]) > result_number:
                current_chunk = phase["world"]["chunks"][result_number]
            if current_chunk is None:
                self.require(event["chunk"] is None, "no invented measured returned chunk before actual finish/load return")
            else:
                self.eq(self.header(event["chunk"]), {name: item for name, item in current_chunk.items() if name != "data"}, "actual retained outer chunk at each event")
            returned_number = result_number if kind in ("acquire_return", "generate_return") or kind == "load_wrapper_return" and self.scenario == "load" else get_result if kind == "get_return" else None
            if kind == "loader_return":
                returned_number = result_number if self.scenario == "load" else None
            if returned_number is None:
                self.require(event["result"] is None, "no invented result at non-return boundary")
            else:
                self.eq(self.header(event["result"]), {name: item for name, item in phase["world"]["chunks"][returned_number].items() if name != "data"}, "exact actual returned identity/header")
            if kind in ("register_tiles", "register_entities"):
                target = phase["world"]["chunks"][result_number]
                self.require(target["flags"]["c"] is True, "active flag set before tile/entity registration")
                callbacks.append(self.callback("entities" if kind == "register_entities" else "tiles", event, graph, target,
                    slice_=entity_slice if kind == "register_entities" else None, coordinates=target["coordinates"]))
            if kind == "register_entities_return":
                entity_slice += 1
            if kind == "get_return":
                gets += 1
        self.eq(entity_slice, 8 if fresh else 0, "all eight entity registrations and returns")
        self.eq(gets, len(get_arguments), "all ordered live population gate lookups")
        self.eq(phase, output["snapshot"], "complete computed coordinator final state equals observed snapshot")
        self.eq(self.header(measured["result"]), {name: item for name, item in phase["world"]["chunks"][result_number].items() if name != "data"}, "actual result at helper end marker")
        output["callbacks"] = callbacks
        return phase


def validate_trace(value, trace, expected, ids, acquire, finish, *, public_proof=False):
    """Return validated MCC1 replay model and complete expected C output.

    Caller first validates the observer document and immutable saved-world inputs.
    Only genuine external callback bodies become explicitly labeled script effects.
    public_proof is only for a caller-validated fingerprint-only public fixture;
    live captures always supply original bytecodes as well as their fingerprints.
    """
    acquire.require(type(public_proof) is bool, "explicit public-proof mode")
    validation = _Validation(value, trace, expected, ids, acquire, finish, public_proof)
    validation.provenance()
    require, equal = validation.require, validation.equal
    require(type(trace["operations"]) is list and len(trace["operations"]) == len(expected["operations"]), "complete measured callback operations")
    completed = copy.deepcopy(expected)
    previous = completed["initial"]
    previous_random, main = None, None
    for index, (measured, row, output) in enumerate(zip(trace["operations"], value["operations"], completed["operations"])):
        if main is None:
            main = measured["thread_id"]
            validation.time_proof(measured)
        require(equal(measured["thread_id"], main), "same measured main across every operation")
        if previous_random is not None:
            require(equal(previous_random, measured["begin_random"]), "provider state unchanged outside measured original calls")
        previous = validation.operation(measured, row, output, previous)
        if previous_random is not None:
            require(equal({name: item for name, item in previous_random.items() if not name.endswith("_seed")},
                {name: item for name, item in measured["end_random"].items() if not name.endswith("_seed")}), "retained provider RNG ownership")
        previous_random = measured["end_random"]
    minimum_events = sum(len(row["events"]) for row in trace["operations"]) + len(trace["object_references"]) + \
        len(trace["operations"]) * 2 + len(trace["time_proof"]) + trace["setup_proof"]["spawn_entries"] + 2
    require(trace["jdi_event_count"] >= minimum_events, "total JDI delivery includes every recorded marker/lifecycle/identity/class event")
    replay = {"seed_hex": value["seed_hex"], "initial": completed["initial"], "operations": [
        {"method": row["method"], "coordinates": row["coordinates"]} for row in completed["operations"]], "scripts": validation.scripts}
    return replay, completed
