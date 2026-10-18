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
Write-Output 'SSLM_AUDIO_LIBRARY_BINDINGS_STATIC_OK 32 categories=8 wolf=1 spells=2 footstep_notify=1'
