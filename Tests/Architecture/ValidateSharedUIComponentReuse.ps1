#requires -Version 7.0
<#
.SYNOPSIS
SSLM公共UI组件复用静态门禁。
.DESCRIPTION
只检查真实C++声明、调用方和反向依赖；不代替UE原生测试与Monolith蓝图检查。
#>
param([string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $WorkspaceRoot).Path
$platform = 'Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient'
$project = 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient'
$issues = [Collections.Generic.List[string]]::new()
$count = 0

function Assert-Contains([string]$Path,[string]$Pattern,[string]$Name) {
    $script:count++
    $file = Join-Path $root $Path
    if (-not (Test-Path -LiteralPath $file)) {
        $script:issues.Add("缺少文件：$Name $Path")
        return
    }
    if ((Get-Content -LiteralPath $file -Raw -Encoding utf8) -notmatch $Pattern) {
        $script:issues.Add("未满足：$Name $Path")
    }
}

Assert-Contains "$platform/Public/Components/GamePlatformSlotWidget.h" 'public UGamePlatformComponentWidget' '槽位继承公共组件'
Assert-Contains "$platform/Public/Components/GamePlatformSlotWidget.h" 'struct GAMEPLATFORMUICLIENT_API FGamePlatformUISlotCellPresentation' '公共槽位表现合同'
Assert-Contains "$platform/Private/Components/GamePlatformSlotWidget.cpp" 'UGamePlatformSlotWidget::ApplyNamedSlotCellPresentation\(' '平台命名槽位绘制'
Assert-Contains "$platform/Private/Components/GamePlatformSlotWidget.cpp" 'AreSlotStatesEquivalent\(SlotState, ClampedState\)' '平台自身使用统一比较'
Assert-Contains "$project/Private/Screens/Inventory/DivineBeastsInventoryCellPresenter.h" 'using FDivineBeastsInventoryCellDisplay = FGamePlatformUISlotCellPresentation' '背包使用平台展示值'
Assert-Contains "$project/Private/Screens/Inventory/DivineBeastsInventoryCellPresenter.cpp" 'UGamePlatformSlotWidget::ApplyNamedSlotCellPresentation\(' '项目背包复用平台显示'
Assert-Contains "$project/Private/ViewModels/Combat/DivineBeastsAbilityBarProjectionDiff.cpp" 'UGamePlatformSlotWidget::AreSlotStatesEquivalent\(A, B, false\)' '技能栏复用平台比较'
Assert-Contains "$platform/Public/ViewModels/GamePlatformUISnapshotScope.h" 'static bool CanAccept\(' '平台来源版本合同'
Assert-Contains "$platform/Private/Components/GamePlatformPartyRosterWidget.cpp" 'FGamePlatformUISnapshotScope::CanAccept\(' '组队控件复用来源合同'
Assert-Contains "$platform/Private/Components/GamePlatformCombatAlertWidget.cpp" 'FGamePlatformUISnapshotScope::CanAccept\(' '战斗警告复用来源合同'
Assert-Contains "$project/Private/ViewModels/Domains/DivineBeastsScopedDomainViewModelBase.cpp" 'FGamePlatformUISnapshotScope::CanAccept\(' '项目视图模型复用来源合同'
Assert-Contains "$platform/Private/Tests/GamePlatformUISlotCommonPresentationTests.cpp" 'GamePlatform.UI.ViewModel.ScopedSnapshot' '新增自动化测试源'

# 继续使用已存在的共享作用域策略，禁止为每个Widget再维护平行校验分支。
Assert-Contains "$platform/Public/ViewModels/GamePlatformUISnapshotScope.h" 'static bool CanAcceptNamedRevision\(' '公共FName身份修订合同'
foreach ($name in @('GamePlatformCastProgressWidget','GamePlatformTargetFrameWidget','GamePlatformStatusEffectTrayWidget')) {
    Assert-Contains "$platform/Private/Components/$name.cpp" 'FGamePlatformUISnapshotScope::CanAccept\(' ('公共FGuid状态来源 '+$name)
}
foreach ($name in @(
    'GamePlatformMinimapWidget','GamePlatformQuestTrackerWidget','GamePlatformCountdownWidget',
    'GamePlatformSlotBarWidget','GamePlatformInteractionPromptWidget','GamePlatformSettingRowWidget',
    'GamePlatformTooltipWidget')) {
    Assert-Contains "$platform/Private/Components/$name.cpp" 'FGamePlatformUISnapshotScope::CanAcceptNamedRevision\(' ('公共FName显示版本 '+$name)
}
Assert-Contains "$project/Private/Layers/DivineBeastsWorldHUDRoute.h" 'struct FDivineBeastsWorldHUDRoute' '世界HUD路由策略与根布局分离'
Assert-Contains "$project/Private/Layers/DivineBeastsRootLayout.cpp" 'FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex\(' '根布局消费世界HUD策略'
Assert-Contains "$project/Private/Tests/DivineBeastsWorldHUDRouteTests.cpp" 'DivineBeasts.UI.Architecture.WorldHUDRouting' '世界HUD路由的原生测试源码'
Assert-Contains "$platform/Private/Tests/GamePlatformUISlotCommonPresentationTests.cpp" 'CanAcceptNamedRevision\(' '公共命名修订号测试源码'

# 背包本地化表现从页面移出，保持稳定机器码与原LOCTEXT翻译目录；
# 禁止项目在各个Widget中解析后端自然语言或另造错误码真源。
Assert-Contains "$project/Private/Screens/Inventory/DivineBeastsInventoryScreen.cpp" 'FDivineBeastsInventoryTextCatalog::ForState\(' '背包页面消费项目状态文字目录'
Assert-Contains "$project/Private/Screens/Inventory/DivineBeastsInventoryScreen.cpp" 'FDivineBeastsInventoryTextCatalog::ForError\(' '背包页面消费项目错误文字目录'
Assert-Contains "$project/Private/Tests/DivineBeastsInventoryTextCatalogTests.cpp" 'DivineBeasts.UI.Inventory.LocalizedStatusCatalog' '背包状态和错误文本测试源码'

$count++
$contents = (Get-Content (Join-Path $root "$platform/Public/Components/GamePlatformSlotWidget.h") -Raw) +
    (Get-Content (Join-Path $root "$platform/Public/ViewModels/GamePlatformUISnapshotScope.h") -Raw)
if ($contents -match '(?i)DivineBeasts|DBAUIPack|MobaCommon') {
    $issues.Add('GamePlatform公开类型不能硬编码项目或竞技类型')
}

$count++
$presenter = Get-Content (Join-Path $root "$project/Private/Screens/Inventory/DivineBeastsInventoryCellPresenter.cpp") -Raw
if ($presenter -match 'NativeTick\s*\(|\bSetTimer\s*\(|\bRequestMove\s*\(') {
    $issues.Add('项目背包展示适配器不得轮询或拥有业务写入命令')
}

if ($issues.Count -ne 0) {
    $issues | ForEach-Object { Write-Warning $_ }
    Write-Output "公共组件复用静态门禁失败：$($issues.Count)项；检查总数=$count"
    exit 1
}
Write-Output "公共组件复用静态门禁通过：$count 项检查。"
Write-Output '注意：静态结果不能替代UE编译、原生自动化、Monolith蓝图重载及联机。'
