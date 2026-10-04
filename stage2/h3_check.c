/*
 * h3_check.c - host-side gate H3 for the IDA* search in ida.c.
 *
 * For every one of the 3,674,160 states (or only the distance-11 ones with
 * --d11), calls ida_solve() and checks that
 *   - the returned length equals the exact distance from a full BFS, and
 *   - applying the returned moves reaches the solved state (0, 0).
 * It also reports search cost: expanded nodes per state, the worst case
 * over the 2,644 distance-11 states, and the reference vector
 * 21345671111111 (permutation rank 720, orientation rank 0).
 *
 * Build:  gcc -O2 -Wall -Wextra -o h3_check h3_check.c ida.c
 * Run:    h3_check          all states (takes minutes)
 *         h3_check --d11    only the 2,644 distance-11 states
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ida.h"
#include "tables.h"

enum {
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    UNSEEN = 0xFF,
};

/* Exact distance of every state, by BFS over the move tables. */
static uint8_t *full_bfs(void)
{
    uint8_t *d = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    if (!d || !queue) {
        free(d);
        free(queue);
        return NULL;
    }
    memset(d, UNSEEN, STATES);
    uint32_t head = 0, tail = 0;
    d[0] = 0;
    queue[tail++] = 0;
    while (head < tail) {
        uint32_t x = queue[head++];
        int p = (int) (x / ORIENTATIONS), o = (int) (x % ORIENTATIONS);
        for (int f = 0; f < 3; ++f) {
            int np = p, no = o;
            for (int t = 0; t < 3; ++t) {
                np = perm_move[f][np];
                no = ori_move[f][no];
                uint32_t y = (uint32_t) np * ORIENTATIONS + (uint32_t) no;
                if (d[y] == UNSEEN) {
                    d[y] = (uint8_t) (d[x] + 1);
                    queue[tail++] = y;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(d);
        return NULL;
    }
    return d;
}

/* Apply path to (p, o); 1 if it ends at solved and every move is valid. */
static int path_solves(int p, int o, const uint8_t *path, int len)
{
    for (int i = 0; i < len; ++i) {
        if (path[i] > 8)
            return 0;
        int f = path[i] / 3, turns = path[i] % 3 + 1;
        for (int t = 0; t < turns; ++t) {
            p = perm_move[f][p];
            o = ori_move[f][o];
        }
    }
    return p == 0 && o == 0;
}

int main(int argc, char **argv)
{
    int only_d11 = argc == 2 && !strcmp(argv[1], "--d11");

    uint8_t *d = full_bfs();
    if (!d) {
        fputs("BFS failed\n", stderr);
        return 1;
    }

    long checked = 0, wrong_len = 0, bad_path = 0;
    uint64_t total_nodes = 0, worst11 = 0;
    uint32_t worst11_state = 0;
    long n11 = 0;
    uint64_t sum11 = 0;
    clock_t start = clock();

    for (uint32_t s = 0; s < STATES; ++s) {
        if (only_d11 && d[s] != 11)
            continue;
        int p = (int) (s / ORIENTATIONS), o = (int) (s % ORIENTATIONS);
        uint8_t path[IDA_MAX_DEPTH];
        uint64_t nodes = 0;
        int len = ida_solve((uint16_t) p, (uint16_t) o, path, &nodes);

        ++checked;
        total_nodes += nodes;
        if (len != d[s]) {
            if (wrong_len < 5)
                fprintf(stderr, "length: p=%d o=%d got %d want %d\n", p, o,
                        len, d[s]);
            ++wrong_len;
        } else if (!path_solves(p, o, path, len)) {
            if (bad_path < 5)
                fprintf(stderr, "path does not solve: p=%d o=%d\n", p, o);
            ++bad_path;
        }
        if (d[s] == 11) {
            ++n11;
            sum11 += nodes;
            if (nodes > worst11) {
                worst11 = nodes;
                worst11_state = s;
            }
        }
    }
    double secs = (double) (clock() - start) / CLOCKS_PER_SEC;

    uint8_t path[IDA_MAX_DEPTH];
    uint64_t ref_nodes = 0;
    int ref_len = ida_solve(720, 0, path, &ref_nodes);

    printf("states checked        %ld (%s)\n", checked,
           only_d11 ? "distance 11 only" : "all");
    printf("wrong length          %ld\n", wrong_len);
    printf("path does not solve   %ld\n", bad_path);
    printf("search time           %.1f s\n", secs);
    printf("expanded, mean        %.1f per state\n",
           checked ? (double) total_nodes / checked : 0.0);
    printf("distance 11           %ld states, mean %.1f, worst %lu "
           "(p=%u o=%u)\n",
           n11, n11 ? (double) sum11 / n11 : 0.0,
           (unsigned long) worst11, worst11_state / ORIENTATIONS,
           worst11_state % ORIENTATIONS);
    printf("21345671111111        %d moves, %lu expanded\n", ref_len,
           (unsigned long) ref_nodes);

    free(d);
    if (wrong_len || bad_path || ref_len != 11) {
        puts("H3 FAILED");
        return 1;
    }
    puts("H3 passed");
    return 0;
}
