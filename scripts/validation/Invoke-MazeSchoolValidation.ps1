<#
.SYNOPSIS
Checks the mine maze, school portals, earned-money settlement and amount-entry payment.
.DESCRIPTION
Run -Players 1 -Rendered and -Players 4 separately. Movement uses normal input, UI payment
uses the same local entry points as the product, and malformed requests use its real RPC.
RPC acknowledgement is separate from authoritative and replicated outcome checks.
Process lifecycle helpers are reused from Invoke-ExcavationValidation.ps1.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameExe,
    [Parameter(Mandatory=$true)][string]$MapPackage,
    [Alias('Project')][string]$ProjectPath,
    [ValidateSet(1,4)][int]$Players=4,
    [string]$OutputRoot,
    [ValidateRange(1024,65535)][int]$Port=0,
    [ValidateRange(60,570)][int]$TimeoutSeconds=570,
    [switch]$Rendered,
    [double]$PositionToleranceCm=3.0
)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
if(-not $MapPackage.StartsWith('/Game/')){throw 'MapPackage must be a /Game/ package path.'}
$MapUrl="${MapPackage}?listen"
$LoopbackHost='127.0.0.1'
$ReadyStreak=3
$SettleStreak=5
$SettleEpsilonCm=0.5
$MinDisplacementCm=20.0
$StaleSeconds=5.0
$PollMs=100
$QuitWaitSeconds=20
$script:Procs=[ordered]@{}
$script:Steps=New-Object System.Collections.ArrayList
$script:Failures=New-Object System.Collections.ArrayList
$script:Sequences=@{}
$script:RunDir=$null
$script:ExePath=$null
$script:ProbeMaxSeconds=0
$script:GlobalDeadline=$null
$script:Result=[ordered]@{}
$script:ScreenshotRecords=New-Object System.Collections.ArrayList
$script:RouteDiagnostics=New-Object System.Collections.ArrayList
$script:StateReadStats=@{}
$script:WaypointProgress=New-Object System.Collections.ArrayList
$script:RejectionProven=$false

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

function Find-FixtureTarget([string]$Role,[string]$Tag) {
    $authority=Read-ReadyState 'Host'
    $found=@($authority.targets | Where-Object {$_.tags -contains $Tag})
    if($found.Count -ne 1){throw "Expected one authority target tagged $Tag, found $($found.Count)"}
    # Runtime tags are not replicated, and dynamically spawned actor names can
    # differ across processes. The fixed fixture's type and unique transform
    # identify its corresponding local actor without writing gameplay state.
    $view=Read-ReadyState $Role
    $local=@($view.targets | Where-Object {$_.name -eq $found[0].name})
    if($local.Count -eq 0) {
        $reference=$found[0]
        $local=@($view.targets | Where-Object {$_.semanticType -eq $reference.semanticType -and $_.class -eq $reference.class -and [math]::Abs([double]$_.x-[double]$reference.x) -lt 2 -and [math]::Abs([double]$_.y-[double]$reference.y) -lt 2 -and [math]::Abs([double]$_.z-[double]$reference.z) -lt 2})
    }
    if($local.Count -ne 1){throw "$Role cannot observe $Tag ($($found[0].name))"}
    return $local[0]
}

function Send-Action([string]$Role,[string]$Name,$Extra=$null) {
    $sequence=Send-Command $Role $Name $Extra
    $null=Wait-Ack $Role $sequence (New-Deadline 8) $false
    return $sequence
}

function Own-Position($State) {
    $pawn=@($State.players | Where-Object {$_.isLocal})
    if($pawn.Count -ne 1){throw "$($State.role) has no unique local pawn"}
    return ConvertTo-Position $pawn[0]
}

function Move-GroupTo([string[]]$Roles,$Goals,[string]$Label) {
    $deadline=New-Deadline 40
    for($iteration=0;(Get-Date) -lt $deadline;$iteration++) {
        $pending=[ordered]@{}
        foreach($role in $Roles) {
            $before=Read-ReadyState $role
            $pos=Own-Position $before
            $goal=$Goals[$role]
            $dx=[double]$goal.x-$pos.x; $dy=[double]$goal.y-$pos.y
            $distance=[math]::Sqrt($dx*$dx+$dy*$dy)
            if($distance -le 35){continue}
            $seconds=[math]::Min(1.2,[math]::Max(0.08,$distance/350.0))
            $seq=Send-Command $role 'Move' ([ordered]@{direction=[ordered]@{x=$dx;y=$dy;z=0};durationSeconds=$seconds})
            $pending[$role]=[ordered]@{sequence=$seq;before=$pos;distance=$distance;frames=[long]$before.moveInjectedFrames;duration=$seconds;sample=$null}
        }
        if($pending.Count -eq 0) {
            $states=All-States $Roles
            $snapshot=[ordered]@{label=$Label;utc=Get-UtcNow;goals=$Goals;states=$states}
            [void]$script:RouteDiagnostics.Add($snapshot)
            return $states
        }
        foreach($role in $pending.Keys){$null=Wait-Ack $role $pending[$role].sequence (New-Deadline 8) $false}
        $pulseDeadline=New-Deadline 8
        do {
            Assert-ProcessesAlive
            $complete=$true
            foreach($role in $pending.Keys) {
                $sample=Read-State $role
                if($null -eq $sample -or [long]$sample.lastCommandSequence -ne $pending[$role].sequence -or $sample.moveActive){$complete=$false}
                else {$pending[$role].sample=$sample}
            }
            if($complete){break}
            Start-Sleep -Milliseconds 50
        } while((Get-Date) -lt $pulseDeadline)
        if(-not $complete){throw "$Label movement pulse did not complete"}
        foreach($role in $pending.Keys) {
            $move=$pending[$role]; $sample=$move.sample; $pos=Own-Position $sample; $goal=$Goals[$role]
            $remaining=[math]::Sqrt([math]::Pow($pos.x-[double]$goal.x,2)+[math]::Pow($pos.y-[double]$goal.y,2))
            $progress=$move.distance-$remaining
            $injected=[long]$sample.moveInjectedFrames-$move.frames
            $passed=$remaining -le 35 -or ($injected -gt 0 -and $progress -ge 1)
            [void]$script:WaypointProgress.Add([ordered]@{label=$Label;role=$role;sequence=$move.sequence;utc=Get-UtcNow;durationSeconds=$move.duration;frames=$injected;goal=$goal;before=$move.before;after=$pos;beforeCm=$move.distance;afterCm=$remaining;progressCm=$progress;passed=$passed})
            if(-not $passed){throw "$Label $role blocked: progress=$([math]::Round($progress,2))cm, remaining=$([math]::Round($remaining,2))cm, position=$($pos | ConvertTo-Json -Compress)"}
        }
    }
    throw "$Label waypoint deadline exceeded"
}

function Move-Formation([string[]]$Roles,[double]$X,[double]$Y,[string]$Label) {
    $offsets=@(@(-65,-65),@(65,-65),@(-65,65),@(65,65))
    $goals=@{}
    for($i=0;$i -lt $Roles.Count;$i++) {
        $offset=$(if($Roles.Count -eq 1){@(0,0)}else{$offsets[$i]})
        $goals[$Roles[$i]]=[ordered]@{x=$X+$offset[0];y=$Y+$offset[1]}
    }
    if($Rendered){Look-At 'Host' $X $Y 160}
    return Move-GroupTo $Roles $goals $Label
}

function Move-One([string]$Role,[double]$X,[double]$Y,[string]$Label) {
    $goals=@{}; $goals[$Role]=[ordered]@{x=$X;y=$Y}
    return Move-GroupTo @($Role) $goals $Label
}

function Assert-EconomyUnchanged($Before,$After) {
    foreach($role in $Before.Keys) {
        $old=Local-Private $Before[$role]; $now=Local-Private $After[$role]
        foreach($field in @('money','lastPayout')) {if($old.$field -ne $now.$field){return "$role $field changed during rejected request"}}
        if(($old.items | ConvertTo-Json -Depth 5 -Compress) -ne ($now.items | ConvertTo-Json -Depth 5 -Compress)){return "$role inventory changed during rejected request"}
    }
    foreach($field in @('phase','day','runId','runNumber','quotaTarget','quotaProgress')) {
        if($Before.Host.public.$field -ne $After.Host.public.$field){return "public $field changed during rejected request"}
    }
    return Check-Privacy $After
}

function Assert-Rejected([string]$Label,[string]$Role,[string]$Command,$Extra,[string[]]$Roles,[string]$ExpectedReason='') {
    $requester=$Role
    $before=All-States $Roles
    $seq=Send-Action $Role $Command $Extra
    $sentAt=(Get-Date).ToUniversalTime()
    $after=Wait-Condition $Label $Roles {param($states)
        $bad=Assert-EconomyUnchanged $before $states; if($bad){return $bad}
        if(((ConvertTo-Utc $states[$requester].utc)-$sentAt).TotalSeconds -lt 0.6){return 'waiting for authoritative response window'}
        if([string]::IsNullOrWhiteSpace([string]$states[$requester].lastServerMessage) -and [string]::IsNullOrWhiteSpace([string]$states[$requester].depositInput.error)){return 'server did not report rejection'}
        if($ExpectedReason -and (([string]$states[$requester].lastServerMessage+' '+[string]$states[$requester].depositInput.error) -notmatch $ExpectedReason)){return "server rejection reason does not match $ExpectedReason"}
        return $null
    } 12
    return [ordered]@{command=$seq;expectedReason=$ExpectedReason;observedMessage=$after[$requester].lastServerMessage;observedInputError=$after[$requester].depositInput.error;before=$before;after=$after}
}

function Assert-DepositMovementBlocked([string]$Role,[string[]]$Roles) {
    $inputOwner=$Role
    $before=All-States $Roles
    $origin=Own-Position $before[$inputOwner]
    $session=[int]$before[$inputOwner].depositInput.sessionId
    $frames=[long]$before[$inputOwner].moveInjectedFrames
    if(-not $before[$inputOwner].depositInput.open -or $session -le 0){throw 'Movement suppression requires an open deposit UI'}
    $seq=Send-Action $inputOwner 'Move' ([ordered]@{direction=[ordered]@{x=0;y=1;z=0};durationSeconds=0.6})
    $after=Wait-Condition 'open deposit UI suppresses normal movement input' $Roles {param($states)
        $view=$states[$inputOwner]
        if([long]$view.lastCommandSequence -ne $seq -or $view.moveActive){return 'movement command has not completed'}
        if([long]$view.moveInjectedFrames -le $frames){return 'probe did not inject movement frames'}
        if(-not $view.depositInput.open -or [int]$view.depositInput.sessionId -ne $session -or -not $view.depositInput.moveInputIgnored){return 'deposit UI did not retain movement suppression'}
        if((Get-Distance $origin (Own-Position $view)) -gt 3){return 'pawn moved while deposit input was active'}
        $authority=Get-Pos $states.Host ([int]$view.localPlayerId)
        if((Get-Distance $origin $authority) -gt 3){return 'authority moved pawn while deposit input was active'}
        return Assert-EconomyUnchanged $before $states
    } 10
    return [ordered]@{command=$seq;assertion='normal movement input is ignored while the deposit UI is open';before=$before;after=$after}
}

function Transfer-Portals([string[]]$Roles,[string]$Tag,[bool]$ToSchool) {
    if($Roles.Count -gt 1) {
        $portal=Find-FixtureTarget 'Host' $Tag
        $rowX=$(if($ToSchool){[double]$portal.x-225}else{[double]$portal.x+250})
        $goals=@{};$rowY=@(-180,-60,60,180)
        for($i=0;$i -lt $Roles.Count;$i++){$goals[$Roles[$i]]=[ordered]@{x=$rowX;y=[double]$portal.y+$rowY[$i]}}
        # The portal-facing row keeps another pawn out of every eye-to-portal ray.
        $null=Move-GroupTo $Roles $goals 'portal-clear-sight-row'
    }
    $portalTargets=@{};$sequences=@{}
    foreach($role in $Roles) {
        $portal=Find-FixtureTarget $role $Tag
        $portalTargets[$role]=$portal
        Look-At $role $portal.x $portal.y $portal.z
    }
    $null=Wait-Condition "portal $Tag aiming rays clear" $Roles {param($s)
        foreach($viewer in $s.Keys){if(-not $s[$viewer].aimTrace.hit -or $s[$viewer].aimTrace.actor -ne $portalTargets[$viewer].name){return "$viewer portal sight line is obstructed or outside reach"}}
        return $null
    } 10
    $before=All-States $Roles
    $fire=[long][math]::Floor(((Get-Date).ToUniversalTime()-[datetime]'1970-01-01').TotalMilliseconds)+5000
    foreach($role in $Roles) {
        $portal=$portalTargets[$role]
        $sequences[$role]=Send-Action $role 'ScheduleInteract' ([ordered]@{target=$portal.name;fireAtUtcMs=$fire;alternate=$false})
    }
    $after=Wait-Condition "portal $Tag transfer" $Roles {param($states)
        $bad=Check-Public $states; if($bad){return $bad}
        $bad=Check-Privacy $states; if($bad){return $bad}
        foreach($role in $states.Keys) {
            if([long]$states[$role].scheduledSequence -ne $sequences[$role] -or $states[$role].scheduledStatus -ne 'Sent'){return "$role portal RPC not sent"}
            $pos=Own-Position $states[$role]
            if($ToSchool -and ($pos.x -lt 19500 -or $pos.x -gt 22500)){return "$role not in school"}
            if([int]$states[$role].mineSceneCount -ne 1 -or @($states[$role].layoutSeeds).Count -ne 1){return "$role has missing or duplicate scene/builder"}
            if($ToSchool -and (-not $states[$role].localFloor.hit -or [double]$states[$role].localFloor.normalZ -lt 0.9 -or [math]::Abs([double]$states[$role].localFloor.z) -gt 5)){return "$role school floor collision not observed"}
            if(-not $ToSchool -and ($pos.x -lt 5200 -or $pos.x -gt 5700 -or $pos.y -lt 2600 -or $pos.y -gt 3400)){return "$role not back in mine"}
            $old=Local-Private $before[$role]; $now=Local-Private $states[$role]
            if($states[$role].public.runId -ne $before[$role].public.runId -or [int]$states[$role].public.phase -ne 1 -or $states[$role].public.day -ne $before[$role].public.day){return "$role portal changed run state"}
            if($old.money -ne $now.money -or $old.lastPayout -ne $now.lastPayout -or (($old.items | ForEach-Object {$_.id} | Sort-Object)-join ',') -ne (($now.items | ForEach-Object {$_.id} | Sort-Object)-join ',')){return "$role portal changed economy/inventory"}
            $authority=Get-Pos $states.Host ([int]$states[$role].localPlayerId)
            if((Get-Distance $pos $authority) -gt 3){return "$role portal position has not converged with authority"}
        }
        return $null
    } 15
    return [ordered]@{sequences=$sequences;before=$before;after=$after}
}

function Approach-Deposit([string]$Role) {
    $current=Own-Position (Read-ReadyState $Role)
    if($current.x -lt 0) {
        # The furnished base has a back wall at x=-500. This inner lane
        # clears both the INFO station and the other return-position pawns.
        $null=Move-One $Role -190 $current.y 'base-inner-lane'
        $innerLane=Own-Position (Read-ReadyState $Role)
        $null=Move-One $Role $innerLane.x 650 'base-north-clearance'
    } else {
        $null=Move-One $Role 500 650 'base-east-clearance'
    }
    $null=Move-One $Role 0 650 'base-deposit-approach'
    $null=Move-One $Role 0 440 'base-deposit-range'
}

function Open-Deposit([string]$Role) {
    $target=Find-FixtureTarget $Role 'PrototypeDeposit'
    Look-At $Role $target.x $target.y $target.z
    $seq=Send-Action $Role 'Interact' ([ordered]@{target=$target.name;alternate=$false})
    $state=Wait-Condition 'deposit UI opened' @($Role) {param($s) if(-not $s[$Role].depositInput.open){return 'deposit UI not open'}; return $null} 10
    return [ordered]@{command=$seq;state=$state}
}

function Submit-Amount([string]$Role,[int]$Amount,[string[]]$Roles) {
    $payingRole=$Role
    $before=All-States $Roles
    $input=Send-Action $Role 'DepositInput' ([ordered]@{text=[string]$Amount})
    $null=Wait-Condition 'entered amount visible' @($Role) {param($s) if($s[$Role].depositInput.text -ne [string]$Amount){return 'input text differs'}; return $null} 8
    $submit=Send-Action $Role 'DepositSubmit'
    $remaining=[int]$before.Host.public.quotaTarget-[int]$before.Host.public.quotaProgress
    $paid=[math]::Min($Amount,$remaining)
    $after=Wait-Condition 'amount submitted and replicated' $Roles {param($states)
        $bad=Check-Public $states; if($bad){return $bad}
        $bad=Check-Privacy $states; if($bad){return $bad}
        foreach($other in $states.Keys) {
            $debit=$(if($other -eq $payingRole){$paid}else{0})
            if([int](Local-Private $states[$other]).money -ne [int](Local-Private $before[$other]).money-$debit){return "$other payment debit differs"}
        }
        if($paid -eq $remaining) {
            if([int]$states.Host.public.quotaProgress -ne 0 -or [int]$states.Host.public.quotaTarget -ne 2*[int]$before.Host.public.quotaTarget -or [int]$states.Host.public.day -ne 1){return 'new quota period differs'}
        } elseif([int]$states.Host.public.quotaProgress -ne [int]$before.Host.public.quotaProgress+$paid){return 'quota credit differs'}
        return $null
    } 12
    $null=Send-Action $Role 'DepositClose'
    return [ordered]@{inputCommand=$input;submitCommand=$submit;sessionId=[int]$before[$payingRole].depositInput.sessionId;requested=$Amount;paid=$paid;before=$before;after=$after}
}

try {
    if($Rendered -and $Players -ne 1){throw 'Rendered mode is limited to the one-player run.'}
    $script:GlobalDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
    $repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if(-not $OutputRoot){$OutputRoot=Join-Path $repoRoot 'DIGETHER\Saved\DeveloperChecks'}
    $script:RunId=[guid]::NewGuid().ToString('N')
    $script:RunDir=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ("MazeSchool-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'),$script:RunId)
    New-Item -ItemType Directory -Path $script:RunDir -Force | Out-Null
    $script:Result=[ordered]@{runId=$script:RunId;startedUtc=Get-UtcNow;finishedUtc=$null;passed=$false;failure=$null;failures=@();players=$Players;gameExe=$GameExe;project=$ProjectPath;map=$MapPackage;port=$Port;timeoutSeconds=$TimeoutSeconds;rendered=[bool]$Rendered;fixture='Normal input in parallel waypoint pulses. Portals and settlement use real owning-controller RPCs; payments use product UI entry points. No money, phase, inventory or transform is forced by this runner.';limitations=@('Same-machine -nosteam processes, not M3.','Fixture identity is not authentication.','State privacy is checked; packet-level privacy is not proven.','Blocked destination, death and reconnect require separate tests.','Distant direct payment is attempted after UI cancellation: rejection proves stale-session protection, not the active-session distance branch. Distant Interact independently checks station opening distance.');steps=@();processes=@()}
    $script:Result['runnerSha256']=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $script:RunDir 'runner.ps1')
    Save-Result
    if(-not [IO.Path]::IsPathRooted($GameExe) -or -not (Test-Path -LiteralPath $GameExe -PathType Leaf)){throw 'GameExe must be an existing absolute executable path.'}
    $script:ExePath=[IO.Path]::GetFullPath($GameExe)
    if($ProjectPath -and (-not [IO.Path]::IsPathRooted($ProjectPath) -or -not (Test-Path -LiteralPath $ProjectPath -PathType Leaf))){throw 'ProjectPath must be an existing absolute uproject path.'}
    if($Port -eq 0){$rng=New-Object Random;for($i=0;$i -lt 100 -and $Port -eq 0;$i++){$candidate=$rng.Next(30000,40000);if(Test-UdpPortFree $candidate){$Port=$candidate}};if($Port -eq 0){throw 'No free UDP port found.'}}
    elseif(-not (Test-UdpPortFree $Port)){throw "UDP port $Port is already in use."}
    $script:Result['port']=$Port
    $script:Result['gameExeSha256']=(Get-FileHash -LiteralPath $script:ExePath -Algorithm SHA256).Hash
    $script:ProbeMaxSeconds=[math]::Min(600,$TimeoutSeconds+30)
    $roles=@('Host','Client1','Client2','Client3')[0..($Players-1)]

    Invoke-Step 'roster-and-fixture' {
        Start-Probe 'Host' $MapUrl $true
        $null=Wait-Roster 'host' @('Host') 100
        foreach($role in @($roles | Where-Object {$_ -ne 'Host'})){Start-Probe $role "${LoopbackHost}:$Port" $false}
        $states=Wait-Roster 'all players' $roles 120
        foreach($role in $roles){$null=Find-FixtureTarget $role 'PrototypeMinePortal';$null=Find-FixtureTarget $role 'PrototypeDeposit'}
        # The distant school portal may legitimately be culled before arrival.
        $null=Find-FixtureTarget 'Host' 'PrototypeSchoolPortal'
        return $states
    }
    Invoke-Step 'start-expedition' {
        $target=Find-FixtureTarget 'Host' 'PrototypeStart'
        $initial=Own-Position (Read-ReadyState 'Host')
        if($Players -gt 1){
            $null=Move-One 'Host' -190 $initial.y 'initial-separated-inner-lane'
            $initial=Own-Position (Read-ReadyState 'Host')
        }
        $null=Move-One 'Host' $initial.x 700 'initial-north-clearance'
        $null=Move-One 'Host' 500 700 'initial-start-east-approach'
        $null=Move-One 'Host' 500 0 'initial-start-south-approach'
        $null=Move-One 'Host' 300 0 'start-station-approach'
        $seq=Send-Action 'Host' 'Interact' ([ordered]@{target=$target.name;alternate=$false})
        $states=Wait-Condition 'expedition started' $roles {param($s)
            $bad=Check-Public $s;if($bad){return $bad}
            if([int]$s.Host.public.phase -ne 1 -or [int]$s.Host.public.runNumber -ne 1){return 'first expedition not active'}
            if(@($s.Host.targets | Where-Object {$_.semanticType -eq 'Pickup'}).Count -ne 10){return 'run pickups not ready'}
            return $null
        } 20
        $script:ExpeditionBefore=$states
        $script:ExpeditionStartedUtc=(Get-Date).ToUniversalTime()
        $script:Result['observedExpeditionBudget']=[ordered]@{initialRemainingSeconds=[double]$states.Host.public.remainingSeconds;observedUtc=Get-UtcNow;source='replicated runtime public state; Blueprint fixture may override the native default'}
        if($Rendered){
            $null=Send-Checked 'Host' 'ToggleHeadlamp'
            $null=Wait-Condition 'rendered headlamp input applied' @('Host') {param($s) if(-not $s.Host.headlampOn){return 'L input did not enable the headlamp'};return $null} 8
        }
        return [ordered]@{command=$seq;states=$states}
    }
    Invoke-Step 'reject-distant-portal-and-expedition-payment' {
        $portal=Find-FixtureTarget 'Host' 'PrototypeMinePortal'
        $before=All-States $roles
        $seq=Send-Action 'Host' 'Interact' ([ordered]@{target=$portal.name;alternate=$false})
        $after=Wait-Condition 'remote portal rejected' $roles {param($s)
            foreach($role in $s.Keys){if((Own-Position $s[$role]).x -gt 19000){return 'distant portal moved a pawn'}}
            return Assert-EconomyUnchanged $before $s
        } 5
        $target=Find-FixtureTarget 'Host' 'PrototypeDeposit'
        $payment=Assert-Rejected 'sessionless payment during expedition rejected' 'Host' 'DepositRpc' ([ordered]@{target=$target.name;amount=1}) $roles '만료'
        return [ordered]@{portalCommand=$seq;portalBefore=$before;portalAfter=$after;payment=$payment}
    }
    Invoke-Step 'mine-maze-outbound' {
        $null=Move-Formation $roles 3000 1100 'outbound-maze-entry'
        Look-At 'Host' 3000 1800 160
        $entryImage=Capture-View 'Host' 'mine-maze-entry'
        foreach($point in @(@(3000,3000),@(4000,3000),@(4000,1750),@(4800,1750),@(4800,3000),@(5400,3000))){$null=Move-Formation $roles $point[0] $point[1] 'outbound-maze'}
        Look-At 'Host' 5600 3000 150
        $image=Capture-View 'Host' 'mine-portal-before-transfer'
        return [ordered]@{states=(All-States $roles);screenshots=@($entryImage,$image)}
    }
    Invoke-Step 'enter-school-through-portal' {
        $transfer=Transfer-Portals $roles 'PrototypeMinePortal' $true
        Look-At 'Host' 21800 0 160
        $image=Capture-View 'Host' 'school-after-portal'
        return [ordered]@{transfer=$transfer;screenshot=$image}
    }
    Invoke-Step 'school-classroom-and-recovery' {
        foreach($point in @(@(20300,0),@(20500,150),@(20500,450),@(20500,600))){$null=Move-Formation $roles $point[0] $point[1] 'school-classroom'}
        Look-At 'Host' 20800 950 160
        $image=Capture-View 'Host' 'school-classroom-loot'
        $claims=@();$script:SchoolValue=0
        for($i=0;$i -lt 3;$i++) {
            # Use the front row in multiplayer so another pawn cannot block the
            # server's line of sight from a rear-row claimant to a small pickup.
            $claimant=$(if($roles.Count -eq 1){'Host'}elseif($i -eq 1){'Client3'}else{'Client2'})
            $tag=('PrototypePickup{0:00}' -f (4+$i))
            $authority=Find-FixtureTarget 'Host' $tag
            $target=Find-FixtureTarget $claimant $tag
            $id=[string]$authority.authorityItemId;$value=[int]$authority.authorityValue
            if($value -le 0 -or $id -match '^0+-'){throw "$tag has invalid server item data"}
            $seq=Send-Action $claimant 'Interact' ([ordered]@{target=$target.name;alternate=$false})
            $states=Wait-Condition "school pickup $tag" $roles {param($s)
                $bad=Check-Privacy $s;if($bad){return $bad}
                if(@((Local-Private $s[$claimant]).items | Where-Object {$_.id -eq $id}).Count -ne 1){return 'item not owned'}
                if(@($s.Host.targets | Where-Object {$_.name -eq $authority.name}).Count -ne 0){return 'world item still exists'}
                return $null
            } 10
            $script:SchoolValue+=$value
            $claims+=@([ordered]@{role=$claimant;tag=$tag;itemId=$id;value=$value;command=$seq;states=$states})
        }
        return [ordered]@{screenshot=$image;claims=$claims;totalValue=$script:SchoolValue}
    }
    Invoke-Step 'return-school-portal-and-mine-maze' {
        foreach($point in @(@(20500,450),@(20500,150),@(20050,0))){$null=Move-Formation $roles $point[0] $point[1] 'school-return'}
        Look-At 'Host' 19800 0 100
        $image=Capture-View 'Host' 'school-portal-before-return'
        $transfer=Transfer-Portals $roles 'PrototypeSchoolPortal' $false
        Look-At 'Host' 4800 3000 160
        $mineImage=Capture-View 'Host' 'mine-after-return-portal'
        foreach($point in @(@(4800,3000),@(4800,1750),@(4000,1750),@(4000,3000),@(3000,3000),@(3000,1100),@(2800,800),@(2800,220))){$null=Move-Formation $roles $point[0] $point[1] 'return-maze-to-elevator'}
        $states=All-States $roles
        foreach($role in $roles){if(-not @($states[$role].players | Where-Object {$_.isLocal})[0].inCab){throw "$role not in cab after return route"}}
        return [ordered]@{transfer=$transfer;screenshots=@($image,$mineImage);elapsedSeconds=((Get-Date).ToUniversalTime()-$script:ExpeditionStartedUtc).TotalSeconds;states=$states}
    }
    Invoke-Step 'elevator-return-and-earned-settlement' {
        if($Players -gt 1){$null=Move-One 'Host' 2800 0 'clear-elevator-lever-sight'}
        $before=All-States $roles
        $elevator=Find-FixtureTarget 'Host' 'PrototypeElevator'
        Look-At 'Host' $elevator.x $elevator.y $elevator.z
        $null=Wait-Condition 'elevator lever sight line clear' @('Host') {param($s) if(-not $s.Host.aimTrace.hit -or $s.Host.aimTrace.actor -ne $elevator.name){return 'elevator lever ray obstructed or outside reach'};return $null} 8
        $seq=Send-Action 'Host' 'Interact' ([ordered]@{target=$elevator.name;alternate=$false})
        $transit=Wait-Condition 'ascending without early payout' $roles {param($s)
            foreach($role in $s.Keys){$p=Local-Private $s[$role];if([int]$p.life -ne 4 -or [int]$p.money -ne [int](Local-Private $before[$role]).money -or -not @($s[$role].players | Where-Object {$_.isLocal})[0].attachParentIsCab){return "$role has not entered unsettled transit"}}
            return $null
        } 12
        $expected=[int][math]::Floor($script:SchoolValue/$Players)
        $states=Wait-Condition 'earned settlement at base' $roles {param($s)
            $bad=Check-Public $s;if($bad){return $bad};$bad=Check-Privacy $s;if($bad){return $bad}
            if([int]$s.Host.public.phase -ne 0 -or [int]$s.Host.public.day -ne 2){return 'not returned to base day 2'}
            foreach($role in $s.Keys){$p=Local-Private $s[$role];if([int]$p.lastPayout -ne $expected -or [int]$p.money -ne [int](Local-Private $before[$role]).money+$expected -or @($p.items | Where-Object {-not $_.equipment}).Count -ne 0){return "$role earned settlement differs"}}
            return $null
        } 20
        $script:EarnedSettlement=$states
        return [ordered]@{command=$seq;expectedShare=$expected;before=$before;transit=$transit;after=$states}
    }
    Invoke-Step 'payment-validation-and-cancel' {
        $role=$roles[-1]
        Approach-Deposit $role
        $open=Open-Deposit $role
        $invalid=@()
        foreach($text in @('0','-1','abc','2147483648')) {
            $before=All-States $roles
            $input=Send-Action $role 'DepositInput' ([ordered]@{text=$text})
            $submit=Send-Action $role 'DepositSubmit'
            $states=Wait-Condition "UI rejects $text" $roles {param($s)
                $bad=Assert-EconomyUnchanged $before $s;if($bad){return $bad}
                if([string]::IsNullOrWhiteSpace([string]$s[$role].depositInput.error)){return 'UI error missing'}
                return $null
            } 8
            $invalid+=@([ordered]@{text=$text;input=$input;submit=$submit;before=$before;after=$states})
        }
        $before=All-States $roles
        $script:CanceledDepositSession=[int]$before[$role].depositInput.sessionId
        $null=Send-Action $role 'DepositInput' ([ordered]@{text='1'})
        $cancel=Send-Action $role 'DepositClose'
        $canceled=Wait-Condition 'payment cancel leaves money intact' $roles {param($s)
            $bad=Assert-EconomyUnchanged $before $s;if($bad){return $bad}
            if($s[$role].depositInput.open){return 'cancel did not close UI'}
            return $null
        } 8
        return [ordered]@{role=$role;opened=$open;invalid=$invalid;cancelCommand=$cancel;canceled=$canceled}
    }
    Invoke-Step 'server-rejects-invalid-payment' {
        $role=$roles[-1];$target=Find-FixtureTarget $role 'PrototypeDeposit'
        $evidence=@()
        $evidence+=@(Assert-Rejected 'canceled session replay rejected' $role 'DepositRpc' ([ordered]@{target=$target.name;amount=1;sessionId=$script:CanceledDepositSession}) $roles '만료')
        foreach($amount in @(-1,0,([int](Local-Private (Read-ReadyState $role)).money+1))) {
            $null=Open-Deposit $role
            $reason=$(if($amount -le 0){'1 이상의'}else{'보유금보다'})
            $evidence+=@(Assert-Rejected "server amount $amount rejected" $role 'DepositRpc' ([ordered]@{target=$target.name;amount=$amount}) $roles $reason)
            $null=Send-Action $role 'DepositClose'
        }
        $wrong=Find-FixtureTarget $role 'PrototypeStart'
        $null=Open-Deposit $role
        $evidence+=@(Assert-Rejected 'wrong station rejected' $role 'DepositRpc' ([ordered]@{target=$wrong.name;amount=1}) $roles '납부대가 일치')
        $null=Send-Action $role 'DepositClose'
        $null=Open-Deposit $role
        $sessionBeforeClosing=[int](Read-ReadyState $role).depositInput.sessionId
        $evidence+=@(Assert-DepositMovementBlocked $role $roles)
        $null=Send-Action $role 'DepositClose'
        $null=Wait-Condition 'deposit closed before leaving range' @($role) {param($s) if($s[$role].depositInput.open -or $s[$role].depositInput.moveInputIgnored){return 'deposit UI still suppresses movement'};return $null} 8
        $null=Move-One $role 0 900 'leave-closed-deposit-range'
        $opening=Assert-Rejected 'distant station opening rejected' $role 'Interact' ([ordered]@{target=$target.name;alternate=$false}) $roles '가까이'
        if($opening.after[$role].depositInput.open){throw 'Distant Interact opened the deposit UI'}
        $evidence+=@($opening)
        $stale=Assert-Rejected 'distant submission with closed session rejected' $role 'DepositRpc' ([ordered]@{target=$target.name;amount=1;sessionId=$sessionBeforeClosing}) $roles '만료'
        $evidence+=@([ordered]@{evidenceScope='stale-session rejection after UI cancellation; active-session submission distance branch is not exercised';result=$stale})
        $null=Move-One $role (800+140*($Players-1)) 900 'payment-test-parking'
        return $evidence
    }
    Invoke-Step 'normal-entered-payment-and-private-balance' {
        $payments=@()
        for($i=0;$i -lt $roles.Count;$i++) {
            $role=$roles[$i];Approach-Deposit $role;$null=Open-Deposit $role
            $null=Send-Action $role 'DepositInput' ([ordered]@{text='10'})
            $image=Capture-View $role 'deposit-entered-before-submit'
            $payment=Submit-Amount $role 10 $roles
            $afterImage=Capture-View $role 'deposit-after-submit'
            $target=Find-FixtureTarget $role 'PrototypeDeposit'
            $replay=Assert-Rejected 'successful session replay rejected' $role 'DepositRpc' ([ordered]@{target=$target.name;amount=1;sessionId=$payment.sessionId}) $roles '만료'
            $payments+=@([ordered]@{role=$role;payment=$payment;replay=$replay;screenshots=@($image,$afterImage)})
            $null=Move-One $role (800+140*$i) 700 'payer-parking'
        }
        return $payments
    }
    Invoke-Step 'quota-completion-and-next-day-one' {
        $payments=@()
        foreach($role in $roles) {
            $snapshot=Read-ReadyState $role
            if([int]$snapshot.public.quotaTarget -gt [int]$script:EarnedSettlement.Host.public.quotaTarget){break}
            $money=[int](Local-Private $snapshot).money
            if($money -le 0){continue}
            Approach-Deposit $role;$null=Open-Deposit $role
            # Requesting the remaining earned balance also exercises the quota remainder cap.
            $payments+=@(Submit-Amount $role $money $roles)
            $null=Move-One $role (800+140*[array]::IndexOf($roles,$role)) 700 'quota-payer-parking'
        }
        $states=All-States $roles
        if([int]$states.Host.public.quotaTarget -le [int]$script:EarnedSettlement.Host.public.quotaTarget -or [int]$states.Host.public.day -ne 1){throw 'earned fixture did not complete first quota'}
        return [ordered]@{payments=$payments;states=$states}
    }
    Invoke-Step 'next-expedition-after-payment' {
        $before=All-States $roles
        $null=Move-One 'Host' 500 0 'next-start-east-approach'
        $null=Move-One 'Host' 300 0 'next-start-range'
        $start=Find-FixtureTarget 'Host' 'PrototypeStart'
        $seq=Send-Action 'Host' 'Interact' ([ordered]@{target=$start.name;alternate=$false})
        $states=Wait-Condition 'next expedition' $roles {param($s)
            $bad=Check-Public $s;if($bad){return $bad}
            if([int]$s.Host.public.phase -ne 1 -or [int]$s.Host.public.runNumber -ne 2 -or $s.Host.public.runId -eq $before.Host.public.runId -or [int]$s.Host.public.day -ne 1){return 'next run not started correctly'}
            foreach($role in $s.Keys){if([int](Local-Private $s[$role]).money -ne [int](Local-Private $before[$role]).money){return "$role balance changed at departure"}}
            return Check-Privacy $s
        } 20
        return [ordered]@{command=$seq;before=$before;after=$states}
    }
    if($script:Steps.Count -lt 13){throw 'Required complete-flow steps missing'}
    Assert-ProcessesAlive
} catch {[void]$script:Failures.Add($_.Exception.Message);Write-Host "[fail] $($_.Exception.Message)"}
finally {
    try{Invoke-Cleanup}catch{[void]$script:Failures.Add("Cleanup error: $($_.Exception.Message)")}
    try{Inspect-ProcessLogs}catch{[void]$script:Failures.Add("Log inspection error: $($_.Exception.Message)")}
    $passed=$script:Failures.Count -eq 0 -and $script:Steps.Count -ge 13
    if($script:RunDir){$script:Result['passed']=$passed;$script:Result['failures']=@($script:Failures);$script:Result['failure']=$(if($script:Failures.Count){[string]$script:Failures[0]}else{$null});$script:Result['finishedUtc']=Get-UtcNow;$script:Result['exitCode']=$(if($passed){0}else{1});try{Save-Result}catch{$passed=$false;Write-Host "result write failed: $($_.Exception.Message)"};Write-Host "[result] passed=$passed $script:RunDir\result.json"}
    $exitCode=$(if($passed){0}else{1})
}
exit $exitCode
