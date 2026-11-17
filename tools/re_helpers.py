#!/usr/bin/env python3
"""Ayudas de ingenieria inversa para isaac.arm64 (el binario de iOS que carga el port). Solo lee; no modifica nada.

  pip install capstone            (PyPI esta permitido en el entorno de Claude)
  export ISAAC_ARM64=/ruta/a/isaac.arm64      # o --img RUTA

DATO CLAVE (verificado): el OFFSET EN EL ARCHIVO ES IGUAL A LA DIRECCION que usa el source (0x4f0ee4 = EvaluateItems).
Para vtables/punteros en datos el binario usa "chained fixups": un puntero a una funcion NO aparece como valor plano.

Subcomandos:
  disas  ADDR [N]          desensambla N instrucciones (por defecto 40)
  start  ADDR              HEURISTICA: busca hacia atras un 'stp ..., [sp,#-N]!' (inicio de funcion). NO ve las funciones que
                           empiezan con 'sub sp,sp,#N' (Spawn 0x88fcbc, HandleCollision 0x71e310): compruebalo con disas/proto
  proto  ADDR              las 4 primeras palabras de la funcion y si el desvio de 4 palabras (detour) es seguro (sin adrp/b/bl/cbz/ldr literal)
  callers ADDR             quien llama (bl) o salta (b) a ADDR en el texto  (si sale vacio: es llamada indirecta por vtable)
  strs   TEXTO             cadenas C que contienen TEXTO (direccion y contenido)
  strrefs TEXTO            lo anterior + todo el codigo que carga cada cadena (pares adrp+add)
  field  OFFSET            todo ldr/str de 32 y 64 bits con ese desplazamiento (p. ej. 0x16c8) agrupado por zona
"""
import argparse, os, re, struct, sys
try:
    import capstone
except ImportError:
    sys.exit("falta capstone: pip install capstone --break-system-packages")

TEXT0, TEXT1 = 0x1000, 0x9a0000       # rango de codigo del isaac.arm64 de esta version (sha 217be349...)

def load(path):
    img = open(path, 'rb').read()
    return img, capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)

def w32(img, a): return struct.unpack_from('<I', img, a)[0]

def cmd_disas(img, md, a, n=40):
    for i in md.disasm(img[a:a + 4 * n], a):
        print(f"{i.address:x}: {i.mnemonic} {i.op_str}")

def func_start(img, a, back=0x2000):
    for off in range(a, a - back, -4):
        w = w32(img, off)
        if (w & 0xffc003e0) in (0xa98003e0, 0x6d8003e0):         # stp xA,xB / dA,dB, [sp, #-N]!  (pre-indexado)
            return off
    return None

def pic_safe(w):
    """True si la palabra no depende de su propia direccion (sirve para copiarla a otra pagina)."""
    if (w & 0x1f000000) == 0x10000000: return False               # adr / adrp
    if (w & 0x7c000000) == 0x14000000: return False               # b / bl
    if (w & 0xff000010) == 0x54000000: return False               # b.cond
    if (w & 0x7e000000) == 0x34000000: return False               # cbz / cbnz
    if (w & 0x7e000000) == 0x36000000: return False               # tbz / tbnz
    if (w & 0x3b000000) == 0x18000000: return False               # ldr literal
    return True

def cmd_proto(img, md, a):
    ws = [w32(img, a + 4 * i) for i in range(4)]
    print('palabras:', ', '.join(f"0x{w:08x}u" for w in ws))
    print('desvio de 4 palabras seguro:', all(pic_safe(w) for w in ws))
    cmd_disas(img, md, a, 6)

def cmd_callers(img, md, t):
    out = []
    for a in range(TEXT0, TEXT1, 4):
        w = w32(img, a)
        if (w & 0x7c000000) == 0x14000000:
            imm = w & 0x3ffffff
            if imm & 0x2000000: imm -= 0x4000000
            if a + imm * 4 == t: out.append((a, 'bl' if w & 0x80000000 else 'b'))
    print('llamadas directas:', [(hex(a), k) for a, k in out] or 'ninguna (probable llamada indirecta)')

def cstrings(img, text):
    res = {}
    for m in re.finditer(re.escape(text.encode()), img):
        a = m.start()
        while a > 0 and img[a - 1] != 0: a -= 1
        b = m.start()
        while b < len(img) and img[b] != 0: b += 1
        s = img[a:b]
        if 2 < len(s) < 200: res[a] = s.decode('latin1')
    return res

def adrp_add_refs(img, targets):
    hits = []
    for a in range(TEXT0, TEXT1, 4):
        w = w32(img, a)
        if (w & 0x9f000000) != 0x90000000: continue
        immlo = (w >> 29) & 3; immhi = (w >> 5) & 0x7ffff; imm = (immhi << 2) | immlo
        if imm & 0x100000: imm -= 0x200000
        page = (a & ~0xfff) + (imm << 12); rd = w & 31
        for k in (1, 2, 3):
            n = w32(img, a + 4 * k)
            if (n & 0xffc00000) == 0x91000000 and ((n >> 5) & 31) == rd:
                t = page + ((n >> 10) & 0xfff)
                if t in targets: hits.append((a + 4 * k, t))
    return hits

def cmd_strs(img, text):
    for a, s in sorted(cstrings(img, text).items()): print(f"{a:#x}  {s!r}")

def cmd_strrefs(img, text):
    ss = cstrings(img, text)
    for a, s in sorted(ss.items()): print(f"{a:#x}  {s!r}")
    for pc, t in adrp_add_refs(img, set(ss)): print(f"   codigo en {pc:#x} carga {t:#x} {ss[t]!r}")

def cmd_field(img, off):
    import collections
    s4, s8 = off // 4, off // 8
    hits = []
    for a in range(TEXT0, TEXT1, 4):
        w = w32(img, a); imm = (w >> 10) & 0xfff
        if off % 4 == 0 and imm == s4 and (w & 0xffc00000) in (0xb9400000, 0xb9000000):
            hits.append((a, 'ldr w' if (w & 0xffc00000) == 0xb9400000 else 'str w', w & 31, (w >> 5) & 31))
        if off % 8 == 0 and imm == s8 and (w & 0xffc00000) in (0xf9400000, 0xf9000000):
            hits.append((a, 'ldr x' if (w & 0xffc00000) == 0xf9400000 else 'str x', w & 31, (w >> 5) & 31))
    c = collections.Counter(hex(a & ~0xfff) for a, *_ in hits)
    print(f"{len(hits)} accesos con desplazamiento {off:#x} (cualquier registro base; puede haber otras estructuras)")
    for k, v in sorted(c.items()): print('  zona', k, v)
    for a, k, rt, rn in hits: print(f"    {a:x} {k}{rt}, [x{rn}]")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--img', default=os.environ.get('ISAAC_ARM64'))
    ap.add_argument('cmd', choices=['disas', 'start', 'proto', 'callers', 'strs', 'strrefs', 'field'])
    ap.add_argument('arg'); ap.add_argument('n', nargs='?', type=int, default=40)
    a = ap.parse_args()
    if not a.img: sys.exit('dime el binario: --img RUTA o export ISAAC_ARM64=RUTA')
    img, md = load(a.img)
    num = lambda s: int(s, 0)
    if a.cmd == 'disas': cmd_disas(img, md, num(a.arg), a.n)
    elif a.cmd == 'start': print(hex(func_start(img, num(a.arg)) or 0))
    elif a.cmd == 'proto': cmd_proto(img, md, num(a.arg))
    elif a.cmd == 'callers': cmd_callers(img, md, num(a.arg))
    elif a.cmd == 'strs': cmd_strs(img, a.arg)
    elif a.cmd == 'strrefs': cmd_strrefs(img, a.arg)
    elif a.cmd == 'field': cmd_field(img, num(a.arg))

if __name__ == '__main__':
    try:
        main()
    except BrokenPipeError:
        pass
