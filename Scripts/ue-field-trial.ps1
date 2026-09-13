param(
    [string]$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$ResumeDirectory
)
$ErrorActionPreference = 'Stop'
$trialRepo = Split-Path -Parent $PSScriptRoot
$trialRoot = [IO.Path]::GetFullPath((Join-Path $trialRepo 'Saved\FieldTrialRuns'))
if ($ResumeDirectory) {
    $trialDirectory = [IO.Path]::GetFullPath($ResumeDirectory)
    if (-not $trialDirectory.StartsWith($trialRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $trialDirectory -PathType Container)) {
        throw 'ResumeDirectory must be an existing run inside Saved\FieldTrialRuns.'
    }
} else {
    $trialDirectory = Join-Path $trialRoot ([Guid]::NewGuid().ToString('N'))
    $null = New-Item -ItemType Directory -Path $trialDirectory
}
$trialLog = Join-Path $trialDirectory ('play-' + [Guid]::NewGuid().ToString('N') + '.log')
Write-Output "Run directory: $trialDirectory"
Write-Output 'Play normally: create a Caster, follow the opening objectives, clear the entry rift, choose a reward and return to the Anchor. Use no quest-state shortcuts.'
Write-Output 'Quit and rerun with -ResumeDirectory pointing to this directory to check save/resume.'
& $Editor (Join-Path $trialRepo 'riors_edge.uproject') -game -windowed -ResX=1920 -ResY=1080 -nop4 -nosplash -BreakerFieldTrial "-UserDir=$trialDirectory" "-abslog=$trialLog"
if ($LASTEXITCODE -ne 0) { throw "Game exited $LASTEXITCODE. Log: $trialLog" }
Write-Output "Recordings are beneath $trialDirectory; search for FieldTrials/*.jsonl. Log: $trialLog"
