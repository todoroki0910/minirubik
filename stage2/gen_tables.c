/*
 * gen_tables.c - host-side generator for the minirubik IDA* tables.
 *
 * Builds every read-only table the on-target search needs and writes them
 * out. Verification lives in test_tables.c, which checks the generated
 * tables.h rather than this program's memory.
 *
 *   perm_move[3][5040]  quarter-turn transition table, permutation coordinate
 *   ori_move [3][729]   quarter-turn transition table, orientation coordinate
 *   perm_dist[5040]     pattern database: moves to place every corner,
 *                       ignoring orientation
 *   ori_dist [729]      pattern database: moves to orient every corner,
 *                       ignoring position
 *
 *   h(p, o) = max(perm_dist[p], ori_dist[o])
 *
 * Output files:
 *   tables.h  C arrays, for the host-side C version of IDA* and the tests
 *   tables.S  .rodata for the RV32I assembly version
 *
 * The cube model (source/twist, quarter_turn, rank/unrank) is taken from
 * solver.c in sysprog21/minirubik.
 *
 * Build:  gcc -O2 -Wall -Wextra -o gen_tables gen_tables.c
 * Run:    gen_tables        (Windows)   ./gen_tables   (Linux/macOS)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Cube model (from solver.c) ------------------------------------- */

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040, /* 7! */
    ORIENTATIONS = 729,  /* 3^6: the 7th twist is fixed by sum % 3 == 0 */
    STATES = PERMUTATIONS * ORIENTATIONS,
    FACES = 3,
    UNSEEN = 0xFF, /* "not reached yet" marker in distance tables */
};

/* p[i]: which cubie sits at position i. o[i]: its twist, 0..2. */
typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[FACES][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6}, /* R */
    {0, 1, 2, 4, 5, 6, 3}, /* B */
    {0, 2, 5, 3, 1, 4, 6}, /* D */
};
/* Twist added to the cubie arriving at each destination. Each row sums to
 * 0 mod 3, so every move preserves sum(o) % 3 == 0. */
static const uint8_t twist[FACES][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* One clockwise quarter turn of `face`. New p depends only on old p and new
 * o only on old o, which is why the two coordinates get separate tables. */
static inline state_t quarter_turn(state_t s, int face)
{
    state_t r;
    for (int i = 0; i < CUBIES; ++i) {
        int from = source[face][i];
        r.p[i] = s.p[from];
        r.o[i] = (uint8_t) ((s.o[from] + twist[face][i]) % 3);
    }
    return r;
}

/*
 * Permutation -> index 0..5039 (Lehmer code / Cantor expansion).
 *
 * The index is the permutation's position in lexicographic order, i.e. how
 * many permutations sort before it.
 *
 * For each position i, n_i ("smaller") counts the entries after position i
 * that are smaller than p[i]. Those are the choices that could have been
 * placed at position i and would sort earlier; each of them leaves
 * (CUBIES-1-i)! ways to arrange the rest. So
 *
 *   index = n_0*6! + n_1*5! + n_2*4! + n_3*3! + n_4*2! + n_5*1! + n_6*0!
 *
 * n_i ranges over 0..CUBIES-1-i, i.e. CUBIES-i possible values, so digit i
 * has radix CUBIES-i (7, 6, 5, 4, 3, 2, 1): a mixed-radix number. It is
 * evaluated with Horner's rule, p = p * radix + digit, like converting a
 * decimal string. Each n_i is multiplied by every later radix, which is
 * where the factorials come from, so no factorial table is needed.
 * The radices multiply to 7! = 5040, so the index is dense: every value in
 * 0..5039 is exactly one permutation.
 *
 * Example: {1,4,2,0,3,5,6} (solved + R)
 *   n = 1,3,1,0,0,0,0
 *   p: 1 -> 9 -> 46 -> 184 -> 552 -> 1104 -> 1104
 *   check: 1*720 + 3*120 + 1*24 = 1104
 */
static inline int perm_index(const state_t *s)
{
    int p = 0;
    for (int i = 0; i < CUBIES; ++i) {
        int smaller = 0; /* n_i: later entries smaller than p[i] */
        for (int j = i + 1; j < CUBIES; ++j)
            smaller += s->p[j] < s->p[i];
        p = p * (CUBIES - i) + smaller; /* Horner step, radix CUBIES - i */
    }
    return p;
}

/*
 * Orientation -> index 0..728.
 *
 * The first six twists, each 0..2, read as a base-3 number:
 *   index = o_0*3^5 + o_1*3^4 + ... + o_5*3^0
 * evaluated with the same Horner step as above, radix 3 at every digit.
 * The seventh twist is not stored: every legal state satisfies
 * sum(o) % 3 == 0, so o_6 is determined by the other six (see unrank).
 * Hence 3^6 = 729 indices, all of them legal.
 *
 * Example: {1,2,0,2,1,0,0} (solved + R)
 *   o: 1 -> 5 -> 15 -> 47 -> 142 -> 426
 */
static inline int ori_index(const state_t *s)
{
    int o = 0;
    for (int i = 0; i < 6; ++i)
        o = o * 3 + s->o[i];
    return o;
}

/*
 * Index -> state: the inverse of perm_index and ori_index.
 *
 * Needed only to build the move tables: quarter_turn works on the p[]/o[]
 * arrays, so each index must be turned back into a concrete state before
 * it can be turned.
 *
 * Permutation: peel off the mixed-radix digits from the most significant.
 *   n_i = perm / (6-i)!, then perm %= (6-i)!
 * This is exact because the later digits together are at most
 * (6-i)! - 1 (sum of m*m! telescopes), so they never reach the next unit.
 * n_i says "take the n_i-th smallest number not used yet", so `available`
 * holds the unused numbers in ascending order, and the chosen one is
 * removed by shifting the tail left.
 *
 * Example: perm = 1104
 *   i  f    q  available        pick
 *   0  720  1  {0,1,2,3,4,5,6}  1
 *   1  120  3  {0,2,3,4,5,6}    4
 *   2  24   1  {0,2,3,5,6}      2
 *   3  6    0  {0,3,5,6}        0
 *   4  2    0  {3,5,6}          3
 *   5  1    0  {5,6}            5
 *   6  1    0  {6}              6     -> {1,4,2,0,3,5,6}
 *
 * Orientation: base-3 digits taken from the least significant end
 * (o_5 first), then o_6 chosen so that the sum of all seven is 0 mod 3.
 */
static inline void unrank(int perm, int ori, state_t *s)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6}; /* unused, ascending */
    int f = 720;                                         /* 6! = weight of n_0 */
    for (int i = 0; i < CUBIES; ++i) {
        int q = perm / f; /* digit n_i */
        perm %= f;        /* what the later digits still encode */
        s->p[i] = available[q];
        for (int j = q; j + 1 < CUBIES - i; ++j) /* remove it from the list */
            available[j] = available[j + 1];
        if (i < CUBIES - 1)
            f /= CUBIES - 1 - i; /* next weight: 6! -> 5! -> ... -> 0! */
    }
    int sum = 0;
    for (int i = 6; i-- > 0;) { /* i = 5, 4, ..., 0 */
        s->o[i] = (uint8_t) (ori % 3);
        sum += s->o[i];
        ori /= 3;
    }
    s->o[6] = (uint8_t) ((3 - sum % 3) % 3); /* restore sum(o) % 3 == 0 */
}


static uint16_t perm_move[FACES][PERMUTATIONS];
static uint16_t ori_move[FACES][ORIENTATIONS];
static uint8_t perm_dist[PERMUTATIONS];
static uint8_t ori_dist[ORIENTATIONS];

/* ---- Tables ----------------------------------------------------------- */

/*
 * Transition tables: index -> state -> quarter turn -> index.
 * Permutation and orientation evolve independently under a quarter turn, so
 * each coordinate gets its own small table. The other coordinate is set to 0
 * while building, since it does not affect the result.
 */
static void build_move_tables(void)
{
    state_t s, t;
    for (int p = 0; p < PERMUTATIONS; ++p) {
        unrank(p, 0, &s);
        for (int f = 0; f < FACES; ++f) {
            t = quarter_turn(s, f);
            perm_move[f][p] = (uint16_t) perm_index(&t);
        }
    }
    for (int o = 0; o < ORIENTATIONS; ++o) {
        unrank(0, o, &s);
        for (int f = 0; f < FACES; ++f) {
            t = quarter_turn(s, f);
            ori_move[f][o] = (uint16_t) ori_index(&t);
        }
    }
}

/*
 * Pattern database: BFS from index 0 (solved) inside the abstract space.
 *   n     number of abstract states (5040 or 729)
 *   move  transition table flattened to 1-D, move[face * n + x] = x after
 *         one quarter turn (so one function serves both table widths)
 *   dist  output, dist[x] = shortest move count from x to 0
 *
 * Half-turn metric: f, f2 and f' each count as one move, so the inner loop
 * applies one, two and three quarter turns and gives all three neighbours
 * distance dist[x] + 1.
 *
 * Every generator has its inverse in the move set (f and f', f2 and f2), so
 * the graph is undirected and "distance from 0" equals "distance to 0".
 */
static void build_dist(int n, const uint16_t *move, uint8_t *dist)
{
    uint16_t queue[PERMUTATIONS]; /* each index is queued at most once */
    int head = 0, tail = 0;

    memset(dist, UNSEEN, (size_t) n);
    dist[0] = 0;
    queue[tail++] = 0;

    while (head < tail) {
        uint16_t x = queue[head++];
        for (int f = 0; f < FACES; ++f) {
            uint16_t y = x;
            for (int turn = 0; turn < 3; ++turn) { /* f, f2, f' */
                y = move[f * n + y];
                if (dist[y] == UNSEEN) {
                    dist[y] = (uint8_t) (dist[x] + 1);
                    queue[tail++] = y;
                }
            }
        }
    }
}

/* ---- Output ----------------------------------------------------------- */

/* Separator before element i of a C initializer, `per_line` values a line. */
static const char *sep(int i, int per_line, const char *indent)
{
    static char buf[16];
    if (i % per_line)
        return ",";
    snprintf(buf, sizeof buf, "%s\n%s", i ? "," : "", indent);
    return buf;
}

static void emit_c_u16(FILE *fp, const char *name, const uint16_t *data,
                       int rows, int cols)
{
    fprintf(fp, "static const uint16_t %s[%d][%d] = {\n", name, rows, cols);
    for (int f = 0; f < rows; ++f) {
        fprintf(fp, "  {");
        for (int i = 0; i < cols; ++i)
            fprintf(fp, "%s%u", sep(i, 16, "   "), data[f * cols + i]);
        fprintf(fp, "},\n");
    }
    fprintf(fp, "};\n\n");
}

static void emit_c_u8(FILE *fp, const char *name, const uint8_t *data, int n)
{
    fprintf(fp, "static const uint8_t %s[%d] = {", name, n);
    for (int i = 0; i < n; ++i)
        fprintf(fp, "%s%u", sep(i, 32, "  "), data[i]);
    fprintf(fp, "\n};\n\n");
}

static int write_header(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return 1;
    }
    fprintf(fp, "/* Generated by gen_tables.c. Do not edit. */\n"
                "#ifndef TABLES_H\n#define TABLES_H\n\n"
                "#include <stdint.h>\n\n");
    emit_c_u16(fp, "perm_move", &perm_move[0][0], FACES, PERMUTATIONS);
    emit_c_u16(fp, "ori_move", &ori_move[0][0], FACES, ORIENTATIONS);
    emit_c_u8(fp, "perm_dist", perm_dist, PERMUTATIONS);
    emit_c_u8(fp, "ori_dist", ori_dist, ORIENTATIONS);
    fprintf(fp, "#endif /* TABLES_H */\n");
    if (fclose(fp)) {
        perror(path);
        return 1;
    }
    return 0;
}

/* One assembler directive per line of 16 values. */
static void emit_asm_block(FILE *fp, const char *label, const char *dir,
                           const void *data, int count, int wide)
{
    fprintf(fp, "%s:\n", label);
    for (int i = 0; i < count; i += 16) {
        int n = count - i < 16 ? count - i : 16;
        fprintf(fp, "    %s ", dir);
        for (int k = 0; k < n; ++k) {
            unsigned v = wide ? ((const uint16_t *) data)[i + k]
                              : ((const uint8_t *) data)[i + k];
            fprintf(fp, "%s%u", k ? ", " : "", v);
        }
        fprintf(fp, "\n");
    }
}

static int write_asm(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return 1;
    }
    fprintf(fp,
            "# Generated by gen_tables.c. Do not edit.\n"
            "# perm_move[f*5040 + p], ori_move[f*729 + o]: halfwords\n"
            "# perm_dist[p], ori_dist[o]: bytes\n"
            "    .section .rodata\n"
            "    .globl perm_move, ori_move, perm_dist, ori_dist\n"
            "    .balign 4\n");
    emit_asm_block(fp, "perm_move", ".half", perm_move,
                   FACES * PERMUTATIONS, 1);
    fprintf(fp, "    .balign 4\n");
    emit_asm_block(fp, "ori_move", ".half", ori_move, FACES * ORIENTATIONS,
                   1);
    emit_asm_block(fp, "perm_dist", ".byte", perm_dist, PERMUTATIONS, 0);
    emit_asm_block(fp, "ori_dist", ".byte", ori_dist, ORIENTATIONS, 0);
    if (fclose(fp)) {
        perror(path);
        return 1;
    }
    return 0;
}

int main(void)
{
    build_move_tables();
    build_dist(PERMUTATIONS, &perm_move[0][0], perm_dist);
    build_dist(ORIENTATIONS, &ori_move[0][0], ori_dist);

    if (write_header("tables.h") || write_asm("tables.S"))
        return 1;

    size_t move = sizeof perm_move + sizeof ori_move;
    size_t dist = sizeof perm_dist + sizeof ori_dist;
    /* %lu rather than %zu: older MinGW runtimes do not support %zu */
    printf("wrote tables.h and tables.S: %lu bytes of read-only data "
           "(move %lu + dist %lu)\n",
           (unsigned long) (move + dist), (unsigned long) move,
           (unsigned long) dist);
    printf("now run test_tables to verify them\n");
    return 0;
}
