<#
.SYNOPSIS
Runs excavation verification in isolated listen-server processes.
.DESCRIPTION
Run -Players 1 and -Players 4 in separate invocations. The four-player run attempts ten simultaneous digs and a same-fixture reconnect.
A command acknowledgement means the RPC was sent; terrain, collision and support states are checked separately.
The trial fixture, limitations, process arguments, snapshots, and failure details are retained in result.json.
.EXAMPLE
powershell -NoProfile -File scripts/validation/Invoke-ExcavationValidation.ps1 -GameExe D:/Package/DIGETHER.exe -MapPackage /Game/Prototype/Maps/L_Prototype -Players 4
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$MapPackage,
    [Alias("Project")][string]$ProjectPath,
    [ValidateSet(1,4)][int]$Players = 4,
    [string]$OutputRoot,
    [ValidateRange(1024, 65535)][int]$Port = 0,
    [ValidateRange(60, 570)][int]$TimeoutSeconds = 570,
    [switch]$Rendered,
    [double]$PositionToleranceCm = 2.0,
    [double]$MoveDurationSeconds = 0.8
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if (-not $MapPackage.StartsWith('/Game/')) { throw 'MapPackage must be a /Game/ package path.' }
$MapUrl = "${MapPackage}?listen"
$LoopbackHost = '127.0.0.1'
$ReadyStreak = 3
$SettleStreak = 5
$SettleEpsilonCm = 0.5
$MinDisplacementCm = 20.0
$StaleSeconds = 5.0
$PollMs = 100
$QuitWaitSeconds = 20

$script:Procs = [ordered]@{}
$script:Steps = New-Object System.Collections.ArrayList
$script:Failures = New-Object System.Collections.ArrayList
$script:Sequences = @{}
$script:RunDir = $null
$script:ExePath = $null
$script:ProbeMaxSeconds = 0
$script:GlobalDeadline = $null
$script:Result = [ordered]@{}
$script:ScreenshotRecords = New-Object System.Collections.ArrayList
$script:RouteDiagnostics = New-Object System.Collections.ArrayList
$script:StateReadStats = @{}
$script:WaypointProgress = New-Object System.Collections.ArrayList
$script:RejectionProven = $false

function ConvertTo-Utc($Value) {
    if ($Value -is [datetime]) { return $Value.ToUniversalTime() }
    return [datetime]::Parse([string]$Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
}

function Get-UtcNow { return (Get-Date).ToUniversalTime().ToString('o') }

function ConvertTo-FiniteDouble($Value) {
    if ($null -eq $Value -or $Value -is [bool] -or $Value -is [string]) { return $null }
    try { $d = [double]$Value } catch { return $null }
    if ([double]::IsNaN($d) -or [double]::IsInfinity($d)) { return $null }
    return $d
}

function Save-Json($Object, [string]$Path) {
    $json = $Object | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($Path, $json, (New-Object Text.UTF8Encoding($false)))
}

function Save-Result {
    if (-not $script:RunDir) { return }
    $script:Result['steps'] = @($script:Steps)
    $script:Result['stateReadStats'] = $script:StateReadStats
    $script:Result['waypointProgress'] = @($script:WaypointProgress)
    $script:Result['screenshots'] = @($script:ScreenshotRecords)
    $script:Result['routeDiagnostics'] = @($script:RouteDiagnostics)
    $procList = @()
    foreach ($entry in $script:Procs.Values) {
        $exited = $entry.Process.HasExited
        $procList += [ordered]@{
            role = $entry.Role; pid = $entry.Pid; startedUtc = $entry.StartedUtc
            probeDir = $entry.Dir; log = $entry.Log; arguments = $entry.Arguments
            exited = $exited; exitCode = $(if ($exited) { $entry.Process.ExitCode } else { $null })
            exitedBeforeCleanup = $entry.ExitedBeforeCleanup; quitSequence = $entry.QuitSequence
            killedByRunner = $entry.KilledByRunner; cleanupNote = $entry.CleanupNote
        }
    }
    $script:Result['processes'] = $procList
    Save-Json $script:Result (Join-Path $script:RunDir 'result.json')
}

function New-Deadline([double]$Seconds) {
    $d = (Get-Date).AddSeconds($Seconds)
    if ($d -gt $script:GlobalDeadline) { return $script:GlobalDeadline }
    return $d
}

function Test-UdpPortFree([int]$Candidate) {
    foreach ($address in @([System.Net.IPAddress]::Loopback, [System.Net.IPAddress]::Any)) {
        try {
            $client = New-Object System.Net.Sockets.UdpClient((New-Object System.Net.IPEndPoint($address, $Candidate)))
            $client.Close()
        } catch { return $false }
    }
    return $true
}

function Get-ProcessExePath([System.Diagnostics.Process]$Process) {
    try { return $Process.MainModule.FileName } catch { }
    $cim = Get-CimInstance Win32_Process -Filter "ProcessId = $($Process.Id)" -ErrorAction SilentlyContinue
    if ($cim -and $cim.ExecutablePath) { return [string]$cim.ExecutablePath }
    return $null
}

# Kills only the root process this runner started, after confirming the cached handle's start time and exact exe path.
function Stop-OwnedRoot($Entry) {
    $p = $Entry.Process
    try {
        if ($p.HasExited) { return }
        $p.Refresh()
        if ($null -ne $Entry.StartTimeUtc) {
            $delta = ($p.StartTime.ToUniversalTime() - $Entry.StartTimeUtc).Duration().TotalMilliseconds
            if ($delta -gt 1) { $Entry.CleanupNote = "refused to kill: start time differs by $delta ms"; return }
        }
        $path = Get-ProcessExePath $p
        if ($null -eq $path -or -not [string]::Equals($path, $script:ExePath, [StringComparison]::OrdinalIgnoreCase)) {
            $Entry.CleanupNote = "refused to kill: exe path '$path' does not match '$script:ExePath'"
            return
        }
        $Entry.KilledByRunner = $true
        $p.Kill()
        if (-not $p.WaitForExit(10000)) { $Entry.CleanupNote = 'Kill issued but process did not exit within 10 s' }
    } catch {
        $Entry.CleanupNote = "kill failed: $($_.Exception.Message)"
    }
}

function Get-NextSequence([string]$Role) {
    if (-not $script:Sequences.ContainsKey($Role)) { $script:Sequences[$Role] = [long]0 }
    $script:Sequences[$Role] = [long]$script:Sequences[$Role] + 1
    return [long]$script:Sequences[$Role]
}

function Send-Command([string]$Role, [string]$Name, $Extra = $null) {
    $seq = Get-NextSequence $Role
    $command = [ordered]@{ sequenceId = $seq; command = $Name }
    if ($null -ne $Extra) { foreach ($key in $Extra.Keys) { $command[$key] = $Extra[$key] } }
    $dir = $script:Procs[$Role].Dir
    $target = Join-Path $dir 'control.json'
    $tmp = Join-Path $dir 'control.json.tmp'
    [IO.File]::WriteAllText($tmp, ($command | ConvertTo-Json -Depth 5 -Compress), (New-Object Text.UTF8Encoding($false)))
    $published = $false
    $lastError = $null
    for ($attempt = 0; $attempt -lt 40 -and -not $published; $attempt++) {
        try {
            if (Test-Path -LiteralPath $target) { [IO.File]::Replace($tmp, $target, [NullString]::Value) } else { [IO.File]::Move($tmp, $target) }
            $published = $true
        } catch { $lastError = $_.Exception.Message; Start-Sleep -Milliseconds 25 }
    }
    if (-not $published) { throw "Could not publish control.json for role=${Role} command=${Name} seq=${seq}: ${lastError}" }
    return $seq
}

function Start-Probe([string]$Role, [string]$FirstArgument, [bool]$IsHost) {
    $dir = Join-Path $script:RunDir $Role
    if (Test-Path -LiteralPath $dir) { throw "Probe directory already exists: $dir" }
    New-Item -ItemType Directory -Path $dir | Out-Null
    $userDir = Join-Path $dir 'UserDir'
    $log = Join-Path $dir "$Role.log"
    $argList = @()
    if ($ProjectPath) { $argList += "`"$ProjectPath`""; $argList += '-game' }
    $fixtureId = $(if ($Role -eq 'Client3Reconnect') { 'Client3' } else { $Role })
    $argList += "${FirstArgument}?PrototypePlayerId=$fixtureId"
    if ($IsHost) { $argList += @("-port=$Port", "-multihome=$LoopbackHost") }
    if ($Rendered) { $argList += @('-RenderOffScreen','-windowed','-ResX=1280','-ResY=720') } else { $argList += '-NullRHI' }
    $argList += @(
        '-nosteam', '-unattended', '-nop4', '-nosplash', '-nosound', '-forcelogflush',
        "-UserDir=`"$userDir`"", "-abslog=`"$log`"",
        "-PrototypeSaveSlot=$script:RunId", "-DigetherPrototypeProbe=$Role", "-DigetherPrototypeProbeDir=`"$dir`"",
        "-DigetherPrototypeProbeMaxSeconds=$script:ProbeMaxSeconds"
    )
    $argLine = $argList -join ' '
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $script:ExePath
    $psi.Arguments = $argLine
    $psi.WorkingDirectory = [IO.Path]::GetDirectoryName($script:ExePath)
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $proc = [System.Diagnostics.Process]::Start($psi)
    $null = $proc.Handle
    $startTime = $null
    try { $startTime = $proc.StartTime.ToUniversalTime() } catch { }
    $script:Procs[$Role] = [pscustomobject]@{
        Role = $Role; Process = $proc; Pid = $proc.Id; Dir = $dir; Log = $log; Arguments = $argLine
        StartedUtc = Get-UtcNow; StartTimeUtc = $startTime; ExpectedExit = $false; KilledByRunner = $false
        ExitedBeforeCleanup = $null; QuitSequence = $null; CleanupNote = $null; AllowNonZeroExit = $false
    }
    Write-Host "[start] $Role pid=$($proc.Id)"
}

function Assert-ProcessesAlive {
    foreach ($entry in $script:Procs.Values) {
        if (-not $entry.ExpectedExit -and $entry.Process.HasExited) {
            throw "Process $($entry.Role) (pid $($entry.Pid)) exited unexpectedly with code $($entry.Process.ExitCode). Log: $($entry.Log)"
        }
    }
}

# State files are atomically replaced. A missing/locked/invalid snapshot is retried
# briefly; persistent failures remain failures and every retry is recorded in result.json.
function Record-StateReadIssue([string]$Role, [string]$Reason) {
    if(-not $script:StateReadStats.ContainsKey($Role)) {
        $script:StateReadStats[$Role]=[ordered]@{retryCount=0; reasons=@{}; lastReason=''; lastUtc=''}
    }
    $entry=$script:StateReadStats[$Role]
    $entry.retryCount++
    if(-not $entry.reasons.ContainsKey($Reason)){$entry.reasons[$Reason]=0}
    $entry.reasons[$Reason]++
    $entry.lastReason=$Reason
    $entry.lastUtc=Get-UtcNow
}
function Read-State([string]$Role) {
    $path=Join-Path $script:Procs[$Role].Dir 'state.json'
    for($attempt=0;$attempt -lt 3;$attempt++) {
        $reason=''
        $snapshotExists=$false
        try{$snapshotExists=Test-Path -LiteralPath $path -PathType Leaf}
        catch{$reason='snapshot existence check denied during replacement'}
        if(-not $snapshotExists){if(-not $reason){$reason='snapshot missing during replacement or startup'}}
        else {
            try {
                $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                try {$reader=New-Object IO.StreamReader($stream,[Text.Encoding]::UTF8);$text=$reader.ReadToEnd()} finally {$stream.Dispose()}
                if([string]::IsNullOrWhiteSpace($text)) {$reason='snapshot empty'}
                else {
                    try {$state=$text | ConvertFrom-Json} catch {$state=$null;$reason='snapshot JSON invalid'}
                    if($null -ne $state) {
                        $required=@('role','sampleSequence','utc','probeStatus','probeFailure','hasWorld','hasBegunPlay','world','netMode','authGameModeCount','hasGameState','localPlayerId','playerCount','players','targets','privateViews','hasPrototype','lastCommandSequence','lastCommandStatus','lastCommandReason','moveActive','moveInjectedFrames','scheduledSequence','scheduledStatus','scheduledReason','scheduledAtUtcMs','scheduledFiredUtcMs')
                        $missing=@($required | Where-Object {$state.PSObject.Properties.Name -notcontains $_})
                        if($missing.Count -gt 0){$reason='snapshot missing fields: '+($missing -join ',')}
                        elseif($state.role -ne $Role){$reason='snapshot role mismatch'}
                        elseif($state.players -isnot [array] -or $state.targets -isnot [array] -or $state.privateViews -isnot [array]){$reason='snapshot arrays invalid'}
                        else {
                            try {
                                $age=((Get-Date).ToUniversalTime()-(ConvertTo-Utc $state.utc)).TotalSeconds
                                if([long]$state.sampleSequence -lt 1){$reason='snapshot sequence invalid'}
                                elseif($age -gt $StaleSeconds -or $age -lt -2){$reason='snapshot timestamp stale or in future'}
                                else{return $state}
                            } catch {$reason='snapshot sequence or timestamp invalid'}
                        }
                    }
                }
            } catch {$reason='snapshot open/read failed'}
        }
        Record-StateReadIssue $Role $reason
        if($attempt -lt 2){Start-Sleep -Milliseconds 25}
    }
    return $null
}
function Read-ReadyState([string]$Role) {
    $deadline=New-Deadline 3
    $reason='no readable snapshot'
    do {
        Assert-ProcessesAlive
        $state=Read-State $Role
        if($null -ne $state) {
            if($state.probeStatus -ne 'Running'){throw "$Role probe is not running: $($state.probeStatus)"}
            if(-not $state.hasWorld -or -not $state.hasBegunPlay){$reason='game world not ready'}
            elseif($state.PSObject.Properties.Name -notcontains 'public' -or $null -eq $state.public){$reason='prototype public state missing'}
            else {
                try {
                    $local=@($state.players | Where-Object {$_.isLocal})
                    if($local.Count -eq 1 -and $local[0].hasPawn){return $state}
                    $reason='local pawn missing or ambiguous'
                } catch {$reason='player snapshot schema invalid'}
            }
        } elseif($script:StateReadStats.ContainsKey($Role)) {$reason=$script:StateReadStats[$Role].lastReason}
        if($null -ne $state){Record-StateReadIssue $Role ('not ready: '+$reason)}
        Start-Sleep -Milliseconds 50
    } while((Get-Date) -lt $deadline)
    throw "$Role did not provide a fresh complete state within 3 seconds: $reason"
}

function Wait-Ack([string]$Role, [long]$Sequence, [datetime]$Deadline, [bool]$AllowExit) {
    while ($true) {
        if (-not $AllowExit) { Assert-ProcessesAlive }
        $state = Read-State $Role
        if ($null -ne $state -and [long]$state.lastCommandSequence -ge $Sequence) {
            if ([long]$state.lastCommandSequence -ne $Sequence -or $state.lastCommandStatus -ne 'Applied') {
                throw "$Role command $Sequence not applied: status=$($state.lastCommandStatus) seq=$($state.lastCommandSequence) reason=$($state.lastCommandReason)"
            }
            return $state
        }
        if ($AllowExit -and $script:Procs[$Role].Process.HasExited) { return $state }
        if ((Get-Date) -gt $Deadline) { throw "$Role did not acknowledge command $Sequence before the deadline" }
        Start-Sleep -Milliseconds $PollMs
    }
}

function ConvertTo-Position($Player) {
    $x = ConvertTo-FiniteDouble $Player.x
    $y = ConvertTo-FiniteDouble $Player.y
    $z = ConvertTo-FiniteDouble $Player.z
    if ($null -eq $x -or $null -eq $y -or $null -eq $z) { return $null }
    return [pscustomobject]@{ x = $x; y = $y; z = $z }
}

function Get-Pos($State, [int]$PlayerId) {
    $found = @(@($State.players) | Where-Object { [int]$_.playerId -eq $PlayerId })
    if ($found.Count -ne 1 -or -not $found[0].hasPawn) { return $null }
    return ConvertTo-Position $found[0]
}

function Get-Distance($A, $B) {
    return [math]::Sqrt([math]::Pow($A.x - $B.x, 2) + [math]::Pow($A.y - $B.y, 2) + [math]::Pow($A.z - $B.z, 2))
}

function Test-Roster($States, [string[]]$Roles) {
    $now = (Get-Date).ToUniversalTime()
    $localIds = @{}
    foreach ($role in $Roles) {
        $s = $States[$role]
        if ($s.role -ne $role) { return "$role state reports role '$($s.role)'" }
        if ($s.probeStatus -ne 'Running' -or $null -ne $s.probeFailure) { return "$role probe not running: $($s.probeStatus) $($s.probeFailure)" }
        if (($now - (ConvertTo-Utc $s.utc)).TotalSeconds -gt $StaleSeconds) { return "$role sample is stale" }
        if ($s.world -ne $MapPackage) { return "$role world is '$($s.world)'" }
        if (-not $s.hasBegunPlay -or -not $s.hasGameState) { return "$role has not begun play / has no GameState" }
        $isHost = ($role -eq 'Host')
        if ($s.netMode -ne $(if ($isHost) { 'ListenServer' } else { 'Client' })) { return "$role netMode is '$($s.netMode)'" }
        if ([int]$s.authGameModeCount -ne $(if ($isHost) { 1 } else { 0 })) { return "$role authGameModeCount is $($s.authGameModeCount)" }
        if ($null -eq $s.localPlayerId) { return "$role has no local PlayerId" }
        $key = [string][int]$s.localPlayerId
        if ($localIds.ContainsKey($key)) { return "$role local PlayerId $key duplicates $($localIds[$key])" }
        $localIds[$key] = $role
    }
    $expected = @($localIds.Keys | ForEach-Object { [int]$_ } | Sort-Object)
    foreach ($role in $Roles) {
        $s = $States[$role]
        $players = @($s.players)
        if ([int]$s.playerCount -ne $Roles.Count -or $players.Count -ne $Roles.Count) { return "$role playerCount=$($s.playerCount) players=$($players.Count), expected $($Roles.Count)" }
        $seen = @($players | ForEach-Object { [int]$_.playerId } | Sort-Object)
        if (($seen -join ',') -ne ($expected -join ',')) { return "$role PlayerIds [$($seen -join ',')] differ from expected [$($expected -join ',')]" }
        $locals = @($players | Where-Object { $_.isLocal })
        if ($locals.Count -ne 1 -or [int]$locals[0].playerId -ne [int]$s.localPlayerId) { return "$role does not have exactly one matching local player" }
        foreach ($p in $players) {
            if (-not $p.hasPawn) { return "$role sees PlayerId $($p.playerId) without a pawn" }
            $want = 'Authority'
            if ($role -ne 'Host') { $want = $(if ($p.isLocal) { 'AutonomousProxy' } else { 'SimulatedProxy' }) }
            if ($p.localRole -ne $want) { return "$role PlayerId $($p.playerId) localRole '$($p.localRole)', expected '$want'" }
            $mine = ConvertTo-Position $p
            if ($null -eq $mine) { return "$role PlayerId $($p.playerId) has a missing or non-finite position" }
            $ref = Get-Pos $States['Host'] ([int]$p.playerId)
            if ($null -eq $ref) { return "Host has no valid position for PlayerId $($p.playerId)" }
            $gap = Get-Distance $ref $mine
            if ($gap -gt $PositionToleranceCm) { return "$role sees PlayerId $($p.playerId) $([math]::Round($gap, 3)) cm from Host (> $PositionToleranceCm)" }
        }
    }
    return $null
}

function Wait-Stable([string]$Step, [string[]]$Roles, [scriptblock]$Evaluate, $Context, [int]$Streak, [datetime]$Deadline) {
    $used = @{}
    foreach ($role in $Roles) {
        $s = Read-State $role
        $used[$role] = $(if ($null -ne $s) { [long]$s.sampleSequence } else { 0 })
    }
    $count = 0
    $reason = 'no sample evaluated yet'
    while ($true) {
        Assert-ProcessesAlive
        $states = @{}
        $allNew = $true
        foreach ($role in $Roles) {
            $s = Read-State $role
            if ($null -eq $s) { $allNew = $false; $reason = "$role has no readable state.json"; continue }
            $states[$role] = $s
            if ([long]$s.sampleSequence -le $used[$role]) { $allNew = $false }
        }
        if ($allNew) {
            foreach ($role in $Roles) { $used[$role] = [long]$states[$role].sampleSequence }
            try { $reason = & $Evaluate $states $Context } catch { $reason = "evaluation error: $($_.Exception.Message)" }
            if ($null -eq $reason) {
                $count++
                if ($count -ge $Streak) { return $states }
            } else { $count = 0 }
        }
        if ((Get-Date) -gt $Deadline) { throw "$Step timed out; last reason: $reason" }
        Start-Sleep -Milliseconds $PollMs
    }
}

function Summarize-States($States) {
    $out = [ordered]@{}
    foreach ($role in $States.Keys) {
        $s = $States[$role]
        $out[$role] = [ordered]@{
            sampleSequence = $s.sampleSequence; utc = $s.utc; netMode = $s.netMode; authGameModeCount = $s.authGameModeCount
            localPlayerId = $s.localPlayerId; playerCount = $s.playerCount; players = @($s.players)
        }
    }
    return $out
}

function Invoke-Step([string]$Name, [scriptblock]$Body) {
    Write-Host "[step] $Name"
    $step = [ordered]@{ name = $Name; startedUtc = Get-UtcNow; finishedUtc = $null; passed = $false; error = $null; errorContext = $null; evidence = $null }
    [void]$script:Steps.Add($step)
    try {
        $step.evidence = & $Body
        $step.passed = $true
    } catch {
        $step.error = $_.Exception.Message
        $step.errorContext = $_.ScriptStackTrace
        if($Rendered -and $script:Procs.Contains('Host') -and -not $script:Procs.Host.Process.HasExited) {
            try { Capture-View 'Host' ("failure-"+$Name) | Out-Null } catch { [void]$script:ScreenshotRecords.Add([ordered]@{label="failure-$Name";error=$_.Exception.Message}) }
        }
        throw
    } finally {
        $step.finishedUtc = Get-UtcNow
        Save-Result
    }
}

function Wait-Roster([string]$Step, [string[]]$Roles, [double]$Seconds) {
    $ctx = @{ Roles = $Roles }
    $states = Wait-Stable $Step $Roles { param($States, $Ctx) Test-Roster $States $Ctx.Roles } $ctx $ReadyStreak (New-Deadline $Seconds)
    return [ordered]@{ roles = $Roles; consecutiveSamples = $ReadyStreak; states = (Summarize-States $states) }
}

function Wait-Settled([string]$Step, [string[]]$Roles, [int]$PlayerId, [double]$Seconds) {
    $ctx = @{ Roles = $Roles; PlayerId = $PlayerId; Prev = $null; Last = $null }
    $evaluate = {
        param($States, $Ctx)
        $bad = Test-Roster $States $Ctx.Roles
        if ($bad) { $Ctx.Prev = $null; return $bad }
        $pos = @{}
        foreach ($role in $Ctx.Roles) {
            $p = Get-Pos $States[$role] $Ctx.PlayerId
            if ($null -eq $p) { $Ctx.Prev = $null; return "$role has no pawn for PlayerId $($Ctx.PlayerId)" }
            $pos[$role] = $p
        }
        $previous = $Ctx.Prev
        $Ctx.Prev = $pos
        $Ctx.Last = $pos
        if ($null -eq $previous) { return 'first settled sample' }
        foreach ($role in $Ctx.Roles) {
            $moved = Get-Distance $pos[$role] $previous[$role]
            if ($moved -gt $SettleEpsilonCm) { return "$role pawn still moving ($([math]::Round($moved, 2)) cm between samples)" }
        }
        return $null
    }
    $states = Wait-Stable $Step $Roles $evaluate $ctx $SettleStreak (New-Deadline $Seconds)
    return [ordered]@{ positions = $ctx.Last; states = $states }
}

function Invoke-MoveCheck([string]$Mover, [string[]]$Roles, $Direction) {
    $stateBefore = Read-State $Mover
    if ($null -eq $stateBefore -or $null -eq $stateBefore.localPlayerId) { throw "$Mover has no readable state or local PlayerId before the move." }
    $playerId = [int]$stateBefore.localPlayerId
    $framesBefore = $(if ($null -eq $stateBefore.moveInjectedFrames) { [long]0 } else { [long]$stateBefore.moveInjectedFrames })

    $settledBefore = Wait-Settled "$Mover-move-before" $Roles $playerId 45
    $before = $settledBefore.positions

    $moveSeq = Send-Command $Mover 'Move' ([ordered]@{ direction = $Direction; durationSeconds = $MoveDurationSeconds })
    $null = Wait-Ack $Mover $moveSeq (New-Deadline 15) $false
    $endAt = New-Deadline ($MoveDurationSeconds + 10)
    $s = $null
    while ($true) {
        Assert-ProcessesAlive
        $s = Read-State $Mover
        if ($null -ne $s -and -not $s.moveActive -and [long]$s.lastCommandSequence -eq $moveSeq) { break }
        if ((Get-Date) -gt $endAt) { throw "$Mover move input did not finish before the deadline." }
        Start-Sleep -Milliseconds $PollMs
    }
    $framesAfter = [long]$s.moveInjectedFrames
    if ($framesAfter - $framesBefore -le 0) { throw "$Mover injected no movement input frames (before=$framesBefore after=$framesAfter)." }
    $stopSeq = Send-Command $Mover 'Stop'
    $null = Wait-Ack $Mover $stopSeq (New-Deadline 15) $false

    $settledAfter = Wait-Settled "$Mover-move-converged" $Roles $playerId 45
    $after = $settledAfter.positions
    $displacements = [ordered]@{}
    $gaps = [ordered]@{}
    foreach ($role in $Roles) {
        $d = Get-Distance $after[$role] $before[$role]
        $displacements[$role] = $d
        $gaps[$role] = Get-Distance $after['Host'] $after[$role]
        if ($d -lt $MinDisplacementCm) { throw "$role observed $Mover displacement of only $([math]::Round($d, 2)) cm (< $MinDisplacementCm)." }
    }
    return [ordered]@{
        mover = $Mover; playerId = $playerId; direction = $Direction
        moveSequence = $moveSeq; moveAck = 'Applied'; stopSequence = $stopSeq; stopAck = 'Applied'
        inputFramesBefore = $framesBefore; inputFramesAfter = $framesAfter; inputFrameIncrease = ($framesAfter - $framesBefore)
        before = $before; after = $after
        observedDisplacementCm = $displacements; hostToPeerDistanceCm = $gaps
    }
}

function Invoke-QuitGroup($Entries, [int]$Seconds) {
    $live = @($Entries | Where-Object { -not $_.Process.HasExited })
    foreach ($entry in $live) {
        $entry.ExpectedExit = $true
        try { $entry.QuitSequence = Send-Command $entry.Role 'Quit' } catch { $entry.CleanupNote = "Quit not published: $($_.Exception.Message)" }
    }
    $deadline = (Get-Date).AddSeconds($Seconds)
    foreach ($entry in $live) {
        $ms = [int][math]::Max(0, ($deadline - (Get-Date)).TotalMilliseconds)
        $null = $entry.Process.WaitForExit($ms)
    }
    foreach ($entry in $live) {
        if (-not $entry.Process.HasExited) { Stop-OwnedRoot $entry }
    }
}

function Invoke-Cleanup {
    $entries = @($script:Procs.Values)
    foreach ($entry in $entries) {
        $entry.ExitedBeforeCleanup = $entry.Process.HasExited
        if (-not $entry.ExpectedExit -and $entry.Process.HasExited) {
            [void]$script:Failures.Add("Process $($entry.Role) (pid $($entry.Pid)) exited unexpectedly with code $($entry.Process.ExitCode).")
        }
    }
    Invoke-QuitGroup @($entries | Where-Object { $_.Role -ne 'Host' }) $QuitWaitSeconds
    Invoke-QuitGroup @($entries | Where-Object { $_.Role -eq 'Host' }) $QuitWaitSeconds

    $remaining = @()
    foreach ($entry in $entries) {
        $entry.Process.Refresh()
        if (-not $entry.Process.HasExited) {
            $remaining += [ordered]@{ role = $entry.Role; pid = $entry.Pid; reason = $(if ($entry.CleanupNote) { $entry.CleanupNote } else { 'still running after cleanup' }) }
            [void]$script:Failures.Add("Own process $($entry.Role) (pid $($entry.Pid)) is still running after cleanup.")
            continue
        }
        if ($entry.KilledByRunner) {
            [void]$script:Failures.Add("Process $($entry.Role) (pid $($entry.Pid)) did not exit after Quit and was force-terminated.")
        } elseif ($entry.ExpectedExit -and $entry.Process.ExitCode -ne 0 -and -not $entry.AllowNonZeroExit) {
            [void]$script:Failures.Add("Process $($entry.Role) (pid $($entry.Pid)) exited with code $($entry.Process.ExitCode) after Quit.")
        }
    }
    $script:Result['cleanup'] = [ordered]@{
        remainingOwnedProcesses = $remaining
        childProcessesReclaimed = 'none: the packaged exe is started directly and only runner-started root processes are ever terminated'
    }
}


# Run after process cleanup so full logs are available. Only the two observed, tightly
# scoped messages for the intentionally denied NewJoin are classified as expected.
function Inspect-ProcessLogs {
    $diagnostics=@()
    foreach($entry in $script:Procs.Values) {
        $record=[ordered]@{role=$entry.Role; log=$entry.Log; readable=$false; unexpectedCount=0; expectedDenialCount=0; examples=@(); expectedDenial=@()}
        if(-not (Test-Path -LiteralPath $entry.Log -PathType Leaf)) {
            [void]$script:Failures.Add("$($entry.Role) log is missing: $($entry.Log)")
            $diagnostics+=@($record); continue
        }
        try {
            $stream=[IO.File]::Open($entry.Log,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            try { $reader=New-Object IO.StreamReader($stream,[Text.Encoding]::UTF8); $content=$reader.ReadToEnd() } finally {$stream.Dispose()}
        } catch {
            [void]$script:Failures.Add("$($entry.Role) log could not be read: $($_.Exception.Message)")
            $diagnostics+=@($record); continue
        }
        $record.readable=$true
        $lines=$content -split "`r?`n"
        if([string]::IsNullOrWhiteSpace($content)) {
            [void]$script:Failures.Add("$($entry.Role) log is empty: $($entry.Log)")
            $diagnostics+=@($record); continue
        }
        $denialLine=-1
        $denialDriver=''
        $lostExcluded=$false
        for($i=0; $i -lt $lines.Count; $i++) {
            $line=$lines[$i]
            if($line -notmatch '(?i)(?:\bLog[A-Za-z0-9_]+:\s*(?:Error|Fatal):|\bEnsure condition failed\b|\bHandled ensure\b|\bFatal error\b|\bAssertion failed\b)'){continue}
            $expected=$false
            if($Players -eq 2 -and $entry.Role -eq 'NewJoin' -and $script:RejectionProven) {
                if($denialLine -lt 0 -and $line -match '^\[[^\]]+\]\[\s*\d+\]LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = FailureReceived, ErrorString = 탐사 중에는 새로 참가할 수 없습니다\., Driver = Name:GameNetDriver Def:GameNetDriver (?<driver>IpNetDriver_\d+)\s*$') {
                    $denialLine=$i; $denialDriver=$Matches.driver; $expected=$true
                } elseif($denialLine -ge 0 -and -not $lostExcluded -and $i-$denialLine -le 20 -and $line -match '^\[[^\]]+\]\[\s*\d+\]LogNet: Error: UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = Your connection to the host has been lost\., Driver = Name:GameNetDriver Def:GameNetDriver (?<driver>IpNetDriver_\d+)\s*$' -and $Matches.driver -eq $denialDriver) {
                    $lostExcluded=$true; $expected=$true
                }
            }
            $item=[ordered]@{line=$i+1; text=$line}
            if($expected){$record.expectedDenialCount++; $record.expectedDenial+=@($item)}
            else {$record.unexpectedCount++; if($record.examples.Count -lt 8){$record.examples+=@($item)}}
        }
        if($record.unexpectedCount -gt 0){[void]$script:Failures.Add("$($entry.Role) log contains $($record.unexpectedCount) unexpected Error/Fatal/ensure/assertion lines: $($entry.Log)")}
        $diagnostics+=@($record)
    }
    $script:Result['logDiagnostics']=$diagnostics
}

function Wait-Condition([string]$Name, [string[]]$Roles, [scriptblock]$Predicate, [double]$Seconds = 30) {
    $deadline = New-Deadline $Seconds
    $used = @{}
    foreach ($role in $Roles) { $used[$role] = 0 }
    $streak = 0
    $last = 'no fresh sample'
    while ((Get-Date) -lt $deadline) {
        Assert-ProcessesAlive
        $states = @{}
        $fresh = $true
        foreach ($role in $Roles) {
            $s = Read-State $role
            if ($null -eq $s -or [long]$s.sampleSequence -le $used[$role]) { $fresh = $false; continue }
            $states[$role] = $s
        }
        if ($fresh) {
            foreach ($role in $Roles) { $used[$role] = [long]$states[$role].sampleSequence }
            try { $last = & $Predicate $states } catch { $last = $_.Exception.Message }
            if ($null -eq $last) { $streak++ } else { $streak = 0 }
            if ($streak -ge 2) { return $states }
        }
        Start-Sleep -Milliseconds $PollMs
    }
    throw "$Name timed out: $last"
}
# Probe semanticType is produced by IsA against the native base, including Blueprint subclasses.
function Test-TargetType($Target, [string]$Class) {
    $expected=switch ($Class) {
        'DigetherPrototypeStation' { 'Station' }
        'DigetherPrototypeDigCell' { 'DigCell' }
        'DigetherPrototypePickup' { 'Pickup' }
        default { return $false }
    }
    return $Target.semanticType -eq $expected
}
function Find-Target([string]$Role, [string]$Class, [int]$Kind = -1, [double]$Y = 0) {
    $s = Read-ReadyState $Role
    $list = @($s.targets | Where-Object { (Test-TargetType $_ $Class) })
    if ($Class -eq 'DigetherPrototypeDigCell') { $list = @($list | Where-Object { [math]::Abs([double]$_.x - 3400) -lt 10 }) }
    if ($Kind -ge 0) { $list = @($list | Where-Object { [int]$_.kind -eq $Kind }) }
    if ($Class -eq 'DigetherPrototypePickup') { $list = @($list | Where-Object { [math]::Abs([double]$_.y - $Y) -lt 10 }) }
    if ($list.Count -ne 1) { throw "$Role target $Class kind=$Kind y=$Y found $($list.Count)" }
    return $list[0]
}
function Move-Input([string]$Role, [double]$X, [double]$Y, [double]$Seconds,
    [Nullable[double]]$GoalX = $null, [Nullable[double]]$GoalY = $null) {
    if (($null -eq $GoalX) -ne ($null -eq $GoalY)) { throw 'waypoint requires both goal coordinates' }
    $hasGoal=$null -ne $GoalX
    $before=Read-ReadyState $Role
    $local=@($before.players | Where-Object {$_.isLocal})
    if ($local.Count -ne 1) { throw "$Role cannot steer without a local pawn" }
    $start=ConvertTo-Position $local[0]
    $frames=[long]$before.moveInjectedFrames
    $seq=Send-Command $Role 'Move' ([ordered]@{direction=[ordered]@{x=$X;y=$Y;z=0};durationSeconds=$Seconds})
    $null=Wait-Ack $Role $seq (New-Deadline 10) $false
    $deadline=New-Deadline ($Seconds+5)
    while ((Get-Date) -lt $deadline) {
        Assert-ProcessesAlive
        $sample=Read-State $Role
        if ($null -ne $sample -and [long]$sample.lastCommandSequence -eq $seq -and -not $sample.moveActive) {
            $p=@($sample.players | Where-Object {$_.isLocal})
            if ($p.Count -ne 1) { throw "$Role steering input lost the local pawn" }
            $end=ConvertTo-Position $p[0]
            if ($null -eq $end) { throw "$Role steering input ended with an invalid position" }
            $displacement=Get-Distance $start $end
            $injected=[long]$sample.moveInjectedFrames-$frames
            if ($hasGoal) {
                $beforeGoal=[math]::Sqrt([math]::Pow($start.x-[double]$GoalX,2)+[math]::Pow($start.y-[double]$GoalY,2))
                $afterGoal=[math]::Sqrt([math]::Pow($end.x-[double]$GoalX,2)+[math]::Pow($end.y-[double]$GoalY,2))
                $progress=$beforeGoal-$afterGoal
                $passed=$injected -gt 0 -and $progress -ge 1.0
                [void]$script:WaypointProgress.Add([ordered]@{role=$Role; sequence=$seq; utc=Get-UtcNow; durationSeconds=$Seconds; frames=$injected; goalX=[double]$GoalX; goalY=[double]$GoalY; beforeCm=$beforeGoal; afterCm=$afterGoal; progressCm=$progress; displacementCm=$displacement; passed=$passed})
                if(-not $passed) {
                    throw "$Role waypoint input made insufficient progress: seq=$seq duration=$Seconds frames=$injected progressCm=$([math]::Round($progress,2)) beforeCm=$([math]::Round($beforeGoal,2)) afterCm=$([math]::Round($afterGoal,2)) displacementCm=$([math]::Round($displacement,2))"
                }
            } elseif ($injected -le 0 -or $displacement -lt 30) {
                throw "$Role steering input was obstructed or insufficient: seq=$seq duration=$Seconds frames=$injected displacementCm=$([math]::Round($displacement,2)) start=($($start.x),$($start.y)) end=($($end.x),$($end.y))"
            }
            return
        }
        Start-Sleep -Milliseconds $PollMs
    }
    throw "$Role steering input did not finish"
}
function Move-To([string]$Role, $Target) {
    $initial=Read-ReadyState $Role
    $local=@($initial.players | Where-Object {$_.isLocal})
    # Return positions, terminal and other players can obstruct a straight line to the deposit station.
    # Walk around the left and north side using normal movement input; this is not a navigation test.
    if ((Test-TargetType $Target 'DigetherPrototypeStation') -and [int]$Target.kind -eq 2 -and $local.Count -eq 1 -and [double]$local[0].x -lt -200) {
        Move-Input $Role -1 0 0.7
        Move-Input $Role 0 1 1.9
        Move-Input $Role 1 0 1.6
    }
    # From the north side of the deposit station go east before approaching the start station.
    elseif ((Test-TargetType $Target 'DigetherPrototypeStation') -and [int]$Target.kind -eq 0 -and $local.Count -eq 1 -and [double]$local[0].x -gt -200 -and [double]$local[0].y -gt 250) {
        Move-Input $Role 1 0 0.9
    }
    # At the start station use a shorter lateral steering input where the terminal is directly ahead.
    elseif ((Test-TargetType $Target 'DigetherPrototypeStation') -and [int]$Target.kind -eq 0 -and $local.Count -eq 1 -and [double]$local[0].x -lt -200 -and [math]::Abs([double]$local[0].y) -lt 120) {
        Move-Input $Role 0 1 0.55
    }
    $seq = Send-Command $Role 'MoveTo' ([ordered]@{target=$Target.name; durationSeconds=30})
    $null = Wait-Ack $Role $seq (New-Deadline 15) $false
    $deadline = New-Deadline 35
    while ((Get-Date) -lt $deadline) {
        Assert-ProcessesAlive
        $s = Read-State $Role
        if ($null -ne $s -and -not $s.moveActive) {
            $p = @($s.players | Where-Object { $_.isLocal })[0]
            if ($null -eq $p -or (Get-Distance (ConvertTo-Position $p) $Target) -gt 330) { throw "$Role failed to reach $($Target.name)" }
            return
        }
        Start-Sleep -Milliseconds $PollMs
    }
    throw "$Role MoveTo timeout"
}
# Position each contender on a different flank using character movement input.
# No actor is teleported; lack of progress is a test failure.
function Move-NearPoint([string]$Role, [double]$X, [double]$Y) {
    for($attempt=0; $attempt -lt 12; $attempt++) {
        $s=Read-ReadyState $Role
        $local=@($s.players | Where-Object {$_.isLocal})
        if($local.Count -ne 1){throw "$Role has no local pawn while positioning"}
        $dx=$X-[double]$local[0].x; $dy=$Y-[double]$local[0].y
        $distance=[math]::Sqrt($dx*$dx+$dy*$dy)
        if($distance -le 50){return}
        $seconds=[math]::Min(1.5,[math]::Max(0.08,$distance/350.0))
        Move-Input $Role $dx $dy $seconds $X $Y
    }
    throw "$Role could not reach the designated contention flank ($X,$Y)"
}
# Each role has a separate vertical lane; player capsules cannot block a peer's lane.
function Get-WaitingLaneX([string]$Role) {
    switch ($Role) {
        'Host' { return 1800.0 }
        'Client1' { return 2000.0 }
        'Client2' { return 2200.0 }
        'Client3' { return 2400.0 }
        'Client3Reconnect' { return 2400.0 }
        default { throw "unsupported waiting role $Role" }
    }
}
# Keep idle players on distinct west-side points outside the cell's collapse radius.
# East-to-west travel uses the approach-side corridor and normal movement.
function Park-OutsideExcavation([string]$Role) {
    $slotY=switch ($Role) {
        'Host' { -300 }
        'Client1' { -100 }
        'Client2' { 100 }
        'Client3' { 300 }
        'Client3Reconnect' { 300 }
        default { throw "unsupported parking role $Role" }
    }
    $laneX=Get-WaitingLaneX $Role
    $s=Read-ReadyState $Role
    $p=@($s.players | Where-Object {$_.isLocal})
    if($p.Count -ne 1){throw "$Role has no local pawn for parking"}
    if([double]$p[0].x -gt 3350){
        $exitY=$(if($script:FlankSides.ContainsKey($Role) -and $script:FlankSides[$Role]){500}else{-650})
        Move-NearPoint $Role 3600 $exitY
        Move-NearPoint $Role 3250 $exitY
        Move-NearPoint $Role $laneX $exitY
    } else {
        # Enter the role's lane before changing Y, so parked peers cannot block it.
        Move-NearPoint $Role $laneX ([double]$p[0].y)
    }
    Move-NearPoint $Role $laneX $slotY
}
function Move-ContentionFlank([string]$Role, [double]$TargetY, [bool]$Upper) {
    # Approach the intact cell from separate north/south corridors.
    $s=Read-ReadyState $Role
    $p=@($s.players | Where-Object {$_.isLocal})
    if($p.Count -ne 1){throw "$Role has no local pawn for contention"}
    if([double]$p[0].x -gt 3350){
        $exitY=$(if($script:FlankSides.ContainsKey($Role) -and $script:FlankSides[$Role]){500}else{-650})
        Move-NearPoint $Role 3600 $exitY
        Move-NearPoint $Role 3250 $exitY
    }
    $entryY=$(if($Upper){500}else{-650})
    $current=Read-ReadyState $Role
    $pawn=@($current.players | Where-Object {$_.isLocal})
    if($pawn.Count -ne 1){throw "$Role lost its local pawn while positioning"}
    if([double]$pawn[0].x -lt 3000){
        # Stationary peers stay at x <= 2400; travel vertically at x=2700.
        # The runner moves only one participant at a time.
        Move-NearPoint $Role 2700 ([double]$pawn[0].y)
        Move-NearPoint $Role 2700 $entryY
    }
    Move-NearPoint $Role 3250 $entryY
    Move-NearPoint $Role 3600 $entryY
    $flankY=$TargetY + $(if($Upper){100}else{-100})
    Move-NearPoint $Role 3600 $flankY
    $script:FlankSides[$Role]=$Upper
}
function Ensure-DigSupport([string[]]$Roles) {
    $target=Find-Target 'Host' 'DigetherPrototypeDigCell'
    if(-not $target.excavated){throw 'the excavated cell collapsed before contention; no state was forced'}
    if([int]$target.supportStrength -ge 2){return}
    Move-To 'Host' $target
    $target=Find-Target 'Host' 'DigetherPrototypeDigCell'
    $seq=Send-Interact 'Host' $target $true
    $states=Wait-Condition 'reinforcement replication' $Roles {param($s)
        foreach($role in $s.Keys){
            $cell=@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypeDigCell') -and [math]::Abs([double]$_.x-3400) -lt 10})
            if($cell.Count -ne 1 -or -not $cell[0].excavated -or [int]$cell[0].supportStrength -lt 2){return "$role does not observe repaired support"}
        }
        return $null
    } 15
    Write-Host "[fixture] support RPC=$seq replicated to $($Roles -join ',')"
}
function Send-Interact([string]$Role, $Target, [bool]$Alternate = $false) {
    $seq = Send-Command $Role 'Interact' ([ordered]@{target=$Target.name; alternate=$Alternate})
    $null = Wait-Ack $Role $seq (New-Deadline 10) $false
    return $seq
}
function Wait-ScheduledSent([string]$Role, [long]$Sequence, [long]$FireAtUtcMs, [datetime]$Deadline) {
    while ((Get-Date) -lt $Deadline) {
        Assert-ProcessesAlive
        $s=Read-State $Role
        if ($null -ne $s -and [long]$s.scheduledSequence -eq $Sequence) {
            if ($s.scheduledStatus -eq 'Rejected' -or $s.scheduledStatus -eq 'Canceled') {
                throw "$Role scheduled RPC $Sequence failed: $($s.scheduledReason)"
            }
            if ($s.scheduledStatus -eq 'Sent') {
                if ([long]$s.scheduledAtUtcMs -ne $FireAtUtcMs -or [long]$s.scheduledFiredUtcMs -lt $FireAtUtcMs) {
                    throw "$Role scheduled RPC $Sequence has invalid fire timing"
                }
                return $s
            }
        }
        Start-Sleep -Milliseconds $PollMs
    }
    throw "$Role did not send scheduled RPC $Sequence before the deadline"
}
function Local-Private($S) {
    $found = @($S.privateViews | Where-Object { $_.isLocal })
    if ($found.Count -ne 1) { throw 'missing local private view' }
    return $found[0]
}
function Check-Public($States) {
    $base = $States['Host'].public
    $seed = @($States['Host'].layoutSeeds)
    if ($null -eq $base) { return 'host lacks prototype public state' }
    foreach ($role in $States.Keys) {
        $p = $States[$role].public
        if ((@($States[$role].layoutSeeds) -join ',') -ne ($seed -join ',')) { return "$role layout seed differs" }
        if ($null -eq $p) { return "$role lacks prototype public state" }
        foreach ($field in @('phase','day','quotaTarget','quotaProgress','runNumber','runId','elevatorTrips','elevatorState','elevatorWeightStage')) {
            if ($p.$field -ne $base.$field) { return "$role public $field differs" }
        }
    }
    return $null
}
function Check-Privacy($States) {
    $hostViews = @($States.Host.privateViews)
    foreach ($role in $States.Keys) {
        $s = $States[$role]
        $own = Local-Private $s
        $authoritative = @($hostViews | Where-Object { [int]$_.playerId -eq [int]$own.playerId })
        if ($authoritative.Count -ne 1) { return "$role has no matching host view" }
        foreach ($field in @('money','health','life','lastPayout','woodCount','burialLevel','holdProgress','message','fixtureId')) {
            if ($own.$field -ne $authoritative[0].$field) { return "$role owner $field has not converged" }
        }
        $localIds = @($own.items | ForEach-Object { $_.id } | Sort-Object)
        $hostIds = @($authoritative[0].items | ForEach-Object { $_.id } | Sort-Object)
        if (($localIds -join ',') -ne ($hostIds -join ',')) { return "$role owner items differ from host" }
        foreach ($item in @($own.items)) {
            $reference=@($authoritative[0].items | Where-Object {$_.id -eq $item.id})
            if ($reference.Count -ne 1) { return "$role item has no unique host match" }
            foreach ($field in @('value','weightKg','twoHanded','equipment','emergencyDevice','hidden')) {
                if ($item.$field -ne $reference[0].$field) { return "$role owner item $field differs from host" }
            }
        }
        if ($role -ne 'Host') {
            foreach ($other in @($s.privateViews | Where-Object { -not $_.isLocal })) {
                if ([int]$other.money -ne 0 -or [double]$other.health -ne 100 -or [int]$other.life -ne 0 -or [int]$other.lastPayout -ne 0 -or [int]$other.woodCount -ne 2 -or [int]$other.burialLevel -ne 0 -or [double]$other.holdProgress -ne 0 -or $other.message -ne '' -or $other.fixtureId -ne '' -or @($other.items).Count -ne 0) {
                    return "$role observed non-default private state for another player"
                }
            }
        }
    }
    return $null
}
function Check-PrivacyDivergence($States) {
    if ($States.Count -lt 2) { return $null }
    foreach ($role in $States.Keys) {
        if ($role -eq 'Host') { continue }
        foreach ($other in @($States[$role].privateViews | Where-Object { -not $_.isLocal })) {
            $authority=@($States.Host.privateViews | Where-Object { [int]$_.playerId -eq [int]$other.playerId })
            if ($authority.Count -eq 1 -and ([int]$authority[0].money -ne 0 -or @($authority[0].items).Count -gt 0)) {
                return $null
            }
        }
    }
    return 'no non-owner view was compared against a host with non-default money or inventory'
}
function All-States([string[]]$Roles) {
    $out = [ordered]@{}
    foreach ($role in $Roles) { $out[$role] = Read-ReadyState $role }
    return $out
}

function Terrain($State, [string]$Tag) {
    $expectedX=switch($Tag){'PrototypeTerrainFloor'{4400} 'PrototypeTerrainWall'{5300} default {throw "unknown terrain $Tag"}}
    $expectedZ=if($Tag -eq 'PrototypeTerrainFloor'){0}else{200}
    $items=@($State.terrains | Where-Object { $_.tags -contains $Tag })
    # Dynamic actor tags are not replicated. The fixture's documented distinct
    # transforms identify the same actors on remote processes without writing state.
    if ($items.Count -eq 0) {
        $items=@($State.terrains | Where-Object { [math]::Abs([double]$_.x-$expectedX) -lt 1 -and [math]::Abs([double]$_.y) -lt 1 })
    }
    if ($items.Count -ne 1) { throw "$($State.role) expected one terrain tagged $Tag, found $($items.Count)" }
    if([math]::Abs([double]$items[0].x-$expectedX) -ge 1 -or [math]::Abs([double]$items[0].y) -ge 1 -or [math]::Abs([double]$items[0].z-$expectedZ) -ge 1){throw "$($State.role) terrain $Tag transform differs from fixture"}
    return $items[0]
}
function Trace($State, [string]$Name) {
    $items=@($State.terrainTraces | Where-Object { $_.name -eq $Name })
    if ($items.Count -ne 1) { throw "$($State.role) missing trace $Name" }
    return $items[0]
}
function Wait-Terrain([string[]]$Roles, [string]$Tag, [long]$MinimumRevision, [double]$Seconds=20) {
    return Wait-Condition "terrain $Tag converges" $Roles { param($states)
        $hostTerrain=Terrain $states.Host $Tag
        if ([long]$hostTerrain.revision -lt $MinimumRevision) { return "host revision $($hostTerrain.revision) < $MinimumRevision" }
        foreach ($role in $states.Keys) {
            $t=Terrain $states[$role] $Tag
            if ([long]$t.revision -ne [long]$hostTerrain.revision -or [long]$t.hash -ne [long]$hostTerrain.hash) { return "$role terrain differs from host" }
        }
        return $null
    } $Seconds
}
function Look-At([string]$Role, [double]$X, [double]$Y, [double]$Z) {
    $s=Read-ReadyState $Role
    $dx=$X-[double]$s.localView.x; $dy=$Y-[double]$s.localView.y; $dz=$Z-[double]$s.localView.z
    $yaw=[math]::Atan2($dy,$dx)*180/[math]::PI
    $pitch=[math]::Atan2($dz,[math]::Sqrt($dx*$dx+$dy*$dy))*180/[math]::PI
    $seq=Send-Command $Role 'Look' ([ordered]@{pitch=$pitch; yaw=$yaw})
    $null=Wait-Ack $Role $seq (New-Deadline 8) $false
}
function Send-Checked([string]$Role,[string]$Name) {
    $seq=Send-Command $Role $Name
    $null=Wait-Ack $Role $seq (New-Deadline 8) $false
    return $seq
}
function Capture-View([string]$Role,[string]$Label) {
    if(-not $Rendered){return $null}
    $seq=Send-Checked $Role 'Screenshot'
    $path=Join-Path $script:Procs[$Role].Dir "screenshot-$seq.png"
    $deadline=New-Deadline 10
    while((Get-Date) -lt $deadline){
        Assert-ProcessesAlive
        if((Test-Path -LiteralPath $path -PathType Leaf) -and (Get-Item -LiteralPath $path).Length -gt 0){
            $record=[ordered]@{label=$Label;role=$Role;sequence=$seq;path=$path;bytes=(Get-Item -LiteralPath $path).Length}
            [void]$script:ScreenshotRecords.Add($record); return $record
        }
        Start-Sleep -Milliseconds 100
    }
    throw "screenshot $Label did not appear: $path"
}
function Move-CarvingRoute([string]$Role,[double]$X,[double]$Y) {
    for($attempt=0;$attempt -lt 12;$attempt++){
        $state=Read-ReadyState $Role; $p=@($state.players | Where-Object {$_.isLocal})[0]
        $dx=$X-[double]$p.x; $dy=$Y-[double]$p.y; $distance=[math]::Sqrt($dx*$dx+$dy*$dy)
        if($distance -le 50){return}
        try { Move-Input $Role $dx $dy ([math]::Min(1.2,[math]::Max(0.1,$distance/350))) $X $Y; continue }
        catch {
            if($_.Exception.Message -notlike '*waypoint input made insufficient progress*'){throw}
            $failed=Read-ReadyState $Role; $seq=[long]$failed.lastCommandSequence
            $history=Join-Path $script:Procs[$Role].Dir 'history.jsonl'
            $sample=$null
            foreach($line in @(Get-Content -LiteralPath $history -Tail 150)){
                $item=$line | ConvertFrom-Json
                if([long]$item.lastCommandSequence -eq $seq -and $item.moveActive -and $item.PSObject.Properties.Name -contains 'movementSweep'){$sample=$item}
            }
            if($null -eq $sample -or -not $sample.movementSweep.hit){throw "movement $seq blocked; no diagnostic blocking sweep"}
            $hit=$sample.movementSweep
            [void]$script:RouteDiagnostics.Add([ordered]@{role=$Role;attempt=$attempt;goal=@($X,$Y);moveSequence=$seq;sweep=$hit})
            $null=Capture-View $Role "route-blocked-$attempt"
            if(@($sample.terrains | Where-Object {$_.name -eq $hit.actor}).Count -ne 1){throw "movement $seq blocked by non-terrain $($hit.actor)/$($hit.component)"}
            Look-At $Role $hit.x $hit.y $hit.z
            $aim=Read-ReadyState $Role
            if($aim.PSObject.Properties.Name -notcontains 'aimTrace'){throw "movement $seq has no aim trace"}
            if(-not $aim.aimTrace.hit -or $aim.aimTrace.actor -ne $hit.actor){throw "movement $seq soil contact cannot be aimed: $($aim.aimTrace.actor)/$($aim.aimTrace.component)"}
            $prior=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 15; $rev=[long](Terrain $prior.Host 'PrototypeTerrainFloor').revision
            $dig=Send-Checked $Role 'Dig'
            $after=Wait-Terrain $roles 'PrototypeTerrainFloor' ($rev+1) 15
            [void]$script:RouteDiagnostics.Add([ordered]@{role=$Role;attempt=$attempt;digSequence=$dig;aim=$aim.aimTrace;revisionBefore=$rev;revisionAfter=(Terrain $after.Host 'PrototypeTerrainFloor').revision})
            $null=Capture-View $Role "route-clear-$attempt"
        }
    }
    throw "$Role could not clear and reach ($X,$Y) after 12 actual movement attempts"
}

$exitCode=1
try {
    if($Rendered -and $Players -ne 1){throw 'Rendered mode is limited to the one-player run.'}
    $script:GlobalDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
    $repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if (-not $OutputRoot) { $OutputRoot=Join-Path $repoRoot 'DIGETHER\Saved\DeveloperChecks' }
    $script:RunId=[guid]::NewGuid().ToString('N')
    $script:RunDir=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ("Excavation-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'),$script:RunId)
    New-Item -ItemType Directory -Path $script:RunDir -Force | Out-Null
    $script:Result=[ordered]@{runId=$script:RunId; startedUtc=Get-UtcNow; finishedUtc=$null; passed=$false; failure=$null; failures=@(); players=$Players; gameExe=$GameExe; project=$ProjectPath; map=$MapPackage; port=$Port; timeoutSeconds=$TimeoutSeconds; rendered=[bool]$Rendered; fixture='Local separate processes with -nosteam. Commands use normal movement and player-controller RPCs; acknowledgements do not prove application.'; limitations=@('Same-machine network, not M3.', 'Fixed traces observe collision only at their stated coordinates.', 'Fixture identity is not authenticated.'); steps=@(); processes=@()}
    Save-Result
    if (-not [IO.Path]::IsPathRooted($GameExe) -or -not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw 'GameExe must be an existing absolute executable path.' }
    $script:ExePath=[IO.Path]::GetFullPath($GameExe)
    if ($ProjectPath -and (-not [IO.Path]::IsPathRooted($ProjectPath) -or -not (Test-Path -LiteralPath $ProjectPath -PathType Leaf))) { throw 'ProjectPath must be an existing absolute uproject path.' }
    if ($Port -eq 0) { $rng=New-Object System.Random; for($i=0;$i -lt 100 -and $Port -eq 0;$i++){ $candidate=$rng.Next(30000,40000); if(Test-UdpPortFree $candidate){$Port=$candidate} }; if($Port -eq 0){throw 'No free UDP port found.'} }
    elseif (-not (Test-UdpPortFree $Port)) { throw "UDP port $Port is already in use." }
    $script:Result['port']=$Port
    $script:Result['gameExeSha256']=(Get-FileHash -LiteralPath $script:ExePath -Algorithm SHA256).Hash
    $script:ProbeMaxSeconds=[math]::Min(600,$TimeoutSeconds+30)
    $roles=@('Host','Client1','Client2','Client3')[0..($Players-1)]
    Invoke-Step 'roster-and-initial-terrain' {
        Start-Probe 'Host' $MapUrl $true
        $null=Wait-Roster 'host' @('Host') 100
        foreach($role in @($roles | Where-Object {$_ -ne 'Host'})){Start-Probe $role "${LoopbackHost}:$Port" $false}
        $null=Wait-Roster 'roster' $roles 120
        $floor=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 30
        $wall=Wait-Terrain $roles 'PrototypeTerrainWall' 0 30
        foreach($role in $roles){
            if(@($wall[$role].terrains).Count -ne 2){throw "$role has unknown or missing terrain actors"}
            $null=Terrain $wall[$role] 'PrototypeTerrainFloor'; $null=Terrain $wall[$role] 'PrototypeTerrainWall'
            foreach($n in @('floor','wall')){
                $t=Trace $wall[$role] $n; if(-not $t.hit -or -not $t.terrain){throw "$role initial $n trace did not hit terrain: $($t.actor)"}
                if($n -eq 'floor' -and [double]$t.normalZ -le 0){throw "$role initial floor collision normal does not point upward"}
            }
        }
        $null=Capture-View 'Host' 'initial-terrain'
        return [ordered]@{floor=$floor;wall=$wall}
    }
    Invoke-Step 'start-and-reject-distant-dig' {
        $start=Find-Target 'Host' 'DigetherPrototypeStation' 0; Move-To 'Host' $start; $null=Send-Interact 'Host' $start
        $before=Wait-Condition 'first expedition' $roles {param($s) if([int]$s.Host.public.phase -ne 1){return 'not in expedition'}; if(@($s.Host.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup')}).Count -ne 10){return 'pickups not ready'}; foreach($role in $s.Keys){if(@((Local-Private $s[$role]).items | Where-Object {$_.equipment}).Count -lt 1){return "$role has no initial equipment"}}; return $null} 30
        if($Rendered){
            $null=Send-Checked 'Host' 'ToggleHeadlamp'
            $null=Wait-Condition 'headlamp input' @('Host') {param($s) if(-not $s.Host.headlampOn){return 'headlamp did not turn on'}; return $null} 10
        }
        $before=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 20
        Look-At 'Host' 4200 0 0; $seq=Send-Checked 'Host' 'Dig'
        Start-Sleep -Seconds 2
        $after=Wait-Terrain $roles 'PrototypeTerrainFloor' ([long](Terrain $before.Host 'PrototypeTerrainFloor').revision) 15
        $a=Terrain $before.Host 'PrototypeTerrainFloor'; $b=Terrain $after.Host 'PrototypeTerrainFloor'
        if($a.hash -ne $b.hash -or $a.revision -ne $b.revision){throw 'distant dig changed terrain'}
        return [ordered]@{command=$seq;before=$before;after=$after}
    }
    Invoke-Step 'floor-carve' {
        Move-NearPoint 'Host' 4070 0
        $before=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 20
        $trace=Trace $before.Host 'floor'; if(-not $trace.terrain){throw 'floor sample obstructed'}
        $attempts=@(); $after=$before
        for($i=0;$i -lt 2;$i++){
            $surface=Trace $after.Host 'floor'; if(-not $surface.terrain){throw 'floor sample obstructed during repeated digs'}
            Look-At 'Host' 4200 0 $surface.z
            $revision=[long](Terrain $after.Host 'PrototypeTerrainFloor').revision
            $seq=Send-Checked 'Host' 'Dig'
            $after=Wait-Terrain $roles 'PrototypeTerrainFloor' ($revision+1) 20
            $null=Capture-View 'Host' "floor-dig-$i"
            $attempts+=@([ordered]@{sequence=$seq;states=$after})
        }
        foreach($role in $roles){
            $prior=Trace $before[$role] 'floor'; $depth=Trace $after[$role] 'floor'
            if(-not $prior.terrain -or -not $depth.terrain -or [double]$depth.distance -le [double]$prior.distance+1){throw "$role floor collision surface did not deepen at sample"}
            if([double]$depth.normalZ -le 0){throw "$role excavated floor collision normal does not point upward"}
            if([math]::Abs([double]$depth.distance-[double](Trace $after.Host 'floor').distance) -gt 2){throw "$role floor collision depth differs from host"}
        }
        return [ordered]@{attempts=$attempts; before=$before; after=$after}
    }
    if($Players -eq 4){
        Invoke-Step 'floor-support-before-contention' {
            $before=Read-ReadyState 'Host'; $wood=[int](Local-Private $before).woodCount
            if($wood -lt 1){throw 'no wood for floor support'}
            Look-At 'Host' 4300 0 0
            $seq=Send-Checked 'Host' 'SupportTerrain'
            $after=Wait-Condition 'floor support replicated' $roles {param($s)
                if([int](Local-Private $s.Host).woodCount -ne $wood-1){return 'wood was not consumed once'}
                foreach($role in $s.Keys){$t=Terrain $s[$role] 'PrototypeTerrainFloor'; if(-not $t.hasSupport -or [int]$t.supportStrength -ne 2){return "$role floor support not ready"}}
                return $null
            } 15
            return [ordered]@{sequence=$seq;before=$before;after=$after}
        }
        Invoke-Step 'ten-simultaneous-digs' {
            $points=@(@(4120,-210),@(3980,-70),@(3980,70),@(4120,210))
            Move-NearPoint 'Host' $points[0][0] $points[0][1]
            for($i=1;$i -lt 4;$i++){
                # Client3 starts north of the elevator station: pass its north side.
                if($roles[$i] -eq 'Client3'){Move-NearPoint 'Client3' 2850 450; Move-NearPoint 'Client3' 3250 450}
                Move-NearPoint $roles[$i] $points[$i][0] $points[$i][1]
            }
            $rounds=@(); $changedRounds=0; $selected=$null
            for($i=0;$i -lt 10;$i++){
                if($i -gt 0 -and $i%3 -eq 0){
                    Look-At 'Host' 4300 0 0
                    $repair=Send-Checked 'Host' 'SupportTerrain'
                    $null=Wait-Condition 'floor support repaired or already full' $roles {param($s) foreach($role in $s.Keys){$t=Terrain $s[$role] 'PrototypeTerrainFloor'; if(-not $t.hasSupport -or [int]$t.supportStrength -ne 2){return "$role support needs repair"}}; return $null} 10
                }
                $candidatePoints=@(@(4050,0,0),@(4100,0,0),@(4150,0,0),@(4100,40,0),@(4100,-40,0),@(4150,40,0),@(4150,-40,0),@(4200,0,-75))
                $aims=$null
                if($i -eq 0){foreach($candidate in $candidatePoints){
                    $candidateAims=[ordered]@{}; $common=$true
                    foreach($role in $roles){
                        Look-At $role $candidate[0] $candidate[1] $candidate[2]
                        $view=Read-ReadyState $role
                        if(-not $view.aimTrace.hit -or $view.aimTrace.actor -ne (Terrain $view 'PrototypeTerrainFloor').name){$common=$false; break}
                        $candidateAims[$role]=$view.aimTrace
                    }
                    if($common){
                        $anchor=$candidateAims.Host
                        foreach($role in $roles){if((Get-Distance $anchor $candidateAims[$role]) -gt 125){$common=$false;break}}
                    }
                    if($common){$selected=$candidate; $aims=$candidateAims; break}
                }
                if($null -eq $selected){throw 'no common reachable terrain surface for the initial contention'}
                } else {
                    $aims=[ordered]@{}
                    foreach($role in $roles){Look-At $role $selected[0] $selected[1] $selected[2]; $view=Read-ReadyState $role; $aims[$role]=$view.aimTrace}
                }
                $before=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 15
                $rev=[long](Terrain $before.Host 'PrototypeTerrainFloor').revision
                $fire=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()+2500; $seqs=[ordered]@{}; $fired=[ordered]@{}
                foreach($role in $roles){$seqs[$role]=Send-Command $role 'Dig' ([ordered]@{fireAtUtcMs=$fire})}
                foreach($role in $roles){$armed=Wait-Ack $role $seqs[$role] (New-Deadline 8) $false; if([long]$armed.scheduledSequence -ne $seqs[$role] -or $armed.scheduledStatus -ne 'Pending'){throw "$role did not arm dig"}}
                foreach($role in $roles){$fired[$role]=Wait-ScheduledSent $role $seqs[$role] $fire (New-Deadline 10)}
                $times=@($roles | ForEach-Object {[long]$fired[$_].scheduledFiredUtcMs} | Sort-Object); $skew=$times[-1]-$times[0]
                if($skew -gt 100){throw "simultaneous dig skew $skew ms"}
                Start-Sleep -Seconds 1
                $after=Wait-Terrain $roles 'PrototypeTerrainFloor' $rev 20
                $old=Terrain $before.Host 'PrototypeTerrainFloor'; $new=Terrain $after.Host 'PrototypeTerrainFloor'
                if([long]$new.revision -lt $rev){throw 'terrain revision regressed'}
                if([long]$new.revision -eq $rev -and [long]$new.hash -ne [long]$old.hash){throw 'hash changed without revision'}
                if([long]$new.revision -gt $rev){if([long]$new.hash -eq [long]$old.hash){throw 'revision changed without observable density change'}; $changedRounds++}
                if($i -eq 0 -and [long]$new.revision -eq $rev){throw 'initial common terrain contention produced no observable change'}
                $rounds+=@([ordered]@{index=$i;target=$selected;aims=$aims;fireAtUtcMs=$fire;skewMs=$skew;before=$before;after=$after})
            }
            if($changedRounds -lt 1){throw 'no simultaneous round produced an observable density change'}
            return [ordered]@{changedRounds=$changedRounds;rounds=$rounds}
        }
        Invoke-Step 'reconnect-same-fixture' {
            $before=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 20
            $entry=$script:Procs.Client3; $entry.ExpectedExit=$true; $seq=Send-Command 'Client3' 'Quit'
            if(-not $entry.Process.WaitForExit(20000)){throw 'Client3 did not exit'}
            Start-Probe 'Client3Reconnect' "${LoopbackHost}:$Port" $false
            $script:roles=@('Host','Client1','Client2','Client3Reconnect')
            $null=Wait-Roster 'reconnected roster' $roles 80
            $after=Wait-Terrain $roles 'PrototypeTerrainFloor' ([long](Terrain $before.Host 'PrototypeTerrainFloor').revision) 30
            if((Local-Private $after.Client3Reconnect).fixtureId -ne 'Client3'){throw 'fixture ID differs'}
            if((Terrain $before.Host 'PrototypeTerrainFloor').hash -ne (Terrain $after.Host 'PrototypeTerrainFloor').hash){throw 'terrain changed during reconnect'}
            return [ordered]@{quitSequence=$seq;before=$before;after=$after}
        }
    }
    if($Players -eq 1){
    Invoke-Step 'enter-and-return-floor-bowl' {
        $before=Read-ReadyState 'Host'
        # Open a route from the bowl toward the physical west staircase.
        foreach($y in @(100,200)){
            Look-At 'Host' 4250 $y 0
            $prior=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 15
            $rev=[long](Terrain $prior.Host 'PrototypeTerrainFloor').revision
            $null=Send-Checked 'Host' 'Dig'
            Start-Sleep -Seconds 1
            $null=Wait-Terrain $roles 'PrototypeTerrainFloor' $rev 20
            $null=Capture-View 'Host' "approach-stair-$y"
        }
        Move-NearPoint 'Host' 4200 0
        $insideStates=Wait-Condition 'pawn descends below surface' @('Host') {param($s) $p=@($s.Host.players | Where-Object {$_.isLocal}); if($p.Count -ne 1 -or [double]$p[0].z -ge 0){return 'pawn has not descended below z=0'}; return $null} 8
        $inside=$insideStates.Host
        $null=Capture-View 'Host' 'inside-bowl'
        # From inside, widen the passage at body height toward the exposed stair.
        foreach($y in @(120,220)){
            Look-At 'Host' 4250 $y -120
            $prior=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 15
            $rev=[long](Terrain $prior.Host 'PrototypeTerrainFloor').revision
            $null=Send-Checked 'Host' 'Dig'
            $null=Wait-Terrain $roles 'PrototypeTerrainFloor' ($rev+1) 20
            $null=Capture-View 'Host' "inside-dig-$y"
        }
        Move-CarvingRoute 'Host' 4325 300
        # The stairs are embedded in the original soil; clear their next step
        # through the same server-authoritative dig before advancing.
        foreach($x in @(4200,4125,4050,4000)){
            Look-At 'Host' $x 300 0
            $prior=Wait-Terrain $roles 'PrototypeTerrainFloor' 0 15
            $rev=[long](Terrain $prior.Host 'PrototypeTerrainFloor').revision
            $null=Send-Checked 'Host' 'Dig'
            Start-Sleep -Seconds 1
            $null=Wait-Terrain $roles 'PrototypeTerrainFloor' $rev 15
            $null=Capture-View 'Host' "stair-dig-$x"
            Move-CarvingRoute 'Host' $x 300
        }
        Move-NearPoint 'Host' 3900 0
        $after=Read-ReadyState 'Host'; $p=@($after.players | Where-Object {$_.isLocal})[0]
        if([double]$p.x -gt 3950 -or [double]$p.z -lt 50){throw 'pawn did not return from bowl'}
        return [ordered]@{before=$before;inside=$inside;after=$after}
    }
    Invoke-Step 'wall-carve' {
        # Route around the excavated floor instead of steering across the bowl.
        Move-NearPoint 'Host' 3900 600
        Move-NearPoint 'Host' 5050 600
        Move-NearPoint 'Host' 5050 0
        $before=Wait-Terrain $roles 'PrototypeTerrainWall' 0 20; $trace=Trace $before.Host 'wall'
        if(-not $trace.terrain){throw 'wall sample obstructed'}
        Look-At 'Host' $trace.x 0 200
        $revision=[long](Terrain $before.Host 'PrototypeTerrainWall').revision
        $seq=Send-Checked 'Host' 'Dig'; $after=Wait-Terrain $roles 'PrototypeTerrainWall' ($revision+1) 20
        $null=Capture-View 'Host' 'wall-dig'
        foreach($role in $roles){
            $prior=Trace $before[$role] 'wall'; $depth=Trace $after[$role] 'wall'
            if(-not $prior.terrain -or -not $depth.terrain -or [double]$depth.distance -le [double]$prior.distance+1){throw "$role wall collision surface did not deepen"}
            if([math]::Abs([double]$depth.distance-[double](Trace $after.Host 'wall').distance) -gt 2){throw "$role wall collision depth differs from host"}
        }
        return [ordered]@{command=$seq;before=$before;after=$after}
    }
    Invoke-Step 'wall-support' {
        # Carving moves the wall away; approach the new surface within tool reach.
        Move-NearPoint 'Host' 5150 0
        $before=Read-ReadyState 'Host'; $wood=[int](Local-Private $before).woodCount
        if([int](Local-Private $before).life -ne 0){throw 'host is not active before wall support'}
        if($wood -lt 1){throw 'no wood for support'}
        $trace=Trace $before 'wall'; if(-not $trace.terrain){throw 'wall surface obstructed before support'}
        Look-At 'Host' $trace.x 0 200
        $aim=Read-ReadyState 'Host'
        if(-not $aim.aimTrace.hit -or $aim.aimTrace.actor -ne (Terrain $aim 'PrototypeTerrainWall').name){throw 'new wall surface is outside reach or obstructed'}
        $seq=Send-Checked 'Host' 'SupportTerrain'
        $after=Wait-Condition 'support replicates and consumes one wood' $roles {param($s) if([int](Local-Private $s.Host).woodCount -ne $wood-1){return 'wood was not consumed once'}; foreach($role in $s.Keys){$t=Terrain $s[$role] 'PrototypeTerrainWall'; if(-not $t.hasSupport -or [int]$t.supportStrength -ne 2){return "$role support is not installed at strength 2"}}; return $null} 15
        return [ordered]@{command=$seq;before=$before;after=$after}
    }
    }
    Assert-ProcessesAlive
} catch { [void]$script:Failures.Add($_.Exception.Message); Write-Host "[fail] $($_.Exception.Message)" }
finally {
    try { Invoke-Cleanup } catch { [void]$script:Failures.Add("Cleanup error: $($_.Exception.Message)") }
    try { Inspect-ProcessLogs } catch { [void]$script:Failures.Add("Log inspection error: $($_.Exception.Message)") }
    $passed=$script:Failures.Count -eq 0 -and $script:Steps.Count -gt 0
    if($script:RunDir){$script:Result['passed']=$passed; $script:Result['failures']=@($script:Failures); $script:Result['failure']=$(if($script:Failures.Count){[string]$script:Failures[0]}else{$null}); $script:Result['finishedUtc']=Get-UtcNow; $script:Result['exitCode']=$(if($passed){0}else{1}); try{Save-Result}catch{$passed=$false;Write-Host "result write failed: $($_.Exception.Message)"}; Write-Host "[result] passed=$passed $script:RunDir\result.json"}
    $exitCode=$(if($passed){0}else{1})
}
exit $exitCode
