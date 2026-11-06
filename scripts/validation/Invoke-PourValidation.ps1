#Requires -Version 5.1
<#
.SYNOPSIS
UE 5.8.4의 정식 붓기 RPC를 별도 프로세스에서 검사한다
.EXAMPLE
.\scripts\validation\Invoke-PourValidation.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8' -OutputRoot "$env:SystemDrive\DGP" -Players 4 -Cycles 10 -PacketLagMs 100 -PacketLossPct 5
.NOTES
빌드·맵 이동·게임 판정 우회·화면 입력을 하지 않는다. 출력 폴더는 새 하위 폴더만 만든다
출력 경로는 실제 드라이브의 쓰기 권한에 맞춰 짧고 공백 없는 위치를 선택한다
위 DGP 경로는 사용 예시이며 사용자 프로필이나 환경의 고정 설정으로 저장하지 않는다
#>
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$ProjectPath = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'DIGETHER/DIGETHER.uproject'),
    [string]$OutputRoot = (Join-Path $env:TEMP 'digether-pour'),
    [ValidateSet(1, 4)][int]$Players = 4,
    [ValidateSet(1, 10)][int]$Cycles = 10,
    [ValidateSet(0, 100)][int]$PacketLagMs = 0,
    [ValidateSet(0, 5)][int]$PacketLossPct = 0,
    [ValidateRange(1024, 65535)][int]$Port = 7865,
    [ValidateRange(60, 1200)][int]$TimeoutSeconds = 420,
    [string]$PythonExe = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-UnrealChecks.ps1') -Mode Automation -EngineRoot $EngineRoot -TimeoutSeconds $TimeoutSeconds

$EngineRoot = Assert-UnrealEngineVersion $EngineRoot
if ([string]::IsNullOrWhiteSpace($PythonExe)) { $PythonExe = Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe' }
$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = Split-Path -Parent $project
$executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$judge = Join-Path $PSScriptRoot 'judge-pour-validation.py'
$runDirectory = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ([guid]::NewGuid().ToString('N').Substring(0, 8))
$sourceIni = Join-Path $projectDirectory 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
$config = Join-Path $projectDirectory 'Config/DefaultEngine.ini'
$module = Join-Path $projectDirectory 'Binaries/Win64/UnrealEditor-DIGETHER.dll'
$mapFile = Join-Path $projectDirectory 'Content/Game/Maps/L_Hub.umap'
$owned = @()
$expectedExits = @{}
$slotCount = 300
$engineCommandLineLimit = 16000
$utf8 = New-Object System.Text.UTF8Encoding($false)
$clock = [Diagnostics.Stopwatch]::StartNew()
$manifest = [ordered]@{
    engine_version = '5.8.4'; started_utc = [DateTime]::UtcNow.ToString('o'); completed_utc = $null
    players = $Players; cycles = $Cycles; packet_lag_ms = $PacketLagMs; packet_loss_pct = $PacketLossPct
    port = $Port; project = $project; map = '/Game/Game/Maps/L_Hub'; status = 'starting'; error = $null
    processes = @(); actions = @(); slot_count = $slotCount; slot_interval_game_seconds = 1
    engine_command_line_limit_chars = $engineCommandLineLimit
    user_ini_before = $null; user_ini_after = $null; config_before = $null; config_after = $null
    module_before = $null; module_after = $null; map_before = $null; map_after = $null
    limits = @('실제 UI 입력·M3·성능 검사가 아닌 로컬 콘솔/RPC 검사', '실제 손실 패킷 수 미측정', 'RNG 시드 미통제')
}

function File-Hash([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
    return $null
}

function Assert-SlotBudget([int]$AvailableSlots, [int]$RequestedCycles) {
    # 실측 실행을 수용하기 위한 검사 예약 예산, 동적 실행 시간 보장은 아님
    $minimumSlots = if ($RequestedCycles -eq 10) { 220 } else { 80 }
    if ($AvailableSlots -lt $minimumSlots) {
        throw ('OutputRoot leaves {0} command slots within the UE command-line budget; {1} cycles require at least {2} reserved slots. Choose a shorter writable OutputRoot.' -f $AvailableSlots, $RequestedCycles, $minimumSlots)
    }
}

function Write-Manifest {
    $manifest.processes = @($owned | ForEach-Object { $_.record })
    $path = Join-Path $runDirectory 'session.json'
    $temporary = $path + '.tmp'
    [IO.File]::WriteAllText($temporary, ($manifest | ConvertTo-Json -Depth 12), $utf8)
    if (Test-Path -LiteralPath $path) { [IO.File]::Replace($temporary, $path, [NullString]::Value) }
    else { [IO.File]::Move($temporary, $path) }
}

function Get-Observation {
    Write-Manifest
    $observed = & $PythonExe -X utf8 $judge $runDirectory --observe
    if ($LASTEXITCODE -ne 0) { throw 'Log observer failed.' }
    return ($observed | ConvertFrom-Json)
}

function Wait-Observed([string]$Label, [scriptblock]$Condition, [int]$Seconds = 25) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($clock.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "Run deadline exceeded: $Label" }
        foreach ($entry in $owned) {
            if ($entry.process.HasExited -and -not $expectedExits.ContainsKey($entry.record.name)) { throw "Participant exited before $Label : $($entry.record.name)" }
        }
        $observed = Get-Observation
        if (& $Condition $observed) { return $observed }
        Start-Sleep -Milliseconds 200
    }
    throw "Observation timed out: $Label"
}

function Get-Snapshots($Log, [string]$Stage) {
    return @($Log.snapshots | Where-Object { $_.stage -eq $Stage })
}

function Own-Balance($Snapshot) {
    $ownRows = @($Snapshot.players | Where-Object { $_.is_self })
    if ($ownRows.Count -ne 1) { return $null }
    $balanceProperty = $ownRows[0].wallet.PSObject.Properties['BalanceCredits']
    if ($null -ne $balanceProperty) { return [long]$balanceProperty.Value }
    return [long]0
}

function Get-Log($Observation, [string]$Name) { return $Observation.PSObject.Properties[$Name].Value }

function Schedule-Action([string]$Stage, [hashtable]$Commands, [double]$LeadSeconds = 1.8, [bool]$WaitForExecution = $true) {
    $observed = Wait-Observed ('slot clocks for ' + $Stage) {
        param($state)
        foreach ($entry in $owned) {
            if ($Commands.ContainsKey($entry.record.name) -and @((Get-Log $state $entry.record.name).slots).Count -lt 4) { return $false }
        }
        return $true
    }
    $targetUtc = [DateTimeOffset]::UtcNow.AddSeconds($LeadSeconds).ToUnixTimeMilliseconds() / 1000.0
    $pending = @()
    foreach ($entry in $owned) {
        $name = $entry.record.name
        if (-not $Commands.ContainsKey($name)) { continue }
        $samples = @((Get-Log $observed $name).slots | Select-Object -Last 8)
        $intervals = @()
        for ($index = 1; $index -lt $samples.Count; $index++) {
            $slotDelta = [int]$samples[$index].slot - [int]$samples[$index - 1].slot
            if ($slotDelta -gt 0) { $intervals += ([double]$samples[$index].utc - [double]$samples[$index - 1].utc) / $slotDelta }
        }
        $ordered = @($intervals | Sort-Object)
        if ($ordered.Count -lt 2) { throw "No observed slot interval: $name" }
        $interval = [double]$ordered[[int][Math]::Floor($ordered.Count / 2)]
        if ($interval -le 0 -or $interval -gt 5) { throw "Invalid observed slot interval: $name $interval" }
        $last = $samples[-1]
        $number = [int]$last.slot + [int][Math]::Round(($targetUtc - [double]$last.utc) / $interval)
        $number = [Math]::Max($number, [int]$last.slot + 2)
        if ($number -gt $slotCount - 4) { throw "Reserved slots exhausted: $name" }
        if ($entry.used.Contains($number)) { throw "Slot already assigned: $name $number" }
        $path = Join-Path $entry.slots ('slot-{0:D3}.txt' -f $number)
        $action = [ordered]@{ participant = $name; stage = $Stage; slot = $number; target_utc = $targetUtc
            predicted_utc = [double]$last.utc + ($number - [int]$last.slot) * $interval
            observed_slot = [int]$last.slot; observed_utc = [double]$last.utc; observed_interval_seconds = $interval
            path = $path; commands = @($Commands[$name]); written_utc = $null }
        $manifest.actions += $action
        $entry.used.Add($number) | Out-Null
        $pending += [pscustomobject]@{ entry = $entry; action = $action }
    }
    Write-Manifest
    foreach ($item in $pending) {
        $temporary = $item.action.path + '.tmp'
        [IO.File]::WriteAllText($temporary, (($item.action.commands -join "`r`n") + "`r`n"), $utf8)
        [IO.File]::Replace($temporary, $item.action.path, [NullString]::Value)
        $item.action.written_utc = [DateTime]::UtcNow.ToString('o')
    }
    Write-Manifest
    if (-not $WaitForExecution) { return }
    $null = Wait-Observed ('executed slots for ' + $Stage) {
        param($state)
        foreach ($item in $pending) {
            if (-not @((Get-Log $state $item.entry.record.name).slots | Where-Object { $_.slot -eq $item.action.slot }).Count) { return $false }
        }
        return $true
    }
}

function Commands-All([string[]]$Lines) {
    $result = @{}
    foreach ($entry in $owned) { $result[$entry.record.name] = $Lines }
    return $result
}

function Dump-All([string]$Stage) {
    Schedule-Action $Stage (Commands-All @('DigetherDumpState'))
    return Wait-Observed ('complete dumps for ' + $Stage) {
        param($state)
        foreach ($entry in $owned) {
            $rows = @(Get-Snapshots (Get-Log $state $entry.record.name) $Stage)
            if (-not $rows.Count -or @($rows[-1].players).Count -ne $Players) { return $false }
        }
        return $true
    }
}

function Start-Participant([string]$Name, [int]$Grant, [bool]$IsHost) {
    $directory = Join-Path $runDirectory $Name
    $slots = Join-Path $runDirectory ('Binaries/' + $Name)
    $null = New-Item -ItemType Directory -Path $directory, $slots
    $isolatedIni = Join-Path $directory 'EditorPerProjectUserSettings.ini'
    $null = Initialize-IsolatedUserSettings $sourceIni $isolatedIni $directory
    $log = Join-Path $directory 'game.log'
    $spec = @()
    for ($index = 1; $index -le $slotCount; $index++) {
        $path = Join-Path $slots ('slot-{0:D3}.txt' -f $index)
        if ($path.Length -ge 500) { throw 'EXEC file path exceeds engine token capacity.' }
        [IO.File]::WriteAllText($path, '', $utf8)
        # AutoCmds의 FParse::Value는 내부 따옴표에서 끝나므로 공백 없는 경로 사용
        if ($path -match '[\s"|]') { throw 'Use an OutputRoot without whitespace, quotes or pipes for EXEC slots.' }
        $spec += ('{0}:exec {1}' -f $index, $path.Replace('\', '/'))
    }
    $url = if ($IsHost) { '/Game/Game/Maps/L_Hub?listen?DigetherNewGame' } else { '127.0.0.1:' + $Port }
    $arguments = @($project, $url, '-game', '-nosteam', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-nosound',
        '-NoLiveCoding', '-forcelogflush', '-LogCmds=LogPlayerManagement Log', ('-port=' + $Port),
        ('-UserDir=' + $directory + '/User/'), ('-EditorPerProjectUserSettingsIni=' + $isolatedIni),
        ('-abslog=' + $log), ('-DigetherAutoCmds=' + ($spec -join '|')),
        '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
        '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=NULL')
    if ($IsHost) { $arguments += '-DigetherDevRpc' }
    else { $arguments += @(('-PktLag=' + $PacketLagMs), ('-PktLoss=' + $PacketLossPct)) }
    $argumentLine = ($arguments | ForEach-Object { ConvertTo-WindowsArgument $_ }) -join ' '
    if ($argumentLine.Length + $executable.Length + 4 -ge $engineCommandLineLimit) {
        throw ('UE command line exceeds the {0}-character validation budget ({1}); choose a shorter OutputRoot.' -f $engineCommandLineLimit, $argumentLine.Length)
    }
    $process = Start-Process -FilePath $executable -ArgumentList $argumentLine -WorkingDirectory $projectDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
    $null = $process.Handle
    $record = [ordered]@{ name = $Name; grant = $Grant; host = $IsHost; pid = $process.Id; executable = $executable
        started_utc = [DateTime]::UtcNow.ToString('o'); arguments = $arguments; log = $log; exit_code = $null; stopped_by_runner = $false }
    $script:owned += [pscustomobject]@{ process = $process; record = $record; slots = $slots; used = (New-Object 'System.Collections.Generic.HashSet[int]') }
    Write-Manifest
    Write-Output ('Started ' + $Name + ' PID=' + $process.Id)
}

$failure = $null
try {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Editor executable missing.' }
    if (-not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) { throw 'Python executable missing; specify -PythonExe.' }
    if (-not (Test-Path -LiteralPath $module -PathType Leaf)) { throw 'Build Editor before running this validation.' }
    if (Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'") { throw 'An existing Unreal process is running; preserve it and use the runner after it exits.' }
    $udp = New-Object Net.Sockets.UdpClient
    try { $udp.Client.ExclusiveAddressUse = $true; $udp.Client.Bind((New-Object Net.IPEndPoint([Net.IPAddress]::Any, $Port))) }
    finally { $udp.Dispose() }
    # Windows 32767보다 작은 UE FCommandLine 16384 상한에 여유를 둔 예약 수
    $samplePath = Join-Path $runDirectory 'Binaries/client3/slot-300.txt'
    while (($samplePath.Length + 14) * $slotCount + 2200 -gt $engineCommandLineLimit) { $slotCount -= 10 }
    Assert-SlotBudget $slotCount $Cycles
    $manifest.slot_count = $slotCount
    $null = New-Item -ItemType Directory -Path $runDirectory
    $manifest.user_ini_before = File-Hash $sourceIni
    $manifest.config_before = File-Hash $config
    $manifest.module_before = File-Hash $module
    $manifest.map_before = File-Hash $mapFile
    Start-Participant 'host' 5000 $true
    $null = Wait-Observed 'host ready' { param($state) (Get-Log $state 'host').ready } 65
    if ($Players -eq 4) {
        foreach ($index in 1..3) { Start-Participant ('client' + $index) (5000 + $index * 100) $false }
    }
    $null = Wait-Observed 'all controllers and slot clocks ready' {
        param($state)
        foreach ($entry in $owned) {
            $log = Get-Log $state $entry.record.name
            if (-not $log.ready -or @($log.slots).Count -lt 4) { return $false }
        }
        return $true
    } 80
    $manifest.status = 'running'
    $initial = Dump-All 'initial'
    foreach ($entry in $owned) {
        $rows = @(Get-Snapshots (Get-Log $initial $entry.record.name) 'initial')
        if ((Own-Balance $rows[-1]) -ne 0) { throw ('Expected isolated initial zero wallet: ' + $entry.record.name) }
    }
    $grantCommands = @{}
    foreach ($entry in $owned) { $grantCommands[$entry.record.name] = @('DigetherGrantCredits ' + $entry.record.grant) }
    Schedule-Action 'grant' $grantCommands
    $granted = Dump-All 'granted'
    foreach ($entry in $owned) {
        $rows = @(Get-Snapshots (Get-Log $granted $entry.record.name) 'granted')
        if ((Own-Balance $rows[-1]) -ne $entry.record.grant) { throw ('Grant not observed: ' + $entry.record.name) }
    }
    Schedule-Action 'teleport' (Commands-All @('DigetherTeleportToDeposit GameTestDeposit'))
    $null = Wait-Observed 'server teleports' {
        param($state)
        $moves = @((Get-Log $state 'host').teleports)
        if ($moves.Count -lt $Players) { return $false }
        if (@($moves | Where-Object { -not $_.success }).Count) { throw 'A server teleport failed.' }
        return $true
    }
    # 이동 RPC·소유자 시선·서버 카메라 캐시가 갱신된 뒤의 덤프를 별도 관측
    $poseReady = $false
    for ($attempt = 0; $attempt -lt 4 -and -not $poseReady; $attempt++) {
        $prepared = Dump-All 'prepared'
        $poseReady = $true
        foreach ($entry in $owned) {
            $views = @((Get-Log $prepared $entry.record.name).views | Where-Object { $_.stage -eq 'prepared' -and $_.IsSelf -eq '1' })
            if (-not $views.Count -or -not $views[-1].valid) { $poseReady = $false }
        }
        $serverViews = @((Get-Log $prepared 'host').views | Where-Object { $_.stage -eq 'prepared' } | Group-Object Player | ForEach-Object { $_.Group[-1] })
        if ($serverViews.Count -ne $Players -or @($serverViews | Where-Object { -not $_.valid }).Count) { $poseReady = $false }
    }
    if (-not $poseReady) { throw 'Prepared viewpoint was not observed after owner/server camera updates.' }
    for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
        $prefix = 'cycle-{0:D2}' -f $cycle
        Schedule-Action ($prefix + '-reset') @{ host = @('DigetherSetQuotaProgress 0') }
        $null = Wait-Observed ($prefix + ' quota reset') {
            param($state)
            return @((Get-Log $state 'host').quota_reset | Where-Object { $_.stage -eq ($prefix + '-reset') -and $_.success }).Count -eq 1
        }
        $null = Dump-All ($prefix + '-pre')
        Schedule-Action ($prefix + '-pour') (Commands-All @('DigetherPour 1.5 GameTestDeposit'))
        $null = Wait-Observed ($prefix + ' normal stop') {
            param($state)
            foreach ($entry in $owned) {
                $responses = @((Get-Log $state $entry.record.name).responses | Where-Object { $_.stage -eq ($prefix + '-pour') })
                if (@($responses | Where-Object { $_.result -ne 'Success' -or $_.rate_limited }).Count) { throw ('Pour rejected: ' + $entry.record.name) }
                if (@($responses | Where-Object { $_.operation -eq 'PourStopped' }).Count -ne 1) { return $false }
                if (@($responses | Where-Object { $_.operation -eq 'PourStarted' }).Count -ne 1 -or
                    @($responses | Where-Object { $_.operation -eq 'Submit' }).Count -eq 0) { throw ('Incomplete pour: ' + $entry.record.name) }
            }
            return $true
        }
        $null = Dump-All ($prefix + '-post')
        Write-Output ('Observed ' + $prefix + ' begin/submit/stop; strict accounting and overlap follow in judgement.json')
    }
    # 클라이언트가 먼저 종료된 것을 확인한 뒤 호스트 종료
    if ($Players -eq 4) {
        $commands = @{}
        foreach ($entry in $owned | Where-Object { -not $_.record.host }) { $commands[$entry.record.name] = @('quit') }
        foreach ($name in $commands.Keys) { $expectedExits[$name] = $true }
        Schedule-Action 'quit-client' $commands 1.8 $false
        foreach ($entry in $owned | Where-Object { -not $_.record.host }) {
            if (-not $entry.process.WaitForExit(15000)) { throw ('Client quit timed out: ' + $entry.record.name) }
        }
    }
    $expectedExits['host'] = $true
    Schedule-Action 'quit-host' @{ host = @('quit') } 1.8 $false
    $hostEntry = @($owned | Where-Object { $_.record.host })[0]
    if (-not $hostEntry.process.WaitForExit(15000)) { throw 'Host quit timed out.' }
    $manifest.status = 'completed'
}
catch {
    $failure = $_.Exception.Message
    $manifest.status = 'failed'
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
    $manifest.user_ini_after = File-Hash $sourceIni
    $manifest.config_after = File-Hash $config
    $manifest.module_after = File-Hash $module
    $manifest.map_after = File-Hash $mapFile
    $manifest.completed_utc = [DateTime]::UtcNow.ToString('o')
    if (Test-Path -LiteralPath $runDirectory) {
        try { Write-Manifest }
        catch {
            if (-not $failure) { $failure = 'Final manifest write failed: ' + $_.Exception.Message }
            Write-Warning ('Final manifest write failed after owned-process cleanup: ' + $_.Exception.Message)
        }
        # 기록 쓰기 실패도 실패로 남기되 이미 만든 manifest의 판정은 계속 시도
        if (Test-Path -LiteralPath (Join-Path $runDirectory 'session.json')) {
            try {
                & $PythonExe -X utf8 $judge $runDirectory
                if ($LASTEXITCODE -ne 0 -and -not $failure) { $failure = 'Strict pour judgement failed; inspect judgement.json.' }
            }
            catch {
                if (-not $failure) { $failure = 'Judge execution failed: ' + $_.Exception.Message }
                Write-Warning ('Judge execution failed: ' + $_.Exception.Message)
            }
        }
    }
    Write-Output ('Pour validation output=' + $runDirectory)
}
if ($failure) { throw $failure }
