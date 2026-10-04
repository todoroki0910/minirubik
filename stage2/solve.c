/*
 * solve.c - command-line front end for ida.c, same input format as solver.c.
 *
 *   solve 21345671111111
 *   R B' D2 R' B R' B' R D2 R B
 *   (11 moves, 38998 nodes expanded)
 *
 * The argument is 14 digits: positions 1..7 (a permutation of 1234567) and
 * orientations 1..3 whose values minus one sum to a multiple of 3.
 *
 * Build:  gcc -O2 -Wall -Wextra -o solve solve.c ida.c
 */
#include <stdio.h>
#include <string.h>

#include "ida.h"

static const char *const move_names[9] = {"R",  "R2", "R'", "B", "B2",
                                          "B'", "D",  "D2", "D'"};

/* Parse and rank the 14-digit input. Returns 0 on invalid input. */
static int parse(const char *s, uint16_t *p_out, uint16_t *o_out)
{
    int p[7], o[7], seen = 0, sum = 0;
    if (strlen(s) != 14)
        return 0;
    for (int i = 0; i < 7; ++i) {
        if (s[i] < '1' || s[i] > '7' || s[i + 7] < '1' || s[i + 7] > '3')
            return 0;
        p[i] = s[i] - '1';
        o[i] = s[i + 7] - '1';
        if (seen & (1 << p[i]))
            return 0; /* repeated cubie */
        seen |= 1 << p[i];
        sum += o[i];
    }
    if (sum % 3)
        return 0; /* violates the orientation invariant */

    int pr = 0, or = 0;
    for (int i = 0; i < 7; ++i) { /* Lehmer code, as in gen_tables.c */
        int smaller = 0;
        for (int j = i + 1; j < 7; ++j)
            smaller += p[j] < p[i];
        pr = pr * (7 - i) + smaller;
    }
    for (int i = 0; i < 6; ++i) /* first six twists in base 3 */
        or = or * 3 + o[i];
    *p_out = (uint16_t) pr;
    *o_out = (uint16_t) or;
    return 1;
}

int main(int argc, char **argv)
{
    uint16_t p, o;
    if (argc != 2 || !parse(argv[1], &p, &o)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", argc ? argv[0] : "solve");
        return 2;
    }
    uint8_t path[IDA_MAX_DEPTH];
    uint64_t nodes = 0;
    int len = ida_solve(p, o, path, &nodes);
    for (int i = 0; i < len; ++i)
        printf("%s%s", i ? " " : "", move_names[path[i]]);
    printf("\n(%d moves, %lu nodes expanded)\n", len,
           (unsigned long) nodes);
    return 0;
}
