"""Disassemble around a module-relative offset, using the file on disk.

Crash dumps give a runtime address; subtracting the module base yields an RVA
that is stable across runs, so faults can be located without a debugger.
"""
import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32


def load(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec, = struct.unpack_from("<H", d, pe + 6)
    opt, = struct.unpack_from("<H", d, pe + 20)
    base, = struct.unpack_from("<I", d, pe + 24 + 28)
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode("latin-1")
        va, = struct.unpack_from("<I", d, o + 12)
        rsz, = struct.unpack_from("<I", d, o + 16)
        rp, = struct.unpack_from("<I", d, o + 20)
        secs.append((name, va, rsz, rp))
    return d, base, secs


def to_off(secs, rva):
    for _n, va, rsz, rp in secs:
        if va <= rva < va + rsz:
            return rp + (rva - va)
    return None


def main(path, rva_hex, back=0x60, fwd=0x40):
    rva = int(rva_hex, 16)
    d, base, secs = load(path)
    off = to_off(secs, rva)
    if off is None:
        raise SystemExit("rva not mapped")

    start_rva, start_off = rva - back, off - back
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False
    code = d[start_off:off + fwd]
    for ins in md.disasm(code, base + start_rva):
        mark = "  <<<< FAULT" if ins.address == base + rva else ""
        print("%08X  %-22s %s %s%s" % (ins.address,
                                       ins.bytes.hex(), ins.mnemonic, ins.op_str, mark))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2],
         int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x60,
         int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x40)
