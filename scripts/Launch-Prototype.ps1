<#
.SYNOPSIS
UE 5.8.4 그레이박스 호스트 또는 참가자 창을 실행합니다.
.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Launch-Prototype.ps1 -EngineRoot 'C:/Program Files/Epic Games/UE_5.8' -Mode Host
.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Launch-Prototype.ps1 -EngineRoot 'C:/Program Files/Epic Games/UE_5.8' -Mode Client -Address '127.0.0.1:7777' -PlayerId Client1
.DESCRIPTION
-GameExe를 생략하면 현재 프로젝트를 UnrealEditor.exe -game으로 실행합니다. 지정하면 패키지의 Binaries/Win64 실제 exe를 실행합니다.
시험 ID는 인증이 아닙니다. 동일 SaveSlot과 PlayerId는 동일한 별도 UserDir을 사용합니다.
다른 프로세스는 종료하지 않습니다. 패키지의 맵은 5.8.4 UnrealPak IoStore 목록으로 사전에 확인합니다.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Host','Client')][string]$Mode='Host',
    [string]$Address='127.0.0.1:7777',
    [ValidateRange(1024,65535)][int]$Port=7777,
    [string]$PlayerId,
    [string]$SaveSlot='PrototypePlay',
    [string]$GameExe
)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
try {
    if (-not [IO.Path]::IsPathRooted($EngineRoot)) { throw 'EngineRoot는 UE 5.8.4 설치 폴더의 절대 경로여야 합니다.' }
    $engine=[IO.Path]::GetFullPath($EngineRoot)
    $versionPath=Join-Path $engine 'Engine/Build/Build.version'
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) { throw "엔진 버전 파일을 찾을 수 없습니다: $versionPath" }
    $version=Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ([int]$version.MajorVersion -ne 5 -or [int]$version.MinorVersion -ne 8 -or [int]$version.PatchVersion -ne 4) { throw "UE 5.8.4가 필요합니다. 확인된 버전: $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)" }
    if (-not $PlayerId) { $PlayerId=$(if($Mode -eq 'Host'){'Host'}else{'Client1'}) }
    if ($PlayerId -notmatch '^[A-Za-z0-9_-]{1,48}$' -or $SaveSlot -notmatch '^[A-Za-z0-9_-]{1,48}$') { throw 'PlayerId와 SaveSlot은 영문·숫자·밑줄·하이픈 1~48자로 지정하세요.' }
    if ($Mode -eq 'Client' -and $Address -notmatch '^[A-Za-z0-9.-]+:[0-9]{1,5}$') { throw 'Client Address는 호스트이름:포트 또는 IPv4:포트 형식이어야 합니다.' }
    if ($Mode -eq 'Client') {
        $addressPort=[int]($Address.Substring($Address.LastIndexOf(':')+1))
        if($addressPort -lt 1024 -or $addressPort -gt 65535){throw 'Client Address 포트는 1024~65535여야 합니다.'}
    }
    $repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $project=Join-Path $repo 'DIGETHER/DIGETHER.uproject'
    $mapPackage='/Game/Prototype/Maps/L_Prototype'
    $userDir=Join-Path $repo ("DIGETHER/Saved/PrototypePlay/{0}/{1}" -f $SaveSlot,$PlayerId)
    New-Item -ItemType Directory -Path $userDir -Force | Out-Null
    $launchId=[guid]::NewGuid().ToString('N')
    $log=Join-Path $userDir ("launch-$launchId.log")
    $launchArgs=@()
    $isEditor = -not [bool]$GameExe
    if ($GameExe) {
        if(-not [IO.Path]::IsPathRooted($GameExe)){throw 'GameExe는 패키지의 실제 exe 절대 경로여야 합니다.'}
        $exe=[IO.Path]::GetFullPath($GameExe)
        if(-not (Test-Path -LiteralPath $exe -PathType Leaf) -or [IO.Path]::GetExtension($exe) -ine '.exe' -or [IO.Path]::GetDirectoryName($exe) -notmatch '[\\/]Binaries[\\/]Win64$') { throw 'GameExe는 패키지 DIGETHER/Binaries/Win64 안의 실제 exe여야 합니다.' }
        $packageProject=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($exe)) '../..'))
        $paks=Join-Path $packageProject 'Content/Paks'
        $utocs=@(Get-ChildItem -LiteralPath $paks -Filter '*.utoc' -File -ErrorAction SilentlyContinue)
        if($utocs.Count -eq 0){throw "IoStore 컨테이너가 없습니다: $paks"}
        $unrealPak=Join-Path $engine 'Engine/Binaries/Win64/UnrealPak.exe'
        if(-not (Test-Path -LiteralPath $unrealPak -PathType Leaf)){throw "패키지 맵을 검증할 UnrealPak이 없습니다: $unrealPak"}
        $listing=Join-Path $userDir ("iostore-$launchId.csv")
        $listLog=Join-Path $userDir ("iostore-$launchId.log")
        $wildcard=Join-Path $paks '*.utoc'
        & $unrealPak "-ListContainer=$wildcard" "-csv=$listing" *> $listLog
        if($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $listing -PathType Leaf)){throw "IoStore 목록 생성에 실패했습니다. 로그: $listLog"}
        $entries=@(Import-Csv -LiteralPath $listing | Where-Object { $_.Filename -and $_.Filename.Trim().Replace('\','/').EndsWith('/DIGETHER/Content/Prototype/Maps/L_Prototype.umap',[StringComparison]::OrdinalIgnoreCase) })
        if($entries.Count -eq 0){throw "선택한 패키지에 L_Prototype 맵이 없습니다. 목록: $listing"}
        Write-Host "패키지 맵 확인: $listing"
    } else {
        $exe=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
        $mapFile=Join-Path $repo 'DIGETHER/Content/Prototype/Maps/L_Prototype.umap'
        $dll=Join-Path $repo 'DIGETHER/Binaries/Win64/UnrealEditor-DIGETHER.dll'
        foreach($file in @($exe,$project,$mapFile,$dll)){if(-not (Test-Path -LiteralPath $file -PathType Leaf)){throw "실행 전 필요한 엔진·프로젝트·맵·빌드 파일이 없습니다: $file"}}
        $sources=@(Get-ChildItem -LiteralPath (Join-Path $repo 'DIGETHER/Source/DIGETHER') -Recurse -File | Where-Object { $_.Extension -in '.cpp','.h','.cs' })
        $newest=$sources | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if($newest -and $newest.LastWriteTimeUtc -gt (Get-Item -LiteralPath $dll).LastWriteTimeUtc){throw "Editor 모듈보다 변경된 소스가 있습니다. UE 5.8.4 Editor 타깃 빌드를 먼저 실행하세요: $($newest.FullName)"}
        $launchArgs += "`"$project`""
        Write-Host "Editor 맵 및 모듈 확인: $mapFile / $dll"
    }
    $url=$(if($Mode -eq 'Host'){"${mapPackage}?listen?PrototypePlayerId=$PlayerId"}else{"${Address}?PrototypePlayerId=$PlayerId"})
    $launchArgs += $url
    if($isEditor){$launchArgs += '-game'}
    if($Mode -eq 'Host'){
        $udp=$null
        try {
            $udp=New-Object System.Net.Sockets.UdpClient
            $udp.ExclusiveAddressUse=$true
            $udp.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any,$Port)))
        } catch { throw "호스트 UDP 포트 $Port 를 사용할 수 없습니다. 다른 프로세스는 종료하지 않습니다." }
        finally { if($null -ne $udp){$udp.Dispose()} }
        $launchArgs += "-port=$Port"
    }
    $launchArgs += @('-nosteam','-windowed','-ResX=1280','-ResY=720','-nosplash','-nop4',"-PrototypePlayerId=$PlayerId","-PrototypeSaveSlot=$SaveSlot","-UserDir=`"$userDir`"","-abslog=`"$log`"")
    $argumentLine=$launchArgs -join ' '
    Write-Host "엔진: $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
    Write-Host "시험 참가자: $Mode / $PlayerId / 저장 슬롯 $SaveSlot"
    Write-Host "실행: `"$exe`" $argumentLine"
    Write-Host "로그: $log"
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$exe; $info.Arguments=$argumentLine; $info.WorkingDirectory=[IO.Path]::GetDirectoryName($exe)
    $info.UseShellExecute=$false; $info.CreateNoWindow=$false
    $process=[System.Diagnostics.Process]::Start($info)
    if($null -eq $process){throw '게임 프로세스를 시작하지 못했습니다.'}
    Write-Host "시작한 게임 PID: $($process.Id). 게임 창에서 직접 조작하세요."
    Write-Host '시험 ID는 인증 수단이 아닙니다. 기존 프로세스는 건드리지 않았습니다.'
    exit 0
} catch {
    Write-Error $_.Exception.Message -ErrorAction Continue
    exit 1
}
