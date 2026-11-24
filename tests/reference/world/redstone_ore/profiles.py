"""Predeclared operands for the bounded unchanged-original Redstone Ore calls."""


def declared():
    result = []

    def add(mode, operation='left', entry_id=73, **changes):
        row = dict(mode='ore-' + mode, operation=operation, xyz=[15, 64, 15],
                   entry_id=entry_id, random_seed=1, cell_error_at=0,
                   range_error_at=0, particle_error_at=0, sound_error_at=0,
                   prequeue_delay=-1, random_player=False, player=False,
                   g=17, cooldown=6001, null_random=False,
                   null_tick_random=False, null_world=False, null_provider=False, null_access=False,
                   null_unlit=False, null_lit=False,
                   cells=[[15, 64, 15, entry_id, 5]], heights=[])
        row.update(changes)
        result.append(row)

    for operation in ('left', 'walk', 'right'):
        for block in (73, 74):
            add(operation + '-' + str(block), operation, block)
    directions = ((0, 1, 0), (0, -1, 0), (0, 0, 1),
                  (0, 0, -1), (1, 0, 0), (-1, 0, 0))
    for face, (dx, dy, dz) in enumerate(directions):
        add('opaque-face-' + str(face), cells=[[15, 64, 15, 73, 5],
                                             [15 + dx, 64 + dy, 15 + dz, 1, 0]])
    add('opaque-all', cells=[[15, 64, 15, 73, 5]] +
        [[15 + dx, 64 + dy, 15 + dz, 1, 0] for dx, dy, dz in directions])
    for y in (0, 127):
        add('height-' + str(y), xyz=[15, y, 15], cells=[[15, y, 15, 73, 5]])
    add('receiver-lit-cell-unlit', entry_id=74, cells=[[15, 64, 15, 73, 5]])
    add('receiver-unlit-cell-lit', cells=[[15, 64, 15, 74, 5]])
    add('height-prepared', heights=[[15, 15, 65]])
    for ordinal in (1, 3, 5):
        add('particle-error-' + str(ordinal), particle_error_at=ordinal)
    add('cell-error', cell_error_at=1)
    add('range-error', range_error_at=1)
    for block in (73, 74):
        add('tick-' + str(block), 'tick', block)
        add('tick-null-argument-' + str(block), 'tick', block, null_tick_random=True)
        add('display-' + str(block), 'display', block)
    add('tick-cell-error', 'tick', 74, cell_error_at=1)
    add('tick-range-error', 'tick', 74, range_error_at=1)
    add('tick-world-random-null', 'tick', 74, null_random=True)
    add('world-random-null', null_random=True)
    add('provider-null', null_provider=True)
    add('random-provider-null', null_random=True, null_provider=True)
    add('unlit-static-null', null_unlit=True)
    add('lit-static-null', null_lit=True)
    add('particle-before-lit-static-null', null_lit=True, particle_error_at=1)
    add('provider-before-unlit-static-null', null_unlit=True, null_provider=True)
    add('tick-unlit-static-null', 'tick', 74, null_unlit=True)
    add('tick-lit-static-null', 'tick', 74, null_lit=True)
    add('tick-unlit-receiver-lit-static-null', 'tick', 73, null_lit=True)
    for block in (73, 74):
        add('world-scheduled-' + str(block), 'world-tick', block, prequeue_delay=0)
    add('world-scheduled-cell-error', 'world-tick', 74, prequeue_delay=0, cell_error_at=1)
    add('world-scheduled-range-error', 'world-tick', 74, prequeue_delay=0, range_error_at=1)
    add('world-random-player', 'world-tick', 74, random_player=True,
        xyz=[-57, 64, -56], g=-332372929, cells=[[-57, 64, -56, 74, 5]])
    for operation in ('mining', 'right-use'):
        add(operation, operation, player=True)
        for callback in ('particle', 'cell', 'range'):
            add(operation + '-' + callback + '-error', operation,
                player=True, **{callback + '_error_at': 1})
    add('right-use-sound-error', 'right-use', player=True, sound_error_at=1)
    for operation, block in (('tick', 73), ('tick', 74), ('left', 73),
                             ('walk', 73), ('right', 73),
                             ('display', 73), ('display', 74)):
        add(operation + '-null-world-' + str(block), operation, block, null_world=True)
    add('access-null', null_access=True)
    add('tick-access-null-73', 'tick', 73, null_access=True)
    add('tick-access-null-74', 'tick', 74, null_access=True)
    add('tick-access-null-cell-74', 'tick', 74, null_access=True, heights=[[15, 15, 65]])
    return result
