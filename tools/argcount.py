"""Recover __thiscall argument counts from a module's vtable.

Each method ends in `ret N` (0xC2 imm16) or `ret` (0xC3). N is the bytes of
stack arguments, so N/4 is the argument count excluding `this`. Scanning for the
first epilogue from each function start is enough for these thin forwarders.
"""
import re, struct, sys


def load_module(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec, = struct.unpack_from("<H", d, pe + 6)
    opt_size, = struct.unpack_from("<H", d, pe + 20)
    image_base, = struct.unpack_from("<I", d, pe + 24 + 28)
    secs = []
    for i in range(nsec):
        off = pe + 24 + opt_size + i * 40
        vaddr, = struct.unpack_from("<I", d, off + 12)
        rawsz, = struct.unpack_from("<I", d, off + 16)
        rawptr, = struct.unpack_from("<I", d, off + 20)
        secs.append((vaddr, rawsz, rawptr))
    return d, image_base, secs


def rva_to_off(secs, rva):
    for vaddr, rawsz, rawptr in secs:
        if vaddr <= rva < vaddr + rawsz:
            return rawptr + (rva - vaddr)
    return None


def find_ret(d, off, limit=400):
    """First ret in the function body; returns arg bytes or None."""
    i = off
    end = off + limit
    while i < end and i + 3 < len(d):
        b = d[i]
        if b == 0xC2:                       # ret imm16
            return struct.unpack_from("<H", d, i + 1)[0]
        if b == 0xC3:                       # ret 0
            return 0
        i += 1
    return None


def main(module_path, base_hex, slots_file):
    d, image_base, secs = load_module(module_path)
    runtime_base = int(base_hex, 16)

    print("%-10s %-12s %s" % ("slot", "address", "args (ret N -> N/4)"))
    for line in open(slots_file):
        m = re.match(r"\w+\[(\d+)\]=([0-9A-Fa-f]+)", line.strip())
        if not m:
            continue
        slot, addr = int(m.group(1)), int(m.group(2), 16)
        rva = addr - runtime_base
        off = rva_to_off(secs, rva)
        if off is None:
            print("%-10d 0x%08X   <outside module>" % (slot, addr))
            continue
        n = find_ret(d, off)
        if n is None:
            print("%-10d 0x%08X   <no ret found>" % (slot, addr))
        else:
            print("%-10d 0x%08X   ret %-4d -> %d arg(s)" % (slot, addr, n, n // 4))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
