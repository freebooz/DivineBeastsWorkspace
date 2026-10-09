#requires -Version 5.1
<#
.SYNOPSIS
审计《神兽联盟》公共/竞技UI登记、真实蓝图资产与客户端/服务器装配边界。
.DESCRIPTION
只读取版本库，不创建、编译或修补.uasset。缺失的规划页面作为Pending（待交付）
独立记录，不误报为存在；已验证的首批6个蓝图与2个纹理丢失才判为静态门禁失败。
支持-ReportPath导出中文JSON（仅写调用者指定的验证产物路径）。
本脚本不是编辑器资产校验、Cook、网络联机或视觉验收的替代品。
#>
[CmdletBinding()]
param([string]$ReportPath)

$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game = 'Game/Plugins/'
$publicSource = 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/'
$arenaSource = 'Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/'
$coreAssets = 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI/'
$arenaAssets = 'Game/Plugins/DivineBeasts/DBAArena/Content/UI/'
$publicCatalog = Get-Content -LiteralPath (Join-Path $workspace ($publicSource+'Private/Screens/DivineBeastsUIScreenCatalog.cpp')) -Raw -Encoding utf8
$arenaCatalog = Get-Content -LiteralPath (Join-Path $workspace ($arenaSource+'Private/UI/DivineBeastsArenaUIScreenCatalog.cpp')) -Raw -Encoding utf8

# 每一项同时对应工程实际Catalogue中的稳定SurfaceId、所属功能域和蓝图软路径。
# 当前仅将6个真实基础Widget列为既有交付，其余缺失是未来工作，不伪造为成功。
$entries = @(
    @('UI.Screen.Boot','Core','DBAClient','P0','Screens/WBP_DBA_UI_Boot.uasset','Screens/Boot/DivineBeastsBootScreen.h'),
    @('UI.Screen.Login','Account','DBAClient','P0','Screens/WBP_DBA_UI_Login.uasset','Screens/Login/DivineBeastsLoginScreen.h'),
    @('UI.Screen.CharacterCreate','Account','DBAClient','P0','Screens/WBP_DBA_UI_CharacterCreate.uasset','Screens/Characters/DivineBeastsCharacterCreateScreen.h'),
    @('UI.Screen.CharacterSelect','Account','DBAClient','P0','Screens/WBP_DBA_UI_CharacterSelect.uasset','Screens/Characters/DivineBeastsCharacterSelectScreen.h'),
    @('UI.Screen.LoadingTravel','Core','DBAClient','P0','Screens/WBP_DBA_UI_LoadingTravel.uasset','Screens/Loading/DivineBeastsLoadingTravelScreen.h'),
    @('UI.Screen.ErrorReconnect','Core','DBAClient','P0','Screens/WBP_DBA_UI_ErrorReconnect.uasset','Screens/Connection/DivineBeastsErrorReconnectScreen.h'),
    @('UI.Screen.SystemMenu','System','DBAClient','P1','Screens/WBP_DBA_UI_SystemMenu.uasset',''),
    @('UI.Screen.Inventory','Inventory','DBAClient','P1','Screens/WBP_DBA_UI_Inventory.uasset','Screens/Inventory/DivineBeastsInventoryScreen.h'),
    @('UI.Screen.Quest','World','DBAClient','P1','Screens/WBP_DBA_UI_Quest.uasset','Screens/World/DivineBeastsQuestScreen.h'),
    @('UI.Screen.Social','Social','DBAClient','P1','Screens/WBP_DBA_UI_Social.uasset','Screens/Social/DivineBeastsSocialScreenBase.h'),
    @('UI.Screen.LiveOps','LiveOps','DBAClient','P1','Screens/WBP_DBA_UI_LiveOps.uasset','Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h'),
    @('UI.HUD.OpenWorld','World','DBAClient','P1','HUD/WBP_DBA_UI_OpenWorldHUD.uasset','HUD/DivineBeastsOpenWorldHUD.h'),
    @('UI.HUD.VillageMain','World','DBAClient','P1','HUD/WBP_DBA_UI_VillageMainHUD.uasset','HUD/DivineBeastsVillageHUD.h'),
    @('UI.HUD.TutorialGuidance','World','DBAClient','P1','HUD/WBP_DBA_UI_TutorialGuidance.uasset','HUD/DivineBeastsTutorialHUD.h'),
    @('UI.HUD.TrainingControls','World','DBAClient','P1','HUD/WBP_DBA_UI_TrainingControls.uasset','HUD/DivineBeastsTrainingHUD.h'),
    @('UI.Notification.Toast','Core','DBAClient','P1','Notifications/WBP_DBA_UI_Toast.uasset',''),
    @('UI.Screen.Matchmaking','Arena','DBAArena','P0','Screens/WBP_DBA_UI_Arena_Matchmaking.uasset','UI/Screens/DivineBeastsMatchmakingScreen.h'),
    @('UI.Screen.MatchFoundReady','Arena','DBAArena','P0','Screens/WBP_DBA_UI_Arena_MatchFoundReady.uasset','UI/Screens/DivineBeastsMatchFoundReadyScreen.h'),
    @('UI.Screen.ArenaHeroSelection','Arena','DBAArena','P0','Screens/WBP_DBA_UI_Arena_HeroSelection.uasset','UI/Screens/DivineBeastsArenaHeroSelectionScreen.h'),
    @('UI.Screen.Scoreboard','Arena','DBAArena','P1','Screens/WBP_DBA_UI_Arena_Scoreboard.uasset','UI/Screens/DivineBeastsScoreboardScreen.h'),
    @('UI.Screen.PostMatchResult','Arena','DBAArena','P0','Screens/WBP_DBA_UI_Arena_PostMatch.uasset','UI/Screens/DivineBeastsPostMatchResultScreen.h'),
    @('UI.HUD.Arena','Arena','DBAArena','P0','HUD/WBP_DBA_UI_ArenaHUD.uasset','UI/HUD/DivineBeastsArenaHUD.h')
)

$issues = [Collections.Generic.List[string]]::new()
$records = [Collections.Generic.List[object]]::new()
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    $id, $domain, $owner, $priority, $assetRel, $codeRel = $entry
    if (!$seen.Add($id)) { $issues.Add("重复的SurfaceId：$id") }
    $root = if ($owner -eq 'DBAArena') { $arenaAssets } else { $coreAssets }
    $source = if ($owner -eq 'DBAArena') { $arenaSource } else { $publicSource }
    $catalog = if ($owner -eq 'DBAArena') { $arenaCatalog } else { $publicCatalog }
    if (!$catalog.Contains('TEXT("'+$id+'")')) { $issues.Add("正式目录中未登记：$id") }
    $assetPath = $root + $assetRel
    $assetExists = Test-Path -LiteralPath (Join-Path $workspace $assetPath)
    $codeExists = $codeRel -and (Test-Path -LiteralPath (Join-Path $workspace ($source+'Public/'+$codeRel)))
    $records.Add([pscustomobject]@{
        SurfaceId=$id; Domain=$domain; Owner=$owner; Priority=$priority
        NativeClassExists=[bool]$codeExists; WidgetAssetExists=[bool]$assetExists
        DeliveryState=if($assetExists){'资源已存在，运行待验证'}elseif($codeExists){'已有C++父类，蓝图待制作'}else{'仅目录登记，蓝图待制作'}
        AssetPath=$assetPath
    })
}

# 必需且已在历史Monolith清单和IoStore门禁列明的资产，在本次代码变更中不得退化为缺失。
$requiredAssets = @(
    'Root/WBP_DBA_UI_RootLayout.uasset',
    'Screens/WBP_DBA_UI_Login.uasset',
    'Screens/WBP_DBA_UI_CharacterCreate.uasset',
    'Screens/WBP_DBA_UI_CharacterSelect.uasset',
    'Components/WBP_DBA_HeroChoice.uasset',
    'Components/WBP_DBA_CharacterChoice.uasset',
    'Textures/DBA_MythicLogin.uasset',
    'Textures/DBA_MythicLogo.uasset'
)
foreach ($asset in $requiredAssets) {
    if (!(Test-Path -LiteralPath (Join-Path $workspace ($coreAssets+$asset)))) {
        $issues.Add("历史已交付资产缺失：$asset")
    }
}


# 十大领域的基类是真正的源码文件，不能以空目录或规划文档冒充落地。
$domainClasses = @(
    @('Core','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/DivineBeastsUIScreen.h'),
    @('Account','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Account/DivineBeastsAccountScreenBase.h'),
    @('World','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/World/DivineBeastsWorldScreenBase.h'),
    @('Character','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Characters/DivineBeastsCharacterScreenBase.h'),
    @('Inventory','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Inventory/DivineBeastsInventoryScreen.h'),
    @('Combat','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Panels/Combat/DivineBeastsCombatPanelBase.h'),
    @('Social','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/Social/DivineBeastsSocialScreenBase.h'),
    @('Arena','Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Public/UI/Screens/DivineBeastsArenaScreenBase.h'),
    @('System','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/System/DivineBeastsSystemScreenBase.h'),
    @('LiveOps','Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h')
)
foreach ($domain in $domainClasses) {
    $domainName,$domainPath = $domain
    $fullPath = Join-Path $workspace $domainPath
    if (!(Test-Path -LiteralPath $fullPath)) {
        $issues.Add("业务域基础类缺失：$domainName -> $domainPath")
        continue
    }
    $content = Get-Content -LiteralPath $fullPath -Raw -Encoding utf8
    if (!$content.Contains('UCLASS(') -or !$content.Contains('GENERATED_BODY()')) {
        $issues.Add("业务域基础类未声明为真实UE反射类：$domainName")
    }
}
# 源端隔离与快照生命周期源文件必须真实存在，而不是仅在文档声称支持。
foreach ($supportFile in @(
 'Public/Contracts/DivineBeastsUIDomainTypes.h',
 'Public/ViewModels/Domains/DivineBeastsScopedDomainViewModelBase.h',
 'Public/ViewModels/Social/DivineBeastsSocialViewModel.h',
 'Public/ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h',
 'Private/Tests/DivineBeastsUIDomainTests.cpp'
)) {
    if (!(Test-Path -LiteralPath (Join-Path $workspace ($publicSource + $supportFile)))) {
        $issues.Add("领域基础契约或测试缺失：$supportFile")
    }
}
# 真实构建目标而非.uproject拥有UI内容包启用职责；Dedicated Server绝不能引入。
$clientTarget = Get-Content -LiteralPath (Join-Path $workspace 'Game/Source/DivineBeastsArenaClient.Target.cs') -Raw
$editorTarget = Get-Content -LiteralPath (Join-Path $workspace 'Game/Source/DivineBeastsArenaEditor.Target.cs') -Raw
$serverTarget = Get-Content -LiteralPath (Join-Path $workspace 'Game/Source/DivineBeastsArenaServer.Target.cs') -Raw
if (!$clientTarget.Contains('EnablePlugins.Add("DBAUIPack_Core")')) { $issues.Add('Client Target未显式启用核心UI内容包') }
if (!$editorTarget.Contains('EnablePlugins.Add("DBAUIPack_Core")')) { $issues.Add('Editor Target未显式启用核心UI内容包') }
if ($serverTarget.Contains('EnablePlugins.Add("DBAUIPack_Core")')) { $issues.Add('Server Target错误启用了客户端UI内容包') }
if ($publicCatalog.Contains('WBP_DBA_UI_Arena_')) { $issues.Add('公共DBAClient注册了竞技专属资源路径') }
if ($arenaCatalog.Contains('MobileWidgetClassPath = FString::Printf') -or
    $arenaCatalog.Contains('WBP_DBA_UI_ArenaHUD_Mobile.')) {
    $issues.Add('竞技目录仍登记未交付的移动端Widget路径')
}

$report = [ordered]@{
    Name='DivineBeasts UI Asset Inventory（神兽联盟界面资产台账）'
    Workspace=$workspace
    TotalRegistered=$records.Count
    PublicRegistered=@($records | Where-Object Owner -eq 'DBAClient').Count
    ArenaRegistered=@($records | Where-Object Owner -eq 'DBAArena').Count
    RegisteredWidgetAssetsOnDisk=@($records | Where-Object WidgetAssetExists).Count
    MissingWidgetAssets=@($records | Where-Object { !$_.WidgetAssetExists }).Count
    PreviouslyDeliveredBaselineAssets=$requiredAssets.Count
    BusinessDomainBaseCount=$domainClasses.Count
    Issues=@($issues)
    Items=@($records)
    Scope='静态源码与磁盘资产检查；不证明UE编译、Cook或实际客户端运行'
}
$json = $report | ConvertTo-Json -Depth 7
if ($ReportPath) {
    $dest = [IO.Path]::GetFullPath($ReportPath)
    $json | Set-Content -LiteralPath $dest -Encoding utf8
}
Write-Output $json
if ($issues.Count -gt 0) { exit 1 }
exit 0
