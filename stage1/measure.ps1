# measure.ps1 -- run one assembly program on the Ripes CLI and record its cost
#
# For each run it launches Ripes in CLI mode and reports:
#   - iret and exectime (ms)  : from Ripes' --iret and --exectime
#   - peak working set (bytes): Windows' PeakWorkingSet64 of the Ripes
#                               process, sampled every 20 ms
#   - process seconds         : whole-process wall clock, startup included
#
# Each run writes report_<name>_<proc>_<run>.txt and appends one line
# (src, proc, run, peak, seconds) to results.csv in the current folder.
#
# Usage:
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\measure.ps1 -Ripes <path\to\Ripes.exe> -Src ips_loop.s -Proc RV32_ISS -Runs 3
#
# Measurement 1 (host bytes per guest byte) uses mem_store.s on RV32_ISS
# with several N; measurement 2 (instructions per second) uses ips_loop.s
# on RV32_ISS and RV32_5S.

param(
    [Parameter(Mandatory = $true)][string]$Ripes,
    [Parameter(Mandatory = $true)][string]$Src,
    [string]$Proc = "RV32_ISS",
    [int]$Runs = 3
)

$srcPath = (Resolve-Path $Src).Path
$csv = Join-Path (Get-Location) "results.csv"
if (-not (Test-Path $csv)) {
    "src,proc,run,peak_working_set_bytes,seconds" | Out-File $csv -Encoding ascii
}

for ($i = 1; $i -le $Runs; $i++) {
    $name = [IO.Path]::GetFileNameWithoutExtension($Src)
    $report = Join-Path (Get-Location) ("report_{0}_{1}_{2}.txt" -f $name, $Proc, $i)
    $argList = "--mode cli --src `"$srcPath`" -t asm --proc $Proc --iret --exectime --runinfo --output `"$report`""

    # Start Ripes and sample its peak working set until it exits.
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $Ripes -ArgumentList $argList -PassThru -WindowStyle Hidden
    $peak = 0
    while (-not $p.HasExited) {
        try {
            $p.Refresh()
            if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 }
        } catch {}
        Start-Sleep -Milliseconds 20
    }
    $sw.Stop()
    $sec = $sw.Elapsed.TotalSeconds

    "{0}  {1}  run {2}: peak = {3:N0} bytes, time = {4:N3} s" -f $Src, $Proc, $i, $peak, $sec
    if (Test-Path $report) { Get-Content $report | ForEach-Object { "    $_" } }
    else { "    (no report file -- check the Ripes path and processor name)" }

    "{0},{1},{2},{3},{4:F3}" -f $Src, $Proc, $i, $peak, $sec | Out-File $csv -Append -Encoding ascii
}
