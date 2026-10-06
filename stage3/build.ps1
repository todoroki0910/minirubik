# build.ps1 -- compile stage 2 and stage 3 IDA* for RV32I, to run in Ripes.
# Needs the xPack RISC-V gcc (riscv-none-elf-gcc) on Path.
#   powershell -ExecutionPolicy Bypass -File build.ps1
#
# Each ELF solves one state and exits with the solution length (ecall 93).
# The state is passed as its ranks: IN_P (permutation), IN_O (twists).

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)
New-Item -ItemType Directory -Force elf | Out-Null

$CC = "riscv-none-elf-gcc"
$F  = "-march=rv32i -mabi=ilp32 -O2 -mno-relax -ffreestanding -nostdlib " +
      "-fno-builtin -Wl,--no-relax -Wl,-Ttext=0x10000 -Wl,-e,main " +
      "-DRIPES -I../stage2"

$impls  = [ordered]@{ stage2 = "../stage2/ida.c"; stage3 = "ida_iterative.c" }
$states = [ordered]@{
    worst = "-DIN_P=3343 -DIN_O=0"   # 54721631111111, worst distance-11 state
    ref   = "-DIN_P=720 -DIN_O=0"    # 21345671111111, reference vector
}

foreach ($n in $impls.Keys) {
    foreach ($s in $states.Keys) {
        $cmd = "$CC $F $($states[$s]) -o elf/${n}_$s.elf $($impls[$n])"
        Write-Host "> $cmd"
        Invoke-Expression $cmd
        if ($LASTEXITCODE -ne 0) { throw "failed: $cmd" }
    }
}
Write-Host "`nRun one in Ripes:"
Write-Host "  Ripes.exe --mode cli --src elf\stage3_worst.elf -t elf --proc RV32_ISS --iret"
