#Requires -Version 5.1
# No Unreal process is started. Run with powershell.exe or pwsh.exe -NoProfile -File <this file>.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-UnrealChecks.ps1') Automation

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('DigetherUnrealRunnerTests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$passed = 0
function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected {
    param([scriptblock]$Action, [string]$Name)
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-True $rejected "Expected rejection: $Name"
    $script:passed++
}
function Write-AutomationFixture {
    param([object[]]$Tests, [int]$Succeeded = 2, [int]$Failed = 0, [int]$NotRun = 0)
    $fixture = Join-Path $testRoot 'index.json'
    @{ tests = @($Tests); succeeded = $Succeeded; succeededWithWarnings = 0; failed = $Failed; notRun = $NotRun; inProcess = 0 } |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $fixture -Encoding UTF8
    return $fixture
}
function Write-ValidationFixture {
    param([int]$Requested = 2, [string[]]$States = @('contains valid data.', 'contains valid data.'), [switch]$Incomplete)
    $fixture = Join-Path $testRoot 'validation.log'
    $lines = @("LogContentValidation: Display: Starting to validate $Requested assets (0 associated objects such as actors)")
    for ($i = 0; $i -lt $States.Count; $i++) { $lines += "AssetCheck: /Script/Engine.DataTable'/Game/DT_$i.DT_$i' $($States[$i])" }
    if (-not $Incomplete) {
        $lines += @('LogContentValidation: Validated asset counts for 1 validators:',
            'LogCore: Engine exit requested (reason: Commandlet DataValidationCommandlet_0 finished execution (result 0))')
    }
    $lines | Set-Content -LiteralPath $fixture -Encoding UTF8
    return $fixture
}

try {
    $engine = Join-Path $testRoot 'engine with spaces'
    New-Item -ItemType Directory -Path (Join-Path $engine 'Engine/Build') -Force | Out-Null
    $versionFile = Join-Path $engine 'Engine/Build/Build.version'
    @{ MajorVersion = 5; MinorVersion = 8; PatchVersion = 2 } | ConvertTo-Json | Set-Content $versionFile -Encoding UTF8
    Assert-True ((Assert-UnrealEngineVersion $engine) -eq $engine) '5.8.2 must be accepted.'
    $passed++
    @{ MajorVersion = 5; MinorVersion = 8; PatchVersion = 1 } | ConvertTo-Json | Set-Content $versionFile -Encoding UTF8
    Assert-Rejected { Assert-UnrealEngineVersion $engine } 'wrong patch'
    Assert-Rejected { Assert-UnrealEngineVersion '' } 'no engine selected'

    $project = Join-Path $testRoot 'project'
    $testDirectory = Join-Path $project 'Plugins/TestPlugin/Source/TestEditor/Private/Tests'
    New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
    @'
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA, "DevelopmentWorkbench.Fixture.A", Flags)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FB,
    "DIGETHER.Workbench.Fixture.B", Flags)
// IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommented, "DevelopmentWorkbench.Commented", Flags)
/* IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlock, "DevelopmentWorkbench.Block", Flags) */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOther, "Unrelated.Fixture", Flags)
'@ | Set-Content (Join-Path $testDirectory 'Fixture.cpp') -Encoding UTF8
    $expected = @(Get-WorkbenchTestNames $project)
    Assert-True ($expected.Count -eq 2) 'Discover exactly the two current Workbench namespaces.'
    $passed++
    $emptyProject = Join-Path $testRoot 'empty-project'
    New-Item -ItemType Directory -Path (Join-Path $emptyProject 'Plugins') -Force | Out-Null
    Assert-Rejected { Get-WorkbenchTestNames $emptyProject } 'zero source tests'

    $tests = @($expected | ForEach-Object { @{ fullTestPath = $_; state = 'Success'; errors = 0 } })
    $fixture = Write-AutomationFixture $tests
    $result = Read-WorkbenchAutomationResult $fixture $expected
    Assert-True ($result.Passed -eq 2) 'Matching successful report must pass.'
    $passed++
    Assert-Rejected { Read-WorkbenchAutomationResult (Join-Path $testRoot 'missing.json') $expected } 'missing report'
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture @() 0) $expected } 'zero report tests'
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture @($tests[0]) 1) $expected } 'one expected test missing'
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture @($tests[0], $tests[0])) $expected } 'duplicate test'
    $unexpected = @($tests[0], @{ fullTestPath = 'DevelopmentWorkbench.Fixture.Extra'; state = 'Success'; errors = 0 })
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture $unexpected) $expected } 'unexpected replacement test'
    $failed = @($tests[0], @{ fullTestPath = $expected[1]; state = 'Fail'; errors = 1 })
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture $failed 1 1) $expected } 'failed state'
    $notRun = @($tests[0], @{ fullTestPath = $expected[1]; state = 'NotRun'; errors = 0 })
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture $notRun 1 0 1) $expected } 'unexecuted state'
    Assert-Rejected { Read-WorkbenchAutomationResult (Write-AutomationFixture $tests 1) $expected } 'inconsistent counters'
    '{malformed' | Set-Content -LiteralPath $fixture -Encoding UTF8
    Assert-Rejected { Read-WorkbenchAutomationResult $fixture $expected } 'malformed JSON'

    $valid = Read-UnrealDataValidationResult (Write-ValidationFixture)
    Assert-True ($valid.Valid -eq 2 -and $valid.Requested -eq 2) 'Two validated DataTables must pass.'
    $passed++
    Assert-Rejected { Read-UnrealDataValidationResult (Write-ValidationFixture -Requested 0 -States @()) } 'zero assets'
    Assert-Rejected { Read-UnrealDataValidationResult (Write-ValidationFixture -States @('contains valid data.', 'contains invalid data.')) } 'invalid asset even after exit zero'
    Assert-Rejected { Read-UnrealDataValidationResult (Write-ValidationFixture -States @('contains valid data.', 'has no data validation.')) } 'unvalidated asset'
    Assert-Rejected { Read-UnrealDataValidationResult (Write-ValidationFixture -States @('contains valid data.')) } 'skipped or missing result'
    Assert-Rejected { Read-UnrealDataValidationResult (Write-ValidationFixture -Incomplete) } 'truncated validation'
    $warning = Read-UnrealDataValidationResult (Write-ValidationFixture -States @('contains valid data, but has warnings.', 'contains valid data.'))
    Assert-True ($warning.Valid -eq 2 -and $warning.ValidWithWarnings -eq 1) 'Valid assets with warnings still pass and keep their count.'
    $passed++

    $buildLog = Join-Path $testRoot 'build.log'
    $ubtOk = @('Building DIGETHEREditor...', 'Result: Succeeded', 'Total execution time: 24.17 seconds')
    $ubtOk | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-True ((Read-UnrealBuildResult $buildLog).UbtResult -eq 'Succeeded') 'Succeeded UBT output must pass.'
    $passed++
    @('Target is up to date', 'Result: UpToDate', 'Total execution time: 0.52 seconds') | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-True ((Read-UnrealBuildResult $buildLog).UbtResult -eq 'UpToDate') 'No-op UBT output must pass.'
    $passed++
    @('error C2374', 'Result: Failed (OtherCompilationError)', 'Total execution time: 24.17 seconds') | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-Rejected { Read-UnrealBuildResult $buildLog } 'failed UBT result'
    '' | Set-Content -LiteralPath $buildLog -Encoding UTF8 -NoNewline
    Assert-Rejected { Read-UnrealBuildResult $buildLog } 'exit zero with empty output'
    @('Building DIGETHEREditor...') | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-Rejected { Read-UnrealBuildResult $buildLog } 'no Result line'
    @('Result: Succeeded') | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-Rejected { Read-UnrealBuildResult $buildLog } 'truncated, no completion time'
    ($ubtOk + $ubtOk) | Set-Content -LiteralPath $buildLog -Encoding UTF8
    Assert-Rejected { Read-UnrealBuildResult $buildLog } 'ambiguous duplicate Result'
    Assert-Rejected { Read-UnrealBuildResult (Join-Path $testRoot 'no-build.log') } 'missing build output'
    $settingsProject = Join-Path $testRoot 'settings project'
    $settingsRun = Join-Path $settingsProject 'Saved/DeveloperChecks/run one'
    New-Item -ItemType Directory -Path $settingsRun -Force | Out-Null
    $paths = Get-UnrealUserSettingsPaths $settingsProject $settingsRun
    Assert-True ($paths.Isolated -eq (Join-Path $settingsRun 'EditorPerProjectUserSettings.ini')) 'Isolated ini must live in the run directory.'
    $passed++
    $nativeArgument = ConvertTo-WindowsArgument "-EditorPerProjectUserSettingsIni=$($paths.Isolated)"
    Assert-True ($nativeArgument.StartsWith('"-EditorPerProjectUserSettingsIni=') -and $nativeArgument.EndsWith('run one\EditorPerProjectUserSettings.ini"')) 'Path with spaces must stay one quoted argument.'
    $passed++

    # Source absent: an empty isolated ini is created and no original appears.
    Assert-True ((Initialize-IsolatedUserSettings $paths.Source $paths.Isolated $settingsRun) -eq $false) 'Missing source must be reported.'
    Assert-True ((Test-Path -LiteralPath $paths.Isolated -PathType Leaf) -and (Get-Item -LiteralPath $paths.Isolated).Length -eq 0) 'Missing source must yield an empty isolated ini.'
    Assert-True (-not (Test-Path -LiteralPath $paths.Source)) 'Original must not be created.'
    $passed++

    # Source present: contents are copied and the original hash is unchanged.
    $secondRun = Join-Path $settingsProject 'Saved/DeveloperChecks/run two'
    New-Item -ItemType Directory -Path $secondRun, (Split-Path -Parent $paths.Source) -Force | Out-Null
    [IO.File]::WriteAllText($paths.Source, "[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]`r`nbAutoStartServer=True`r`n")
    $before = (Get-FileHash -LiteralPath $paths.Source -Algorithm SHA256).Hash
    $secondPaths = Get-UnrealUserSettingsPaths $settingsProject $secondRun
    Assert-True ((Initialize-IsolatedUserSettings $secondPaths.Source $secondPaths.Isolated $secondRun) -eq $true) 'Existing source must be reported.'
    Assert-True ((Get-FileHash -LiteralPath $secondPaths.Isolated -Algorithm SHA256).Hash -eq $before) 'Isolated copy must match the original.'
    Assert-True ((Get-FileHash -LiteralPath $paths.Source -Algorithm SHA256).Hash -eq $before) 'Original hash must not change.'
    $passed++

    # Existing target is refused and left untouched.
    Assert-Rejected { Initialize-IsolatedUserSettings $secondPaths.Source $secondPaths.Isolated $secondRun } 'existing isolated ini'
    Assert-True ((Get-FileHash -LiteralPath $secondPaths.Isolated -Algorithm SHA256).Hash -eq $before) 'Rejected overwrite must not alter the target.'
    Assert-Rejected { Initialize-IsolatedUserSettings $paths.Source (Join-Path $settingsProject 'EditorPerProjectUserSettings.ini') $secondRun } 'target outside run directory'
    Assert-Rejected { Initialize-IsolatedUserSettings $paths.Source $paths.Source $secondRun } 'target equals original'

    Write-Output "PASS: $passed Unreal runner fixture checks; no engine process started."
}
finally {
    # Delete only this test's GUID folder, after checking its resolved parent and name.
    $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path
    $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
    if ((Split-Path -Parent $resolvedTestRoot) -ne $tempParent -or (Split-Path -Leaf $resolvedTestRoot) -notmatch '^DigetherUnrealRunnerTests-[0-9a-f]{32}$') {
        throw "Refusing fixture cleanup outside the owned temporary folder: $resolvedTestRoot"
    }
    Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
}
