/*
 * ida.h - interface between the IDA* search (ida.c, yours) and the H3 checker.
 *
 * Move encoding, the same as solver.c:
 *   m in 0..8, face = m / 3 (0 = R, 1 = B, 2 = D),
 *   quarter turns   = m % 3 + 1 (1 = f, 2 = f2, 3 = f')
 *   so 0..8 are R R2 R' B B2 B' D D2 D'.
 */
#ifndef IDA_H
#define IDA_H

#include <stdint.h>

#define IDA_MAX_DEPTH 11 /* HTM diameter of the 2x2x2 */

/*
 * Solve the state (p, o): p is the permutation rank 0..5039, o the
 * orientation rank 0..728, as in tables.h. (0, 0) is solved.
 *
 * Writes the moves to path[0..len-1] and returns len, the number of moves.
 * Adds the number of nodes expanded during the whole search, all bounds
 * included, to *expanded.
 */
int ida_solve(uint16_t p, uint16_t o, uint8_t path[IDA_MAX_DEPTH],
              uint64_t *expanded);

#endif /* IDA_H */
