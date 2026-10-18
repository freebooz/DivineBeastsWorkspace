#requires -Version 7
<#
SSLM按钮音效与Village背景音乐运行接线静态门禁。
确认平台语义目录分组隔离、UMG按钮绑定/解绑、音乐异步Active通知及跨世界取消。
静态门禁不等同于真实客户端听音/多人压测/服务器Cook验证。
#>
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$client=Join-Path $root 'Game/Plugins/DivineBeasts/DBAClient/Source'
$audio=Get-Content -Raw (Join-Path $client 'DivineBeastsPresentationClient/Private/Audio/DivineBeastsStarterAudio.cpp')
$coord=Get-Content -Raw (Join-Path $client 'DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp')
$catalog=Get-Content -Raw (Join-Path $client 'DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp')
$screen=Get-Content -Raw (Join-Path $client 'DivineBeastsUIClient/Private/Screens/DivineBeastsUIScreen.cpp')
$login=Get-Content -Raw (Join-Path $client 'DivineBeastsUIClient/Private/Screens/Login/DivineBeastsLoginScreen.cpp')
foreach($item in @(
  'BuildStarterAudioFragment(Group)',
  'EnsureGroup(TEXT("UI"), 3, StarterAudioPack)',
  'EnsureGroup(TEXT("Spell"), 3, StarterSpellPack)',
  'EnsureGroup(TEXT("Music.Village"), 1, StarterMusicPack)',
  'EnsureGroup(TEXT("Ambience.Forest"), 1, StarterAmbiencePack)',
  'StarterMusicRequestId',
  'StarterMusicTag',
  'PredictionState = EGamePlatformPresentationPredictionState::Cancelled',
  'L_Village_Start',
  'SSLM场景音频请求已受理'
)) {
 if(-not $audio.Contains($item)){throw "音频主链路缺少：$item"}
}
if(!$audio.Contains('Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;')) {
 throw '首批音效分类的Catalog Entry仍为Project作用域，平台门禁会拒绝预载完成后发布'
}
if(!$catalog.Contains('BuildStarterAudioFragment(FName Category)') -or
   !$catalog.Contains('if (!Category.IsNone())') -or
   !$catalog.Contains('EntryName.StartsWith(Prefix + TEXT("."))')) {
 throw 'Starter音频目录未按类别独立解析'
}
if(!$coord.Contains('StarterMusicPack.Id == PackHandle.Id') -or
   !$coord.Contains('StarterAmbiencePack.Id == PackHandle.Id') -or
   !$coord.Contains('SubmitStarterAudioForWorld()') -or
   !$coord.Contains('PlayerControllerChanged(APlayerController* NewPlayerController)')) {
 throw '客户端音频异步Active/Controller就绪重触发链路缺失'
}
foreach($item in @(
 'ForEachWidgetAndDescendants',
 'BindButtonAudio()',
 'UnbindButtonAudio()',
 'OnClicked.AddUniqueDynamic',
 'OnClicked.RemoveDynamic',
 'PlayStarterUISound(ActionId)',
 'HandleButtonAudioClick',
 'HandleButtonAudioConfirm',
 'HandleButtonAudioCancel'
)) {
 if(!$screen.Contains($item)){throw "UI按钮统一声音绑定缺少：$item"}
}
if($login.Contains('Audio->PlayStarterUISound(TEXT("Click"))')) {
 throw '登录页仍有独立单声调用，可能重复播放UI点击音'
}
if($screen -match '\bNativeTick\b' -or $audio -match '\bLoadSynchronous\b') {
 throw '发现轮询或同步加载；不符合事件驱动设计'
}
& (Join-Path $PSScriptRoot 'TestStarterAudioStructure.ps1')
Write-Output 'SSLM_BUTTON_VILLAGE_BGM_WIRING_STATIC_OK 3-UI 1-VillageMusic 1-Ambience 3-Spells'
