# Stage 2: IDA* with pattern databases (C)

| File | Purpose |
|---|---|
| `gen_tables.c` | builds the move tables and the two pattern databases, writes `tables.h` and `tables.S` |
| `tables.h`, `tables.S` | generated tables, for C and for Ripes assembly |
| `test_tables.c` | checks the tables (H1, H2) |
| `ida.h`, `ida.c` | IDA* search: `ida_solve()` |
| `solve.c` | command-line solver, same input format as `solver.c` |
| `h3_check.c` | checks optimality against a full BFS (H3) |

## Run (Windows, MinGW gcc)

```powershell
cd stage2
powershell -ExecutionPolicy Bypass -File run.ps1
powershell -ExecutionPolicy Bypass -File run.ps1 -State 25314672313211
```

`run.ps1` generates the tables, checks them, then solves one cube:

```
> .\solve.exe 21345671111111
R B' D2 R' B R' B' R D2 R B
(11 moves, 38998 nodes expanded)
```

The same steps by hand:

```powershell
gcc -O2 -o gen_tables.exe gen_tables.c;   .\gen_tables.exe
gcc -O2 -o test_tables.exe test_tables.c; .\test_tables.exe
gcc -O2 -o solve.exe solve.c ida.c;       .\solve.exe 21345671111111
```

Input: 14 digits, positions 1..7 (a permutation of 1234567) then twists 1..3.
