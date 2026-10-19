#requires -Version 7
<#
SSLM音频库第二阶段静态接线门禁（只读）。
核验所有32条语义、客户端分类租约、生命周期、狼/技能/脚步适配。
静态结果不等于UE编译、动画原生Notify已添加或实机声音试听。
#>
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$base=Join-Path $root 'Game/Plugins/DivineBeasts/DBAClient/Source'
$manifest=Get-Content -Raw (Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/Docs/SSLM_AudioLibraryExpansion_20261018.json') | ConvertFrom-Json
$catalog=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsAudioLibraryCatalog.cpp')
$runtime=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/Audio/DivineBeastsAudioLibrary.cpp')
$subsystem=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp')
$wolf=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/Creatures/DivineBeastsVillageWolfAnimInstance.cpp')
$magic=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/Adapters/DivineBeastsDevelopmentMagicPresentation.cpp')
$footstep=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/Audio/DivineBeastsAudioFootstepNotify.cpp')
$footer=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Public/Audio/DivineBeastsAudioFootstepNotify.h')
$waterAnchorHeader=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Public/Audio/DivineBeastsWaterAudioAnchorComponent.h')
$waterAnchor=Get-Content -Raw (Join-Path $base 'DivineBeastsPresentationClient/Private/Audio/DivineBeastsWaterAudioAnchorComponent.cpp')
if($manifest.entries.Count -ne 32){throw 'Manifest新增音效定义数不为32'}
foreach($row in $manifest.entries){
  foreach($s in @($row.semanticTag,$row.logicalId)){
    if(!$catalog.Contains([string]$s)){throw "音频目录未映射：$s"}
  }
}
foreach($symbol in @('BuildAudioLibraryFragment','ActivateAudioLibraryCategory','EnsureAudioLibraryForWorld','StopAudioLibraryForWorld','ResolveAudioLibraryClip','PlayAudioLibraryClip','PlayAudioLibraryFactClip','StartAudioLibraryLoop','StopAudioLibraryLoop')){
  if(!($catalog+$runtime+$subsystem).Contains($symbol)){throw "缺音频服务方法：$symbol"}
}
if(!$runtime.Contains('AudioLibraryCategories') -or !$runtime.Contains('DeactivateContentPack') -or !$runtime.Contains('PredictionState::Cancelled')){
  throw '客户端分类生命周期/循环取消不完整'
}
if(!$wolf.Contains('EmitWolfAudio') -or !$wolf.Contains('Current.ActivationId') -or !$wolf.Contains('bLifeSnapshotInitialized')){
  throw '狼攻击去重/首次死亡快照守卫不完整'
}
if(!$magic.Contains('Spell.ArcaneCast') -or !$magic.Contains('Spell.Healing') -or !$magic.Contains('Event.EventId')){
  throw '法术确认音效接线不完整'
}
if(!$footstep.Contains('LineTraceSingleByChannel') -or !$footstep.Contains('PhysicalMaterialToClip') -or !$footer.Contains('public UAnimNotify')){
  throw '动画落脚与物理材质选择缺失'
}
if($runtime -match '(?i)LoadSynchronous\s*\(|SpawnSound(2D|Attached|AtLocation)\s*\('){
  throw '项目音频库绕过统一SFX播放器/异步资源'
}
if(($subsystem|Select-String 'EnsureAudioLibraryForWorld' -AllMatches).Matches.Count -lt 4){
  throw '客户端新世界/账号重建未完整调用音频库挂载'
}
# 专项防回归：PhysicalMaterial物理类型、同一脚步重复动画事件、真实场景水源和退出释放。
if(!$footer.Contains('SurfaceTypeToClip') -or !$footstep.Contains('SurfaceTypeToClip') -or
   !$runtime.Contains('PlayAudioLibraryFootstepClip') -or !$runtime.Contains('RecentFootsteps.FindOrAdd') -or
   !$runtime.Contains('RecentFootsteps.Reset()') -or !$subsystem.Contains('StopAudioLibraryForWorld')){
  throw '脚步Surface物理材质回退、同脚掌去重或世界清理缺失'
}
if(!$waterAnchorHeader.Contains('public USceneComponent') -or
   !$waterAnchorHeader.Contains('RequiredSourceTag') -or
   !$waterAnchor.Contains('ActorHasTag') -or !$waterAnchor.Contains('GetComponentLocation()') -or
   !$waterAnchor.Contains('StartAudioLibraryLoop') -or !$waterAnchor.Contains('StopAudioLibraryLoop') -or
   !$waterAnchor.Contains('EndPlay') -or !$waterAnchor.Contains('OnContentPackChanged') -or
   !$waterAnchor.Contains('NM_DedicatedServer')){
  throw '新手村水源Actor锚点、异步接线或生命周期保护缺失'
}
if($waterAnchor -match '(?i)SpawnSound(2D|Attached|AtLocation)\s*\(|LoadSynchronous\s*\(|PrimaryComponentTick\.bCanEverTick\s*=\s*true'){
  throw '水声锚点绕过已有GamePlatformSFX或引入逐帧执行'
}
$looping=@($manifest.entries | Where-Object { $_.looping -eq $true })
if($looping.Count -ne 2 -or
   @($looping | Where-Object { $_.category -ne 'Ambience' -or $_.id -notin @('WaterStreamA','WaterStreamB') }).Count -ne 0){
  throw '循环水声白名单与32条音源清单不一致'
}
Write-Output 'SSLM_AUDIO_LIBRARY_BINDINGS_STATIC_OK 32 categories=8 wolf=1 spells=2 footstep_notify=1 footstep_dedupe=1 surface_mapping=1 water_anchor=1'
