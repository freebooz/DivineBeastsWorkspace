#requires -Version 7
<#
SSLM首批音频静态发布检查（只读）。
测试资源是否真实存在、来源哈希是否正确、主资产扫描与项目语义注册是否齐全。
无法替代UE编辑器资产冷重载、真实听觉、Cook和5v5音频性能验收。
#>
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$audio = Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core'
$tags = Get-Content -Raw (Join-Path $root 'Game/Config/DefaultGameplayTags.ini')
$registry = Get-Content -Raw (Join-Path $root 'Game/Config/DefaultGame.ini')
$catalog = Get-Content -Raw (Join-Path $root 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp')
$manifest = Get-Content -Raw (Join-Path $audio 'Docs/SSLMStarterAudioSources_20261018.md')
$rows = @(
 @('UI','SW_DBA_UI_Click_01.ogg','S_DBA_UI_Click_01','DA_DBA_SFX_UI_Click','UI.Click','adcd1f4adc35f1b41bc1b5bbefeff7aa44f2f3f0d96d3199b544140c7c1e761c'),
 @('UI','SW_DBA_UI_Confirm_01.ogg','S_DBA_UI_Confirm_01','DA_DBA_SFX_UI_Confirm','UI.Confirm','063564703b6094d70718a3e787a55cc9141611e4ecd6b6637f8828f79b4a8c3a'),
 @('UI','SW_DBA_UI_Cancel_01.ogg','S_DBA_UI_Cancel_01','DA_DBA_SFX_UI_Cancel','UI.Cancel','07db973f79f6ae0f2edc34561e7592e24d0577455919fb602cb8ecc0da991dcf'),
 @('Spells','SW_DBA_Spell_Frost_Cast_01.ogg','S_DBA_Spell_Frost_Cast_01','DA_DBA_SFX_Spell_FrostCast','Spell.Frost.Cast','119f9061a566c72c1c58444b03f8685b833079ef268dda02fe742eb34d3bc68f'),
 @('Spells','SW_DBA_Spell_Frost_Impact_01.ogg','S_DBA_Spell_Frost_Impact_01','DA_DBA_SFX_Spell_FrostImpact','Spell.Frost.Impact','9fac648a1de869cf9ca2f4b3f1cd47f1766a3ee9d2fdec45831234401d7b2392'),
 @('Spells','SW_DBA_Spell_Fire_Impact_01.wav','S_DBA_Spell_Fire_Impact_01','DA_DBA_SFX_Spell_FireImpact','Spell.Fire.Impact','3992598c814de25c318780e506aeda673a7e410b7b3c4f0793883483ff9766b8'),
 @('Music','SW_DBA_Music_Login_01.ogg','S_DBA_Music_Login_01','DA_DBA_SFX_Music_Login','Music.Login','172d95262348d020d7d1428046b100aefbad0a6cbaa93ef87efafeb8add107d0'),
 @('Music','SW_DBA_Music_Village_01.mp3','S_DBA_Music_Village_01','DA_DBA_SFX_Music_Village','Music.Village','b6ce65fceb9373f32697ed27455ed995fa03a77649b04a6f3e86a0243d25d4b3'),
 @('Ambience','SW_DBA_Ambience_Forest_01.mp3','S_DBA_Ambience_Forest_01','DA_DBA_SFX_Ambience_Forest','Ambience.Forest','9850aa1d0d5d66bd9c5daf8bb77c6d852e01f2f4de22f283bd5621e8bed13b75')
)
foreach($r in $rows) {
 $folder,$sourceName,$soundName,$definitionName,$semantic,$hash = $r
 $source = Join-Path $audio "SourceArt/Audio/$folder/$sourceName"
 $wave = Join-Path $audio "Content/Audio/$folder/Waves/$soundName.uasset"
 $definition = Join-Path $audio "Content/Audio/$folder/Definitions/$definitionName.uasset"
 foreach($file in @($source,$wave,$definition)) {
  if(-not (Test-Path -LiteralPath $file -PathType Leaf)) {throw "音频资源缺失：$file"}
 }
 $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()
 if($actual -ne $hash) {throw "音频原件SHA256变化：$sourceName"}
 if($tags -notlike "*Presentation.SSLM.Audio.$semantic*"){throw "GameplayTag未登记：$semantic"}
 if($catalog -notlike "*Presentation.SSLM.Audio.$semantic*"){throw "Catalog未登记：$semantic"}
 if($catalog -notlike "*presentation.dba.audio.*"){throw "Catalog缺逻辑Definition"}
 if($manifest -notlike "*$hash*"){throw "源授权清单中不存在SHA256：$sourceName"}
 if($registry -notlike "*/DBASFXPack_Core/Audio/$folder/Definitions*"){throw "AssetManager未扫描音频定义目录：$folder"}
 Write-Output "SSLM_AUDIO_STATIC_ASSET_OK $folder/$soundName"
}
foreach($name in @('S_DBA_SpellShort_Attenuation','S_DBA_UI_Click_Concurrency','S_DBA_Spell_Short_Concurrency','S_DBA_Music_Concurrency','S_DBA_Ambience_Concurrency')) {
 $path=Join-Path $audio "Content/Audio/Shared/Policies/$name.uasset"
 if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "音频策略原生资产缺失：$name"}
}
$runtime=Get-Content -Raw (Join-Path $root 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Audio/DivineBeastsStarterAudio.cpp')
foreach($required in @('EnsureStarterAudioForWorld','SubmitStarterAudioForWorld','PlayStarterUISound','PlayStarterSpellSound','StopStarterAudioForWorld')) {
 if(-not $runtime.Contains($required)){throw "首批音效接线方法缺失：$required"}
}
Write-Output 'SSLM_AUDIO_STATIC_OK 9-SoundWave 9-Definition 5-Policies 9-GameplayTags'
