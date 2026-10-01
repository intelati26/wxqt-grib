<#
.SYNOPSIS
  Starts wxqt and records how the run ended (wxqt-run.log, next to this script).

.DESCRIPTION
  Run it from PowerShell in the portable folder:   .\wxqt.ps1
  If scripts are blocked:   powershell -ExecutionPolicy Bypass -File .\wxqt.ps1

  Every run adds a start line and an end line to wxqt-run.log - the end line has the exit code (0 = closed normally;
  anything else is explained) and, after an abnormal end, the tail of wxqt.log and the console output. The app's own
  log is wxqt.log; with -DebugLog the detailed trace goes to wxqt-debug.log.

.PARAMETER DebugLog
  Switches on the app's verbose debug logging (wxqt-debug.log). Same as running wxqt.exe --debug.

.PARAMETER Console
  Runs the app attached to this window, so anything it prints to the console shows live (it is not captured to a file).

.PARAMETER ResetSettings
  Moves Runnerwxqt.ini aside (kept as Runnerwxqt.ini.bak-<time>) so the app starts with default settings.

.PARAMETER Rest
  Anything else is passed to wxqt.exe unchanged.
#>
param(
    [switch]$DebugLog,
    [switch]$Console,
    [switch]$ResetSettings,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)

$here = $PSScriptRoot
$exe = Join-Path $here 'wxqt.exe'
if (-not (Test-Path $exe)) {
    Write-Error "wxqt.exe not found next to this script ($here)."
    exit 1
}

# the bundled GDAL tools and data (the same environment wxqt.bat sets up)
$gdal = Join-Path $here 'gdal'
if (Test-Path $gdal) {
    $env:PATH = "$gdal;$env:PATH"
    $zip = Join-Path $gdal 'gdal-data.zip'
    if (Test-Path $zip) {
        $env:GDAL_DATA = '/vsizip/' + ($zip -replace '\\', '/')
    } else {
        $env:GDAL_DATA = Join-Path $gdal 'gdal-data'
    }
    $env:PROJ_DATA = Join-Path $gdal 'proj-data'
    $env:PROJ_LIB = $env:PROJ_DATA
}
if ($DebugLog) {
    $env:WXQT_DEBUG = '1'
}

if ($ResetSettings) {
    $ini = Join-Path $here 'Runnerwxqt.ini'
    if (Test-Path $ini) {
        $backup = "$ini.bak-" + (Get-Date -Format 'yyyyMMdd-HHmmss')
        Move-Item $ini $backup
        Write-Host "Settings moved to $backup"
    }
}

$arguments = @()
if ($DebugLog) { $arguments += '--debug' }
if ($Rest) { $arguments += $Rest }

$runLog = Join-Path $here 'wxqt-run.log'
$consoleLog = Join-Path $here 'wxqt-console.log'
$began = Get-Date
$options = @(); if ($DebugLog) { $options += 'debug' }; if ($Console) { $options += 'console' }
"{0} start  options=[{1}] args=[{2}]" -f $began.ToString('yyyy-MM-dd HH:mm:ss'), ($options -join ','), ($arguments -join ' ') | Add-Content $runLog

$start = @{ FilePath = $exe; WorkingDirectory = $here; PassThru = $true; Wait = $true }
if ($arguments.Count -gt 0) { $start.ArgumentList = $arguments }
if ($Console) {
    $start.NoNewWindow = $true
} else {
    $start.RedirectStandardOutput = $consoleLog
    $start.RedirectStandardError = "$consoleLog.err"
}
$process = Start-Process @start
$code = $process.ExitCode
$ended = Get-Date

$hex = '{0:X8}' -f $code
$meaning = switch ($hex) {
    '00000000' { 'closed normally' }
    'C0000005' { 'crashed: access violation (bad memory access)' }
    'C00000FD' { 'crashed: stack overflow' }
    'C0000409' { 'crashed: fast-fail / abort (stack buffer overrun, std::terminate, heap checks)' }
    'C0000374' { 'crashed: heap corruption' }
    'C000001D' { 'crashed: illegal instruction' }
    'C0000094' { 'crashed: integer divide by zero' }
    'C0000135' { 'did not start: a required DLL was not found' }
    'C0000142' { 'did not start: a DLL failed to initialise' }
    'C000007B' { 'did not start: bad image (32/64-bit DLL mix-up)' }
    'C000013A' { 'ended by Ctrl+C' }
    default    { if ($code -eq 0) { 'closed normally' } else { 'ended abnormally' } }
}
"{0} end    exit code {1} (0x{2}) after {3:N0} s - {4}" -f $ended.ToString('yyyy-MM-dd HH:mm:ss'), $code, $hex, ($ended - $began).TotalSeconds, $meaning | Add-Content $runLog

if ($code -ne 0) {
    $appLog = Join-Path $here 'wxqt.log'
    if (Test-Path $appLog) {
        "---- tail of wxqt.log ----" | Add-Content $runLog
        Get-Content $appLog -Tail 40 | Add-Content $runLog
    }
    foreach ($file in @($consoleLog, "$consoleLog.err")) {
        if ((Test-Path $file) -and (Get-Item $file).Length -gt 0) {
            "---- tail of $(Split-Path $file -Leaf) ----" | Add-Content $runLog
            Get-Content $file -Tail 20 | Add-Content $runLog
        }
    }
    "----" | Add-Content $runLog
    Write-Host "wxqt $meaning (exit code 0x$hex). Details: $runLog"
}
exit $code
