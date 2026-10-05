#include "ida.h"
volatile uint32_t in_p, in_o, out_len, out_nodes;
uint8_t path[IDA_MAX_DEPTH];
__attribute__((noinline)) void done(void) { __asm__ volatile("nop"); }
void _start(void)
{
    __asm__ volatile("li sp, 0x80000");
    uint64_t n = 0;
    out_len = (uint32_t) ida_solve((uint16_t) in_p, (uint16_t) in_o, path, &n);
    out_nodes = (uint32_t) n;
    done();
    for (;;) {}
}
