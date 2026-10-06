# Stage 3: IDA* without recursion (C)

| File | Purpose |
|---|---|
| `ida_iterative.c` | same search as `stage2/ida.c`, rewritten as one loop over an explicit stack, with no multiply and no calls |
| `build.ps1` | compiles stage 2 and stage 3 for RV32I with gcc -O2 |
| `elf/` | the compiled programs, ready for Ripes |

`ida_iterative.c` keeps the `ida_solve()` interface, so stage 2's `solve.c` works with it unchanged:

```powershell
gcc -O2 -I../stage2 -o solve3.exe ../stage2/solve.c ida_iterative.c
.\solve3.exe 21345671111111
```

## Run in Ripes

`ida.c` and `ida_iterative.c` end with a `main` inside `#ifdef RIPES`. It solves one state and exits with the solution length, so Ripes should report exit code 11.

```powershell
cd stage3
powershell -ExecutionPolicy Bypass -File build.ps1      # needs riscv-none-elf-gcc
Ripes.exe --mode cli --src elf\stage3_worst.elf -t elf --proc RV32_ISS --iret
```

| ELF | Code | State |
|---|---|---|
| `stage2_worst.elf` | `stage2/ida.c` | `54721631111111` (worst distance-11 state) |
| `stage2_ref.elf` | `stage2/ida.c` | `21345671111111` |
| `stage3_worst.elf` | `ida_iterative.c` | `54721631111111` |
| `stage3_ref.elf` | `ida_iterative.c` | `21345671111111` |
