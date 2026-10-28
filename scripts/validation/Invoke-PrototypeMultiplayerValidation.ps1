<#
.SYNOPSIS
Runs a prototype gameplay scenario in isolated listen-server processes.
.DESCRIPTION
Pass -Players 1, 2, and 4 in separate invocations. The four-player run attempts ten contested pickups across two expeditions.
A command acknowledgement only means the RPC was sent; state transitions are independently checked.
The trial fixture, untested features, process arguments, state snapshots, and failure details are retained in result.json.
.EXAMPLE
powershell -NoProfile -File scripts/validation/Invoke-PrototypeMultiplayerValidation.ps1 -GameExe D:/Package/DIGETHER.exe -MapPackage /Game/Prototype/Maps/L_Prototype -Players 4
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$MapPackage,
    [Alias("Project")][string]$ProjectPath,
    [ValidateSet(1,2,4)][int]$Players = 4,
    [string]$OutputRoot,
    [ValidateRange(1024, 65535)][int]$Port = 0,
    [ValidateRange(60, 570)][int]$TimeoutSeconds = 570,
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
    $argList += @(
        '-nosteam', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-nosound', '-forcelogflush',
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
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){$reason='snapshot missing during replacement or startup'}
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
$exitCode = 1
try {
    $script:GlobalDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if (-not $OutputRoot) { $OutputRoot = Join-Path $repoRoot 'DIGETHER\Saved\DeveloperChecks' }
    $runId = [guid]::NewGuid().ToString('N')
    $script:RunId = $runId
    $script:RunDir = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ("Prototype-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $runId)
    New-Item -ItemType Directory -Path $script:RunDir | Out-Null
    $script:Result = [ordered]@{runId=$runId; startedUtc=Get-UtcNow; finishedUtc=$null; passed=$false; failure=$null; failures=@();
        players=$Players; gameExe=$GameExe; project=$ProjectPath; map=$MapPackage; port=$Port; timeoutSeconds=$TimeoutSeconds;
        fixture='GameMode and WorldBuilder use trial values. Movement uses character input, including fixed base steering and separated contention flanks; actions use ServerInteract. Four-player contention uses distinct role-specific waiting lanes and separate north/south corridors; actual digging and support happen after contention before return. Contending local processes prepare and fire on a shared UTC clock, with a 100 ms maximum observed send skew. This is not a navigation or network-latency test. No gameplay state is forced by the probe.';
        limitations=@('The local fixture ID is not authentication; save restoration and request-id deduplication are not verified.', 'Owner versus host and default other-player views are compared; packet-level privacy is not proven by state snapshots.', 'Mid-run admission denial is attempted in the two-player run and fixture-ID reconnect in the four-player run.', 'Visual/UI, M3 and voice are not exercised.');
        steps=@(); processes=@()}
    Save-Result
    if (-not [IO.Path]::IsPathRooted($GameExe) -or -not (Test-Path -LiteralPath $GameExe -PathType Leaf)) { throw 'GameExe must be an existing absolute executable path.' }
    $script:ExePath = [IO.Path]::GetFullPath($GameExe)
    if ($ProjectPath -and (-not [IO.Path]::IsPathRooted($ProjectPath) -or -not (Test-Path -LiteralPath $ProjectPath -PathType Leaf))) { throw 'ProjectPath must be an existing absolute uproject path.' }
    if ($Port -eq 0) {
        $rng = New-Object System.Random
        for ($i=0; $i -lt 100 -and $Port -eq 0; $i++) { $candidate=$rng.Next(30000,40000); if (Test-UdpPortFree $candidate) { $Port=$candidate } }
        if ($Port -eq 0) { throw 'No free UDP port found.' }
    } elseif (-not (Test-UdpPortFree $Port)) { throw "UDP port $Port is already in use." }
    $script:Result['port']=$Port
    $script:Result['gameExeSha256']=(Get-FileHash -LiteralPath $script:ExePath -Algorithm SHA256).Hash
    $script:ProbeMaxSeconds=[math]::Min(600,$TimeoutSeconds+30)
    $roles = @('Host','Client1','Client2','Client3')[0..($Players-1)]
    Invoke-Step 'roster' {
        Start-Probe 'Host' $MapUrl $true
        $null=Wait-Roster 'host' @('Host') 100
        foreach ($role in @($roles | Where-Object { $_ -ne 'Host' })) { Start-Probe $role "${LoopbackHost}:$Port" $false }
        $null=Wait-Roster 'all players' $roles 120
        $states=Wait-Condition 'prototype-ready' $roles { param($s) foreach($role in $s.Keys) { if (-not $s[$role].hasPrototype -or @($s[$role].targets).Count -eq 0) { return "$role missing prototype state/targets" }; if(@($s[$role].layoutSeeds).Count -ne 1){return "$role layout seed unavailable"}; foreach($kind in @(0,1,2)){if(@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypeStation') -and [int]$_.kind -eq $kind}).Count -ne 1){return "$role station $kind not ready"}} }; Check-Public $s } 90
        return $states
    }
    $script:FlankSides=@{}
    $remote3 = 'Client3'
    $rounds = $(if ($Players -eq 4) { 2 } else { 1 })
    for ($round=1; $round -le $rounds; $round++) {
        Invoke-Step "run-$round-start" {
            $target=Find-Target 'Host' 'DigetherPrototypeStation' 0
            Move-To 'Host' $target
            $previousRunId=(Read-ReadyState 'Host').public.runId
            $seq=Send-Interact 'Host' $target
            $states=Wait-Condition 'start' $roles { param($s) $bad=Check-Public $s; if($bad){return $bad}; if([int]$s.Host.public.phase -ne 1 -or [int]$s.Host.public.runNumber -ne $round){return 'phase/run not started'}; if($s.Host.public.runId -eq $previousRunId -or (($s.Host.public.runId -replace '-', '') -match '^0+$')){return 'new run ID not assigned'}; foreach($role in $s.Keys){if(@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup')}).Count -ne 10){return "$role run pickup fixture not ready"}}; return $null } 20
            return [ordered]@{command=$seq; states=$states}
        }
        if ($Players -eq 2 -and $round -eq 1) {
            Invoke-Step 'mid-run-new-join-denied' {
                Start-Probe 'NewJoin' "${LoopbackHost}:$Port" $false
                $entry=$script:Procs['NewJoin']
                $entry.ExpectedExit=$true
                $deadline=New-Deadline 35
                $found=$false
                while ((Get-Date) -lt $deadline) {
                    if (Test-Path -LiteralPath $entry.Log) {
                        try {
                            $stream=[IO.File]::Open($entry.Log,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                            try { $reader=New-Object IO.StreamReader($stream,[Text.Encoding]::UTF8); $log=$reader.ReadToEnd() } finally { $stream.Dispose() }
                            if ($log.Contains('탐사 중에는 새로 참가할 수 없습니다.')) { $found=$true; break }
                        } catch { }
                    }
                    if ($entry.Process.HasExited) { break }
                    Start-Sleep -Milliseconds $PollMs
                }
                if (-not $found) { throw 'No explicit mid-run admission rejection reason was observed in the new client log.' }
                $entry.AllowNonZeroExit=$true
                $roster=Wait-Roster 'roster after rejected join' $roles 20
                $script:RejectionProven=$true
                return [ordered]@{reason='탐사 중에는 새로 참가할 수 없습니다.'; attemptedPid=$entry.Pid; log=$entry.Log; roster=$roster}
            }
        }
        if($Players -ne 4) {
            Invoke-Step "run-$round-dig" {
                $target=Find-Target 'Host' 'DigetherPrototypeDigCell'
                Move-To 'Host' $target
                $seq=Send-Interact 'Host' $target
                $states=Wait-Condition 'dig replication' $roles { param($s) foreach($role in $s.Keys){$t=@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypeDigCell') -and [math]::Abs([double]$_.x-3400) -lt 10}); if($t.Count -ne 1 -or -not $t[0].excavated){return "$role dig not replicated"}}; return $null } 15
                return [ordered]@{command=$seq; states=$states}
            }
        }
        $pairs = @(@('Host','Client1'),@('Client2',$remote3),@('Host','Client2'),@('Client1',$remote3),@('Client2',$remote3))
        $count = $(if ($Players -eq 4) { 5 } else { 1 })
        for ($index=0; $index -lt $count; $index++) {
            Invoke-Step "run-$round-pickup-$index" {
                $contenders = @(if($Players -eq 4) { $pairs[$index] } else { 'Host' })
                $y = -450 + $index*100
                if($Players -eq 4){
                    foreach($role in $roles){
                        $state=Read-ReadyState $role
                        $life=(Local-Private $state).life
                        if([int]$life -ne 0){throw "$role cannot enter contention: life state is $life (burial/death is not bypassed)"}
                    }
                    foreach($role in $roles){if($contenders -notcontains $role){Park-OutsideExcavation $role}}
                    # The cell remains intact during contention; route below it and use separate flanks.
                    # Clear the upper participant's previous position before the lower participant enters.
                    Move-ContentionFlank $contenders[1] $y $true
                    Move-ContentionFlank $contenders[0] $y $false
                } else {
                    $t=Find-Target $contenders[0] 'DigetherPrototypePickup' -1 $y
                    Move-To $contenders[0] $t
                }
                $before=All-States $roles
                $beforeCount=0
                $beforeIds=@{}
                foreach($role in $roles){
                    $beforeIds[$role]=@((Local-Private $before[$role]).items | ForEach-Object {$_.id})
                    $beforeCount += $beforeIds[$role].Count
                }
                $authorityTarget=Find-Target 'Host' 'DigetherPrototypePickup' -1 $y
                $expectedItemId=[string]$authorityTarget.authorityItemId
                if([string]::IsNullOrWhiteSpace($expectedItemId) -or (($expectedItemId -replace '-', '') -match '^0+$')) {throw 'server target has no valid item ID'}
                foreach($role in $roles){if($beforeIds[$role] -contains $expectedItemId){throw 'target item ID was already carried'}}
                $seqs=[ordered]@{}
                $fired=[ordered]@{}
                $fireAtUtcMs=$null
                $skewMs=$null
                if($contenders.Count -gt 1){
                    # Local processes share this machine's UTC clock. Both peers prepare before a common fire time.
                    # The 100 ms send-skew limit is a test criterion, not a gameplay timing guarantee.
                    $fireAtUtcMs=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()+4000
                    foreach($role in $contenders){
                        $t=Find-Target $role 'DigetherPrototypePickup' -1 $y
                        $seqs[$role]=Send-Command $role 'ScheduleInteract' ([ordered]@{target=$t.name; alternate=$false; fireAtUtcMs=$fireAtUtcMs})
                    }
                    foreach($role in $contenders){
                        $armed=Wait-Ack $role $seqs[$role] (New-Deadline 8) $false
                        if([long]$armed.scheduledSequence -ne $seqs[$role] -or $armed.scheduledStatus -ne 'Pending' -or [long]$armed.scheduledAtUtcMs -ne $fireAtUtcMs){throw "$role did not prepare the scheduled target before firing"}
                    }
                    foreach($role in $contenders){$fired[$role]=Wait-ScheduledSent $role $seqs[$role] $fireAtUtcMs (New-Deadline 12)}
                    $times=@($contenders | ForEach-Object {[long]$fired[$_].scheduledFiredUtcMs} | Sort-Object)
                    $skewMs=$times[-1]-$times[0]
                    if($skewMs -gt 100){throw "contender RPC send skew $skewMs ms exceeds 100 ms"}
                } else {
                    $role=$contenders[0]
                    $t=Find-Target $role 'DigetherPrototypePickup' -1 $y
                    $seqs[$role]=Send-Command $role 'Interact' ([ordered]@{target=$t.name; alternate=$false})
                    $null=Wait-Ack $role $seqs[$role] (New-Deadline 10) $false
                }
                $winner=@{role=$null}
                $states=Wait-Condition 'pickup one winner' $roles { param($s)
                    $total=0; $ids=@(); $winners=@()
                    foreach($role in $s.Keys){
                        $items=@((Local-Private $s[$role]).items); $total+=$items.Count
                        $nowIds=@($items | ForEach-Object {$_.id}); $ids+=$nowIds
                        $added=@($nowIds | Where-Object {$beforeIds[$role] -notcontains $_})
                        $removed=@($beforeIds[$role] | Where-Object {$nowIds -notcontains $_})
                        if($removed.Count -ne 0){return "$role lost a previously held item"}
                        if($added.Count -gt 0){if($added.Count -ne 1 -or $added[0] -ne $expectedItemId -or $contenders -notcontains $role){return "$role gained unexpected item"}; $winners+=@($role)}
                    }
                    $bad=Check-Privacy $s; if($bad){return $bad}; $bad=Check-PrivacyDivergence $s; if($bad){return $bad}
                    if($total -ne $beforeCount+1 -or $winners.Count -ne 1){return "item count/winners $total/$($winners.Count), expected $($beforeCount+1)/1"}
                    if(@($ids | Select-Object -Unique).Count -ne $ids.Count){return 'duplicate item ID'}
                    foreach($role in $s.Keys){if(@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and [math]::Abs([double]$_.y-$y) -lt 10}).Count -ne 0){return "$role still sees item"}}
                    $winner.role=$winners[0]
                    return $null
                } 20
                return [ordered]@{contenders=$contenders; commands=$seqs; scheduledAtUtcMs=$fireAtUtcMs; sentStates=$fired; sendSkewMs=$skewMs; expectedItemId=$expectedItemId; winner=$winner.role; beforeCount=$beforeCount; states=$states}
            }
        }
        if ($Players -eq 4 -and $round -eq 2) {
            Invoke-Step 'client3-reconnect-preserves-state-and-pawn' {
                $target=Find-Target 'Client3' 'DigetherPrototypePickup' -1 50
                Move-To 'Client3' $target
                $pickupSeq=Send-Interact 'Client3' $target
                $states=Wait-Condition 'reconnect fixture pickup' $roles { param($s)
                    $p=Local-Private $s.Client3
                    if (@($p.items).Count -lt 1 -or [int]$p.money -le 0) { return 'Client3 needs earned money and carried item' }
                    if (@($s.Client3.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and [math]::Abs([double]$_.y-50) -lt 10}).Count -ne 0) { return 'fixture item remains' }
                    $bad=Check-Privacy $s; if($bad){return $bad}; return $null
                } 20
                $prior = Local-Private $states.Client3
                if ($prior.fixtureId -ne 'Client3') { throw 'client did not receive its expected fixture ID' }
                $priorId=[int]$states.Client3.localPlayerId
                $priorPlayer=@($states.Host.players | Where-Object { [int]$_.playerId -eq $priorId })
                if ($priorPlayer.Count -ne 1 -or -not $priorPlayer[0].pawnName) { throw 'Host does not see Client3 pawn before disconnect' }
                $pawnName=[string]$priorPlayer[0].pawnName
                $priorPosition=ConvertTo-Position $priorPlayer[0]
                $priorItems=@($prior.items | ForEach-Object {$_.id} | Sort-Object)
                $old=$script:Procs['Client3']; $old.ExpectedExit=$true
                $quitSeq=Send-Command 'Client3' 'Quit'; $old.QuitSequence=$quitSeq
                $quitAck=Wait-Ack 'Client3' $quitSeq (New-Deadline 15) $true
                if (-not $old.Process.WaitForExit(20000) -or $old.Process.ExitCode -ne 0) { throw 'Client3 did not cleanly disconnect' }
                $quitFinal=Read-State 'Client3'
                if ($null -eq $quitFinal -or [long]$quitFinal.lastCommandSequence -ne $quitSeq -or $quitFinal.lastCommandStatus -ne 'Applied') { throw 'Client3 final quit acknowledgement was missing' }
                $remaining=@('Host','Client1','Client2')
                $afterQuit=Wait-Condition 'disconnected pawn retained' $remaining {param($s)
                    $bad=Test-Roster $s $remaining; if($bad){return $bad}
                    $p=@($s.Host.worldPawns | Where-Object {$_.name -eq $pawnName})
                    if($p.Count -ne 1 -or $p[0].hasController){return 'host did not retain the unpossessed pawn'}
                    if((Get-Distance $priorPosition $p[0]) -gt 30){return 'retained pawn moved more than 30 cm'}
                    if([int]$s.Host.public.phase -ne 1){return 'expedition stopped after disconnect'}
                    return $null
                } 25
                Start-Probe 'Client3Reconnect' "${LoopbackHost}:$Port" $false
                $returning=@('Host','Client1','Client2','Client3Reconnect')
                $null=Wait-Roster 'reconnected roster' $returning 80
                $after=Wait-Condition 'reconnected identity and state' $returning {param($s)
                    $p=Local-Private $s.Client3Reconnect
                    if($p.fixtureId -ne 'Client3'){return 'fixture ID differs'}
                    foreach($field in @('money','health','life','woodCount','burialLevel','lastPayout')){if($p.$field -ne $prior.$field){return "restored $field differs"}}
                    $ids=@($p.items | ForEach-Object {$_.id} | Sort-Object)
                    if(($ids -join ',') -ne ($priorItems -join ',')){return 'restored item IDs differ'}
                    $id=[int]$s.Client3Reconnect.localPlayerId
                    $seen=@($s.Host.players | Where-Object {[int]$_.playerId -eq $id})
                    if($seen.Count -ne 1 -or $seen[0].pawnName -ne $pawnName){return 'host sees a different pawn'}
                    $bad=Check-Public $s; if($bad){return $bad}; $bad=Check-Privacy $s; if($bad){return $bad}; return $null
                } 35
                $script:remote3='Client3Reconnect'
                if($script:FlankSides.ContainsKey('Client3')){$script:FlankSides['Client3Reconnect']=$script:FlankSides['Client3']}
                $script:roles=$returning
                return [ordered]@{pickupSequence=$pickupSeq; quitSequence=$quitSeq; pawnName=$pawnName; before=$states; disconnected=$afterQuit; after=$after}
            }
        }
        if($Players -eq 4) {
            Invoke-Step "run-$round-dig-and-support" {
                foreach($role in $roles){
                    $state=Read-ReadyState $role
                    if([int](Local-Private $state).life -ne 0){throw "$role cannot reach safe parking because life state is not Active"}
                    Park-OutsideExcavation $role
                }
                Move-NearPoint 'Host' 3250 -650
                $target=Find-Target 'Host' 'DigetherPrototypeDigCell'
                Move-To 'Host' $target
                $seq=Send-Interact 'Host' $target
                $states=Wait-Condition 'dig replication' $roles {param($s)
                    foreach($role in $s.Keys){$cell=@($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypeDigCell') -and [math]::Abs([double]$_.x-3400) -lt 10});if($cell.Count -ne 1 -or -not $cell[0].excavated){return "$role dig not replicated"}}
                    return $null
                } 15
                Ensure-DigSupport $roles
                Move-NearPoint 'Host' 3250 -650
                Move-NearPoint 'Host' 2650 -650
                Move-NearPoint 'Host' 2650 -300
                return [ordered]@{digCommand=$seq; excavated=$states; supportedAndParked=(All-States $roles)}
            }
        }
        Invoke-Step "run-$round-settlement" {
            foreach($role in $roles){$target=Find-Target $role 'DigetherPrototypeStation' 1; Move-To $role $target}
            $beforeDrop=All-States $roles
            $totalValue=0
            $dropper=$null
            foreach($role in $roles){
                $items=@((Local-Private $beforeDrop[$role]).items)
                foreach($item in $items){if(-not $item.hidden -and -not $item.equipment){$totalValue += [int]$item.value}}
                if($null -eq $dropper -and $items.Count -gt 0){$dropper=$role}
            }
            if($null -eq $dropper){throw 'no carried item for the floor-cargo exercise'}
            $dropItem=@((Local-Private $beforeDrop[$dropper]).items)[0]
            $dropSeq=Send-Command $dropper 'Drop' ([ordered]@{slot=1})
            $null=Wait-Ack $dropper $dropSeq (New-Deadline 10) $false
            $floorStates=Wait-Condition 'floor item in cab' $roles {param($s)
                $picked=@($s.Host.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and $_.authorityItemId -eq $dropItem.id})
                if($picked.Count -ne 1 -or -not $picked[0].inCab -or $picked[0].reserved){return 'dropped item missing, outside cab, or already reserved'}
                if(@((Local-Private $s[$dropper]).items | Where-Object {$_.id -eq $dropItem.id}).Count -ne 0){return 'dropped item still carried'}
                foreach($role in $s.Keys){if($role -ne 'Host'){foreach($item in @($s[$role].targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup')})){if($item.PSObject.Properties.Name -contains 'authorityItemId' -or $item.PSObject.Properties.Name -contains 'authorityValue'){return "$role exposes server-only cargo detail"}}}}
                return $null
            } 20
            $target=Find-Target 'Host' 'DigetherPrototypeStation' 1
            $cabBefore=@($floorStates.Host.cabs)
            if($cabBefore.Count -ne 1){throw 'expected one observed cab'}
            $seq=Send-Interact 'Host' $target
            $expected=[math]::Floor($totalValue / $Players)
            $inTransit=Wait-Condition 'elevator ascending before settlement' $roles {param($s)
                foreach($role in $s.Keys){
                    $view=$s[$role]; $life=Local-Private $view
                    if([int]$view.public.phase -ne 1 -or ([int]$view.public.elevatorState -notin @(1,4)) -or [double]$view.public.elevatorProgress -le 0.15 -or [double]$view.public.elevatorProgress -ge 0.85){return "$role has not observed in-progress ascent"}
                    if([int]$life.life -ne 4 -or [int]$life.money -ne [int](Local-Private $beforeDrop[$role]).money -or [int]$life.lastPayout -ne [int](Local-Private $beforeDrop[$role]).lastPayout){return "$role settled before arrival"}
                    $player=@($view.players | Where-Object {$_.isLocal})
                    if($player.Count -ne 1 -or -not $player[0].attachParentIsCab){return "$role pawn not attached to cab"}
                    $cab=@($view.cabs)
                    $priorPlayer=@($beforeDrop[$role].players | Where-Object {$_.isLocal})
                    if($cab.Count -ne 1 -or $priorPlayer.Count -ne 1 -or [double]$cab[0].z -le [double]$cabBefore[0].z+20 -or [double]$player[0].z -le [double]$priorPlayer[0].z+20){return "$role has not observed cab and pawn rise"}
                    $visibleCargo=@($view.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and $_.reserved -and $_.attachParentIsCab})
                    if($visibleCargo.Count -ne 1){return "$role has not observed the reserved cargo attached to the cab"}
                }
                $cargo=@($s.Host.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and $_.authorityItemId -eq $dropItem.id})
                if($cargo.Count -ne 1 -or -not $cargo[0].reserved -or -not $cargo[0].attachParentIsCab){return 'floor cargo was not reserved and attached'}
                return $null
            } 20
            $states=Wait-Condition 'settlement after arrival' $roles {param($s)
                $bad=Check-Public $s; if($bad){return $bad}; $bad=Check-Privacy $s; if($bad){return $bad}
                if([int]$s.Host.public.phase -ne 0){return 'not base'}
                if(@($s.Host.targets | Where-Object {(Test-TargetType $_ 'DigetherPrototypePickup') -and $_.authorityItemId -eq $dropItem.id}).Count -ne 0){return 'floor cargo still exists after successful arrival'}
                foreach($role in $s.Keys){$p=Local-Private $s[$role]; if([int]$p.lastPayout -ne $expected -or [int]$p.money -ne [int](Local-Private $beforeDrop[$role]).money+$expected -or @($p.items).Count -ne 0){return "$role settlement differs from $expected"}}
                return $null
            } 30
            return [ordered]@{command=$seq; dropCommand=$dropSeq; dropper=$dropper; droppedItemId=$dropItem.id; totalValue=$totalValue; expectedShare=$expected; before=$beforeDrop; floor=$floorStates; transit=$inTransit; after=$states}
        }
        Invoke-Step "run-$round-deposit" {
            $before=Read-ReadyState 'Host'
            $money=[int](Local-Private $before).money
            $quota=[int]$before.public.quotaProgress
            $goal=[int]$before.public.quotaTarget
            if($money -le 0){throw 'no earned money to deposit'}
            $target=Find-Target 'Host' 'DigetherPrototypeStation' 2
            Move-To 'Host' $target
            $seq=Send-Interact 'Host' $target
            $started=Wait-Condition 'deposit debits' $roles {param($s) $p=Local-Private $s.Host; if([int]$p.money -ge $money){return 'deposit did not debit'}; if([int]$s.Host.public.quotaProgress -le $quota){return 'quota did not increase'}; return $null} 15
            if($Players -eq 4 -and $round -eq 2){
                $remaining=$goal-$quota
                if($money -lt $remaining){throw 'fixture has insufficient earned money to test goal completion'}
                $after=Wait-Condition 'quota reached once' $roles {param($s)
                    $bad=Check-Public $s; if($bad){return $bad}
                    if([int]$s.Host.public.quotaTarget -le $goal -or [int]$s.Host.public.quotaProgress -ne 0 -or [int]$s.Host.public.day -ne 1){return 'new quota period not reached'}
                    if([int](Local-Private $s.Host).money -ne $money-$remaining){return 'quota completion deducted wrong amount'}
                    return $null
                } 45
                return [ordered]@{command=$seq; targetReached=$true; before=$before; started=$started; after=$after}
            }
            $cancelSeq=Send-Command 'Host' 'CancelInteraction'
            $null=Wait-Ack 'Host' $cancelSeq (New-Deadline 10) $false
            $tracker=@{money=$null; quota=$null; streak=0}
            $after=Wait-Condition 'deposit canceled and stable' $roles {param($s)
                $bad=Check-Public $s; if($bad){$tracker.streak=0; return $bad}
                $m=[int](Local-Private $s.Host).money; $q=[int]$s.Host.public.quotaProgress
                if($m -ge $money -or $q -le $quota -or $money-$m -ne $q-$quota){$tracker.streak=0; return 'debit and credit do not match'}
                if($tracker.money -eq $m -and $tracker.quota -eq $q){$tracker.streak++}else{$tracker.streak=0}
                $tracker.money=$m; $tracker.quota=$q
                if($tracker.streak -lt 3){return 'waiting for canceled deposit to stop'}
                return $null
            } 15
            return [ordered]@{command=$seq; cancelCommand=$cancelSeq; targetReached=$false; before=$before; started=$started; after=$after}
        }
    }
    Invoke-Step 'next-run' {
        $target=Find-Target 'Host' 'DigetherPrototypeStation' 0
        Move-To 'Host' $target
        $previousRunId=(Read-ReadyState 'Host').public.runId
        $seq=Send-Interact 'Host' $target
        $states=Wait-Condition 'next run' $roles {param($s) $bad=Check-Public $s; if($bad){return $bad}; if([int]$s.Host.public.phase -ne 1 -or [int]$s.Host.public.runNumber -ne $rounds+1){return 'next run not started'}; if($s.Host.public.runId -eq $previousRunId -or (($s.Host.public.runId -replace '-', '') -match '^0+$')){return 'next run ID not assigned'}; return $null} 20
        return [ordered]@{command=$seq; states=$states}
    }
    if ($script:Steps.Count -eq 0) { throw 'No test steps executed.' }
    Assert-ProcessesAlive
} catch {
    [void]$script:Failures.Add($_.Exception.Message)
    Write-Host "[fail] $($_.Exception.Message)"
} finally {
    try { Invoke-Cleanup } catch { [void]$script:Failures.Add("Cleanup error: $($_.Exception.Message)") }
    try { Inspect-ProcessLogs } catch { [void]$script:Failures.Add("Log inspection error: $($_.Exception.Message)") }
    $passed = $script:Failures.Count -eq 0 -and $script:Steps.Count -gt 0
    if ($script:RunDir) {
        $script:Result['passed']=$passed; $script:Result['failures']=@($script:Failures)
        $script:Result['failure']=$(if($script:Failures.Count){[string]$script:Failures[0]}else{$null})
        $script:Result['finishedUtc']=Get-UtcNow; $script:Result['exitCode']=$(if($passed){0}else{1})
        try { Save-Result } catch { $passed=$false; Write-Host "result write failed: $($_.Exception.Message)" }
        Write-Host "[result] passed=$passed $script:RunDir\result.json"
    }
    $exitCode=$(if($passed){0}else{1})
}
exit $exitCode
