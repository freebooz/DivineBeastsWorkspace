#requires -Version 5.1
<#
.SYNOPSIS
《神兽联盟》项目岩石写实材质第一批：下载Poly Haven CC0同源4K PBR贴图。
.DESCRIPTION
资源归第三层DBAWorldPack_Village SourceArt；保留所有既有技术校准岩石纹理、
材质和正式Village地图。源资产ID：rock_face_04，摄影Dimitrios Savva。
官方提供Diffuse（基础色）、nor_dx（DirectX法线）、arm（AO/粗糙度/金属度）、
Displacement（高度）。仅允许官方API https链接，校验源MD5、文件大小和SHA256。
文件先写.part再验证并重命名，已有文件不得被不相同内容覆盖。
产物是等待人工审核的3A扫描候选，不自动当作用户已经签收。
#>
[CmdletBinding()]
param(
    [ValidateSet('4k','2k')][string]$Resolution='4k',
    [switch]$VerifyOnly
)
$ErrorActionPreference='Stop'
$assetId='rock_face_04'
$api='https://api.polyhaven.com/files/'+$assetId
$sourceFolder=Join-Path (Get-Location).Path 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/PBR/RockRealism01'
$manifest=Join-Path $sourceFolder 'RockRealism01_SourceManifest.json'
$official=Invoke-RestMethod -Uri $api -TimeoutSec 25 -ErrorAction Stop
$specs=@(
    @{map='Diffuse';label='BaseColor';expected='_diff_'},
    @{map='nor_dx';label='NormalDX';expected='_nor_dx_'},
    @{map='arm';label='ORM';expected='_arm_'},
    @{map='Displacement';label='Height';expected='_disp_'}
)
$preflight=@()
$space=(Get-PSDrive E).Free
$totalExpected=0L
foreach($spec in $specs){
    $entry=$official.($spec.map).($Resolution).png
    if(!$entry -or !$entry.url -or !$entry.md5 -or !$entry.size){
        throw ('官方同源纹理缺少实际PNG映射：'+$spec.map)
    }
    $uri=[Uri]$entry.url
    if($uri.Scheme -ne 'https' -or $uri.Host -ne 'dl.polyhaven.org' -or
        !$uri.AbsolutePath.StartsWith('/file/ph-assets/Textures/png/'+$Resolution+'/rock_face_04/') -or
        !$uri.AbsolutePath.EndsWith('.png') -or !$uri.AbsolutePath.Contains($spec.expected)){
        throw ('来源URL不符合官方纹理目录/角色：'+$uri.AbsoluteUri)
    }
    if([int64]$entry.size -lt 1024*256 -or [int64]$entry.size -gt 190MB){
        throw ('官方文件长度不在合理范围：'+$spec.map)
    }
    $target=Join-Path $sourceFolder ('T_DBA_RockRealism01_'+$spec.label+'_4K_Source.png')
    $preflight+=@{
        label=$spec.label; map=$spec.map; sourceUrl=$entry.url
        expectedMd5=([string]$entry.md5).ToLowerInvariant()
        expectedBytes=[int64]$entry.size
        path=$target
    }
    $totalExpected+=[int64]$entry.size
}
if($space -lt $totalExpected*3+650MB){
    throw 'E盘剩余容量不足扫描PBR原图、UE导入和审查预览的空间'
}
Write-Host ('ROCK_REALISM_OFFICIAL_ASSET='+$assetId+' RES='+$Resolution+' FILES=4 EXPECTED_TOTAL_BYTES='+$totalExpected)
if(Test-Path $manifest){
    $existing=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8|ConvertFrom-Json
    if($existing.sourceId -ne $assetId -or $existing.sourceResolution -ne $Resolution){
        throw ('已有来源清单和本次资产身份不符，拒绝覆盖：'+$manifest)
    }
}
if(!$VerifyOnly){
    New-Item -ItemType Directory -Force -Path $sourceFolder|Out-Null
}
$resolved=@()
foreach($entry in $preflight){
    $path=$entry.path
    if(!(Test-Path $path)){
        if($VerifyOnly){throw ('只读模式中原始源图缺失：'+$path)}
        $tmp=$path+'.part'
        if(Test-Path $tmp){throw ('发现未确认的旧下载部分文件：'+$tmp)}
        Write-Host ('ROCK_REALISM_DOWNLOAD_START '+$entry.label+' expected='+$entry.expectedBytes+' bytes')
        try{
            Invoke-WebRequest -Uri $entry.sourceUrl -OutFile $tmp -UseBasicParsing -TimeoutSec 900 -ErrorAction Stop
            if((Get-Item $tmp).Length -ne $entry.expectedBytes){throw ('源图字节长度不匹配：'+$entry.label)}
            $actualMd5=(Get-FileHash -LiteralPath $tmp -Algorithm MD5).Hash.ToLowerInvariant()
            if($actualMd5 -ne $entry.expectedMd5){throw ('源图MD5与官方值不一致：'+$entry.label)}
            Move-Item -LiteralPath $tmp -Destination $path -ErrorAction Stop
        }catch{
            if(Test-Path $tmp){Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue}
            throw
        }
    }
    $file=Get-Item -LiteralPath $path
    $md5=(Get-FileHash -LiteralPath $path -Algorithm MD5).Hash.ToLowerInvariant()
    $sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if($file.Length -ne $entry.expectedBytes -or $md5 -ne $entry.expectedMd5){
        throw ('源图校验不合格或内容改变：'+$entry.label)
    }
    Write-Host ('ROCK_REALISM_SOURCE_VALIDATED '+$entry.label+' bytes='+$file.Length+' sha256='+$sha256)
    $resolved+=@{
        role=$entry.label; upstreamMap=$entry.map; sourceUrl=$entry.sourceUrl
        localPath=($path.Replace((Get-Location).Path+'\','')).Replace('\','/')
        expectedBytes=$entry.expectedBytes
        upstreamMd5=$entry.expectedMd5
        sha256=$sha256
    }
}
$doc=[ordered]@{
    schemaVersion=1
    kind='third_party_pbr_scan_candidate'
    sourceId=$assetId
    sourceResolution=$Resolution
    assetUrl='https://polyhaven.com/a/rock_face_04'
    sourceFilesApi=$api
    license='CC0 1.0'
    licenseUrl='https://polyhaven.com/license'
    creator='Dimitrios Savva'
    referenceWidthMeters=2.0
    physicallyMatchedMaps=$true
    productionAssetApproved=$false
    noteZh='神兽联盟项目世界层岩石贴图第一版，美术候选待人工审核，不作为已签收正式地形资源'
    maps=$resolved
}
$payload=$doc|ConvertTo-Json -Depth 12
if(Test-Path $manifest){
    $a=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8|ConvertFrom-Json
    foreach($item in $resolved){
        $known=@($a.maps|Where-Object {$_.role -eq $item.role})
        if($known.Count -ne 1 -or $known[0].sha256 -ne $item.sha256){
            throw '已有清单和真实文件指纹不一致，禁止覆写'
        }
    }
}else{
    if($VerifyOnly){throw '只读模式清单尚不存在'}
    [IO.File]::WriteAllText($manifest,$payload+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
    Write-Host 'ROCK_REALISM_MANIFEST_CREATED'
}
Write-Host ('ROCK_REALISM_PBR_4MAP_SOURCE_PASS mapCount=4 license=CC0 resolution='+$Resolution)
