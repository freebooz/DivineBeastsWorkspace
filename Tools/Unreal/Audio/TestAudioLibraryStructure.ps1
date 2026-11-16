#requires -Version 7
<#
SSLM音频资源库32份声音只读静态门禁：
验证合法来源原件指纹、UE原生uasset是否真实存在、Definition扫描目录和GameplayTags。
不替代UE冷回读、声音试听或Client/Server Cook。
#>
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$pack=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core'
$m=Get-Content -Raw (Join-Path $pack 'Docs/SSLM_AudioLibraryExpansion_20261018.json')|ConvertFrom-Json
if($m.schemaVersion -ne 1 -or $m.entries.Count -ne 32){throw '扩展声音清单不是32项'}
$tags=Get-Content -Raw (Join-Path $root 'Game/Config/DefaultGameplayTags.ini')
$game=Get-Content -Raw (Join-Path $root 'Game/Config/DefaultGame.ini')
if(!$game.Contains('(Path="/DBASFXPack_Core/Audio/Library")')){throw 'AssetManager未扫描新增音频定义'}
$identity=@{}
foreach($entry in $m.entries){
  if($identity.ContainsKey($entry.logicalId)){throw "定义身份重复: $($entry.logicalId)"}
  $identity[$entry.logicalId]=1
  $path=Join-Path $pack ("SourceArt/Audio/Library/$($entry.category)/$($entry.filename)")
  $wave=Join-Path $pack ("Content/Audio/Library/$($entry.category)/Waves/$([IO.Path]::GetFileNameWithoutExtension($entry.filename)).uasset")
  $definition=Join-Path $pack ("Content/Audio/Library/$($entry.category)/Definitions/DA_DBA_SFX_$($entry.category)_$($entry.id).uasset")
  foreach($f in @($path,$wave,$definition)){if(-not(Test-Path -LiteralPath $f -PathType Leaf)){throw "缺少声音/定义: $f"}}
  if(((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()) -ne $entry.sha256){throw "SHA256错误: $path"}
  if(!$tags.Contains(('Tag="'+$entry.semanticTag+'"'))){throw "GameplayTag未登记: $($entry.semanticTag)"}
  if($entry.license -ne 'CC0-1.0'){throw "资源许可不符合首批导入门禁: $($entry.filename)"}
  Write-Output "SSLM_AUDIO_LIBRARY_STATIC_OK $($entry.category)/$($entry.id)"
}
foreach($name in @('S_DBA_AudioLibrary_Footstep_Concurrency','S_DBA_AudioLibrary_Creature_Concurrency','S_DBA_AudioLibrary_Ambience_Concurrency','S_DBA_AudioLibrary_Ambient_Attenuation')){
  if(!(Test-Path -LiteralPath (Join-Path $pack "Content/Audio/Shared/Policies/$name.uasset"))){throw "缺少音频策略: $name"}
}
Write-Output 'SSLM_AUDIO_LIBRARY_STATIC_VERIFY_OK 32 4'
