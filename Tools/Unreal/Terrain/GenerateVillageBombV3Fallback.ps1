#requires -Version 5.1
<#
.SYNOPSIS
  SSLM新手村V3.0草地小体积共享默认颜色纹理制作。
.DESCRIPTION
  只从已审批候选的CC0草地V3派生2048源按高质量重采样生成256的sRGB基础色PNG，
  用于共享中立Landscape母材质的低成本编辑器/缺少客户端世界租约的默认外观。
  不重新生成用户V3源、不修改十张生产纹理、不改变法线/ORM/地形权威。
  原SourceArt哈希以GenerationManifest.json为准；新文件创建前必须确保不存在。
  默认 -VerifyOnly只校验已有源、目标与manifest，不写文件；-Apply显式首次生成。
#>
[CmdletBinding()]
param([switch]$Apply, [switch]$VerifyOnly)
$ErrorActionPreference='Stop'
if ($Apply -and $VerifyOnly) { throw '-Apply与-VerifyOnly不能同时使用' }
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$source=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Terrain/V3/T_DBA_Terrain_Grass_BaseColor_V3_Source.png'
$provenance=Join-Path $root 'Saved/ArtReview/TerrainColorV3/GenerationManifest.json'
$outputFolder=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/BombingV3'
$output=Join-Path $outputFolder 'T_DBA_Village_GroundAlbedoV3Default_Source.png'
$manifest=Join-Path $outputFolder 'VillageBombFallbackManifest.json'
if (!(Test-Path -LiteralPath $source) -or !(Test-Path -LiteralPath $provenance)) {
    throw '缺少经已批准V3来源校验的2048草地源PNG'
}
$generation=Get-Content -LiteralPath $provenance -Raw -Encoding UTF8|ConvertFrom-Json
$grass=@($generation.layers|Where-Object{$_.layer -eq 'Grass'})
if($grass.Count -ne 1){throw 'V3来源草地记录不唯一'}
$srcsha=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($srcsha -ne $grass[0].derivedSha256){throw '输入PNG与原V3颜色派生SHA不一致'}
Add-Type -AssemblyName System.Drawing.Common
if(!$Apply) {
    if(!(Test-Path -LiteralPath $manifest) -or !(Test-Path -LiteralPath $output)) {
        Write-Output 'SSLM_V3_BOMB_FALLBACK_PLAN source_verified=true output_png_pending=true'
        return
    }
    $m=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8|ConvertFrom-Json
    $digest=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
    $img=[System.Drawing.Bitmap]::new($output)
    try{$size="$($img.Width)x$($img.Height)"} finally {$img.Dispose()}
    if($m.schemaVersion -ne 1 -or $m.sourceSha256 -ne $srcsha -or
        $m.outputSha256 -ne $digest -or $size -ne '256x256') {
        throw '已输出的sRGB小纹理清单或像素尺寸校验失败'
    }
    Write-Output ('SSLM_V3_BOMB_FALLBACK_VERIFY_OK size='+$size+' sha256='+$digest)
    return
}
if(Test-Path $outputFolder){throw '输出共享小纹理目录已存在，拒绝重新覆盖或冒充源图'}
New-Item -ItemType Directory -Path $outputFolder -Force |Out-Null
$original=[System.Drawing.Bitmap]::new($source)
$bitmap=[System.Drawing.Bitmap]::new(256,256,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try{
    if($original.Width -ne 2048 -or $original.Height -ne 2048){throw '输入草地V3真实图不是2048²'}
    $gfx=[System.Drawing.Graphics]::FromImage($bitmap)
    try{
        $gfx.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $gfx.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gfx.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $gfx.DrawImage($original,0,0,256,256)
    } finally {$gfx.Dispose()}
    $bitmap.Save($output,[System.Drawing.Imaging.ImageFormat]::Png)
} finally{$original.Dispose();$bitmap.Dispose()}
$sha=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
[ordered]@{
 schemaVersion=1
 roleZh='正式Village共享中性母材质的低体积V3草地基础色默认源图'
 sourcePath='Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Terrain/V3/T_DBA_Terrain_Grass_BaseColor_V3_Source.png'
 sourceSha256=$srcsha
 outputSourcePath='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/BombingV3/T_DBA_Village_GroundAlbedoV3Default_Source.png'
 outputSha256=$sha
 width=256
 height=256
 colorSpaceZh='仅基础色sRGB；Normal/ORM继续使用已有默认或客户端高精资源'
 licenseZh='原Poly Haven CC0扫描基础色的V3.0派生，来源与原始Manifest追溯'
 userReviewStatusZh='待正式UE客户端与人工场景检查'
}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $manifest -Encoding UTF8
Write-Output ('SSLM_V3_BOMB_FALLBACK_CREATED width=256 height=256 source_verified=true sha256='+$sha)
