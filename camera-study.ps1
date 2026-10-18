param(
    [Parameter(Mandatory=$true)][string]$InputVideo,
    [string]$ObsRoot = (Join-Path $env:ProgramFiles 'obs-studio'),
    [switch]$Margin,
    [switch]$Bands,
    [switch]$Reference,
    [string]$BaseLut = '',
    [string]$ReactiveLut = ''
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$oldPath = $env:PATH
$name = if ($Margin) { 'camera-margin' } else { 'camera-study' }
if ($Bands) { $name += '-bands' }
if ($Reference) { $name += '-reference' }
$out = Join-Path $root "tests\evidence\$name"
$sequence = Join-Path $root "build\$name-input.rgba"
$prefix = Join-Path $root "build\$name-output"
if (!(Test-Path -LiteralPath $InputVideo -PathType Leaf)) { throw 'No existe el vídeo de entrada.' }
if ($Reference -and (!(Test-Path -LiteralPath $BaseLut -PathType Leaf) -or !(Test-Path -LiteralPath $ReactiveLut -PathType Leaf))) { throw 'Para Reference, indica BaseLut y ReactiveLut con tus propios archivos.' }
if (!(Test-Path -LiteralPath (Join-Path $root 'build\camera-study.exe'))) { throw 'Compile primero con build.py.' }
New-Item -ItemType Directory -Path $out -Force | Out-Null
$scale = if ($Margin) { 'scale=384:216,pad=512:288:64:36:color=0x008080' } else { 'scale=512:288' }
try {
    & ffmpeg -v error -ss 3 -i $InputVideo -an -frames:v 120 -vf $scale -pix_fmt rgba -f rawvideo -y $sequence
    if ($LASTEXITCODE -ne 0) { throw 'Falló la extracción de cámara.' }
    $env:PATH = (Join-Path $ObsRoot 'bin\64bit') + ';' + $oldPath
    $renderArgs = @($ObsRoot,(Join-Path $root 'dist\nexus-reactive-echo\bin\64bit\nexus-reactive-echo.dll'),$sequence,$prefix)
    if ($Bands -and $Reference) { throw 'Elige Bands o Reference.' }
    if ($Bands) { $renderArgs += 'bands' }
    if ($Reference) {
        $audioFile = Join-Path $root 'build\camera-audio.f32'
        & ffmpeg -v error -ss 3 -i $InputVideo -t 2 -vn -ac 1 -ar 48000 -f f32le -y $audioFile
        if ($LASTEXITCODE -ne 0) { throw 'Falló la extracción de audio.' }
        $renderArgs += @('reference',$audioFile,$BaseLut,$ReactiveLut)
    }
    & (Join-Path $root 'build\camera-study.exe') @renderArgs *> (Join-Path $out 'render.log')
    if ($LASTEXITCODE -ne 0) { throw 'Falló el render OBS; revise render.log.' }
    $ffArgs = @('-v','error')
    foreach ($i in 0..2) {
        $ffArgs += @('-f','rawvideo','-pixel_format','rgba','-video_size','512x288','-framerate','60','-i',"$prefix-$i.rgba")
    }
    if ($Reference) { $ffArgs += @('-ss','3','-t','2','-i',$InputVideo) }
    $ffArgs += @('-filter_complex','[0:v]format=rgb24[a];[1:v]format=rgb24[b];[2:v]format=rgb24[c];[a][b][c]hstack=inputs=3,format=yuv420p[v]',
        '-map','[v]','-c:v','libx264','-crf','18','-an','-y',(Join-Path $out 'comparison.mp4'))
    if ($Reference) { $ffArgs = @($ffArgs | Where-Object { $_ -ne '-an' }); $ffArgs = $ffArgs[0..($ffArgs.Length-2)] + @('-map','3:a?','-c:a','aac','-shortest',$ffArgs[-1]) }
    & ffmpeg @ffArgs
    if ($LASTEXITCODE -ne 0) { throw 'Falló la exportación de comparación.' }
    & ffmpeg -v error -ss 1.5 -i (Join-Path $out 'comparison.mp4') -frames:v 1 -y (Join-Path $out 'comparison.png')
    if ($LASTEXITCODE -ne 0) { throw 'Falló la captura de comparación.' }
    [ordered]@{ input=$InputVideo; start_seconds=3; frames=120; width=512; height=288;
        input_scale=$(if ($Margin) {0.75} else {1}); filter=$scale;
        modes=$(if ($Reference) { @('Plugin anterior','Recursion con degradado','Recursion con LUT de la escena') } elseif ($Bands) { @('Degradado suave','3 bandas','6 bandas') } else { @('Chroma sin eco','Expansion 1.015','Expansion 1.04') });
        bands_study=[bool]$Bands; reference_study=[bool]$Reference;
        feedback=$(if($Reference){@(0.94,0.8,0.8)}else{0.94}); audio_enabled=$(if($Reference){@($false,$true,$true)}else{$false});
        dll_sha256=(Get-FileHash (Join-Path $root 'dist\nexus-reactive-echo\bin\64bit\nexus-reactive-echo.dll')).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'settings.json') -Encoding utf8
    Write-Output (Join-Path $out 'comparison.mp4')
} finally { $env:PATH = $oldPath }
