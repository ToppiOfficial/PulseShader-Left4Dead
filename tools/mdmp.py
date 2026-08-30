"""Minimal minidump reader: reports the faulting address and owning module.

Enough of the format to answer "where did it crash" without a debugger.
"""
import struct, sys

EXCEPTION_STREAM, MODULE_LIST_STREAM = 6, 4


def read_string(d, rva):
    (n,) = struct.unpack_from("<I", d, rva)
    return d[rva + 4: rva + 4 + n].decode("utf-16-le", "replace")


def main(path):
    d = open(path, "rb").read()
    sig, ver, nstreams, dir_rva = struct.unpack_from("<IIII", d, 0)
    if sig != 0x504D444D:
        raise SystemExit("not a minidump")

    streams = {}
    for i in range(nstreams):
        t, size, rva = struct.unpack_from("<III", d, dir_rva + i * 12)
        streams[t] = (size, rva)

    exc_addr = exc_code = None
    if EXCEPTION_STREAM in streams:
        _, rva = streams[EXCEPTION_STREAM]
        tid, _pad = struct.unpack_from("<II", d, rva)
        code, flags, rec, addr, nparams = struct.unpack_from("<IIQQI", d, rva + 8)
        exc_code, exc_addr = code, addr
        params = struct.unpack_from("<15Q", d, rva + 8 + 32)
        print("exception code    : 0x%08X" % code)
        print("faulting address  : 0x%016X" % addr)
        print("thread id         : %d" % tid)
        if code == 0xC0000005 and nparams >= 2:
            kind = {0: "read", 1: "write", 8: "execute"}.get(params[0], params[0])
            print("access violation  : %s at 0x%X" % (kind, params[1]))

    mods = []
    if MODULE_LIST_STREAM in streams:
        _, rva = streams[MODULE_LIST_STREAM]
        (n,) = struct.unpack_from("<I", d, rva)
        for i in range(n):
            off = rva + 4 + i * 108
            base, size, _csum, _ts, name_rva = struct.unpack_from("<QIIII", d, off)
            mods.append((base, size, read_string(d, name_rva)))

    if exc_addr is not None:
        owner = [m for m in mods if m[0] <= exc_addr < m[0] + m[1]]
        print()
        if owner:
            base, size, name = owner[0]
            print("FAULTING MODULE   : %s" % name)
            print("  base 0x%X  size 0x%X  offset +0x%X" % (base, size, exc_addr - base))
        else:
            print("FAULTING MODULE   : <not in any loaded module> (bad function pointer)")

    print()
    print("modules of interest:")
    for base, size, name in sorted(mods):
        low = name.lower()
        if any(k in low for k in ("pulse", "tier0", "shaderapi", "materialsystem", "engine", "d3d9", "studiorender")):
            print("  0x%08X-0x%08X  %s" % (base, base + size, name))


if __name__ == "__main__":
    main(sys.argv[1])
