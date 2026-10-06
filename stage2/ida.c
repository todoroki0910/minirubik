/* stage2/ida.c - IDA* search for the 2x2x2 */
#include "ida.h"
#include "tables.h"

#define NOT_FOUND 0
#define FOUND     1
#define NO_FACE   3            /* "no previous move" at the root */

static uint8_t *cur_path;      /* where the moves are written */
static uint64_t *cur_expanded; /* node counter */
static int next_bound;         /* smallest f that exceeded the bound */

/* Admissible: each table is an exact distance in an abstraction, and the
 * max of two lower bounds is still a lower bound. */
static int heuristic(int p, int o)
{
    int hp = perm_dist[p], ho = ori_dist[o];
    return hp > ho ? hp : ho;
}

static int search(int p, int o, int g, int bound, int last_face)
{
    int f = g + heuristic(p, o);

    /* Every solution through this node has length >= f > bound: prune, and
     * remember the smallest such f as the next bound worth trying. */
    if (f > bound) {
        if (f < next_bound)
            next_bound = f;
        return NOT_FOUND;
    }

    /* Rank 0 in both coordinates is the solved cube. */
    if (p == 0 && o == 0)
        return FOUND;

    ++*cur_expanded;

    for (int face = 0; face < 3; ++face) {
        /* Two turns of the same face in a row merge into one turn or none,
         * so a shortest solution never contains them. */
        if (face == last_face)
            continue;

        int np = p, no = o;
        for (int turn = 0; turn < 3; ++turn) {   /* f, f2, f' */
            np = perm_move[face][np];
            no = ori_move[face][no];
            cur_path[g] = (uint8_t) (face * 3 + turn);
            if (search(np, no, g + 1, bound, face) == FOUND)
                return FOUND;
        }
    }
    return NOT_FOUND;
}

int ida_solve(uint16_t p, uint16_t o, uint8_t path[IDA_MAX_DEPTH],
              uint64_t *expanded)
{
    cur_path = path;
    cur_expanded = expanded;

    int bound = heuristic(p, o);
    for (;;) {
        next_bound = 255;
        /* No solution is shorter than bound (earlier iterations ruled them
         * out) and the one found has g <= bound, so its length is bound. */
        if (search(p, o, 0, bound, NO_FACE) == FOUND)
            return bound;

        /* Every solution of length <= bound has been ruled out; the
         * shortest one left is at least the smallest pruned f. */
        bound = next_bound;
    }
}

#ifdef RIPES
/* Entry point for Ripes: solve one state and exit with the solution length.
 * Build with -DRIPES -DIN_P=... -DIN_O=... (default: the worst state). */
#ifndef IN_P
#define IN_P 3343   /* 54721631111111, worst distance-11 state */
#define IN_O 0
#endif

static uint8_t ripes_path[IDA_MAX_DEPTH];

int main(void)
{
    volatile uint16_t p = IN_P, o = IN_O;  /* volatile: no compile-time solve */
    uint64_t expanded = 0;
    int len = ida_solve(p, o, ripes_path, &expanded);

    register int a0 __asm__("a0") = len;   /* exit code = solution length */
    register int a7 __asm__("a7") = 93;    /* Ripes ecall 93: exit */
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7));
    for (;;) {}
}
#endif
