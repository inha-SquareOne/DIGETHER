#Requires -Version 5.1
<#
.SYNOPSIS
Runs DIGETHER builds, Workbench automation, or DataTable validation with UE 5.8.2.
.EXAMPLE
.\scripts\Invoke-UnrealChecks.ps1 BuildEditor -EngineRoot 'C:\Program Files\Epic Games\UE_5.8' -DryRun
.EXAMPLE
$env:UE_ENGINE_ROOT = 'C:\Program Files\Epic Games\UE_5.8'
.\scripts\Invoke-UnrealChecks.ps1 Automation -TimeoutSeconds 600
.EXAMPLE
.\scripts\Invoke-UnrealChecks.ps1 ValidateData -AssetType /Script/Engine.DataTable
.NOTES
Windows PowerShell 5.1 and PowerShell 7 on Windows. Build modes use Epic's Build.bat.
Automation does not build first. Run BuildEditor after changing C++ tests.
Each real run creates a new project Saved/DeveloperChecks folder; DryRun writes nothing.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('BuildEditor', 'BuildGame', 'Automation', 'ValidateData')]
    [string]$Mode,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidatePattern('^(?:/Script/[A-Za-z0-9_]+\.)?[A-Za-z_][A-Za-z0-9_]*$')]
    [string]$AssetType = '/Script/Engine.DataTable',
    [ValidateRange(1, 86400)]
    [int]$TimeoutSeconds = 900,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-UnrealEngineVersion {
    param([string]$Root)
    if ([string]::IsNullOrWhiteSpace($Root)) {
        throw 'Specify -EngineRoot or UE_ENGINE_ROOT; automatic engine selection is disabled.'
    }
    $resolved = (Resolve-Path -LiteralPath $Root).Path
    $version = Get-Content -LiteralPath (Join-Path $resolved 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 2) {
        throw "UE 5.8.2 is required; found $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)."
    }
    return $resolved
}

function Get-WorkbenchTestNames {
    param([string]$ProjectDirectory)
    $names = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    # Limit discovery to source test directories, never Saved/Intermediate or old reports.
    $sources = @(Get-ChildItem -LiteralPath (Join-Path $ProjectDirectory 'Plugins') -Directory |
        ForEach-Object {
            $sourceRoot = Join-Path $_.FullName 'Source'
            if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
                Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter '*.cpp'
            }
        } |
        Where-Object { $_.FullName -match '[\\/]Source[\\/].*[\\/]Private[\\/]Tests[\\/]' })
    $pattern = 'IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*"((?:DevelopmentWorkbench\.|DIGETHER\.Workbench\.)[^\"]+)"'
    foreach ($source in $sources) {
        $content = Get-Content -LiteralPath $source.FullName -Raw
        # Do not treat a commented-out macro as an expected compiled test.
        $content = [regex]::Replace($content, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
        foreach ($match in [regex]::Matches($content, $pattern)) {
            if (-not $names.Add($match.Groups[1].Value)) {
                throw "Duplicate source test name: $($match.Groups[1].Value)"
            }
        }
    }
    if ($names.Count -eq 0) { throw 'No Workbench automation tests were found in project plugin source.' }
    return @($names | Sort-Object)
}

function Read-WorkbenchAutomationResult {
    param([string]$ReportPath, [string[]]$ExpectedTests)
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "Automation report missing: $ReportPath" }
    $report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
    $tests = @($report.tests)
    if ($ExpectedTests.Count -eq 0 -or $tests.Count -eq 0) { throw 'Automation ran zero tests.' }
    $actual = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($test in $tests) {
        if ([string]::IsNullOrWhiteSpace($test.fullTestPath) -or -not $actual.Add($test.fullTestPath)) {
            throw "Missing or duplicate fullTestPath in automation report: $($test.fullTestPath)"
        }
        if ($test.state -cne 'Success' -or $test.errors -ne 0) {
            throw "Automation test did not succeed: $($test.fullTestPath) ($($test.state), errors=$($test.errors))"
        }
    }
    $missing = @($ExpectedTests | Where-Object { -not $actual.Contains($_) })
    $unexpected = @($actual | Where-Object { $ExpectedTests -cnotcontains $_ })
    if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
        throw "Automation test set differs from source. Missing: $($missing -join ', '); unexpected: $($unexpected -join ', ')"
    }
    foreach ($field in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess')) {
        $value = $report.$field
        if ($null -eq $value -or "$value" -notmatch '^\d+$') { throw "Invalid automation counter: $field" }
    }
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or
        ([int]$report.succeeded + [int]$report.succeededWithWarnings) -ne $ExpectedTests.Count) {
        throw 'Automation counters show failures, unexecuted tests, or an incomplete report.'
    }
    return [ordered]@{ Expected = $ExpectedTests.Count; Passed = $tests.Count; WithWarnings = $report.succeededWithWarnings; Report = $ReportPath }
}

function Read-UnrealDataValidationResult {
    param([string]$LogPath)
    $content = Get-Content -LiteralPath $LogPath -Raw
    $starts = [regex]::Matches($content, 'LogContentValidation:.*Starting to validate (\d+) assets')
    if ($starts.Count -ne 1) { throw 'Data validation target count is missing or ambiguous.' }
    $requested = [int]$starts[0].Groups[1].Value
    $valid = [regex]::Matches($content, '(?m)^.*AssetCheck:.* contains valid data(?:, but has warnings)?\.[\r]?$').Count
    $invalid = [regex]::Matches($content, '(?m)^.*AssetCheck:.* contains invalid data\.[\r]?$').Count
    $unable = [regex]::Matches($content, '(?m)^.*AssetCheck:.* has no data validation\.[\r]?$').Count
    # UE 5.8.2's stock commandlet returns 0 even for invalid assets. Check its per-asset results.
    if ($requested -eq 0 -or $invalid -gt 0 -or $unable -gt 0 -or $valid -ne $requested) {
        throw "Data validation incomplete or failed: requested=$requested, valid=$valid, invalid=$invalid, unable=$unable."
    }
    if ($content -notmatch 'LogContentValidation:.*Validated asset counts for' -or
        $content -notmatch 'Commandlet DataValidationCommandlet_\d+ finished execution \(result 0\)') {
        throw 'Data validation completion markers are missing.'
    }
    $warnings = [regex]::Matches($content, '(?m)^.*AssetCheck:.* contains valid data, but has warnings\.[\r]?$').Count
    return [ordered]@{ Requested = $requested; Valid = $valid; ValidWithWarnings = $warnings; Invalid = $invalid; UnableToValidate = $unable; Log = $LogPath }
}

function Read-UnrealBuildResult {
    param([string]$StdoutPath)
    if (-not (Test-Path -LiteralPath $StdoutPath -PathType Leaf)) { throw "Build output missing: $StdoutPath" }
    $content = Get-Content -LiteralPath $StdoutPath -Raw
    if ([string]::IsNullOrEmpty($content)) { throw 'Build output is empty; exit code 0 alone is not a build success.' }
    $results = @([regex]::Matches($content, '(?m)^\s*Result:\s*(\S+)'))
    if ($results.Count -ne 1) { throw "Build output must contain exactly one UBT Result line; found $($results.Count)." }
    $status = $results[0].Groups[1].Value
    if ($status -cne 'Succeeded' -and $status -cne 'UpToDate') { throw "UBT result was not successful: $status" }
    $time = [regex]::Match($content, '(?m)^\s*Total execution time:\s*([0-9.,]+) seconds')
    if (-not $time.Success) { throw 'UBT completion time line is missing; the build output looks truncated.' }
    return [ordered]@{ UbtResult = $status; TotalSeconds = $time.Groups[1].Value; Log = $StdoutPath }
}

function ConvertTo-WindowsArgument {
    param([AllowEmptyString()][string]$Value)
    # Windows native argument quoting; no Invoke-Expression or shell interpolation.
    return '"' + ([regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1')) + '"'
}

function Get-UnrealUserSettingsPaths {
    param([string]$ProjectDirectory, [string]$RunDirectory)
    return [ordered]@{
        Source   = Join-Path $ProjectDirectory 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
        Isolated = Join-Path $RunDirectory 'EditorPerProjectUserSettings.ini'
    }
}

function Initialize-IsolatedUserSettings {
    param([string]$SourcePath, [string]$IsolatedPath, [string]$RunDirectory)
    # Copy only; the original is never written, moved, or restored. Never overwrite an existing target.
    $run = [IO.Path]::GetFullPath($RunDirectory).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($IsolatedPath)
    $source = [IO.Path]::GetFullPath($SourcePath)
    if (-not (Test-Path -LiteralPath $run -PathType Container)) { throw "Run directory missing: $run" }
    if ([IO.Path]::GetDirectoryName($target).TrimEnd('\', '/') -ine $run -or
        [IO.Path]::GetFileName($target) -cne 'EditorPerProjectUserSettings.ini') {
        throw "Isolated settings must be EditorPerProjectUserSettings.ini directly inside the run directory: $target"
    }
    if ($source -ieq $target) { throw 'Isolated settings path must differ from the original.' }
    if (Test-Path -LiteralPath $target) { throw "Refusing to overwrite isolated settings: $target" }
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        [IO.File]::Copy($source, $target, $false)
        return $true
    }
    $stream = [IO.File]::Open($target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    $stream.Dispose()
    return $false
}

function Stop-OwnedCheckProcess {
    param([System.Diagnostics.Process]$Process)
    if ($null -ne $Process -and -not $Process.HasExited) {
        # The PID comes only from this invocation's Start-Process, never from a name search.
        & "$env:SystemRoot\System32\taskkill.exe" /PID $Process.Id /T /F | Out-Null
        $Process.WaitForExit(10000) | Out-Null
        if (-not $Process.HasExited) { throw "Could not stop owned process $($Process.Id)." }
    }
}

# Permit fixture tests to import just these small validation helpers without launching UE.
if ($MyInvocation.InvocationName -eq '.') { return }

$run = $null
$process = $null
$scriptExitCode = 1
try {
    if ($env:OS -ne 'Windows_NT') { throw 'This runner supports Windows only.' }
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    $projects = @(Get-ChildItem -LiteralPath $repositoryRoot -File -Filter '*.uproject')
    $projects += @(Get-ChildItem -LiteralPath $repositoryRoot -Directory |
        Where-Object { -not $_.Name.StartsWith('.') } |
        ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Filter '*.uproject' })
    if ($projects.Count -ne 1) { throw "Expected one .uproject at repository root or one directory below it; found $($projects.Count)." }
    $project = $projects[0]
    $EngineRoot = Assert-UnrealEngineVersion $EngineRoot
    $runDirectory = Join-Path $project.DirectoryName ('Saved/DeveloperChecks/{0}-{1}-{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), $Mode, [guid]::NewGuid().ToString('N'))
    $logPath = Join-Path $runDirectory 'engine.log'
    $expectedTests = @()
    if ($Mode -like 'Build*') {
        $target = $project.BaseName
        if ($Mode -eq 'BuildEditor') { $target += 'Editor' }
        $targetFile = Join-Path $project.DirectoryName "Source/$target.Target.cs"
        if (-not (Test-Path -LiteralPath $targetFile -PathType Leaf)) { throw "Build target missing: $targetFile" }
        $buildScript = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
        if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) { throw "Build script missing: $buildScript" }
        # Build.bat uses CALL and delayed expansion. Reject paths those cmd features can rewrite.
        if (($buildScript + $project.FullName) -match '[%!*?"&|<>^\r\n]') { throw 'Build paths cannot contain cmd.exe metacharacters.' }
        $executable = "$env:SystemRoot\System32\cmd.exe"
        $arguments = @($buildScript, $target, 'Win64', 'Development', "-Project=$($project.FullName)", '-WaitMutex', '-NoHotReloadFromIDE')
        $argumentLine = '/d /s /c "' + (($arguments | ForEach-Object { ConvertTo-WindowsArgument $_ }) -join ' ') + '"'
    }
    else {
        $executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        $arguments = @($project.FullName, '-unattended', '-nop4', '-nosplash', '-NullRHI', '-culture=en', "-abslog=$logPath",
            '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False')
        $userSettings = Get-UnrealUserSettingsPaths $project.DirectoryName $runDirectory
        $arguments += "-EditorPerProjectUserSettingsIni=$($userSettings.Isolated)"
        if ($Mode -eq 'Automation') {
            $expectedTests = @(Get-WorkbenchTestNames $project.DirectoryName)
            $arguments += @('-ExecCmds=Automation RunTests Workbench.', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$(Join-Path $runDirectory 'automation')")
        }
        else { $arguments += @('-run=DataValidation', "-AssetType=$AssetType", '-LogCmds=LogDataValidation Log') }
        $argumentLine = ($arguments | ForEach-Object { ConvertTo-WindowsArgument $_ }) -join ' '
    }
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Executable missing: $executable" }
    $run = [ordered]@{
        Mode = $Mode; EngineVersion = '5.8.2'; EngineRoot = $EngineRoot; Project = $project.FullName
        StartedUtc = [DateTime]::UtcNow.ToString('o'); CompletedUtc = $null; TimeoutSeconds = $TimeoutSeconds
        OutputDirectory = $runDirectory; Executable = $executable; Arguments = $arguments; NativeArguments = $argumentLine
        ExpectedTests = $expectedTests; ProcessId = $null; ProcessExitCode = $null; State = 'Planned'; Result = $null; Error = $null
    }
    if ($Mode -notlike 'Build*') {
        $run.UserSettingsSource = $userSettings.Source
        $run.IsolatedUserSettings = $userSettings.Isolated
        $run.UserSettingsSourceExisted = $null
    }
    if ($DryRun) { $run | ConvertTo-Json -Depth 8; exit 0 }
    if (Test-Path -LiteralPath $runDirectory) { throw "Refusing to reuse output directory: $runDirectory" }
    New-Item -ItemType Directory -Path $runDirectory | Out-Null
    if ($Mode -notlike 'Build*') {
        $run.UserSettingsSourceExisted = Initialize-IsolatedUserSettings $userSettings.Source $userSettings.Isolated $runDirectory
    }
    $run.State = 'Running'
    $run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runDirectory 'execution.json') -Encoding UTF8
    $workingDirectory = $project.DirectoryName
    if ($Mode -like 'Build*') { $workingDirectory = Join-Path $EngineRoot 'Engine/Source' }
    Write-Host "Running $Mode; output: $runDirectory"
    $process = Start-Process -FilePath $executable -ArgumentList $argumentLine -WorkingDirectory $workingDirectory -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runDirectory 'stdout.log') -RedirectStandardError (Join-Path $runDirectory 'stderr.log')
    # Cache the native handle while alive so ExitCode stays available in Windows PowerShell 5.1.
    $null = $process.Handle
    $run.ProcessId = $process.Id
    $run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runDirectory 'execution.json') -Encoding UTF8
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $run.State = 'TimedOut'
        $scriptExitCode = 124
        throw "Timed out after $TimeoutSeconds seconds; stopping owned process $($process.Id) and its children."
    }
    $process.WaitForExit()
    $run.ProcessExitCode = $process.ExitCode
    if ($null -eq $run.ProcessExitCode -or $run.ProcessExitCode -ne 0) { throw "Unreal process exit code: $($run.ProcessExitCode)" }
    if ($Mode -like 'Build*') { $run.Result = Read-UnrealBuildResult (Join-Path $runDirectory 'stdout.log') }
    elseif ($Mode -eq 'Automation') {
        $run.Result = Read-WorkbenchAutomationResult (Join-Path $runDirectory 'automation/index.json') $expectedTests
    }
    elseif ($Mode -eq 'ValidateData') { $run.Result = Read-UnrealDataValidationResult $logPath }
    $run.State = 'Passed'
    $scriptExitCode = 0
}
catch {
    if ($null -ne $run) {
        if ($run.State -ne 'TimedOut') { $run.State = 'Failed' }
        $run.Error = $_.Exception.Message
    }
    [Console]::Error.WriteLine($_.Exception.Message)
}
finally {
    try { Stop-OwnedCheckProcess $process }
    catch {
        $scriptExitCode = 1
        if ($null -ne $run) {
            $run.State = 'Failed'
            $run.Error += " Cleanup failed: $($_.Exception.Message)"
        }
        [Console]::Error.WriteLine($_.Exception.Message)
    }
    if ($null -ne $run -and -not $DryRun -and (Test-Path -LiteralPath $run.OutputDirectory -PathType Container)) {
        if ($null -ne $process -and $process.HasExited) { $run.ProcessExitCode = $process.ExitCode }
        $run.CompletedUtc = [DateTime]::UtcNow.ToString('o')
        $run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run.OutputDirectory 'execution.json') -Encoding UTF8
    }
}
if ($null -ne $run) { $run | ConvertTo-Json -Depth 8 }
exit $scriptExitCode
