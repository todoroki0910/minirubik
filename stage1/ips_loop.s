# ips_loop.s -- Measurement 2: retired instructions per second
#
# A simple memory loop: each iteration does one load, one add, one store,
# and the loop control (5 instructions per iteration).
#
# Retired-instruction count is read from Ripes' --iret, and simulation
# time from --exectime (wall-clock ms, excluding startup and assembly):
#   rate = iret / (exectime_ms / 1000)
#
# Expected count: iret = 5 * ITER + 9, used to check the run is correct.
#
# ITER is chosen so a run lasts about 10-30 seconds. Values used:
#   RV32_ISS : ITER = 100000000  (5.0e8 instructions)
#   RV32_5S  : ITER = 1000000    (5.0e6 instructions)

.equ ITER, 100000000
.equ BASE, 0x10000000

.text
main:
    li   t0, ITER           # loop counter
    li   t1, BASE           # t1 = address of one word in guest memory
    sw   zero, 0(t1)

loop:
    lw   t2, 0(t1)          # load
    addi t2, t2, 1          # modify
    sw   t2, 0(t1)          # store
    addi t0, t0, -1
    bnez t0, loop           # 5 instructions per iteration

    lw   a0, 0(t1)          # print final value (should equal ITER)
    li   a7, 1
    ecall

    li   a7, 10
    ecall
