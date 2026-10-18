param([string]$ObsRoot = (Join-Path $env:ProgramFiles 'obs-studio'))
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $ObsRoot 'bin\64bit') + ';' + $oldPath
    $dll = Join-Path $root 'dist\nexus-reactive-echo\bin\64bit\nexus-reactive-echo.dll'
    foreach ($test in @('passthrough-test', 'feedback-test', 'silhouette-test', 'controls-test')) {
        $exe = Join-Path $root "build\$test.exe"
        if (!(Test-Path -LiteralPath $exe)) { throw 'Compile primero con python build.py.' }
        & $exe $ObsRoot $dll *> (Join-Path $root "build\$test.log")
        if ($LASTEXITCODE -ne 0) { throw "Prueba fallida: $test. Revise build/$test.log." }
        Write-Output "$test : PASS"
    }
    $fixtures = Join-Path $root 'build\lut-fixtures'
    & (Join-Path $root 'build\cube-test.exe') $fixtures *> (Join-Path $root 'build\cube-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: cube-test. Revise build/cube-test.log.' }
    Write-Output 'cube-test : PASS'
    & (Join-Path $root 'build\color-test.exe') $ObsRoot $dll $fixtures *> (Join-Path $root 'build\color-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: color-test. Revise build/color-test.log.' }
    Write-Output 'color-test : PASS'
    & (Join-Path $root 'build\trail-color-test.exe') $ObsRoot $dll $fixtures *> (Join-Path $root 'build\trail-color-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: trail-color-test. Revise build/trail-color-test.log.' }
    Write-Output 'trail-color-test : PASS'
    & (Join-Path $root 'build\gradient-test.exe') $ObsRoot $dll $fixtures *> (Join-Path $root 'build\gradient-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: gradient-test. Revise build/gradient-test.log.' }
    Write-Output 'gradient-test : PASS'
    & (Join-Path $root 'build/reference-test.exe') $ObsRoot $dll (Join-Path $fixtures 'inverse.cube') *> (Join-Path $root 'build/reference-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: reference-test.' }
    Write-Output 'reference-test : PASS' 
    & (Join-Path $root 'build\preset-test.exe') $ObsRoot $dll $fixtures *> (Join-Path $root 'build\preset-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: preset-test. Revise build/preset-test.log.' }
    Write-Output 'preset-test : PASS'
    & (Join-Path $root 'build\chroma-chain-test.exe') $ObsRoot $dll (Join-Path $root 'build\chroma-input.rgba') *> (Join-Path $root 'build\chroma-chain-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: chroma-chain-test. Revise build/chroma-chain-test.log.' }
    Write-Output 'chroma-chain-test : PASS'
    & (Join-Path $root 'build\audio-test.exe') $ObsRoot $dll *> (Join-Path $root 'build\audio-test.log')
    if ($LASTEXITCODE -ne 0) { throw 'Prueba fallida: audio-test. Revise build/audio-test.log.' }
    Write-Output 'audio-test : PASS'
} finally { $env:PATH = $oldPath }
