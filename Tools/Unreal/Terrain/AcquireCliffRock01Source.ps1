#requires -Version 5.1
<#
.SYNOPSIS
CliffRock01（青灰悬崖岩石）第一批4K同源扫描纹理获取与证据签名。
.DESCRIPTION
归属：DivineBeasts项目层/DBAWorldPack_Village世界内容包SourceArt。
仅从Poly Haven官方CC0的marble_cliff_05读取Diffuse、nor_dx、arm、Displacement；
只下载到独立Original目录，不覆盖Rock01、十层主材质或正式Village。
每张严格校验官方MD5、文件字节长度及本地SHA256；已存在文件不覆盖，
失败清理自身新建的.part；最终SourceManifest声明未经过人工签收。
#>
[CmdletBinding()]
param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$id='marble_cliff_05'
$api='https://api.polyhaven.com/files/'+$id
$base=Join-Path (Get-Location).Path 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/PBR/CliffRock01'
$dir=Join-Path $base 'Original'
$manifest=Join-Path $base 'CliffRock01_SourceManifest.json'
$sources=Invoke-RestMethod $api -TimeoutSec 35 -ErrorAction Stop
$roles=@(
 @{role='BaseColor';type='Diffuse';token='_diff_'},
 @{role='NormalDX';type='nor_dx';token='_nor_dx_'},
 @{role='ORM';type='arm';token='_arm_'},
 @{role='Height';type='Displacement';token='_disp_'}
)
$files=@()
[long]$needed=0
foreach($role in $roles){
 $spec=$sources.($role.type).'4k'.png
 if(!$spec -or !$spec.url -or !$spec.md5 -or !$spec.size){throw ('官方PBR通道缺失：'+$role.type)}
 $uri=[Uri]$spec.url
 if($uri.Scheme -ne 'https' -or $uri.Host -ne 'dl.polyhaven.org' -or
    !$uri.AbsolutePath.StartsWith('/file/ph-assets/Textures/png/4k/'+$id+'/') -or
    !$uri.AbsolutePath.Contains($role.token) -or !$uri.AbsolutePath.EndsWith('.png')){
   throw ('官方PBR原图来源非法：'+$uri.AbsoluteUri)
 }
 [long]$bytes=$spec.size
 if($bytes -lt 300000 -or $bytes -gt 180MB){throw ('官方图大小异常：'+$role.type)}
 $name='T_DBA_CliffRock01_'+$role.role+'_4K_Original.png'
 $files+=@{role=$role.role;type=$role.type;url=$spec.url;md5=([string]$spec.md5).ToLowerInvariant();size=$bytes;path=(Join-Path $dir $name)}
 $needed+=$bytes
}
if(!$VerifyOnly -and (Get-PSDrive E).Free -lt ($needed*2+250MB)){
 throw 'E盘空间不足原图及导入：不能下载以免破坏项目'
}
if(!$VerifyOnly){New-Item -ItemType Directory -Path $dir -Force|Out-Null}
$manifestRows=@()
foreach($f in $files){
 if(!(Test-Path $f.path -PathType Leaf)){
   if($VerifyOnly){throw ('只读校验发现原图不存在：'+$f.path)}
   $part=$f.path+'.part'
   if(Test-Path $part){throw ('旧未校验下载片段尚存在：'+$part)}
   Write-Host ('CLIFFROCK_SOURCE_DOWNLOAD '+$f.role+' '+$f.size)
   try{
     Invoke-WebRequest $f.url -OutFile $part -UseBasicParsing -TimeoutSec 900
     if((Get-Item $part).Length -ne $f.size){throw '字节数与官方API不符'}
     if((Get-FileHash $part -Algorithm MD5).Hash.ToLowerInvariant() -ne $f.md5){throw '官方MD5不符'}
     Move-Item -LiteralPath $part -Destination $f.path
   }catch{
     if(Test-Path $part){Remove-Item $part -ErrorAction SilentlyContinue}
     throw ('CliffRock源图下载不合格：'+$f.role+': '+$_.Exception.Message)
   }
 }
 $entry=Get-Item $f.path
 $actualMd5=(Get-FileHash $f.path -Algorithm MD5).Hash.ToLowerInvariant()
 $sha=(Get-FileHash $f.path -Algorithm SHA256).Hash.ToLowerInvariant()
 if($entry.Length -ne $f.size -or $actualMd5 -ne $f.md5){
   throw ('原始扫描贴图与官方签名不一致，禁止导入：'+$f.path)
 }
 $relative=$f.path.Replace((Get-Location).Path+'\','').Replace('\','/')
 $manifestRows+=@{role=$f.role;upstreamMap=$f.type;sourceUrl=$f.url;localPath=$relative;
                  bytes=$f.size;upstreamMd5=$f.md5;sha256=$sha}
 Write-Host ('CLIFFROCK_SOURCE_PBR_VERIFIED '+$f.role+' bytes='+$entry.Length+' sha256='+$sha)
}
$manifestData=[ordered]@{
  schemaVersion=1
  kind='third_party_pbr_scan_candidate'
  sourceId=$id
  sourceResolution='4k'
  sourceAssetUrl=('https://polyhaven.com/a/'+$id)
  sourceFilesApi=$api
  license='CC0 1.0'
  licenseUrl='https://polyhaven.com/license'
  creator='Amal Kumar'
  referenceWidthMeters=20.0
  actualAssetWidthMeters=20.0
  scanSourceIsWarmBeige=$true
  projectTargetColorZh='青灰色悬崖裸岩；只对BaseColor执行可追溯的低饱和冷色校正'
  originalMapsImmutable=$true
  productionAssetApproved=$false
  notesZh='UE5.8神兽联盟第三层内容包项目候选，未通过用户本人最终审核不得替换正式Village'
  maps=$manifestRows
}
$payload=$manifestData|ConvertTo-Json -Depth 13
if(Test-Path $manifest){
 $prev=Get-Content $manifest -Raw -Encoding UTF8|ConvertFrom-Json
 if($prev.sourceId -ne $id -or $prev.maps.Count -ne 4){throw '已有SourceManifest来源身份不相符'}
 foreach($row in $manifestRows){
   $matches=@($prev.maps|Where-Object {$_.role -eq $row.role -and $_.sha256 -eq $row.sha256})
   if($matches.Count -ne 1){throw 'SourceManifest记录与扫描源实际签名不一致'}
 }
}else{
 if($VerifyOnly){throw '验证模式不允许新建Manifest'}
 [IO.File]::WriteAllText($manifest,$payload+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
 Write-Host 'CLIFFROCK_SOURCE_MANIFEST_SAVED'
}
Write-Host ('CLIFFROCK_4K_CC0_SOURCE_QA_PASS source='+$id+' maps=4 total_bytes='+$needed)
