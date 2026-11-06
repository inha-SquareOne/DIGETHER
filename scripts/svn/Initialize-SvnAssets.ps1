<#
.SYNOPSIS
현재 PC의 Content·SourceArt를 그 자리에서 SVN 작업 사본으로 만들고 첫 커밋을 한다(ADR-004).

.DESCRIPTION
- 저장소 구조: <RepoUrl>/trunk/Content, <RepoUrl>/trunk/SourceArt
- Git 폴더(Game/Blueprints, Game/UI, Game/Input, Data, Input, Editor, Frontend/UI, Frontend/Localization)는 svn:ignore로 제외
- 바이너리는 svn:auto-props로 svn:needs-lock 지정
- 끝나면 저장소 루트 content.rev에 SVN 리비전을 기록
최신 Content를 가진 PC에서 한 번만 실행한다. 다른 PC는 svn checkout으로 받는다.

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

foreach ($t in $Targets) {
    $url = "$RepoUrl/trunk/$($t.Name)"
    $wc = Join-Path $Project $t.Name
    if (-not (Test-Path $wc)) { Write-Warning "$wc 없음, 건너뜀"; continue }

    Invoke-Svn @('mkdir', '--parents', $url, '-m', "$($t.Name) 저장소 생성")
    Invoke-Svn @('checkout', $url, $wc, '--force')

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
$rev = (& svn info --show-item last-changed-revision (Join-Path $Project 'Content')).Trim()
[System.IO.File]::WriteAllText((Join-Path $RepoRoot 'content.rev'), "$rev`n", (New-Object System.Text.UTF8Encoding $false))
Write-Host "content.rev = $rev"
