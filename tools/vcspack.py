"""Compile a Source .fxc into a VCS v6 archive.

Consumes the work-list block that Valve's fxc_prep.pl appends to
filelistgen.txt, so combo ranges, skip rules and the fxc command line all come
from Valve's own generator rather than being reimplemented here.

Format (materialsystem/shaderapidx9/vertexshaderdx8.cpp, utils/shadercompile):
  header 28B | static combo records 8B each (last = sentinel, offset = EOF)
  | uint32 dup count | dup records 8B each | per-combo blocks
Each block: uint32 size (0x80000000 = uncompressed) then packed dynamic combos
of (uint32 dynamic id, uint32 length, bytecode), terminated by 0xffffffff.
"""
import argparse, lzma, os, re, struct, subprocess, sys, zlib
from concurrent.futures import ThreadPoolExecutor

MAX_UNPACKED_BLOCK = 1 << 17
UNCOMPRESSED = 0x80000000
LZMA_COMPRESSED = 0x40000000
SENTINEL = 0xFFFFFFFF


def valve_lzma(raw):
    """Valve's lzma_header_t + raw LZMA1 stream (tier1/lzmaDecoder.h).

    L4D2 only takes the compressed path: every block in every shipped shader
    we have is 0x40000000, and emitting uncompressed blocks made the engine's
    combo lookup return nothing and fault on the null result.
    """
    comp = lzma.compress(
        raw, format=lzma.FORMAT_ALONE,
        filters=[{"id": lzma.FILTER_LZMA1, "dict_size": MAX_UNPACKED_BLOCK,
                  "lc": 3, "lp": 0, "pb": 2}])
    props, stream = comp[:5], comp[13:]   # drop FORMAT_ALONE's 8-byte size field
    return b"LZMA" + struct.pack("<II", len(raw), len(stream)) + props + stream


def parse_worklist(path, shader):
    """Pull one #BEGIN..#END block out of filelistgen.txt."""
    text = open(path, encoding="utf-8", errors="replace").read()
    m = re.search(r"#BEGIN " + re.escape(shader) + r"\n(.*?)\n#END", text, re.S)
    if not m:
        raise SystemExit("no work-list block for %r in %s" % (shader, path))
    body = m.group(1)

    def section(name, nxt=None):
        # nxt=None means the section runs to the end of the block.
        rows = body.splitlines()
        try:
            first = rows.index(chr(35) + name) + 1
        except ValueError:
            return ''
        last = len(rows)
        if nxt:
            marker = chr(35) + nxt
            if marker in rows[first:]:
                last = rows.index(marker, first)
        return chr(10).join(rows[first:last]).strip()

    def defines(block):
        out = []
        for line in block.splitlines():
            line = line.strip()
            if not line:
                continue
            name, rng = line.split("=")
            lo, hi = rng.split("..")
            out.append((name, int(lo), int(hi)))
        return out

    return {
        "source": body.splitlines()[0].strip(),
        "dynamic": defines(section("DEFINES-D:", "DEFINES-S:")),
        "static": defines(section("DEFINES-S:", "SKIP:")),
        "skip": section("SKIP:", "COMMAND:"),
        "command": section("COMMAND:").replace("\n", " "),
    }


def to_python(expr):
    """Valve writes skip rules as perl boolean expressions over $VARS."""
    expr = re.sub(r"\$(\w+)", r"V['\1']", expr)
    expr = expr.replace("&&", " and ").replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    return expr


def decode(ordinal, defs):
    """Mixed-radix decode; the first define is least significant."""
    vals, rest = {}, ordinal
    for name, lo, hi in defs:
        span = hi - lo + 1
        vals[name] = lo + rest % span
        rest //= span
    return vals


def compile_combo(fxc, base_args, source, values, workdir, tag):
    out = os.path.join(workdir, "c%s.o" % tag)
    cmd = [fxc] + base_args
    cmd += ["/D%s=%d" % (k, v) for k, v in sorted(values.items())]
    cmd += ["/Fo" + out, source]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=workdir)
    if r.returncode != 0 or not os.path.exists(out):
        raise SystemExit("fxc failed for %s\n%s\n%s" % (values, " ".join(cmd), r.stdout + r.stderr))
    data = open(out, "rb").read()
    os.remove(out)
    return data


def pack_static_combo(dyn_map):
    """Serialise one static combo's dynamic combos into size-capped blocks."""
    out, block = bytearray(), bytearray()
    for did in sorted(dyn_map):
        code = dyn_map[did]
        entry = struct.pack("<II", did, len(code)) + code
        if block and len(block) + len(entry) > MAX_UNPACKED_BLOCK:
            blob = valve_lzma(bytes(block))
            out += struct.pack("<I", LZMA_COMPRESSED | len(blob)) + blob
            block = bytearray()
        block += entry
    if block:
        blob = valve_lzma(bytes(block))
        out += struct.pack("<I", LZMA_COMPRESSED | len(blob)) + blob
    out += struct.pack("<I", SENTINEL)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("worklist")
    ap.add_argument("--shader", required=True)
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--fxc", required=True)
    ap.add_argument("--extra-skip", action="append", default=[],
                    help="perl-style expr; combos matching it are not compiled. "
                         "Use this rather than removing a define: the declared "
                         "ranges must stay identical to the generated .inc or "
                         "every static index resolves to the wrong shader.")
    ap.add_argument("--include", action="append", default=[],
                    help="extra include dir passed to fxc")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args()

    spec = parse_worklist(args.worklist, args.shader)
    statics, dynamics = spec["static"], spec["dynamic"]

    n_dyn = 1
    for _, lo, hi in dynamics:
        n_dyn *= hi - lo + 1
    n_static = 1
    for _, lo, hi in statics:
        n_static *= hi - lo + 1

    terms = ([spec["skip"]] if spec["skip"] else []) + list(args.extra_skip)
    skip_expr = to_python(" || ".join("(%s)" % t for t in terms)) if terms else "False"
    base = [a for a in spec["command"].split() if a.startswith("/")
            and not a.startswith("/Fo") and not a.startswith("/DTOTALSHADERCOMBOS")
            and not a.startswith("/DNUMDYNAMICCOMBOS")]
    base += ["/DTOTALSHADERCOMBOS=%d" % (n_static * n_dyn),
             "/DNUMDYNAMICCOMBOS=%d" % n_dyn]
    base += ["/I" + os.path.abspath(d) for d in args.include]

    print("%s: %d static x %d dynamic = %d combos" % (args.shader, n_static, n_dyn, n_static * n_dyn))

    jobs = []
    for s in range(n_static):
        sv = decode(s, statics)
        for d in range(n_dyn):
            V = dict(sv)
            V.update(decode(d, dynamics))
            if eval(skip_expr, {"V": V}):
                continue
            jobs.append((s, d, V))
    print("  %d after skips (%d skipped)" % (len(jobs), n_static * n_dyn - len(jobs)))

    results = {}
    src = os.path.basename(spec["source"])

    def run(job):
        s, d, V = job
        return s, d, compile_combo(args.fxc, base, src, V, args.workdir, "%d_%d" % (s, d))

    done = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for s, d, code in pool.map(run, jobs):
            results.setdefault(s, {})[d] = code
            done += 1
            if done % 250 == 0 or done == len(jobs):
                print("  compiled %d/%d" % (done, len(jobs)), flush=True)

    packed = {s: pack_static_combo(m) for s, m in sorted(results.items())}

    # Static combos with byte-identical payloads collapse into alias records.
    unique, dups, seen = {}, {}, {}
    for s in sorted(packed):
        key = packed[s]
        if key in seen:
            dups[s] = seen[key]
        else:
            seen[key] = s
            unique[s] = key

    n_records = len(unique) + 1  # + sentinel
    header_len = 28 + n_records * 8 + 4 + len(dups) * 8
    offset, records = header_len, []
    for s in sorted(unique):
        records.append((s, offset))
        offset += len(unique[s])
    records.append((SENTINEL, offset))

    crc = zlib.crc32(open(os.path.join(args.workdir, src), "rb").read()) & 0xFFFFFFFF
    with open(args.out, "wb") as f:
        f.write(struct.pack("<iiiIIII", 6, n_static * n_dyn, n_dyn, 0, 0, n_records, crc))
        for cid, off in records:
            f.write(struct.pack("<II", cid, off))
        f.write(struct.pack("<I", len(dups)))
        for cid in sorted(dups):
            f.write(struct.pack("<II", cid, dups[cid]))
        for s in sorted(unique):
            f.write(unique[s])

    print("wrote %s (%d bytes): %d unique + %d duplicate static combos"
          % (args.out, offset, len(unique), len(dups)))


if __name__ == "__main__":
    main()
