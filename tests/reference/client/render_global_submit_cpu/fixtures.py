"""Declared RAW source fields; all cases fail before U302."""


def cases():
    defaults = dict(dirty=[1, 1, 1], array=[1, 2, 3], queue=[1, 0, 1],
                    cursor=0, app=0, settings=0, distance=0, old_distance=0, pass_number=0)
    profiles = [
        ("zero_spatial", dict(array=[], cursor=5)),
        ("null_spatial", dict(array=None, cursor=7)),
        ("negative_cursor", dict(cursor=-2)),
        ("wrapped_cursor", dict(cursor=2147483647)),
        ("null_dk", dict(cursor=-1, array=[0, 1, 2])),
        ("null_dirty_first", dict(cursor=-1, queue=None)),
        ("null_dirty_after_clean", dict(cursor=-1, dirty=[0, 1, 0], queue=None)),
        ("null_app_after_polls", dict()),
        ("null_settings_after_polls", dict(app=1)),
        ("null_camera_reset", dict(app=1, settings=1, distance=3, old_distance=3)),
        ("null_camera_pass1", dict(app=1, settings=1, distance=3, old_distance=3, pass_number=1)),
        ("rebuild_old_dk_null_passes", dict(app=1, settings=1, distance=3, dirty=[0, 0, 0])),
    ]
    return [dict(defaults, name=name, **changes) for name, changes in profiles]


def encode():
    lines = []
    for index, case in enumerate(cases()):
        array, queue = case["array"], case["queue"]
        lines.append(" ".join(map(str, [index, case["name"], case["cursor"], case["app"],
                                       case["settings"], case["distance"], case["old_distance"],
                                       case["pass_number"], len(case["dirty"]),
                                       -1 if array is None else len(array),
                                       -1 if queue is None else len(queue)])))
        lines.append(" ".join(map(str, case["dirty"])))
        lines.append(" ".join(map(str, array or [])))
        lines.append(" ".join(map(str, queue or [])))
    return "\n".join(lines) + "\n"
