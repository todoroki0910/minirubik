/*
 * stage3/ida_iterative.c - IDA* reshaped for RV32I, same interface and same search
 * order as stage2/ida.c (so the same paths and node counts come out).
 *
 * What changed from stage 2, and why:
 *
 *   1. No recursion. The search is one loop over an explicit stack of at
 *      most IDA_MAX_DEPTH + 1 frames, held in fixed-size arrays. The
 *      assembly may not recurse and has no heap.
 *   2. No multiply. perm_move[face][x] needs face * 5040, ori_move needs
 *      face * 729, and the move code needs face * 3. The two table rows are
 *      reached through 3-entry pointer tables instead (one shift and one
 *      load), and the move code is carried along and incremented.
 *   3. No call per node. h is inlined; nothing is passed or saved per level.
 *   4. One table step per child. The next turn of the same face starts from
 *      the previous sibling's state, already stored in the child's frame,
 *      so f2 and f' cost one lookup each, not two and three.
 *   5. Word-sized frame arrays. RV32I loads a byte or halfword as cheaply
 *      as a word, but every narrow store and every narrow value reused in
 *      arithmetic costs extra masking; 48 bytes of stack instead of 24.
 *
 * Build with the stage 2 tables and checker:
 *   gcc -O2 -Wall -Wextra -I../stage2 -o h3_check ../stage2/h3_check.c ida_iterative.c
 */
#include "ida.h"
#include "tables.h"

#define NO_FACE 3

/* Row base of each face: replaces the multiply in perm_move[face][x]. */
static const uint16_t *const perm_row[3] = {perm_move[0], perm_move[1],
                                            perm_move[2]};
static const uint16_t *const ori_row[3] = {ori_move[0], ori_move[1],
                                           ori_move[2]};

int ida_solve(uint16_t p0, uint16_t o0, uint8_t path[IDA_MAX_DEPTH],
              uint64_t *expanded)
{
    /* Frame g holds the state at depth g, the face that reached it
     * (last[g]), and the iterator over its children: the face being tried
     * (face[g]) and the move code of the child last generated (path[g]). */
    uint32_t P[IDA_MAX_DEPTH + 1], O[IDA_MAX_DEPTH + 1];
    uint32_t last[IDA_MAX_DEPTH + 1], face[IDA_MAX_DEPTH + 1];

    uint32_t hp = perm_dist[p0], ho = ori_dist[o0];
    uint32_t bound = hp > ho ? hp : ho;

    for (;;) {
        uint32_t next_bound = 255;
        uint32_t g = 0;
        P[0] = p0;
        O[0] = o0;
        last[0] = NO_FACE;

    enter: /* first visit to the node in frame g */
        hp = perm_dist[P[g]];
        ho = ori_dist[O[g]];
        {
            uint32_t f = g + (hp > ho ? hp : ho);
            if (f > bound) {
                if (f < next_bound)
                    next_bound = f;
                goto leave;
            }
        }
        if ((P[g] | O[g]) == 0)
            return (int) g; /* g == bound, see stage 2 */
        ++*expanded;
        face[g] = 0;
        goto first_turn;

    next_child: /* back in frame g after a child returned */
        if (path[g] != face[g] + face[g] + face[g] + 2) {
            /* next turn of the same face: one more quarter turn applied to
             * the sibling that just returned, which is still in frame g+1 */
            path[g]++;
            P[g + 1] = perm_row[face[g]][P[g + 1]];
            O[g + 1] = ori_row[face[g]][O[g + 1]];
            last[g + 1] = face[g];
            g++;
            goto enter;
        }
        face[g]++;

    first_turn: /* start face[g], skipping the face that reached frame g */
        if (face[g] == last[g])
            face[g]++;
        if (face[g] >= 3) /* >=: at the root last is NO_FACE (3), so a */
            goto leave;   /* face of 3 can be bumped to 4 just above   */
        path[g] = (uint8_t) (face[g] + face[g] + face[g]); /* turn 0 */
        P[g + 1] = perm_row[face[g]][P[g]];
        O[g + 1] = ori_row[face[g]][O[g]];
        last[g + 1] = face[g];
        g++;
        goto enter;

    leave: /* frame g is done: return to its parent */
        if (g == 0) {
            bound = next_bound;
            continue;
        }
        g--;
        goto next_child;
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
