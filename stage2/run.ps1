# run.ps1 -- build and run stage 2 on Windows (MinGW gcc).
#   powershell -ExecutionPolicy Bypass -File run.ps1
# Optional: -State 25314672313211 to solve another cube.
param([string]$State = "21345671111111")

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Step($title, $cmd) {
    Write-Host "`n== $title" -ForegroundColor Cyan
    Write-Host "> $cmd"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) { throw "failed: $cmd" }
}

$CF = "-O2 -Wall -Wextra"

# 1. Generate the pattern databases (tables.h for C, tables.S for Ripes).
Step "build gen_tables" "gcc $CF -o gen_tables.exe gen_tables.c"
Step "generate tables"  ".\gen_tables.exe"

# 2. Check the tables (H1, H2).
Step "build test_tables" "gcc $CF -o test_tables.exe test_tables.c"
Step "check tables"      ".\test_tables.exe"

# 3. Solve one cube with IDA*.
Step "build solve" "gcc $CF -o solve.exe solve.c ida.c"
Step "solve $State" ".\solve.exe $State"
