$ErrorActionPreference = 'Stop'

# TestServerArchitecture.ps1（GamePlatformServer服务器控制面架构门禁）
# 目标：阻止三层反向依赖、实现依赖泄漏、无界HTTP响应、缺少超时/重试以及服务器Cook携带无关内容等回退。

$PluginRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Utf8 = [System.Text.UTF8Encoding]::new($false)

$BuildPath = Join-Path $PluginRoot 'Source\GamePlatformServer\GamePlatformServer.Build.cs'
$PluginPath = Join-Path $PluginRoot 'GamePlatformServer.uplugin'
$HeaderPath = Join-Path $PluginRoot 'Source\GamePlatformServer\Public\Server\GamePlatformServerLifecycleSubsystem.h'
$LifecyclePath = Join-Path $PluginRoot 'Source\GamePlatformServer\Private\Server\GamePlatformServerLifecycleSubsystem.cpp'
$HttpPath = Join-Path $PluginRoot 'Source\GamePlatformServer\Private\Server\GamePlatformHttpControlProvider.cpp'

$Build = [System.IO.File]::ReadAllText($BuildPath, $Utf8)
$PublicBlock = ($Build -split 'PrivateDependencyModuleNames')[0]
foreach($PrivateOnly in @('HTTP','Json')){
    if($PublicBlock -match ('"' + $PrivateOnly + '"')){
        throw "实现依赖泄漏到Public依赖: $PrivateOnly"
    }
}

$Descriptor = [System.IO.File]::ReadAllText($PluginPath, $Utf8)
if($Descriptor -notmatch 'CanContainContent\s*"?\s*:\s*false'){
    throw 'GamePlatformServer必须保持纯代码插件，CanContainContent应为false。'
}
if($Descriptor -notmatch 'Type\s*"?\s*:\s*"ServerOnly"'){
    throw 'GamePlatformServer模块必须保持ServerOnly。'
}

$AllSource = (Get-ChildItem (Join-Path $PluginRoot 'Source') -Recurse -File -Include *.h,*.cpp,*.cs |
    ForEach-Object { [System.IO.File]::ReadAllText($_.FullName, $Utf8) }) -join "`n"
foreach($ForbiddenLayer in @('DivineBeasts','MobaCommon','GamePlatformArena','DBAServer','DBAArena')){
    if($AllSource.Contains('#include "' + $ForbiddenLayer)){
        throw "检测到平台层反向/跨层依赖: $ForbiddenLayer"
    }
}

$Header = [System.IO.File]::ReadAllText($HeaderPath, $Utf8)
foreach($Required in @(
    'StartHeartbeatPump(',
    'StopHeartbeatPump(',
    'ConsecutiveHeartbeatFailures',
    'ConsecutiveControlFailures',
    'public IModularFeature'
)){
    if(-not $Header.Contains($Required)){
        throw "生命周期公开契约缺失: $Required"
    }
}

$Lifecycle = [System.IO.File]::ReadAllText($LifecyclePath, $Utf8)
foreach($Required in @(
    'ScheduleControlRetry(',
    'ComputeControlRetryDelaySeconds()',
    'DeferredControlOperation',
    'ControlRetryTickerHandle.IsValid()',
    'TryStartDeferredControlOperation()',
    'HeartbeatPlayerCountInvalid'
)){
    if(-not $Lifecycle.Contains($Required)){
        throw "生命周期可靠性机制缺失: $Required"
    }
}

$Http = [System.IO.File]::ReadAllText($HttpPath, $Utf8)
foreach($Required in @(
    'SetTimeout(',
    'EHttpRequestRedirectPolicy::Reject',
    'SetResponseBodyReceiveStreamDelegateV2',
    'MaxAcceptedResponseBytes',
    'UE_BUILD_SHIPPING',
    'ControlPlaneRetryableHttpStatus',
    'ControlPlaneAuthorizationRejected'
)){
    if(-not $Http.Contains($Required)){
        throw "HTTP控制面加固机制缺失: $Required"
    }
}

Write-Host 'GamePlatformServer architecture gate passed: layers=clean server-only=yes code-only=yes timeout=yes redirect-reject=yes bounded-response=yes heartbeat=yes retry-backoff=yes drain-gate=yes.'
