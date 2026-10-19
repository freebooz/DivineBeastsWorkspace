#requires -Version 7
<#
SSLM丑牛V6图标输出：8张64×64审核纹理和8张48×48游戏纹理。
只使用当前项目已审核来源的PNG原型；保留真实来源与待人审状态。
严格校验48图与已导入UE5.8 Texture2D的源PNG字节相同；拒绝覆盖别人不同内容。
#>
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$manifest=Get-Content -Raw (Join-Path $root 'Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json')|ConvertFrom-Json
if($manifest.heroId -ne 'Hero.Zodiac.Ox' -or $manifest.items.Count -ne 8){throw '牛V6八技能身份不完整'}
$staging=Join-Path $root 'Game/Saved/ArtReview/OxV6_Source_20261018/WorkingArtFromProject'
$hero=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/SourceArt/UI/SkillIcons/V6'
$dest64=Join-Path $hero 'Review64'
$dest48=Join-Path $hero 'Runtime48'
# 现有UE作者导入器把48px源图放在V6根下，Textures只是.uasset挂载目录。
$native=$hero
$bundleRoot=Join-Path $root 'Game/Saved/ArtReview/OxV6_Source_20261018/OxV6_64_48_Bundle'
$archive=Join-Path $root 'Game/Saved/ArtReview/OxV6_Source_20261018/SSLM_OxV6_8Review64_8Runtime48.zip'
function Sha([string]$f){return (Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash.ToLowerInvariant()}
function Assert-Png([string]$f,[int]$pixels){
  if(!(Test-Path -LiteralPath $f -PathType Leaf)){throw '图片文件不存在：'+$f}
  $img=[Drawing.Image]::FromFile($f)
  try{if($img.Width -ne $pixels -or $img.Height -ne $pixels){throw '图标尺寸不正确：'+$f}}
  finally{$img.Dispose()}
}
function Safe-Copy([string]$source,[string]$target){
  $folder=Split-Path $target -Parent
  if(!(Test-Path -LiteralPath $folder)){New-Item -ItemType Directory -Force -Path $folder|Out-Null}
  if(Test-Path -LiteralPath $target){
    if((Sha $source) -ne (Sha $target)){throw '已存在不同内容，拒绝覆盖：'+$target}
    return
  }
  Copy-Item -LiteralPath $source -Destination $target
  if((Sha $source) -ne (Sha $target)){throw '复制哈希不一致：'+$target}
}
if(Test-Path -LiteralPath $bundleRoot){throw '审核打包目录已存在，拒绝覆盖'}
if(Test-Path -LiteralPath $archive){throw '审核压缩包已存在，拒绝覆盖'}
$hash64=[Collections.Generic.HashSet[string]]::new()
$hash48=[Collections.Generic.HashSet[string]]::new()
$rows=@()
foreach($entry in $manifest.items){
  $key=[string]$entry.artFileName
  $src64=Join-Path $staging $entry.preview64
  $src48=Join-Path $staging $entry.preview48
  Assert-Png $src64 64
  Assert-Png $src48 48
  $h64=Sha $src64
  $h48=Sha $src48
  if(!$hash64.Add($h64) -or !$hash48.Add($h48)){throw '存在完全重复的技能图标：'+$key}
  $used=Join-Path $native ($key+'.png')
  if(!(Test-Path -LiteralPath $used) -or (Sha $used) -ne $h48){
    throw '当前48×48游戏PNG不等于UE纹理正式导入源：'+$key
  }
  $f64=$key+'_64.png'
  $f48=$key+'_48.png'
  Safe-Copy $src64 (Join-Path $dest64 $f64)
  Safe-Copy $src48 (Join-Path $dest48 $f48)
  $rows+= [ordered]@{
    order=$entry.order;nameZh=$entry.displayNameZh;slot=$entry.slotId;abilityId=$entry.abilityId
    review64='Review64/'+$f64;sha256Review64=$h64
    runtime48='Runtime48/'+$f48;sha256Runtime48=$h48
    texture2D=$entry.targetTexturePath
    approval='PENDING_HUMAN_ART_REVIEW'
  }
  Write-Output ('OX_V6_PAIR_OK '+$entry.order+' '+$entry.displayNameZh+' 64x64 48x48')
}
$report=[ordered]@{
  project='SSLM';heroId='Hero.Zodiac.Ox';review64Count=8;runtime48Count=8
  sourceKind='DERIVED_FROM_EXISTING_PROJECT_OX_ART'
  approval='PENDING_HUMAN_ART_REVIEW'
  notesZh='与当前会话生成原图不是同一来源。本次16张PNG由丑牛内容包已有5张高清美术衍生；部分技能意象需重绘。48×48图与现有8张UE Texture2D的导入源逐字节匹配；未修改技能授权和HUD。'
  entries=$rows
}
New-Item -ItemType Directory -Path $bundleRoot -Force|Out-Null
foreach($row in $rows){
  Safe-Copy (Join-Path $hero $row.review64) (Join-Path $bundleRoot $row.review64)
  Safe-Copy (Join-Path $hero $row.runtime48) (Join-Path $bundleRoot $row.runtime48)
}
$body=$report|ConvertTo-Json -Depth 8
$encoder=[Text.UTF8Encoding]::new($false)
$mainMeta=Join-Path $hero 'OxV6_64_48_SourceManifest.json'
$zipMeta=Join-Path $bundleRoot 'OxV6_64_48_SourceManifest.json'
if(Test-Path -LiteralPath $mainMeta){throw '正式美术来源清单已存在，拒绝覆盖'}
[IO.File]::WriteAllText($mainMeta,$body,$encoder)
[IO.File]::WriteAllText($zipMeta,$body,$encoder)
[IO.Compression.ZipFile]::CreateFromDirectory($bundleRoot,$archive,[IO.Compression.CompressionLevel]::Optimal,$false)
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try{$count=@($zip.Entries|Where-Object{$_.FullName.EndsWith('.png')}).Count}finally{$zip.Dispose()}
if($count -ne 16){throw '输出ZIP中PNG必须恰好16张'}
Write-Output ('SSLM_OX_V6_64_48_COMPLETE review64=8 runtime48=8 bytes='+(Get-Item $archive).Length+' archive='+$archive)
