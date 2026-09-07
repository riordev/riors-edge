param(
    [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [switch]$ActTwo,
    [switch]$Photos
)
$ErrorActionPreference = 'Stop'
$repoDirectory = Split-Path -Parent $PSScriptRoot
$probeDirectory = Join-Path $repoDirectory ('Saved\LoopProbe\' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $probeDirectory
$probeLog = Join-Path $probeDirectory 'loop.log'
$projectFile = Join-Path $repoDirectory 'riors_edge.uproject'
# A unique engine user directory isolates character, roster and account saves.
# Leave its files intact for diagnosis. This is a map/persistence integration
# check with accelerated kills, not a balance or hands-on interaction test.
$probeOptions = @()
if ($ActTwo) { $probeOptions += '-BreakerActTwoLoop' }
if ($Photos) { $probeOptions += @('-BreakerActTwoPhotos', '-windowed', '-ResX=1920', '-ResY=1080') }
else { $probeOptions += '-nullrhi' }
& $Editor $projectFile -game -unattended -nop4 -nosplash -BreakerAutoPlay=Anchor -BreakerLoopProbe @probeOptions "-UserDir=$probeDirectory" "-abslog=$probeLog"
$probeExit = $LASTEXITCODE
if ($probeExit -ne 0) { throw "Rift loop probe exited $probeExit. Log: $probeLog" }
$probeText = [IO.File]::ReadAllText($probeLog)
if ($probeText -notmatch '\[LoopProbe\] PASS:') { throw "Rift loop probe did not complete. Log: $probeLog" }
($probeText -split '\r?\n' | Where-Object { $_ -match '\[LoopProbe\]' })
Write-Output "Evidence: $probeLog"
