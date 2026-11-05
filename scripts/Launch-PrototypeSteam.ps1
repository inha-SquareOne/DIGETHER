<#
.SYNOPSIS
Steam 480 그레이박스 방 생성·참가. 서로 다른 PC와 Steam 계정에서 같은 패키지를 사용하세요.
#>
[CmdletBinding()]
param(
    [ValidateSet('Host','Join','Invite')][string]$Mode='Host',
    [ValidatePattern('^[A-Za-z0-9_-]{1,32}$')][string]$Room='DIGETHER-TEST',
    [string]$GameExe
)
Set-StrictMode -Version 2.0
$ErrorActionPreference='Stop'
try {
    if(-not $GameExe) {
        $candidates=@(
            (Join-Path $PSScriptRoot '../DIGETHER/Binaries/Win64/DIGETHER.exe'),
            (Join-Path $PSScriptRoot '../DIGETHER/Saved/Packages/prototype-maze-school-05/Windows/DIGETHER/Binaries/Win64/DIGETHER.exe')
        )
        $GameExe=$candidates | Where-Object {
            (Test-Path -LiteralPath $_ -PathType Leaf) -and
            (Test-Path -Path (Join-Path ([IO.Path]::GetDirectoryName($_)) '../../Content/Paks/*.utoc'))
        } | Select-Object -First 1
    }
    if(-not $GameExe){throw 'Steam 테스트 패키지가 없습니다. -GameExe로 DIGETHER/Binaries/Win64/DIGETHER.exe를 지정하세요.'}
    $exe=$PSCmdlet.GetUnresolvedProviderPathFromPSPath($GameExe)
    if(-not (Test-Path -LiteralPath $exe -PathType Leaf) -or [IO.Path]::GetDirectoryName($exe) -notmatch '[\\/]Binaries[\\/]Win64$') {throw '패키지의 실제 Binaries/Win64 실행 파일이 필요합니다.'}
    if(-not (Get-Process -Name steam -ErrorAction SilentlyContinue)){throw 'Steam 클라이언트를 실행하고 로그인한 뒤 다시 실행하세요.'}
    if(Get-Process -Name DIGETHER -ErrorAction SilentlyContinue){throw '이미 실행 중인 DIGETHER 창을 먼저 닫으세요. 한 Steam 계정으로 여러 참가자를 실행하지 않습니다.'}
    $binaryDir=[IO.Path]::GetDirectoryName($exe)
    $projectDir=[IO.Path]::GetFullPath((Join-Path $binaryDir '../..'))
    if(-not (Test-Path -Path (Join-Path $projectDir 'Content/Paks/*.utoc'))){throw '쿠킹된 콘텐츠가 없는 실행 파일입니다. 패키지 전체를 사용하세요.'}
    $manifestPath=Join-Path (Split-Path $projectDir -Parent) 'SteamPrototype.build.json'
    if(-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)){throw 'Steam 테스트 빌드 표식이 없습니다. 기존 IP 패키지와 혼용할 수 없습니다.'}
    $manifest=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if($manifest.appId -ne 480 -or $manifest.protocol -ne 'maze-school-v1' -or $manifest.exeSha256 -ne (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash){throw 'Steam 빌드 표식과 실행 파일이 일치하지 않습니다. 전체 패키지를 다시 받으세요.'}
    $appIdFile=Join-Path $binaryDir 'steam_appid.txt'
    if((Test-Path -LiteralPath $appIdFile) -and ([IO.File]::ReadAllText($appIdFile).Trim() -ne '480')) {throw '다른 App ID 파일이 존재합니다. 덮어쓰지 않았습니다.'}
    [IO.File]::WriteAllText($appIdFile,"480`n",[Text.Encoding]::ASCII)
    $userDir=Join-Path $projectDir ("Saved/SteamPrototype/{0}/{1}" -f $Room,$Mode)
    New-Item -ItemType Directory -Force -Path $userDir | Out-Null
    $log=Join-Path $userDir ("steam-{0}.log" -f [guid]::NewGuid().ToString('N'))
    $argsList=@('/Game/Prototype/Maps/L_Prototype','-windowed','-ResX=1280','-ResY=720','-nosplash','-nop4',"-PrototypeSteamRoom=$Room",'-PrototypeSaveSlot=Steam480',"-UserDir=`"$userDir`"","-abslog=`"$log`"",'-forcelogflush')
    if($Mode -eq 'Host'){$argsList+='-PrototypeSteamHost'}
    if($Mode -eq 'Join'){$argsList+='-PrototypeSteamJoin'}
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$exe; $info.Arguments=$argsList -join ' '; $info.WorkingDirectory=$binaryDir
    $info.UseShellExecute=$false; $info.CreateNoWindow=$false
    $process=[Diagnostics.Process]::Start($info)
    if($null -eq $process){throw '게임을 실행하지 못했습니다.'}
    [pscustomobject]@{Mode=$Mode;Room=$Room;Pid=$process.Id;Exe=$exe;Log=$log;StartedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $userDir 'last-launch.json') -Encoding UTF8
    Write-Host "Steam 480 / $Mode / 방 코드: $Room / PID: $($process.Id)"
    Write-Host "로그: $log"
    Write-Host '프로세스 시작은 방 생성 성공을 뜻하지 않습니다. 화면의 Steam 호스트 상태를 확인하세요.'
    Write-Host '참가자는 다른 PC의 별도 Steam 계정으로 같은 패키지와 방 코드를 사용하세요. 친구 초대는 Shift+Tab에서 직접 보낼 수 있습니다.'
    exit 0
} catch {
    Write-Error $_.Exception.Message -ErrorAction Continue
    exit 1
}
