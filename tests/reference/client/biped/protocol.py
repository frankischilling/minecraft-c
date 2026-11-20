"""Complete ordered source records; no expected pixel/geometry table."""
import re


def require(value, message):
    if not value:
        raise ValueError(message)


def parse(data, mode):
    require(mode in ['cold','pair','pool','notice'], 'profile')
    require(isinstance(data, bytes) and len(data) < 1024 * 1024, 'bounded complete bytes')
    rows = data.decode('ascii').splitlines()
    cursor = 0

    def exact(text):
        nonlocal cursor
        require(cursor < len(rows) and rows[cursor] == text, 'ordered record: ' + text)
        cursor += 1

    def geometry(label):
        nonlocal cursor
        exact('CASE ' + label)
        require(cursor < len(rows) and re.fullmatch(r'BIPED [0-9a-f]{8}(?: [01]){4}', rows[cursor]), 'Biped root')
        cursor += 1
        for part in range(9):
            require(cursor < len(rows), 'part present')
            cells = rows[cursor].split()
            require(len(cells) == 15 and cells[:2] == ['PART',str(part)] and
                    all(re.fullmatch(r'-?[0-9]+', x) for x in cells[2:9]) and
                    all(re.fullmatch(r'[0-9a-f]{8}', x) for x in cells[9:]), 'ordered actual part fields')
            cursor += 1
            for vertex in range(8):
                require(cursor < len(rows), 'vertex present')
                cells = rows[cursor].split()
                require(len(cells) == 8 and cells[:3] == ['VERTEX',str(part),str(vertex)] and
                        all(re.fullmatch(r'[0-9a-f]{16}', x) for x in cells[3:6]) and
                        all(re.fullmatch(r'[0-9a-f]{8}', x) for x in cells[6:]), 'ordered actual vertex bits')
                cursor += 1
            for face in range(6):
                require(cursor < len(rows), 'quad present')
                cells = rows[cursor].split()
                require(len(cells) == 21 and cells[:5] == ['QUAD',str(part),str(face),'4','0'], 'quad fields/order')
                for index in range(5,21,4):
                    require(re.fullmatch('[0-7]',cells[index]) and cells[index+1] == '1' and
                            re.fullmatch('[0-9a-f]{8}',cells[index+2]) and re.fullmatch('[0-9a-f]{8}',cells[index+3]), 'actual UV alias/copy/bits')
                cursor += 1
    if mode == 'notice':
        geometry('notice-base')
    geometry(mode)
    exact('ALIASES 72 216 1')
    if mode == 'pair':
        geometry('pair-second')
        exact('DISTINCT 1')
    exact('RANDOM unchanged 1')
    if mode == 'pool':
        exact('POOL 2 2 unchanged 1')
    if mode == 'notice':
        exact('NOTICE distinct 1')
        exact('LIFETIME notice-first 1')
    exact('END clean')
    require(cursor == len(rows), 'no omitted/trailing records')
    return rows
