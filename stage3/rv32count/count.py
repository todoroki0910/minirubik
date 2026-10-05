"""
count.py - count retired RV32I instructions of ida_solve, by instruction class.

Compiles nothing itself: give it ELF files built from harness.c and an ida.c,
for example (clang 18 with lld):

  clang --target=riscv32-unknown-elf -march=rv32i -mabi=ilp32 -O2 \
        -ffreestanding -nostdlib -fno-builtin -fuse-ld=lld \
        -Wl,-Ttext=0x10000 -Wl,-e,_start -I../../stage2 \
        -o stage3.elf harness.c ../ida_iterative.c

  python3 count.py stage2.elf stage3.elf

Runs each ELF in the Unicorn CPU emulator (pip install unicorn pyelftools)
on the worst distance-11 state and on 21345671111111, counting every
instruction from _start until done() is reached. This is a host-side
estimate of operation counts for stage 3, not the Ripes --iret figure.
"""
import collections
import re
import subprocess
import sys

from elftools.elf.elffile import ELFFile
from unicorn import UC_ARCH_RISCV, UC_HOOK_CODE, UC_MODE_RISCV32, Uc
from unicorn.riscv_const import UC_RISCV_REG_SP

STATES = [("worst 54721631111111", 3343, 0), ("ref   21345671111111", 720, 0)]


def kind(m):
    if m in ("lw", "lh", "lhu", "lb", "lbu"):
        return "load"
    if m in ("sw", "sh", "sb"):
        return "store"
    if m in ("j", "jal", "jalr", "ret", "call", "tail", "jr"):
        return "jump"
    if m.startswith("b"):
        return "branch"
    return "alu"


def run(path, p, o):
    dis = subprocess.run(["llvm-objdump", "-d", "--no-show-raw-insn", path],
                         capture_output=True, text=True).stdout
    mnem = {int(a, 16): m for a, m in
            re.findall(r"^\s*([0-9a-f]+):\s+(\S+)", dis, re.M)}
    elf = ELFFile(open(path, "rb"))
    sym = {s.name: s["st_value"]
           for s in elf.get_section_by_name(".symtab").iter_symbols()}
    mu = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
    mu.mem_map(0, 0x100000)
    for seg in elf.iter_segments():
        if seg["p_type"] == "PT_LOAD":
            mu.mem_write(seg["p_paddr"], seg.data())
    mu.mem_write(sym["in_p"], p.to_bytes(4, "little"))
    mu.mem_write(sym["in_o"], o.to_bytes(4, "little"))
    mu.reg_write(UC_RISCV_REG_SP, 0x80000)
    hits = collections.Counter()
    mu.hook_add(UC_HOOK_CODE, lambda uc, a, sz, ud: hits.update((a,)))
    mu.emu_start(elf.header["e_entry"], sym["done"])
    length = int.from_bytes(mu.mem_read(sym["out_len"], 4), "little")
    nodes = int.from_bytes(mu.mem_read(sym["out_nodes"], 4), "little")
    mix = collections.Counter()
    for a, n in hits.items():
        mix[kind(mnem.get(a, "?"))] += n
    return length, nodes, sum(hits.values()), mix


for path in sys.argv[1:]:
    for name, p, o in STATES:
        length, nodes, total, mix = run(path, p, o)
        per = "  ".join(f"{k} {mix[k] / nodes:.1f}"
                        for k in ("alu", "load", "store", "branch", "jump"))
        print(f"{path}  {name}  {length} moves  {nodes} nodes  "
              f"{total} instr  {total / nodes:.1f}/node  [{per}]")
