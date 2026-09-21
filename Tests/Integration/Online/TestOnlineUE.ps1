#requires -Version 7.0
<#
.SYNOPSIS
仅在正式工程、真实资产和本轮受控凭据齐备后启动Online开发验证，绝不新建宿主绕过阻断。
.DESCRIPTION
0=所选真实进程到达本轮标记且存活观察通过；1=失败；2=未执行/前置缺失。
NullRHI不能证明三维可见性。凭据仅由子进程环境传文件路径，不记录文件内容。
#>
param([string]$EngineRoot,[switch]$Execute,[Parameter(Mandatory)][guid]$RunId,
    [string]$CredentialsFile,[string]$GatewayUrl='http://127.0.0.1:28081',
    [ValidateSet('Editor','Client')][string]$Target='Editor',[string]$StageDirectory,
    [ValidateRange(10,1800)][int]$TimeoutSeconds=240,[switch]$Exercise,[switch]$NullRHI)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $workspace 'Build/Game/FoundationTools.psm1') -Force
$directory=Join-Path $workspace "Saved/Validation/GamePlatformOnline/$RunId/UE-$Target"
$result=@{RunId=$RunId.ToString('D');Status='NotExecuted';ExitCode=2;Target=$Target;Configuration='Development';Platform='Win64';
    GraphicalValidation='NotExecuted';Evidence=$directory;Cases=@();Message='尚未执行'}
$code=2
try {
    if ($RunId -eq [guid]::Empty) { throw 'RunId不能为零GUID。' }
    if (Test-Path -LiteralPath $directory) { throw '拒绝覆盖本次UE证据目录；请使用新RunId。' }
    $null=New-Item -ItemType Directory -Path $directory
    if (-not $Execute) { throw [IO.FileNotFoundException]::new('须显式-Execute，不自动启动UE。') }
    $engine=Get-FoundationEngine $EngineRoot
    $result.EngineVersion=$engine.Version
    $project=Join-Path $workspace 'Game/DivineBeastsArena.uproject'
    Assert-FoundationFile $project
    foreach ($descriptor in Get-ChildItem (Join-Path $workspace 'Game/Plugins') -Recurse -File -Filter '*.uplugin') {
        $body=Get-Content -LiteralPath $descriptor.FullName -Raw
        if ([string]::IsNullOrWhiteSpace($body) -or -not ($body | ConvertFrom-Json).FileVersion) {
            throw "正式工程插件描述阻断；原位保留，不启动替代宿主：$($descriptor.FullName)"
        }
    }
    foreach ($asset in @('Maps/L_FoundationBootstrap.umap','Maps/L_FoundationSandbox.umap',
        'Definitions/DA_FoundationProbe.uasset','Definitions/DA_FoundationOnlineFlow.uasset')) {
        Assert-FoundationFile (Join-Path $workspace "Game/Content/Development/Foundation/$asset")
    }
    if (-not $CredentialsFile) { throw [IO.FileNotFoundException]::new('须提供本RunId受限短期凭据文件路径。') }
    $credentials=[IO.Path]::GetFullPath($CredentialsFile)
    Assert-FoundationFile $credentials
    # 拒绝有常见宽泛读取权限的秘密文件；不把ACL检查当作磁盘内容加密。
    $acl=Get-Acl -LiteralPath $credentials
    foreach ($access in $acl.Access) {
        $sid=$access.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($access.AccessControlType -eq 'Allow' -and $sid -in @('S-1-1-0','S-1-5-11','S-1-5-32-545')) {
            throw '凭据文件仍向Everyone/Authenticated Users/Users开放；拒绝传递。'
        }
    }
    $uri=[uri]$GatewayUrl
    if (-not $uri.IsAbsoluteUri -or $uri.UserInfo -or $uri.Query -or $uri.Fragment -or
        $uri.AbsolutePath -ne '/' -or $uri.Scheme -notin @('http','https') -or
        ($uri.Scheme -eq 'http' -and $uri.Host -notin @('127.0.0.1','localhost','[::1]','::1'))) {
        throw '网关必须无凭据、无路径/查询的HTTPS origin；开发HTTP只允许回环。'
    }
    $arguments=@()
    if ($Target -eq 'Editor') {
        $executable=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
        $arguments+=@($project,'/Game/Development/Foundation/Maps/L_FoundationBootstrap','-game')
    } else {
        if (-not $StageDirectory) { throw [IO.FileNotFoundException]::new('客户端验证须指定本次干净暂存目录。') }
        $candidates=@(Get-ChildItem -LiteralPath $StageDirectory -Recurse -File -Filter 'DivineBeastsArenaClient.exe' |
            Where-Object {$_.Directory.Name -eq 'Win64'})
        if ($candidates.Count -ne 1) { throw [IO.FileNotFoundException]::new('暂存目录内必须有唯一Win64客户端程序。') }
        $executable=$candidates[0].FullName
        $arguments+='/Game/Development/Foundation/Maps/L_FoundationBootstrap'
    }
    Assert-FoundationFile $executable
    $log=Join-Path $directory 'Unreal.log'
    $arguments+=@('-FoundationOnlineIntegration',"-FoundationRunId=$RunId","-FoundationOnlineGateway=$GatewayUrl",
        '-CustomConfig=FoundationOnlineIntegration','-unattended','-nosplash','-nosound','-stdout','-FullStdOutLogOutput',
        '-UTF8Output',"-abslog=$log",'-NoLiveCoding','-NoHotReload','-UDPMESSAGING_TRANSPORT_ENABLE=0','-MULTIHOME=127.0.0.1')
    if ($Exercise) { $arguments+='-FoundationOnlineExercise' }
    if ($NullRHI) { $arguments+='-nullrhi' }
    $marker=if ($Exercise) {'FoundationOnlineVerified'} else {'FoundationOnlineReady'}
    $process=Invoke-FoundationProcess -FilePath $executable -Arguments $arguments -WorkingDirectory (Split-Path $executable) `
        -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds -ReadyLog $log -ReadyMarker "$marker RunId=$RunId" `
        -Environment @{DBA_ONLINE_CREDENTIALS_FILE=$credentials}
    $result.Process=$process
    $code=$process.ExitCode
    $result.Status=if ($code -eq 0) {'Passed'} else {'Failed'}
    $result.Cases=@(@{Name=$marker;Status=$result.Status})
    $result.Message='仅证明本次进程、本次RunId的所选链路。数据库重启、多PIE和三维观察须独立证据。'
} catch [IO.FileNotFoundException] {
    $code=2;$result.Status='NotExecuted';$result.Message=$_.Exception.Message
} catch {
    $code=1;$result.Status='Failed';$result.Message=$_.Exception.Message
}
$result.ExitCode=$code
if ((Test-Path -LiteralPath $directory) -and -not (Test-Path (Join-Path $directory 'result.json'))) {
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $directory 'result.json') -Encoding utf8
}
Write-Host "Online UE：$($result.Status)，退出码=$code；$($result.Message)"
exit $code
