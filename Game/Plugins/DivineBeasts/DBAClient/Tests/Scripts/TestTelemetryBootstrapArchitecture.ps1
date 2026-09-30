# 《神兽联盟》客户端遥测装配静态门禁。
# 只验证项目组合层的认证代次/断线重试隔离合同，不冒充真实登录、网卡断线或Gateway联调。
$ErrorActionPreference = 'Stop'
$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Utf8 = [System.Text.Encoding]::UTF8
$BootstrapPath = Join-Path $PluginRoot 'Source\DivineBeastsApplicationFlowClient\Private\Telemetry\DivineBeastsClientTelemetryBootstrapSubsystem.cpp'
$HeaderPath = Join-Path $PluginRoot 'Source\DivineBeastsApplicationFlowClient\Private\Telemetry\DivineBeastsClientTelemetryBootstrapSubsystem.h'
$Bootstrap = [System.IO.File]::ReadAllText($BootstrapPath, $Utf8)
$Header = [System.IO.File]::ReadAllText($HeaderPath, $Utf8)

foreach($Required in @(
    'ConfigureNetworkSinkForAuthenticatedSession()',
    'SwitchToNullSink()',
    'DiscardBufferedRecordsForPrivacyBoundary()',
    'ApplyAuthorization(Request)',
    'TelemetryAuthGeneration'
)){
    if(-not ($Bootstrap.Contains($Required) -or $Header.Contains($Required))){
        throw "Telemetry auth-generation isolation contract missing: $Required"
    }
}

# NetworkSink必须在Authenticated分支内按认证代次创建，不能在GameInstance初始化阶段永久复用。
$InitializeStart = $Bootstrap.IndexOf('void UDivineBeastsClientTelemetryBootstrapSubsystem::Initialize(')
$AuthStart = $Bootstrap.IndexOf('void UDivineBeastsClientTelemetryBootstrapSubsystem::HandleAuthStateChanged(')
if($InitializeStart -ge 0 -and $AuthStart -gt $InitializeStart){
    $InitializeRegion = $Bootstrap.Substring($InitializeStart, $AuthStart - $InitializeStart)
    if($InitializeRegion.Contains('MakeShared<FGamePlatformTelemetryNetworkSink')){
        throw 'Telemetry NetworkSink must not be permanently created in Initialize; bind it to AuthGeneration.'
    }
}

# 项目组合层不得保存AccessToken字段；每次请求由Online动态授权。
if($Header -match 'FString\s+\w*(AccessToken|RefreshToken)\w*\s*;' -or
   $Bootstrap -match 'FString\s+\w*(AccessToken|RefreshToken)\w*\s*='){
    throw 'Telemetry bootstrap must not store access/refresh token fields or local token strings.'
}

Write-Host 'DBAClient telemetry bootstrap gate passed: auth-generation=yes old-retry-isolation=yes privacy-discard=yes dynamic-authorization=yes.'
