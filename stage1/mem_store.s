# mem_store.s -- Measurement 1: host bytes per guest byte
#
# Writes N bytes of guest memory with word stores (sw), then spins in a
# delay loop that touches no memory, then halts. The delay keeps Ripes
# alive for a couple of seconds after the writes, so the peak-memory
# sampler in measure.ps1 is certain to observe the final footprint.
#
# Run once per N (control + several sizes) on RV32_ISS and record the
# peak working set. ratio = slope of peak against N.
#
# Every variant uses the same program text and the same DELAY, so the
# fixed cost of Ripes and of the program image cancels in the subtraction.

.equ N,     4194304         # bytes to write; must be a multiple of 4
.equ BASE,  0x10000000      # start of Ripes' data segment
.equ DELAY, 25000000        # ~5e7 instructions, about 2 s on RV32_ISS

.text
main:
    li   t0, BASE           # t0 = current address
    li   t1, N
    add  t1, t0, t1         # t1 = end address (exclusive)
    li   t2, 0x12345678     # non-zero pattern

store_loop:
    sw   t2, 0(t0)          # one 32-bit store = 4 guest bytes
    addi t0, t0, 4
    bne  t0, t1, store_loop

    li   t3, DELAY          # register-only spin: no new guest memory
delay_loop:
    addi t3, t3, -1
    bnez t3, delay_loop

    li   a0, N              # print N so the report shows which size ran
    li   a7, 1
    ecall

    li   a7, 10
    ecall
