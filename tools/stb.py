# -*- coding: utf-8 -*-
"""STB1 rooms files (Repentance) - format read from the game's own loader (0x750cf0 / room reader 0x751324).
header: 'STB1', u32 count. room: u32 type, u32 variant, u32 subtype, u8 difficulty, u16 nameLen + name, f32 weight,
u8 width, u8 height, u8 shape, u8 numDoors, u16 numSpawns, doors[numDoors]{i16 x, i16 y, u8 exists},
spawns[numSpawns]{i16 x, i16 y, u8 stack, stack*{u16 type, u16 variant, u16 subtype, f32 weight}}."""
import struct

class Room:
    __slots__ = ('type', 'variant', 'subtype', 'diff', 'name', 'weight', 'w', 'h', 'shape', 'doors', 'spawns')

def read(path_or_bytes):
    d = open(path_or_bytes, 'rb').read() if isinstance(path_or_bytes, str) else path_or_bytes
    if d[:4] != b'STB1': raise ValueError('not STB1')
    n = struct.unpack_from('<I', d, 4)[0]; o = 8; rooms = []
    for _ in range(n):
        r = Room()
        r.type, r.variant, r.subtype = struct.unpack_from('<III', d, o); o += 12
        r.diff = d[o]; o += 1
        nl = struct.unpack_from('<H', d, o)[0]; o += 2
        r.name = d[o:o + nl].decode('latin-1'); o += nl
        r.weight = struct.unpack_from('<f', d, o)[0]; o += 4
        r.w, r.h, r.shape, nd = d[o], d[o + 1], d[o + 2], d[o + 3]; o += 4
        ns = struct.unpack_from('<H', d, o)[0]; o += 2
        r.doors = []
        for _ in range(nd):
            x, y, ex = struct.unpack_from('<hhB', d, o); o += 5; r.doors.append((x, y, ex))
        r.spawns = []
        for _ in range(ns):
            x, y, st = struct.unpack_from('<hhB', d, o); o += 5; stack = []
            for _ in range(st):
                t, v, s, w = struct.unpack_from('<HHHf', d, o); o += 10; stack.append((t, v, s, w))
            r.spawns.append((x, y, stack))
        rooms.append(r)
    if o != len(d): raise ValueError('trailing bytes: parsed %d of %d' % (o, len(d)))
    return rooms

def write(rooms):
    out = bytearray(b'STB1' + struct.pack('<I', len(rooms)))
    for r in rooms:
        nm = r.name.encode('latin-1')
        out += struct.pack('<IIIBH', r.type, r.variant, r.subtype, r.diff, len(nm)) + nm + struct.pack('<f', r.weight)
        out += bytes((r.w, r.h, r.shape, len(r.doors))) + struct.pack('<H', len(r.spawns))
        for x, y, ex in r.doors: out += struct.pack('<hhB', x, y, ex)
        for x, y, stack in r.spawns:
            out += struct.pack('<hhB', x, y, len(stack))
            for t, v, s, w in stack: out += struct.pack('<HHHf', t, v, s, w)
    return bytes(out)
