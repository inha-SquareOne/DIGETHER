<#
.SYNOPSIS
Packaged-build listen-server multiplayer check driven by the development-only DigetherNetworkProbe.

.DESCRIPTION
Starts one listen host (loopback only) and up to four hidden NullRHI processes from the exact packaged executable,
reads each process's probe state.json, and verifies roster, roles, replicated movement of Client1, Host, Client2 and
Client3 (one Move/Stop each), Client3 quit and Client3 reconnect.
Template movement/connection behaviour is observed only; no game policy is defined here.
PositionToleranceCm is a test acceptance tolerance, not a product performance target.
Exit code 0 = every step passed and every own process was reclaimed cleanly, 1 = any failure.
Evidence is written to <run>\result.json.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\validation\Invoke-PackagedMultiplayerValidation.ps1 -GameExe D:\Pkg\Windows\DIGETHER\Binaries\Win64\DIGETHER.exe
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [string]$OutputRoot,
    [ValidateRange(1024, 65535)][int]$Port = 0,
    [ValidateRange(60, 570)][int]$TimeoutSeconds = 420,
    [double]$PositionToleranceCm = 2.0,
    [double]$MoveDurationSeconds = 0.8
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$MapPackage = '/Game/ThirdPerson/Lvl_ThirdPerson'
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
    $argList = @($FirstArgument)
    if ($IsHost) { $argList += @("-port=$Port", "-multihome=$LoopbackHost") }
    $argList += @(
        '-nosteam', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-nosound', '-forcelogflush',
        "-UserDir=`"$userDir`"", "-abslog=`"$log`"",
        "-DigetherNetworkProbe=$Role", "-DigetherNetworkProbeDir=`"$dir`"",
        "-DigetherNetworkProbeMaxSeconds=$script:ProbeMaxSeconds"
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
        ExitedBeforeCleanup = $null; QuitSequence = $null; CleanupNote = $null
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

function Read-State([string]$Role) {
    $path = Join-Path $script:Procs[$Role].Dir 'state.json'
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try {
        $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        try {
            $reader = New-Object IO.StreamReader($stream, [Text.Encoding]::UTF8)
            $text = $reader.ReadToEnd()
        } finally { $stream.Dispose() }
        if ([string]::IsNullOrWhiteSpace($text)) { return $null }
        return ($text | ConvertFrom-Json)
    } catch { return $null }
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
    $step = [ordered]@{ name = $Name; startedUtc = Get-UtcNow; finishedUtc = $null; passed = $false; error = $null; evidence = $null }
    [void]$script:Steps.Add($step)
    try {
        $step.evidence = & $Body
        $step.passed = $true
    } catch {
        $step.error = $_.Exception.Message
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
        } elseif ($entry.ExpectedExit -and $entry.Process.ExitCode -ne 0) {
            [void]$script:Failures.Add("Process $($entry.Role) (pid $($entry.Pid)) exited with code $($entry.Process.ExitCode) after Quit.")
        }
    }
    $script:Result['cleanup'] = [ordered]@{
        remainingOwnedProcesses = $remaining
        childProcessesReclaimed = 'none: the packaged exe is started directly and only runner-started root processes are ever terminated'
    }
}

$exitCode = 1
$startedUtc = Get-UtcNow
try {
    $script:GlobalDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if (-not $OutputRoot) { $OutputRoot = Join-Path $repoRoot 'DIGETHER\Saved\DeveloperChecks' }
    $OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
    $runId = [guid]::NewGuid().ToString('N')
    $script:RunDir = Join-Path $OutputRoot ("PackagedMultiplayer-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $runId)
    if (Test-Path -LiteralPath $script:RunDir) { throw "Run directory already exists: $script:RunDir" }
    New-Item -ItemType Directory -Path $script:RunDir -Force | Out-Null

    $script:Result = [ordered]@{
        runId = $runId; startedUtc = $startedUtc; finishedUtc = $null; passed = $false; failure = $null; failures = @()
        gameExe = $GameExe; gameExeSha256 = $null; runDirectory = $script:RunDir
        port = $Port; host = $LoopbackHost; map = $MapPackage; timeoutSeconds = $TimeoutSeconds
        criteria = [ordered]@{
            positionToleranceCm = $PositionToleranceCm
            positionToleranceNote = 'Test acceptance tolerance for host-relative coordinate convergence; not a product performance target.'
            moveDurationSeconds = $MoveDurationSeconds
            minObservedDisplacementCm = $MinDisplacementCm; settleEpsilonCm = $SettleEpsilonCm
            readyConsecutiveSamples = $ReadyStreak; settleConsecutiveSamples = $SettleStreak
        }
        notes = [ordered]@{
            client3Reconnect = 'Client3Reconnect is only observed (roster, roles, coordinate convergence); no movement command is sent to it.'
            movement = 'Client1, Host, Client2 and Client3 each receive one Move then Stop through the probe; no teleport or forced position change is used.'
        }
        steps = @(); processes = @()
    }
    Save-Result

    foreach ($v in @($PositionToleranceCm, $MoveDurationSeconds)) {
        if ([double]::IsNaN($v) -or [double]::IsInfinity($v)) { throw 'PositionToleranceCm and MoveDurationSeconds must be finite numbers.' }
    }
    if ($PositionToleranceCm -le 0 -or $MoveDurationSeconds -le 0 -or $MoveDurationSeconds -gt 2) {
        throw 'PositionToleranceCm must be > 0 and MoveDurationSeconds must be in (0, 2].'
    }
    if (-not [IO.Path]::IsPathRooted($GameExe)) { throw 'GameExe must be an absolute path.' }
    $script:ExePath = [IO.Path]::GetFullPath($GameExe)
    if (-not (Test-Path -LiteralPath $script:ExePath -PathType Leaf) -or [IO.Path]::GetExtension($script:ExePath) -ne '.exe') {
        throw "GameExe is not an existing .exe file: $script:ExePath"
    }
    if ([IO.Path]::GetDirectoryName($script:ExePath) -notmatch '\\Binaries\\Win64$') {
        throw 'GameExe must be the actual packaged executable under Binaries\Win64, not the launcher stub.'
    }
    if ($Port -eq 0) {
        $rng = New-Object System.Random
        for ($i = 0; $i -lt 100 -and $Port -eq 0; $i++) {
            $candidate = $rng.Next(30000, 40000)
            if (Test-UdpPortFree $candidate) { $Port = $candidate }
        }
        if ($Port -eq 0) { throw 'No free UDP port found.' }
    } elseif (-not (Test-UdpPortFree $Port)) {
        throw "UDP port $Port is already in use; refusing to touch existing processes."
    }
    $script:Result['port'] = $Port
    $script:Result['gameExe'] = $script:ExePath

    $script:ProbeMaxSeconds = [math]::Min(600, $TimeoutSeconds + 30)
    $script:Result['gameExeSha256'] = (Get-FileHash -LiteralPath $script:ExePath -Algorithm SHA256).Hash
    Save-Result

    Invoke-Step 'host-ready-1-player' {
        Start-Probe 'Host' $MapUrl $true
        Wait-Roster 'host-ready-1-player' @('Host') 120
    }
    Invoke-Step 'client1-joined-2-players' {
        Start-Probe 'Client1' "${LoopbackHost}:$Port" $false
        Wait-Roster 'client1-joined-2-players' @('Host', 'Client1') 120
    }
    Invoke-Step 'client2-client3-joined-4-players' {
        Start-Probe 'Client2' "${LoopbackHost}:$Port" $false
        Start-Probe 'Client3' "${LoopbackHost}:$Port" $false
        Wait-Roster 'client2-client3-joined-4-players' @('Host', 'Client1', 'Client2', 'Client3') 150
    }

    $fourRoles = @('Host', 'Client1', 'Client2', 'Client3')
    Invoke-Step 'move-client1' { Invoke-MoveCheck 'Client1' $fourRoles ([ordered]@{ x = 1; y = 0; z = 0 }) }
    Invoke-Step 'move-host' { Invoke-MoveCheck 'Host' $fourRoles ([ordered]@{ x = 1; y = 0; z = 0 }) }
    Invoke-Step 'move-client2' { Invoke-MoveCheck 'Client2' $fourRoles ([ordered]@{ x = 0; y = 1; z = 0 }) }
    Invoke-Step 'move-client3' { Invoke-MoveCheck 'Client3' $fourRoles ([ordered]@{ x = 0; y = -1; z = 0 }) }

    Invoke-Step 'client3-quit-3-players' {
        $quit = $script:Procs['Client3']
        $quit.ExpectedExit = $true
        $quitSeq = Send-Command 'Client3' 'Quit'
        $quit.QuitSequence = $quitSeq
        $null = Wait-Ack 'Client3' $quitSeq (New-Deadline 30) $true
        if (-not $quit.Process.WaitForExit(30000)) { throw 'Client3 did not exit after the acknowledged Quit.' }
        if ($quit.Process.ExitCode -ne 0) { throw "Client3 exited with code $($quit.Process.ExitCode)." }
        $final = Read-State 'Client3'
        if ($null -eq $final -or [long]$final.lastCommandSequence -ne $quitSeq -or $final.lastCommandStatus -ne 'Applied') { throw 'Client3 final state.json lacks the Quit acknowledgement.' }
        $roster = Wait-Roster 'client3-quit-3-players' @('Host', 'Client1', 'Client2') 90
        $roster['client3ExitCode'] = $quit.Process.ExitCode
        $roster['client3QuitSequence'] = $quitSeq
        $roster['client3FinalAck'] = [ordered]@{ lastCommandSequence = $final.lastCommandSequence; lastCommandStatus = $final.lastCommandStatus }
        $roster
    }
    Invoke-Step 'client3-reconnect-4-players' {
        Start-Probe 'Client3Reconnect' "${LoopbackHost}:$Port" $false
        $roster = Wait-Roster 'client3-reconnect-4-players' @('Host', 'Client1', 'Client2', 'Client3Reconnect') 120
        $roster['client3ReconnectObservedOnly'] = $true
        $roster
    }

    Assert-ProcessesAlive
} catch {
    [void]$script:Failures.Add($_.Exception.Message)
    Write-Host "[fail] $($_.Exception.Message)"
} finally {
    try { Invoke-Cleanup } catch { [void]$script:Failures.Add("Cleanup error: $($_.Exception.Message)") }
    $passed = ($script:Failures.Count -eq 0)
    $exitCode = $(if ($passed) { 0 } else { 1 })
    if ($script:RunDir) {
        $script:Result['passed'] = $passed
        $script:Result['failures'] = @($script:Failures)
        $script:Result['failure'] = $(if ($script:Failures.Count -gt 0) { [string]$script:Failures[0] } else { $null })
        $script:Result['finishedUtc'] = Get-UtcNow
        $script:Result['exitCode'] = $exitCode
        try { Save-Result } catch { Write-Host "[warn] result.json write failed: $($_.Exception.Message)"; $exitCode = 1; $passed = $false }
        Write-Host "[result] passed=$passed $script:RunDir\result.json"
    } else {
        Write-Host "[result] passed=$passed (no run directory was created)"
    }
}
exit $exitCode
