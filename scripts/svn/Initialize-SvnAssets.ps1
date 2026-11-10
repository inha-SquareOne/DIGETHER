<#
.SYNOPSIS
현재 PC의 프로젝트 폴더를 SVN trunk 작업 사본으로 만들고 Content·SourceArt를 첫 커밋한다(ADR-004).

.DESCRIPTION
- 저장소 구조: <RepoUrl>/trunk/Content, <RepoUrl>/trunk/SourceArt
- 작업 사본은 프로젝트 폴더(DIGETHER/DIGETHER) 하나이며 trunk의 svn:ignore '*'로 Content·SourceArt 외에는 무시
- Git 폴더(Game/Blueprints, Game/UI, Game/Input, Data, Input, Editor, Frontend/UI, Frontend/Localization)는 svn:ignore로 제외
- 바이너리는 svn:auto-props로 svn:needs-lock 지정
- 끝나면 저장소 루트 content.rev에 SVN 리비전을 기록
최신 Content를 가진 PC에서 한 번만 실행한다. 다른 PC는 프로젝트 폴더에 trunk를 svn checkout으로 받는다.
2026-11-10 첫 등록은 폴더별 작업 사본 방식으로 실행했고 같은 날 이 구조로 바꿨다. 이 순서 그대로의 실행은 검증하지 않았다.

.EXAMPLE
.\scripts\svn\Initialize-SvnAssets.ps1 -RepoUrl svn://team-pc/DIGETHER -DryRun
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RepoUrl,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Project = Join-Path $RepoRoot 'DIGETHER'
$RepoUrl = $RepoUrl.TrimEnd('/')

function Invoke-Svn {
    param([string[]]$SvnArgs)
    Write-Host ("svn " + ($SvnArgs -join ' '))
    if ($DryRun) { return }
    & svn @SvnArgs
    if ($LASTEXITCODE -ne 0) { throw "svn 실패: $($SvnArgs -join ' ')" }
}

function Set-SvnPropFile {
    param([string]$Name, [string[]]$Lines, [string]$Target)
    $file = [System.IO.Path]::GetTempFileName()
    [System.IO.File]::WriteAllText($file, ($Lines -join "`n") + "`n", (New-Object System.Text.UTF8Encoding $false))
    try { Invoke-Svn @('propset', $Name, '-F', $file, $Target) } finally { Remove-Item $file -Force }
}

if (-not $DryRun -and -not (Get-Command svn -ErrorAction SilentlyContinue)) {
    throw 'svn 명령을 찾을 수 없다. TortoiseSVN 설치 시 command line client tools를 선택한다.'
}

$AutoProps = @(
    '*.uasset = svn:needs-lock=*;svn:mime-type=application/octet-stream'
    '*.umap = svn:needs-lock=*;svn:mime-type=application/octet-stream'
    '*.blend = svn:needs-lock=*'
    '*.fbx = svn:needs-lock=*'
    '*.obj = svn:needs-lock=*'
    '*.psd = svn:needs-lock=*'
    '*.png = svn:needs-lock=*'
    '*.tga = svn:needs-lock=*'
    '*.exr = svn:needs-lock=*'
    '*.wav = svn:needs-lock=*'
    '*.spp = svn:needs-lock=*'
)

# Content: Git 폴더를 svn:ignore로 제외
$Targets = @(
    @{ Name = 'Content'; Ignore = @{ '.' = @('Data', 'Input', 'Editor'); 'Game' = @('Blueprints', 'UI', 'Input'); 'Frontend' = @('UI', 'Localization') } }
    @{ Name = 'SourceArt'; Ignore = @{} }
)

# 프로젝트 폴더를 trunk 작업 사본으로 만들고 전부 무시한다
# 등록된 Content·SourceArt는 무시 규칙의 영향을 받지 않으므로 그 둘만 관리된다
$TrunkUrl = "$RepoUrl/trunk"
Invoke-Svn @('mkdir', '--parents', $TrunkUrl, '-m', 'trunk 생성')
Invoke-Svn @('checkout', $TrunkUrl, $Project, '--force')
Set-SvnPropFile 'svn:ignore' @('*') $Project
Invoke-Svn @('commit', '--depth=empty', '-m', 'trunk 무시 규칙 설정', $Project)

foreach ($t in $Targets) {
    $wc = Join-Path $Project $t.Name
    if (-not (Test-Path $wc)) { Write-Warning "$wc 없음, 건너뜀"; continue }

    # 경로를 직접 지정하면 무시 규칙과 무관하게 등록된다
    Invoke-Svn @('add', '--depth=empty', $wc)

    # 속성부터 커밋해야 이후 add에 auto-props·ignore가 적용된다
    $propTargets = @($wc)
    Set-SvnPropFile 'svn:auto-props' $AutoProps $wc
    foreach ($dir in $t.Ignore.Keys) {
        $path = if ($dir -eq '.') { $wc } else { Join-Path $wc $dir }
        if ($dir -ne '.') { Invoke-Svn @('add', '--depth=empty', $path); $propTargets += $path }
        Set-SvnPropFile 'svn:ignore' $t.Ignore[$dir] $path
    }
    Invoke-Svn (@('commit', '--depth=empty', '-m', "$($t.Name) 속성 설정") + $propTargets)

    Invoke-Svn @('add', '--force', $wc)
    Invoke-Svn @('commit', $wc, '-m', "$($t.Name) 최초 등록")
}

if ($DryRun) { Write-Host 'DryRun: content.rev 미작성'; return }
# 작업 사본의 폴더 리비전은 속성 커밋 시점에 머무르므로 서버 trunk의 마지막 변경 리비전을 쓴다
$rev = (& svn info --show-item last-changed-revision "$RepoUrl/trunk").Trim()
[System.IO.File]::WriteAllText((Join-Path $RepoRoot 'content.rev'), "$rev`n", (New-Object System.Text.UTF8Encoding $false))
Write-Host "content.rev = $rev"
