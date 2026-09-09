param(
 [string]$Repo='C:/Users/Administrator/Desktop/riors-edge',
 [string]$Editor='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',
 [int]$Port=17777,
 [int]$TimeoutSeconds=70,
 [switch]$Visible,
 [switch]$KeepRunning
)
$ErrorActionPreference='Stop'
$Repo=[IO.Path]::GetFullPath($Repo)
$project=Join-Path $Repo 'riors_edge.uproject'
if(!(Test-Path -LiteralPath $Editor) -or !(Test-Path -LiteralPath $project)){throw 'Editor/project missing'}
$logDirectory=Join-Path $Repo ('Saved/CoopSmoke/'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$hostUserDirectory=[IO.Path]::GetFullPath((Join-Path $logDirectory 'host-user'))
$guestUserDirectory=[IO.Path]::GetFullPath((Join-Path $logDirectory 'guest-user'))
foreach($directory in @($hostUserDirectory,$guestUserDirectory)){
 if(Test-Path -LiteralPath $directory){throw 'Disposable user directory unexpectedly exists'}
 New-Item -ItemType Directory -Path $directory | Out-Null
}
$saveDirectory=Join-Path $Repo 'Saved/SaveGames'
function ReadSharedLog([string]$Path) {
 if(!(Test-Path -LiteralPath $Path)){return ''}
 $stream=$null;$reader=$null
 try {
  $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
  $reader=[IO.StreamReader]::new($stream)
  return $reader.ReadToEnd()
 } catch [IO.IOException] { return '' }
 finally {if($reader){$reader.Dispose()}elseif($stream){$stream.Dispose()}}
}
function ReadSaveHashes {
 $rows=@{};if(Test-Path -LiteralPath $saveDirectory){Get-ChildItem -LiteralPath $saveDirectory -Filter '*.sav' -File|ForEach-Object{$rows[$_.Name]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}};return $rows
}
$before=ReadSaveHashes
$hostLog=Join-Path $logDirectory 'host.log';$guestLog=Join-Path $logDirectory 'guest.log'
$style=if($Visible){'Normal'}else{'Hidden'}
$hostProcess=$null;$guestProcess=$null
try {
 $hostArgs=@(('"'+$project+'"'),'/Game/Breaker/Maps/Lvl_Fernhall?listen?BreakerCoopCombat=1','-game','-windowed','-ResX=960','-ResY=540',('-port='+$Port),'-BreakerCoopCombatTest','-BreakerCoopCombatVerify','-nosplash','-unattended',('-abslog="'+$hostLog+'"'))
 $hostArgs+=('-UserDir="'+$hostUserDirectory+'"')
 $hostProcess=Start-Process -FilePath $Editor -ArgumentList $hostArgs -WindowStyle $style -PassThru
 $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
 do {
  Start-Sleep -Milliseconds 250
  if($hostProcess.HasExited){throw 'Host exited before listen socket became ready'}
  $hostText=if(Test-Path -LiteralPath $hostLog){(ReadSharedLog $hostLog)}else{''}
 } until($hostText -match 'listening on port' -or [DateTime]::UtcNow -ge $deadline)
 if($hostText -notmatch 'listening on port'){throw 'Host listen readiness timed out; inspect host.log'}
 $guestArgs=@(('"'+$project+'"'),("127.0.0.1:$Port"+'?BreakerCoopCombat=1'),'-game','-windowed','-ResX=960','-ResY=540','-BreakerCoopCombatTest','-BreakerCoopCombatVerify','-nosplash','-unattended',('-abslog="'+$guestLog+'"'))
 $guestArgs+=('-UserDir="'+$guestUserDirectory+'"')
 $guestProcess=Start-Process -FilePath $Editor -ArgumentList $guestArgs -WindowStyle $style -PassThru
 $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
 do {
  Start-Sleep -Milliseconds 250
  if($hostProcess.HasExited -or $guestProcess.HasExited){throw 'One smoke process exited unexpectedly'}
  $hostText=(ReadSharedLog $hostLog)
  $ids=@([regex]::Matches($hostText,'\[CoopTest\] isolated profile=([A-Fa-f0-9-]+) authority=1')|ForEach-Object{$_.Groups[1].Value}|Select-Object -Unique)
  $guestText=if(Test-Path -LiteralPath $guestLog){(ReadSharedLog $guestLog)}else{''}
 } until(($ids.Count -ge 2 -and $guestText -match '\[CoopTest\].*(authority=0|received server profile)' -and $guestText -match '\[CoopTest\] local environment ready:' -and $hostText -match '\[CoopVerify\] server target-death' -and $guestText -match '\[CoopVerify\] client replicated-death' -and $hostText -match '\[CoopVerify\] server guest-respawn' -and $guestText -match '\[CoopVerify\] client guest-revived-presentation') -or [DateTime]::UtcNow -ge $deadline)
 if($ids.Count -lt 2){throw 'Server never initialized two distinct test profiles'}
 if($guestText -notmatch '\[CoopTest\].*(authority=0|received server profile)'){throw 'Guest has no initialized test profile'}
 if($guestText -notmatch '\[CoopTest\] local environment ready:'){throw 'Guest never built its Fernhall geometry and lighting'}
 foreach($pattern in @('server guest-movement','server guest-weapon-hit','server target-death')){
  if($hostText -notmatch ('\[CoopVerify\] '+$pattern)){throw ('Missing authoritative observation: '+$pattern)}
 }
 foreach($pattern in @('client owner-slot-definitions','client distinct-player-presence','client initial-health','client fire-request','client replicated-health','client replicated-death')){
  if($guestText -notmatch ('\[CoopVerify\] '+$pattern)){throw ('Missing guest observation: '+$pattern)}
 }
 foreach($pattern in @('server guest-lethal profile=[A-Fa-f0-9-]+ health=0\.00 death-events=1 awaiting=1','server guest-respawn profile=[A-Fa-f0-9-]+ health=[1-9][0-9]*\.\d+ death-events=1 restore-events=1')){
  if($hostText -notmatch ('\[CoopVerify\] '+$pattern)){throw ('Missing authoritative guest death/respawn: '+$pattern)}
 }
 foreach($pattern in @('client guest-dead-presentation profile=[A-Fa-f0-9-]+ health=0\.00 awaiting=1 input=0 death-events=0 restore-events=0','client guest-revived-presentation profile=[A-Fa-f0-9-]+ health=[1-9][0-9]*\.\d+ awaiting=0 input=1 death-events=0 restore-events=0')){
  if($guestText -notmatch ('\[CoopVerify\] '+$pattern)){throw ('Missing guest replicated death/revival presentation: '+$pattern)}
 }
 $requestProfile=[regex]::Match($guestText,'client fire-request profile=([A-Fa-f0-9-]+)').Groups[1].Value
 $slotProfile=[regex]::Match($guestText,'client owner-slot-definitions profile=([A-Fa-f0-9-]+)').Groups[1].Value
 if(!$slotProfile -or $slotProfile -ne $requestProfile){throw 'Owner slot metadata was not observed for the guest that actually fired'}
 $hitProfiles=@([regex]::Matches($hostText,'server guest-weapon-hit profile=([A-Fa-f0-9-]+)')|ForEach-Object{$_.Groups[1].Value}|Select-Object -Unique)
 if(!$requestProfile -or $hitProfiles -notcontains $requestProfile){throw 'Client request identity does not match server-attributed weapon damage'}
 foreach($observation in @('server guest-lethal','server guest-respawn','client guest-dead-presentation','client guest-revived-presentation')){
  $text=if($observation.StartsWith('server')){$hostText}else{$guestText}
  $profile=[regex]::Match($text,($observation+' profile=([A-Fa-f0-9-]+)')).Groups[1].Value
  if($profile -ne $requestProfile){throw ('Death/respawn identity mismatch: '+$observation)}
 }
 # Displacement is observed during guest movement input; this does not prove
 # every centimetre came from input rather than knockback or another force.
 [pscustomobject]@{ServerProfiles=$ids;HostProcessId=$hostProcess.Id;GuestProcessId=$guestProcess.Id;Logs=$logDirectory;HostUserDir=$hostUserDirectory;GuestUserDir=$guestUserDirectory;Scope='Observed remote displacement during input, guest fire RPC, server weapon damage/death and client replicated health/death; actual guest death/server timer respawn/client presentation revival with no client gameplay delegates; displacement cause is not isolated'}|ConvertTo-Json
}
finally {
 if(!$KeepRunning){foreach($ownedProcess in @($guestProcess,$hostProcess)){if($ownedProcess -and !$ownedProcess.HasExited){Stop-Process -Id $ownedProcess.Id -Force}}}
 $after=ReadSaveHashes
 if((@($before.Keys|Sort-Object|ForEach-Object{"$_=$($before[$_])"}) -join "`n") -ne (@($after.Keys|Sort-Object|ForEach-Object{"$_=$($after[$_])"}) -join "`n")){throw 'SaveGames changed during isolated smoke; inspect before using real profiles'}
 foreach($directory in @($hostUserDirectory,$guestUserDirectory)){
  if(@(Get-ChildItem -LiteralPath $directory -Filter '*.sav' -Recurse -File).Count){throw ('Nonsaving test wrote a disposable save: '+$directory)}
 }
}
