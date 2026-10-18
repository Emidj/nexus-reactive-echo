param(
    [string]$ObsRoot = (Join-Path $env:ProgramFiles 'obs-studio'),
    [string]$DllPath = '',
    [string]$OutputPrefix = 'build\benchmark',
    [ValidateRange(0,3600)][int]$SoakSeconds = 0
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (!$DllPath) { $DllPath = Join-Path $root 'dist\nexus-reactive-echo\bin\64bit\nexus-reactive-echo.dll' }
if (![IO.Path]::IsPathRooted($OutputPrefix)) { $OutputPrefix = Join-Path $root $OutputPrefix }
$exe = Join-Path $root 'build\benchmark.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Compile primero con python build.py.' }
if (!(Test-Path -LiteralPath $DllPath)) { throw 'No existe la DLL indicada.' }
$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $ObsRoot 'bin\64bit') + ';' + $oldPath
    $extra = @()
    if ($SoakSeconds -gt 0) { $extra = @([string]$SoakSeconds) }
    & $exe $ObsRoot $DllPath (Join-Path $root 'build\lut-fixtures') "$OutputPrefix.csv" @extra *> "$OutputPrefix.log"
    if ($LASTEXITCODE -ne 0) { throw "Prueba fallida. Revise $OutputPrefix.log" }
    Write-Output "PASS: $OutputPrefix.log"
    if ($SoakSeconds -eq 0) { Write-Output "Medidas GPU y CPU: $OutputPrefix.csv" }
} finally { $env:PATH = $oldPath }
