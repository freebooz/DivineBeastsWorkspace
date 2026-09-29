<#
.SYNOPSIS
GamePlatformInteraction（游戏平台交互插件）静态工程门禁。
.DESCRIPTION
只检查可以由源码/描述文件确定的架构与安全边界；不替代 UE5.8 构建、Automation（自动化测试）、
Multi-PIE（多实例编辑器运行）或 Dedicated Server（专用服务器）联机验证。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}

$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/World/GamePlatformInteraction'
$descriptorPath = Join-Path $pluginRoot 'GamePlatformInteraction.uplugin'
$buildPath = Join-Path $pluginRoot 'Source/GamePlatformInteraction/GamePlatformInteraction.Build.cs'
$interactorHeader = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Public/Components/GamePlatformInteractorComponent.h'
$interactorCpp = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Private/Components/GamePlatformInteractorComponent.cpp'
$interactableCpp = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Private/Components/GamePlatformInteractableComponent.cpp'
$requestHeader = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Public/Types/GamePlatformInteractionRequest.h'
$settingsHeader = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Public/Settings/GamePlatformInteractionSettings.h'
$interactableInterface = Join-Path $pluginRoot 'Source/GamePlatformInteraction/Public/Interfaces/GamePlatformInteractable.h'

$findings = New-Object 'System.Collections.Generic.List[string]'

function Assert-True {
    param(
        [Parameter(Mandatory)][bool]$Condition,
        [Parameter(Mandatory)][string]$Message
    )
    if (-not $Condition) {
        $script:findings.Add($Message)
    }
}

foreach ($path in @(
    $descriptorPath, $buildPath, $interactorHeader, $interactorCpp,
    $interactableCpp, $requestHeader, $settingsHeader, $interactableInterface)) {
    Assert-True -Condition (Test-Path -LiteralPath $path -PathType Leaf) -Message ("缺少文件：{0}" -f $path)
}
if ($findings.Count -gt 0) {
    $findings | ForEach-Object { Write-Output ("FAIL: {0}" -f $_) }
    exit 1
}

$descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
$build = Get-Content -LiteralPath $buildPath -Raw -Encoding UTF8
$interactorH = Get-Content -LiteralPath $interactorHeader -Raw -Encoding UTF8
$interactorC = Get-Content -LiteralPath $interactorCpp -Raw -Encoding UTF8
$interactableC = Get-Content -LiteralPath $interactableCpp -Raw -Encoding UTF8
$request = Get-Content -LiteralPath $requestHeader -Raw -Encoding UTF8
$settings = Get-Content -LiteralPath $settingsHeader -Raw -Encoding UTF8
$interactableI = Get-Content -LiteralPath $interactableInterface -Raw -Encoding UTF8

Assert-True -Condition ($descriptor.Category -eq 'GamePlatform/World') -Message 'uplugin Category 必须为 GamePlatform/World。'
Assert-True -Condition (@($descriptor.Modules).Count -eq 1) -Message '插件必须只有一个真实 Runtime（共享运行时）模块。'
if (@($descriptor.Modules).Count -eq 1) {
    Assert-True -Condition ($descriptor.Modules[0].Name -eq 'GamePlatformInteraction') -Message '模块名必须为 GamePlatformInteraction。'
    Assert-True -Condition ($descriptor.Modules[0].Type -eq 'Runtime') -Message 'GamePlatformInteraction 模块必须为 Runtime。'
}

$publicMatch = [regex]::Match(
    $build,
    'PublicDependencyModuleNames\.AddRange\(new string\[\]\s*\{(?<body>.*?)\}\);',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$privateMatch = [regex]::Match(
    $build,
    'PrivateDependencyModuleNames\.AddRange\(new string\[\]\s*\{(?<body>.*?)\}\);',
    [Text.RegularExpressions.RegexOptions]::Singleline)

Assert-True -Condition $publicMatch.Success -Message 'Build.cs 缺少可识别的 PublicDependencyModuleNames。'
Assert-True -Condition $privateMatch.Success -Message 'Build.cs 缺少可识别的 PrivateDependencyModuleNames。'
if ($publicMatch.Success) {
    Assert-True -Condition (-not $publicMatch.Groups['body'].Value.Contains('"GamePlatformGameplay"')) -Message 'GamePlatformGameplay 只能是 Private（私有实现）依赖。'
    Assert-True -Condition (-not $publicMatch.Groups['body'].Value.Contains('"NetCore"')) -Message 'NetCore 只能是 Private（私有实现）依赖。'
    Assert-True -Condition (-not $publicMatch.Groups['body'].Value.Contains('"GamePlatformCore"')) -Message '不得保留未使用的 GamePlatformCore 公开依赖。'
}
if ($privateMatch.Success) {
    Assert-True -Condition ($privateMatch.Groups['body'].Value.Contains('"GamePlatformGameplay"')) -Message '缺少 GamePlatformGameplay Private（私有）依赖。'
    Assert-True -Condition ($privateMatch.Groups['body'].Value.Contains('"NetCore"')) -Message '缺少 NetCore Private（私有）依赖。'
}

$forbidden = @(
    'MobaCommon', 'DivineBeastsRuntime', 'DivineBeastsArenaRuntime',
    'GamePlatformInputClient', 'GamePlatformUI', 'GamePlatformInventory',
    'GamePlatformQuest', 'GamePlatformCombat', 'GamePlatformCharacter',
    'GamePlatformVFX'
)
$sourceForBoundary = $build + [Environment]::NewLine + $interactorH + [Environment]::NewLine + $interactorC + [Environment]::NewLine + $interactableC
foreach ($token in $forbidden) {
    Assert-True -Condition (-not $sourceForBoundary.Contains($token)) -Message ("发现禁止的上层/横向直接依赖：{0}" -f $token)
}

Assert-True -Condition (-not $interactorH.Contains('TickComponent(')) -Message 'Interactor 不应声明 TickComponent（每帧轮询）。'
Assert-True -Condition (-not $interactorC.Contains('PrimaryComponentTick.bCanEverTick = true')) -Message 'Interactor 不得启用每帧 Tick。'
Assert-True -Condition (-not $interactableC.Contains('PrimaryComponentTick.bCanEverTick = true')) -Message 'Interactable 不得启用每帧 Tick。'

$serverRpcCount = ([regex]::Matches($interactorH, 'UFUNCTION\(Server\s*,\s*Reliable\)')).Count
Assert-True -Condition ($serverRpcCount -eq 2) -Message ("服务器 Reliable RPC 应恰好为 Begin/Cancel 两个；当前={0}" -f $serverRpcCount)

foreach ($requiredField in @('RequestId','TargetActor','TargetInstanceId','TargetGeneration','ObservedTargetRevision','OptionId')) {
    Assert-True -Condition ($request.Contains($requiredField)) -Message ("Request 缺少身份字段：{0}" -f $requiredField)
}
foreach ($forbiddenField in @('MaxDistance','HoldDuration','MaxConcurrent','LineOfSight')) {
    Assert-True -Condition (-not $request.Contains($forbiddenField)) -Message ("Request 不得携带服务器权威规则字段：{0}" -f $forbiddenField)
}

Assert-True -Condition ($interactorC.Contains('FindInteractableComponentByInstanceId')) -Message '服务器必须按 TargetInstanceId 精确解析可交互组件。'
Assert-True -Condition ($interactorC.Contains('RecentRequestResults.Find(Request.RequestId)')) -Message 'Begin 请求必须具备终态幂等缓存查询。'
Assert-True -Condition ($interactorC.Contains('RecentRequestResults.Find(RequestId)')) -Message 'Cancel 请求必须具备终态幂等缓存查询。'
Assert-True -Condition ($interactableC.Contains('PruneInvalidActiveSessions')) -Message '目标必须清理失效弱会话占用。'
Assert-True -Condition ($interactableC.Contains('CommittedResults.Find(Session.SessionId)')) -Message 'Commit 必须能回放首次成功结果。'
Assert-True -Condition ($settings.Contains('MaxHoldDuration')) -Message '缺少 MaxHoldDuration（最大长按时长）配置。'
Assert-True -Condition ($settings.Contains('CancelRequestWindowSeconds')) -Message '缺少独立 Cancel（取消请求）限流窗口配置。'
Assert-True -Condition (-not $interactableC.Contains('++TargetGeneration')) -Message 'TargetGeneration（目标代次）不得直接自增，必须使用安全回绕计数器。'
Assert-True -Condition ($interactableC.Contains('TargetGeneration = AdvancePositiveCounter(TargetGeneration);')) -Message 'EndPlay/代次推进必须使用 AdvancePositiveCounter（安全正整数计数器）。'
Assert-True -Condition ([regex]::IsMatch($interactableI, 'CommitInteraction\s*\([\s\S]*?\)\s*\{\s*return false;\s*\}')) -Message 'Custom CommitInteraction（自定义提交）默认实现必须 Fail-Closed（失败关闭），不得静默成功。'

if ($findings.Count -gt 0) {
    Write-Output ("GamePlatformInteraction 静态门禁失败：{0} 项" -f $findings.Count)
    $findings | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformInteraction 静态门禁通过。'
Write-Output '已确认：平台层/World归属、单Runtime模块、公开依赖最小化、无上层直接依赖、无Tick、2个低频Server RPC、组件实例身份、请求/提交幂等、弱会话清理、Hold上限、独立Cancel限流窗口、代次安全推进与Custom提交失败关闭。'
Write-Output '此结果不替代 UE5.8 构建、Automation、Cook、Multi-PIE 或 Dedicated Server 联机测试。'
exit 0
