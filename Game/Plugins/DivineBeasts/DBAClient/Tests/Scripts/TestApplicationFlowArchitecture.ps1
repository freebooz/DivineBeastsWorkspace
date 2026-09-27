# 《神兽联盟》应用流程静态架构门禁。
# 该脚本只验证源码结构契约，不冒充UE编译、网络联调或运行验收。
$ErrorActionPreference = 'Stop'

$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$SourceRoot = Join-Path $PluginRoot 'Source\DivineBeastsApplicationFlowClient'
$Descriptor = Join-Path $PluginRoot 'DBAClient.uplugin'

if (-not (Test-Path $SourceRoot)) { throw 'DivineBeastsApplicationFlowClient source directory was not found.' }
if (-not (Test-Path $Descriptor)) { throw 'DBAClient.uplugin was not found.' }

$SourceFiles = Get-ChildItem $SourceRoot -Recurse -File -Include *.h,*.cpp,*.cs
# Windows PowerShell 5 的 Get-Content 默认使用本地 ANSI 编码；工程源码统一按 UTF-8 读取，避免中文注释导致误解析。
$Utf8 = [System.Text.Encoding]::UTF8
$SourceText = ($SourceFiles | ForEach-Object { [System.IO.File]::ReadAllText($_.FullName, $Utf8) }) -join "`n"

# 旧流程控制API会重新制造项目层第二套状态机，因此正式源码必须为零。
$ForbiddenLegacy = @(
    'StartRun(',
    'TransitionTo(',
    'BeginOperation(',
    'IsOperationCurrent(',
    'InvalidateRun(',
    'RegisterNode(',
    'UnregisterNode(',
    'AllowedNextNodes',
    'FGamePlatformFlowOperationToken'
)
foreach ($Token in $ForbiddenLegacy) {
    if ($SourceText.Contains($Token)) { throw "Legacy ApplicationFlow API detected: $Token" }
}

# 项目应用流程必须消费现行平台能力，而不是只靠文档声称已经迁移。
$RequiredCurrent = @(
    'RegisterNodeFactory(',
    'StartFlow(',
    'SubmitEvent(',
    'FGamePlatformFlowNodeToken',
    'UGamePlatformFlowDefinition'
)
foreach ($Token in $RequiredCurrent) {
    if (-not $SourceText.Contains($Token)) { throw "Required ApplicationFlow API missing: $Token" }
}

# 性能门禁：项目层主应用流程不得引入业务Ticker或Tick函数。
if ($SourceText.Contains('FTSTicker')) { throw 'Business FTSTicker is forbidden in DivineBeastsApplicationFlowClient.' }
$TickMatches = Select-String -Path ($SourceFiles.FullName) -Pattern '\bTick\s*\(' -AllMatches
if ($TickMatches) { throw 'Business Tick is forbidden in DivineBeastsApplicationFlowClient.' }

# ClientOnly（仅客户端）边界必须继续由插件描述声明，避免服务器目标带入客户端流程。
# uplugin 中包含中文 Description，必须显式按 UTF-8 读取；否则 Windows PowerShell 5 会把 JSON 中文内容破坏后再解析。
$Json = [System.IO.File]::ReadAllText($Descriptor, $Utf8) | ConvertFrom-Json
$FlowModule = @($Json.Modules | Where-Object { $_.Name -eq 'DivineBeastsApplicationFlowClient' })
if ($FlowModule.Count -ne 1) { throw 'DivineBeastsApplicationFlowClient module declaration must be unique.' }
if ($FlowModule[0].Type -ne 'ClientOnly') { throw 'DivineBeastsApplicationFlowClient must remain ClientOnly.' }

Write-Host 'ApplicationFlow architecture gate passed: legacy-api=0 current-flow=yes business-tick=0 module=ClientOnly.'
