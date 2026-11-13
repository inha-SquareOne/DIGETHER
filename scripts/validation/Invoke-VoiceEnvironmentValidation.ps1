#Requires -Version 5.1
<#
.SYNOPSIS
UE 5.8.4 음성 환경 투영값과 구역 라우팅을 별도 프로세스에서 검사한다
.NOTES
실제 마이크·Steam 음성을 검사하지 않는다. 시험 Pawn과 서버 시험 볼륨을 주입한다.
공유 맵·설정은 저장하지 않으며, 이번 실행이 시작한 프로세스만 정리한다.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$ProjectPath = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'DIGETHER/DIGETHER.uproject'),
    [string]$OutputRoot = (Join-Path $env:TEMP 'digether-voice'),
    [ValidateSet(2, 4)][int]$Players = 4,
    [ValidateRange(1024, 65535)][int]$Port = 7867,
    [ValidateRange(120, 600)][int]$TimeoutSeconds = 360
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-UnrealChecks.ps1') -Mode Automation -EngineRoot $EngineRoot -TimeoutSeconds $TimeoutSeconds
$EngineRoot = Assert-UnrealEngineVersion $EngineRoot
$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = Split-Path -Parent $project
$executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$runDirectory = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ([guid]::NewGuid().ToString('N').Substring(0, 8))
$utf8 = New-Object System.Text.UTF8Encoding($false)
$slotCount = 120
$slotInterval = 2
$owned = @()
$expectedExits = @{}
$checks = New-Object System.Collections.Generic.List[object]
$clock = [Diagnostics.Stopwatch]::StartNew()
$protectedPaths = @(
    (Join-Path $projectDirectory 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'),
    (Join-Path $projectDirectory 'Config/DefaultEngine.ini'),
    (Join-Path $projectDirectory 'Config/DefaultGame.ini'),
    (Join-Path $projectDirectory 'Content/Game/Maps/L_Hub.umap'),
    (Join-Path $projectDirectory 'Binaries/Win64/UnrealEditor-DIGETHER.dll')
)
$manifest = [ordered]@{
    engine_version = '5.8.4'; project = $project; map = '/Game/Game/Maps/L_Hub'
    started_utc = [DateTime]::UtcNow.ToString('o'); completed_utc = $null
    players = $Players; port = $Port; state = 'starting'; error = $null
    scope = '로컬 별도 프로세스의 음성 환경 투영 복제·구역/역할 라우팅 검사'
    injected = @('시험 매몰 인터페이스 Pawn', '서버 시험 던전 볼륨', '시험 생사 상태')
    not_tested = @('실제 마이크 청취', 'Steam 두 PC M3', '실제 플레이어 매몰 기능', '늦은 참가·재접속', '크레딧 전환')
    processes = @(); actions = @(); protected_before = @(); protected_after = @()
    passed = 0; failed = 0; checks = @()
}

function Get-ProtectedHashes {
    return @($protectedPaths | ForEach-Object {
        [ordered]@{ path = $_; hash = $(if (Test-Path -LiteralPath $_ -PathType Leaf) { (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash } else { $null }) }
    })
}

function Save-Manifest {
    $manifest.processes = @($owned | ForEach-Object { $_.record })
    $manifest.checks = @($checks.ToArray())
    $manifest.passed = @($checks | Where-Object { $_.passed }).Count
    $manifest.failed = @($checks | Where-Object { -not $_.passed }).Count
    [IO.File]::WriteAllText((Join-Path $runDirectory 'session.json'), ($manifest | ConvertTo-Json -Depth 15), $utf8)
}

function Read-Observation($Entry) {
    $snapshots = @()
    $lastSlot = 0
    if (Test-Path -LiteralPath $Entry.record.log) {
        $stream = $null
        $reader = $null
        try {
            $stream = [IO.File]::Open($Entry.record.log, [IO.FileMode]::Open, [IO.FileAccess]::Read,
                ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true)
            $content = $reader.ReadToEnd()
        }
        finally {
            if ($reader) { $reader.Dispose() }
            if ($stream) { $stream.Dispose() }
        }
        foreach ($match in [regex]::Matches($content, 'VOICE_ENV_SNAPSHOT\s+(\{[^\r\n]*\})')) {
            $snapshots += ($match.Groups[1].Value | ConvertFrom-Json)
        }
        $slots = [regex]::Matches($content, 'AutoCmd [^\r\n]*?:\s*exec\s+[^\r\n]*?slot-(\d{3})\.txt')
        if ($slots.Count) { $lastSlot = [int]$slots[$slots.Count - 1].Groups[1].Value }
    }
    return [pscustomobject]@{ snapshots = $snapshots; last_slot = $lastSlot }
}

function Wait-Condition([string]$WaitLabel, [scriptblock]$Predicate, [int]$Seconds = 35) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($clock.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "Run deadline exceeded: $WaitLabel" }
        foreach ($waitingEntry in $owned) {
            if ($waitingEntry.process.HasExited -and -not $expectedExits.ContainsKey($waitingEntry.record.name)) { throw "Participant exited: $($waitingEntry.record.name)" }
        }
        if (& $Predicate) { return }
        Start-Sleep -Milliseconds 200
    }
    throw "Observation timed out: $WaitLabel"
}

function Schedule($Entry, [string[]]$Commands, [string]$Label) {
    Wait-Condition ('slot clock ' + $Entry.record.name) { (Read-Observation $Entry).last_slot -gt 0 }
    $number = (Read-Observation $Entry).last_slot + 2
    if ($number -gt $slotCount - 2) { throw 'Reserved EXEC slots exhausted.' }
    if (-not $Entry.used.Add($number)) { throw 'EXEC slot collision.' }
    $path = Join-Path $Entry.slots ('slot-{0:D3}.txt' -f $number)
    $temporary = $path + '.tmp'
    [IO.File]::WriteAllText($temporary, (($Commands -join "`r`n") + "`r`n"), $utf8)
    [IO.File]::Replace($temporary, $path, [NullString]::Value)
    $manifest.actions += [ordered]@{ participant = $Entry.record.name; label = $Label; slot = $number; commands = $Commands; utc = [DateTime]::UtcNow.ToString('o') }
    Save-Manifest
}

function Last-Snapshot($Entry, [string]$Label) {
    $rows = @((Read-Observation $Entry).snapshots | Where-Object { $_.label -eq $Label })
    if ($rows.Count) { return $rows[$rows.Count - 1] }
    return $null
}

function Snapshot-All([string]$Label) {
    for ($attempt = 0; $attempt -lt 6; $attempt++) {
        $beforeCounts = @{}
        foreach ($entry in $owned) {
            $beforeCounts[$entry.record.name] = @((Read-Observation $entry).snapshots | Where-Object { $_.label -eq $Label }).Count
            Schedule $entry @('Digether.VoiceTest snapshot ' + $Label) $Label
        }
        Wait-Condition ('snapshots ' + $Label) {
            foreach ($entry in $owned) {
                $count = @((Read-Observation $entry).snapshots | Where-Object { $_.label -eq $Label }).Count
                if ($count -le $beforeCounts[$entry.record.name]) { return $false }
            }
            return $true
        }
        $complete = $true
        foreach ($entry in $owned) {
            $snapshot = Last-Snapshot $entry $Label
            if (@($snapshot.players).Count -ne $Players) { $complete = $false }
        }
        if ($complete) { return }
    }
    throw ('Expected player count not observed: ' + $Label)
}

function Check([string]$Name, [bool]$Passed, [string]$Detail) {
    $checks.Add([ordered]@{ name = $Name; passed = $Passed; detail = $Detail })
    Save-Manifest
    if (-not $Passed) { throw ('Check failed: ' + $Name + ': ' + $Detail) }
}

function Check-Projections([string]$Label, [int]$Target, [bool]$Buried, [string]$Life) {
    foreach ($entry in $owned) {
        $snapshot = Last-Snapshot $entry $Label
        $ids = @($snapshot.players | ForEach-Object { [int]$_.id })
        Check ($Label + ':' + $entry.record.name + ':unique-players') (@($ids | Select-Object -Unique).Count -eq $Players) ('ids=' + ($ids -join ','))
        foreach ($player in $snapshot.players) {
            $isTarget = [int]$player.id -eq $Target
            $expectedBuried = $isTarget -and $Buried
            $expectedLife = if ($isTarget) { $Life } else { 'Active' }
            Check ($Label + ':' + $entry.record.name + ':player-' + $player.id + ':buried') ([bool]$player.buried -eq $expectedBuried) ('actual=' + $player.buried + ';expected=' + $expectedBuried)
            Check ($Label + ':' + $entry.record.name + ':player-' + $player.id + ':life') ($player.life -eq $expectedLife) ('actual=' + $player.life + ';expected=' + $expectedLife)
        }
    }
}

function Run-HostAction([string]$Label, [string[]]$Commands) {
    $hostEntry = @($owned | Where-Object { $_.record.host })[0]
    Schedule $hostEntry ($Commands + @('Digether.VoiceTest snapshot action-' + $Label)) ('action-' + $Label)
    Wait-Condition ('host action ' + $Label) { $null -ne (Last-Snapshot $hostEntry ('action-' + $Label)) }
    Snapshot-All $Label
}

function Check-ZoneRoutes([string]$Label, [int]$Target, [int]$Zone, [bool]$Dead) {
    $hostEntry = @($owned | Where-Object { $_.record.host })[0]
    $snapshot = Last-Snapshot $hostEntry $Label
    $targetRows = @($snapshot.players | Where-Object { [int]$_.id -eq $Target })
    Check ($Label + ':server:target-zone') ($targetRows.Count -eq 1 -and [int]$targetRows[0].zone -eq $Zone) ('expected=' + $Zone)
    Check ($Label + ':server:route-count') (@($snapshot.routes).Count -eq $Players) ('actual=' + @($snapshot.routes).Count)
    foreach ($route in $snapshot.routes) {
        $desiredMutes = @($route.blocked | ForEach-Object { [int]$_ } | Sort-Object)
        $appliedMutes = @($route.applied_muted | ForEach-Object { [int]$_ } | Where-Object { $_ -ne [int]$route.listener } | Sort-Object)
        Check ($Label + ':server:listener-' + $route.listener + ':applied-mutes') (($desiredMutes -join ',') -eq ($appliedMutes -join ',')) ('desired=' + ($desiredMutes -join ',') + ';applied=' + ($appliedMutes -join ','))
        if ([int]$route.listener -eq $Target) { continue }
        $expectedBlocked = $Zone -eq 1 -or $Dead
        $blocked = @($route.blocked | ForEach-Object { [int]$_ })
        Check ($Label + ':server:listener-' + $route.listener + ':target-route') ($blocked.Contains($Target) -eq $expectedBlocked) ('blocked=' + ($blocked -join ','))
    }
}

function Start-Participant([string]$Name, [bool]$IsHost) {
    $directory = Join-Path $runDirectory $Name
    # ULocalPlayer::ExecMacro는 경로에 Binaries가 없으면 ../../Binaries/를 앞에 붙임
    $slots = Join-Path $runDirectory ('Binaries/' + $Name)
    $null = New-Item -ItemType Directory -Path $directory, $slots
    $isolatedIni = Join-Path $directory 'EditorPerProjectUserSettings.ini'
    $null = Initialize-IsolatedUserSettings $protectedPaths[0] $isolatedIni $directory
    $spec = @()
    for ($index = 1; $index -le $slotCount; $index++) {
        $path = Join-Path $slots ('slot-{0:D3}.txt' -f $index)
        if ($path -match '[\s"|]') { throw 'OutputRoot must have no whitespace, quotes, or pipes.' }
        [IO.File]::WriteAllText($path, '', $utf8)
        $spec += ('{0}:exec {1}' -f ($index * $slotInterval), $path.Replace('\', '/'))
    }
    $url = if ($IsHost) { '/Game/Game/Maps/L_Hub?listen?DigetherNewGame' } else { '127.0.0.1:' + $Port }
    $log = Join-Path $directory 'game.log'
    $arguments = @($project, $url, '-game', '-nosteam', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-nosound', '-NoLiveCoding', '-forcelogflush',
        ('-port=' + $Port), ('-UserDir=' + $directory + '/User/'), ('-EditorPerProjectUserSettingsIni=' + $isolatedIni), ('-abslog=' + $log),
        ('-DigetherAutoCmds=' + ($spec -join '|')),
        '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
        '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL')
    $argumentLine = ($arguments | ForEach-Object { ConvertTo-WindowsArgument $_ }) -join ' '
    if ($argumentLine.Length + $executable.Length + 4 -ge 16000) { throw 'UE command-line budget exceeded; use a shorter OutputRoot.' }
    $process = Start-Process -FilePath $executable -ArgumentList $argumentLine -WorkingDirectory $projectDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
    $null = $process.Handle
    $record = [ordered]@{ name = $Name; host = $IsHost; pid = $process.Id; executable = $executable; arguments = $arguments; log = $log; started_utc = [DateTime]::UtcNow.ToString('o'); exit_code = $null; stopped_by_runner = $false }
    $script:owned += [pscustomobject]@{ process = $process; record = $record; slots = $slots; used = (New-Object 'System.Collections.Generic.HashSet[int]') }
    Save-Manifest
    Write-Output ('Started ' + $Name + ' PID=' + $process.Id)
}

$failure = $null
try {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Editor executable missing.' }
    if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw 'Requested port is in use.' }
    $null = New-Item -ItemType Directory -Path $runDirectory
    $manifest.protected_before = Get-ProtectedHashes
    Start-Participant 'host' $true
    Wait-Condition 'host clock' { (Read-Observation $owned[0]).last_slot -gt 0 } 60
    for ($index = 1; $index -lt $Players; $index++) { Start-Participant ('client-' + $index) $false }
    Snapshot-All 'connected'
    $hostEntry = $owned[0]
    $connected = Last-Snapshot $hostEntry 'connected'
    $targetIds = @($connected.players | ForEach-Object { [int]$_.id } | Sort-Object)
    $target = $targetIds[$targetIds.Count - 1]
    Run-HostAction 'base' @('Digether.VoiceTest setup')
    Check-Projections 'base' $target $false 'Active'
    Check-ZoneRoutes 'base' $target 0 $false
    Run-HostAction 'buried' @('Digether.VoiceTest buried ' + $target + ' 1')
    Check-Projections 'buried' $target $true 'Active'
    Run-HostAction 'dungeon' @('Digether.VoiceTest zone ' + $target + ' 1')
    Check-Projections 'dungeon' $target $true 'Active'
    Check-ZoneRoutes 'dungeon' $target 1 $false
    Run-HostAction 'dead' @('Digether.VoiceTest role ' + $target + ' dead')
    Check-Projections 'dead' $target $false 'Dead'
    Check-ZoneRoutes 'dead' $target 1 $true
    Run-HostAction 'active' @('Digether.VoiceTest role ' + $target + ' active')
    Check-Projections 'active' $target $true 'Active'
    Run-HostAction 'clear' @('Digether.VoiceTest buried ' + $target + ' 0')
    Check-Projections 'clear' $target $false 'Active'
    Run-HostAction 'mine' @('Digether.VoiceTest zone ' + $target + ' 0')
    Check-Projections 'mine' $target $false 'Active'
    Check-ZoneRoutes 'mine' $target 0 $false
    Run-HostAction 'dungeon-repeat' @('Digether.VoiceTest zone ' + $target + ' 1')
    Check-Projections 'dungeon-repeat' $target $false 'Active'
    Check-ZoneRoutes 'dungeon-repeat' $target 1 $false
    Run-HostAction 'mine-repeat' @('Digether.VoiceTest zone ' + $target + ' 0')
    Check-Projections 'mine-repeat' $target $false 'Active'
    Check-ZoneRoutes 'mine-repeat' $target 0 $false
    foreach ($entry in $owned | Where-Object { -not $_.record.host }) {
        $expectedExits[$entry.record.name] = $true
        Schedule $entry @('quit') 'quit-client'
    }
    foreach ($entry in $owned | Where-Object { -not $_.record.host }) {
        if (-not $entry.process.WaitForExit(15000)) { throw ('Client quit timed out: ' + $entry.record.name) }
    }
    $expectedExits['host'] = $true
    Schedule $hostEntry @('quit') 'quit-host'
    if (-not $hostEntry.process.WaitForExit(15000)) { throw 'Host quit timed out.' }
    $manifest.state = 'completed'
}
catch {
    $failure = $_.Exception.Message
    $manifest.state = 'failed'
    $manifest.error = $failure
    Write-Warning $failure
}
finally {
    foreach ($entry in $owned) {
        if (-not $entry.process.HasExited) {
            $native = Get-CimInstance Win32_Process -Filter ('ProcessId = ' + $entry.process.Id)
            if ($native -and $native.ExecutablePath -ieq $executable -and $native.CommandLine.Contains($runDirectory)) {
                Stop-Process -Id $entry.process.Id
                $entry.record.stopped_by_runner = $true
                $null = $entry.process.WaitForExit(10000)
            }
        }
        if ($entry.process.HasExited) { $entry.process.WaitForExit(); $entry.record.exit_code = $entry.process.ExitCode }
    }
    if ($manifest.state -eq 'completed') {
        $badExits = @($owned | Where-Object { $_.record.exit_code -ne 0 -or $_.record.stopped_by_runner })
        if ($badExits.Count) {
            $failure = 'A participant did not exit normally after verification.'
            $manifest.state = 'failed'; $manifest.error = $failure
        }
    }
    $manifest.protected_after = Get-ProtectedHashes
    $before = $manifest.protected_before | ConvertTo-Json -Compress
    $after = $manifest.protected_after | ConvertTo-Json -Compress
    if ($before -ne $after) {
        if (-not $failure) { $failure = 'Protected map/config/module hashes changed.' }
        $manifest.state = 'failed'; $manifest.error = $failure
    }
    $manifest.completed_utc = [DateTime]::UtcNow.ToString('o')
    if (Test-Path -LiteralPath $runDirectory) { Save-Manifest }
    Write-Output ('Voice environment validation output=' + $runDirectory)
}
if ($failure) { throw $failure }
