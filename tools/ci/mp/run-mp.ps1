<#
T3 multi-process launcher (ADR-0008 section 1, until Gauntlet lands in story tdc-020).

Starts one listen host and N client processes of the Development editor binary in game mode
(UnrealEditor.exe <uproject> -game), each running the StealthTestHarness scenario named by -Scenario.
Every process writes Saved/Harness/<RunId>/{server,client<n>}.jsonl (records) and <role>.log (engine log).
The scenario ends the run: the host exits when it is done and the clients exit when the connection drops.
-TimeoutSeconds is only a failure ceiling (a hung process is killed), never a pass/fail predicate.

Net emulation: -ServerProfile / -ClientProfile apply -PktEmulationProfile=<Name> to that side only
(engine packet simulation acts on the outgoing packets of the process that applies it).

Examples:
  # tdc-017 RTT, profile on the client only, then on both sides
  tools/ci/mp/run-mp.ps1 -Scenario Rtt -Clients 1 -ClientProfile StealthLeak1 -RunId rtt-leak1-client
  tools/ci/mp/run-mp.ps1 -Scenario Rtt -Clients 1 -ServerProfile StealthLeak1 -ClientProfile StealthLeak1 -RunId rtt-leak1-both
  # IVP S1 capture with the ADR-0001 trims, and the negative control
  tools/ci/mp/run-mp.ps1 -Scenario S1 -Clients 3 -ServerProfile StealthLeak2 -ClientProfile StealthLeak2 -Capture -Trims 1 -RunId s1-trims
  tools/ci/mp/run-mp.ps1 -Scenario S1 -Clients 3 -ServerProfile StealthLeak2 -ClientProfile StealthLeak2 -Capture -Trims 17 -RunId s1-negctl
#>
param(
  [Parameter(Mandatory = $true)][string]$Scenario,
  [int]$Clients = 1,
  [string]$RunId = (Get-Date -Format 'yyyyMMdd-HHmmss'),
  [string]$ServerProfile = '',
  [string]$ClientProfile = '',
  [switch]$Capture,
  [int]$Trims = -1,
  [int]$Duration = 30,
  [int]$NpcCount = -1,
  [switch]$Render,
  [string]$ExtraArgs = '',
  [int]$TimeoutSeconds = 420,
  [int]$ServerBootSeconds = 30,
  [string]$CookedExe = '',
  # Session-travel spikes (stories 002-004): options appended to the host's map URL (e.g. the spike game mode),
  # a distinct ?IdentityKey= per client (same format as FStealthConnectionEventHook::IdentityKeyForSlot),
  # a frame cap on every process, packet timing (UStealthTimingIpConnection), and one rendering client.
  [string]$ServerUrlOptions = '',
  [switch]$IdentityKeys,
  [int]$MaxFps = 0,
  [switch]$PacketTiming,
  [int]$RenderClient = 0
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path (Split-Path $PSScriptRoot))
$proj = Join-Path $root 'StealthGame.uproject'
$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
# -CookedExe <path to a staged StealthGame.exe>: run the cooked Development game instead of the editor binary (story tdc-020).
if ($CookedExe) { $exe = $CookedExe }
$out = Join-Path $root "Saved\Harness\$RunId"
New-Item -ItemType Directory -Force $out | Out-Null

$common = @($(if ($CookedExe) { '-windowed' } else { '-game' }), '-unattended', '-nosplash', '-nosound', '-NoVerifyGC', '-log', "-StealthHarnessScenario=$Scenario",
  "-StealthHarnessRunId=$RunId", "-StealthHarnessOut=`"$out`"", "-StealthHarnessClients=$Clients", "-StealthHarnessDuration=$Duration")
if ($Render) { $common += @('-windowed', '-ResX=640', '-ResY=360') } else { $common += '-nullrhi' }
if ($Capture) { $common += @('-StealthNetCapture', '-LogCmds="LogRepProperties VeryVerbose, LogNetTraffic Log, LogRepTraffic Log"') }
if ($Trims -ge 0) { $common += "-StealthHarnessTrims=$Trims" }
if ($NpcCount -ge 0) { $common += "-ini:Game:[/Script/StealthGame.StealthSettings]:NpcCount=$NpcCount" }
if ($ExtraArgs) { $common += $ExtraArgs }
if ($MaxFps -gt 0) { $common += "-StealthHarnessMaxFps=$MaxFps" }
if ($PacketTiming) { $common += '-StealthPacketTiming' }

function Start-Role([string]$url, [string]$role, [string]$profileName, [string[]]$roleArgs, [bool]$roleRenders = $false) {
  $roleCommon = $common
  if ($roleRenders -and -not $Render) { $roleCommon = @($common | Where-Object { $_ -ne '-nullrhi' }) + @('-windowed', '-ResX=640', '-ResY=360') }
  $a = @($(if ($CookedExe) { $null } else { "`"$proj`"" }), $url) + $roleCommon + $roleArgs + @("-abslog=`"$out\$role.log`"")
  if ($profileName) { $a += "-PktEmulationProfile=$profileName" }
  $p = Start-Process -FilePath $exe -ArgumentList ($a -join ' ') -PassThru
  "$role pid=$($p.Id) args: $($a -join ' ')" | Out-File (Join-Path $out 'launch.txt') -Append -Encoding utf8
  return $p
}

$procs = @()
$procs += Start-Role "/Game/StealthGame/Maps/L_Prototype?listen$ServerUrlOptions" 'server' $ServerProfile @('-StealthHarnessRole=server')
$serverLog = Join-Path $out 'server.log'
$deadline = (Get-Date).AddSeconds([Math]::Max($ServerBootSeconds, 10) * 4)
do { Start-Sleep -Seconds 2; $ready = (Test-Path $serverLog) -and (Select-String -Path $serverLog -Pattern 'Listen.*on port|IpNetDriver listening|LogNet: GameNetDriver' -Quiet) } until ($ready -or (Get-Date) -gt $deadline)
Start-Sleep -Seconds 3
for ($i = 1; $i -le $Clients; $i++) {
  $clientUrl = "127.0.0.1?Name=C$i"
  $clientArgs = @('-StealthHarnessRole=client', "-StealthHarnessClient=$i")
  if ($IdentityKeys) { $key = $i.ToString('x8') + '5374656c' + ('0' * 16); $clientUrl += "?IdentityKey=$key"; $clientArgs += "-IdentityKey=$key" }
  $procs += Start-Role $clientUrl "client$i" $ClientProfile $clientArgs ($RenderClient -eq $i)
  Start-Sleep -Seconds 2
}

$stop = (Get-Date).AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $stop) {
  $alive = @($procs | Where-Object { -not $_.HasExited })
  if ($alive.Count -eq 0) { break }
  Start-Sleep -Seconds 2
}
$killed = 0
foreach ($p in $procs) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -Confirm:$false; $killed++ } }
"run $RunId finished; killed after ceiling: $killed; output: $out"
Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize | Out-String
