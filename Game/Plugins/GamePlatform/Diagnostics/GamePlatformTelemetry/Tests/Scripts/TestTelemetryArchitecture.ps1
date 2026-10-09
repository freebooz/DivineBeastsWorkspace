# GamePlatformTelemetry（游戏平台遥测）静态架构/性能门禁。
# 该脚本只验证源码结构契约，不冒充UE运行、真实断网、后端Ingest或NATS集成验收。
$ErrorActionPreference = 'Stop'

$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Utf8 = [System.Text.Encoding]::UTF8
$DescriptorPath = Join-Path $PluginRoot 'GamePlatformTelemetry.uplugin'
$BuildPath = Join-Path $PluginRoot 'Source\GamePlatformTelemetry\GamePlatformTelemetry.Build.cs'
$SubsystemPath = Join-Path $PluginRoot 'Source\GamePlatformTelemetry\Private\Subsystems\GamePlatformTelemetrySubsystem.cpp'
$SinkPath = Join-Path $PluginRoot 'Source\GamePlatformTelemetry\Private\Sinks\GamePlatformTelemetryNetworkSink.cpp'
$TransportPath = Join-Path $PluginRoot 'Source\GamePlatformTelemetry\Private\Transport\GamePlatformTelemetryTransport.cpp'

$Descriptor = [System.IO.File]::ReadAllText($DescriptorPath, $Utf8) | ConvertFrom-Json
if($Descriptor.CanContainContent -ne $false){ throw 'Telemetry mechanism plugin must not contain content assets.' }

$Build = [System.IO.File]::ReadAllText($BuildPath, $Utf8)
$PublicBlock = ($Build -split 'PrivateDependencyModuleNames')[0]
foreach($PrivateOnly in @('"HTTP"','"Json"','"TraceLog"')){
    if($PublicBlock.Contains($PrivateOnly)){ throw "Implementation dependency leaked to Public: $PrivateOnly" }
}

$Subsystem = [System.IO.File]::ReadAllText($SubsystemPath, $Utf8)
if($Subsystem.Contains('return true;') -and $Subsystem.Contains('TickFlush(float)')){
    $tickRegion = $Subsystem.Substring($Subsystem.IndexOf('TickFlush(float)'))
    if($tickRegion.Substring(0,[Math]::Min(500,$tickRegion.Length)).Contains('return true;')){
        throw 'Telemetry flush ticker must be one-shot, not a permanent repeating ticker.'
    }
}
if(-not $Subsystem.Contains('RequestFlushAfterRecord()')){ throw 'Event/Metric threshold flush hook is missing.' }
if(-not $Subsystem.Contains('Definition->SustainedRatePerSecond')){ throw 'Metric/event schema rate limiting is missing.' }
if(-not $Subsystem.Contains('SchemaRegistry->Freeze()')){ throw 'Runtime schema freeze is missing.' }

$BufferPath = Join-Path $PluginRoot 'Source\GamePlatformTelemetry\Private\Buffer\GamePlatformTelemetryBoundedBuffer.cpp'
$Buffer = [System.IO.File]::ReadAllText($BufferPath, $Utf8)
if(-not $Buffer.Contains('HeadIndex')){ throw 'Amortized head-index buffer consumption is missing.' }
if(-not $Buffer.Contains('TryCoalesceMetric(')){ throw 'Counter/Gauge metric coalescing is missing.' }
if(-not $Buffer.Contains('ResolveSharedContext(')){ throw 'Shared telemetry context buffering is missing.' }
if(-not $Buffer.Contains('DiscardQueuedRecords(')){ throw 'Privacy-boundary buffer discard is missing.' }
if($Buffer.Contains('Records.RemoveAt(' + [Environment]::NewLine + '        0,' + [Environment]::NewLine + '        ConsumeCount')){
    throw 'Per-batch front-array shifting returned.'
}

$Sink = [System.IO.File]::ReadAllText($SinkPath, $Utf8)
foreach($Required in @('ScheduleRetry(','CancelRetryTickers(','FinalizeShutdownAfterBudget(','RetryAfterSeconds')){
    if(-not $Sink.Contains($Required)){ throw "Reconnect/retry contract missing: $Required" }
}
# 按实际提交调用的第二实参捕获列表判断，独立ScheduleRetry不能污染该调用的结果。
Import-Module (Join-Path $PSScriptRoot 'TelemetrySubmitCaptureAudit.psm1') -Force
$UnsafeSubmitCaptures = @(Get-TelemetryUnsafeSubmitCaptures -Source $Sink)
if($UnsafeSubmitCaptures.Count -gt 0){
    throw ('Unsafe BeginSubmitBatch argument/capture evaluation order detected: ' + ($UnsafeSubmitCaptures -join '; '))
}

$Transport = [System.IO.File]::ReadAllText($TransportPath, $Utf8)
if($Transport.Contains('ContextJson(Event.Context)') -or $Transport.Contains('ContextJson(Metric.Context)')){
    throw 'Per-record context serialization returned; batch context must be deduplicated.'
}
if(-not $Transport.Contains('DynamicHeaderProvider')){ throw 'Dynamic credential provider is missing.' }
if(-not $Transport.Contains('payload_too_large')){ throw 'Final serialized payload hard limit is missing.' }

$AllSource = (Get-ChildItem (Join-Path $PluginRoot 'Source') -Recurse -File -Include *.h,*.cpp,*.cs |
    ForEach-Object { [System.IO.File]::ReadAllText($_.FullName, $Utf8) }) -join "`n"
foreach($ForbiddenLayer in @('DivineBeasts','MobaCommon','GamePlatformOnlineClient','GamePlatformSession')){
    if($AllSource.Contains('#include "' + $ForbiddenLayer)){
        throw "Reverse/cross-domain dependency detected in Telemetry: $ForbiddenLayer"
    }
}

Write-Host 'Telemetry architecture gate passed: one-shot-flush=yes retry=yes backpressure=yes context-dedup=yes metric-coalesce=yes privacy-discard=yes schema-freeze=yes head-index=yes dynamic-auth=yes public-deps=clean.'
