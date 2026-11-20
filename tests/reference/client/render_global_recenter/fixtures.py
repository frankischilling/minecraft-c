"""Declared actual fields for unchanged recenter/JO methods; no game code."""
import json
import struct


def i32(value):
    return (value + (1 << 31)) % (1 << 32) - (1 << 31)


def div(left, right):
    return i32(abs(left) // abs(right) * (-1 if (left < 0) != (right < 0) else 1))


def actor(q=0, r=0, s=0, c=0, d=0, e=0, dirty=0):
    return [c, d, e, q, r, s, dirty]


def cases():
    result = []

    def jo(name, camera, actors, order=None):
        result.append(dict(kind="JO", name=name, camera=camera, grid=[0, 0, 0],
                           origin=[0, 0, 0], actors=[actor(*a) for a in actors],
                           array=list(range(1, len(actors) + 1)) if order is None else order,
                           queue=[]))

    jo("empty", [0, 0, 0], [])
    jo("singleton", [0, 0, 0], [(5, 2, 3)])
    jo("ordered_ties", [0, 0, 0], [(1, 0, 0), (-1, 0, 0), (2, 0, 0), (-2, 0, 0)])
    jo("reverse", [.25, -.5, .75], [(8, 2, 3), (4, 1, 1), (2, 0, 1), (0, 0, 0)])
    jo("identities_equal_centers", [2, 3, 4], [(1, 2, 3)] * 8, [8, 3, 7, 2, 6, 1, 5, 4])
    jo("fractional_zero_compare", [.49999999999999994, 0, 0], [(1, 0, 0), (0, 0, 0)] * 4)
    jo("signed_zero", [-0.0, 0.0, -0.0], [(0, 0, 0), (1, 0, 0), (-1, 0, 0)])
    jo("nan", [float("nan"), 1, 2], [(8, 9, 10), (1, 2, 3), (-4, -5, -6)])
    jo("infinity", [float("inf"), 1, 2], [(8, 9, 10), (1, 2, 3), (-4, -5, -6)])
    jo("max_double", [float.fromhex("0x1.fffffffffffffp+1023"), -1, 2],
       [(8, 9, 10), (1, 2, 3), (-4, -5, -6)])
    jo("integer_extremes", [.1, -.2, .3],
       [(2147483647, 0, 0), (-2147483648, 0, 0), (0, 0, 0), (1, 2, 3)])
    jo("mixed_timsort", [5.125, -8.75, 2.5],
       [((i * 37) % 101 - 50, (i * 7) % 13 - 6, (i * 11) % 29 - 14) for i in range(80)])
    jo("null_left", [0, 0, 0], [(1, 0, 0)], [1, 0])
    jo("null_right", [0, 0, 0], [(1, 0, 0)], [0, 1])
    jo("null_after_insertion", [0, 0, 0], [(5, 0, 0), (6, 0, 0), (7, 0, 0), (1, 0, 0), (8, 0, 0)],
       [1, 2, 3, 4, 5, 0])
    jo("null_late_timsort", [3, 2, 1],
       [((i * 29) % 71 - 35, i % 5, -i % 11) for i in range(70)],
       list(range(1, 54)) + [0] + list(range(54, 71)))
    jo("null_camera", None, [(1, 0, 0)])
    jo("null_array", [1, 2, 3], [(1, 0, 0)])
    result[-1]["array"] = None
    jo("null_camera_and_array", None, [(1, 0, 0)])
    result[-1]["array"] = None

    def recenter(name, grid, origin, mode="full"):
        p, q, r = grid
        actors = []
        width = i32(p * 16)
        if 0 < p <= 8 and 0 <= q <= 4 and 0 <= r <= 8:
            half = div(width, 2)
            for iz in range(r):
                for iy in range(q):
                    for ix in range(p):
                        values = []
                        for slot, center in [(ix, origin[0]), (iz, origin[2])]:
                            coordinate = i32(slot * 16)
                            delta = i32(i32(coordinate + half) - i32(center - 8))
                            if delta < 0:
                                delta = i32(delta - i32(width - 1))
                            values.append(i32(coordinate - i32(div(delta, width) * width)))
                        actors.append(actor(c=values[0], d=iy * 16, e=values[1],
                                            dirty=len(actors) % 2))
        array = list(range(1, len(actors) + 1))
        if mode == "null":
            array = None
        elif mode == "short":
            array = array[:max(0, len(array) - 2)]
        elif mode == "null_slot":
            array[min(3, len(array) - 1)] = 0
        result.append(dict(kind="REC", name=name, camera=None, grid=grid, origin=origin,
                           actors=actors, array=array, queue=[len(actors), 1] if actors else []))

    recenter("empty_x", [0, 2, 3], [19, 7, -23], "null")
    recenter("negative_x", [-1, 2, 3], [19, 7, -23], "null")
    recenter("empty_z", [2, 2, 0], [-17, 7, 99], "null")
    recenter("empty_y", [3, 0, 2], [-17, -2147483648, 99], "null")
    recenter("origin", [2, 2, 2], [0, 0, 0])
    recenter("negative", [3, 2, 3], [-53, -99, -47])
    recenter("rectangular", [2, 2, 3], [-19, 2147483647, 55])
    recenter("int_extremes", [3, 2, 2], [-2147483648, 0, 2147483647])
    recenter("null_array_prefix", [2, 2, 2], [19, 7, -23], "null")
    recenter("short_array_prefix", [2, 2, 2], [19, 7, -23], "short")
    recenter("null_slot_prefix", [2, 2, 2], [19, 7, -23], "null_slot")
    recenter("wrapped_zero_width", [268435456, 0, 0], [19, 7, -23], "null")
    return result


def encode():
    lines = []
    for index, case in enumerate(cases()):
        camera = case["camera"]
        bits = [struct.pack(">d", float(v)).hex() for v in (camera or [0, 0, 0])]
        array = case["array"]
        lines.append(" ".join(map(str, [case["kind"], index, case["name"], int(camera is not None),
                                       *bits, *case["grid"], *case["origin"], len(case["actors"]),
                                       -1 if array is None else len(array), len(case["queue"])])))
        lines.extend(" ".join(map(str, actor)) for actor in case["actors"])
        lines.append(" ".join(map(str, array or [])))
        lines.append(" ".join(map(str, case["queue"])))
    return "\n".join(lines) + "\n"
