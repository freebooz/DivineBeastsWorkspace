# SSLM水体RGBA技术纹理及UE原生制作者静态合同测试。
# 只检查签名和防误写语义，不替代Unreal资产、Shader、Cook及视觉验收。
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source=Join-Path $root 'Docs/Implementation/Water/SourceTextures'
$doc=Get-Content (Join-Path $source 'WaterTextureSourceManifest.json') -Encoding UTF8 -Raw | ConvertFrom-Json
if($doc.version -ne 1 -or @($doc.items).Count -ne 4) { throw '源图清单数量不符合要求' }
$seen=@{}
foreach($row in $doc.items) {
 $file=[string]$row.file
 if($file -cnotmatch '^T_GP_Water_[A-Za-z0-9_]+_Source\.png$' -or $seen.ContainsKey($file)) { throw "源图重名/命名不合法：$file" }
 $seen[$file]=$true
 $path=Join-Path $source $file
 if(-not (Test-Path -LiteralPath $path)) { throw "源图缺失：$file" }
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne [string]$row.sha256) { throw "源图指纹失配：$file" }
 if($row.width -ne 1024 -or $row.height -ne 1024 -or $row.sRGB -ne $false) { throw "源图尺寸/色彩空间错误：$file" }
}
$author=Get-Content (Join-Path $root 'Tools/Unreal/Surface/ImportWaterSourceTexturesUE58.py') -Encoding UTF8 -Raw
foreach($guard in @('replace_existing", False','import_asset_tasks','save_loaded_asset','sha256','MODE == "inspect"','GamePlatformSurface/Textures/Water')) { if(-not $author.Contains($guard)){ throw "原生制作门禁缺失：$guard" } }
Write-Output 'WATER_TEXTURE_SOURCE_CONTRACT_PASS four_source_png_verified'
