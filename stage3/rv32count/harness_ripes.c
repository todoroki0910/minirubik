/* harness_ripes.c - run ida_solve once on a fixed state and exit through a
 * Ripes environment call, so the compiled ELF can be loaded into Ripes and
 * its "Instrs. retired" read directly. Build with -DIN_P=... -DIN_O=... */
#include "ida.h"
volatile uint32_t in_p = IN_P, in_o = IN_O, out_len, out_nodes;
uint8_t path[IDA_MAX_DEPTH];
__attribute__((noinline)) void done(void) { __asm__ volatile("nop"); }
void _start(void)
{
    uint64_t n = 0;
    out_len = (uint32_t) ida_solve((uint16_t) in_p, (uint16_t) in_o, path, &n);
    out_nodes = (uint32_t) n;
    done();
    register uint32_t a0 __asm__("a0") = out_len;
    register uint32_t a7 __asm__("a7") = 93; /* exit with the solution length */
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7));
    for (;;) {}
}
