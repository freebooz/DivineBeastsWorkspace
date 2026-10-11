#requires -Version 5.1
<#
用途：在用户把五组高清参考ZIP复制到Incoming目录以后，安全导入真实原始PNG和分离精灵。
不会重命名PNG为.uasset，不会修改已存在Unreal资源，最终材质替换由UE5.8专用脚本执行。
#>
[CmdletBinding()]
param(
    [string]$ZipPath,
    [switch]$Apply
)
$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$source=Join-Path $workspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature'
$incoming=Join-Path $source 'Incoming'
$target=Join-Path $source 'ExternalReferences'
if(!$ZipPath){$ZipPath=Join-Path $incoming 'DivineBeasts_AmbientReferencePack_20261011.zip'}
if(!(Test-Path $ZipPath -PathType Leaf)){throw "缺少真正的原图ZIP文件，请先将用户下载的ZIP复制到：$ZipPath"}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage=Join-Path $workspace 'Saved/Validation/FallingFoliage/ReferenceArtUploadStaging'
if(Test-Path $stage){throw '检测到历史暂存目录，拒绝清理或覆盖；请先人工检查：'+$stage}
New-Item -ItemType Directory -Path $stage -Force|Out-Null
try{
    $archive=[IO.Compression.ZipFile]::OpenRead((Resolve-Path $ZipPath).Path)
    try{
        foreach($entry in $archive.Entries){
            $name=$entry.FullName.Replace('\','/')
            if([IO.Path]::IsPathRooted($name) -or $name.Contains('../') -or $name.StartsWith('/') -or $name.Contains(':')){
                throw "ZIP中出现不允许的目录穿越路径：$name"
            }
        }
        [IO.Compression.ZipFileExtensions]::ExtractToDirectory($archive,$stage)
    }finally{$archive.Dispose()}
    $manifestFile=Join-Path $stage 'SourceManifest.json'
    if(!(Test-Path $manifestFile)){throw 'ZIP内容缺少SourceManifest.json来源清单'}
    $manifest=Get-Content $manifestFile -Raw -Encoding utf8 |ConvertFrom-Json
    $expected=@('PearPetal','CherryPetal','Ginkgo','PlaneLeaf','Maple')
    if(@($manifest.groups).Count -ne 5){throw '原图文件清单不是预期的5组'}
    $verified=0
    $cropped=0
    foreach($style in $expected){
        $item=@($manifest.groups | Where-Object {$_.styleId -eq $style})
        if($item.Count -ne 1){throw "原图组不存在或重复：$style"}
        $original=Join-Path $stage ("OriginalAtlases/$($style)_Original.png")
        if(!(Test-Path $original)){throw "原始PNG缺失：$style"}
        $digest=(Get-FileHash $original -Algorithm SHA256).Hash.ToLowerInvariant()
        if($digest -ne ([string]$item[0].originalSha256).ToLowerInvariant()){throw "原始PNG哈希错误：$style"}
        $verified++
        foreach($sprite in $item[0].cropAssets){
            if($sprite.file -notmatch '^DerivedSprites/[A-Za-z]+/T_DBA_[A-Za-z]+_Variant[0-9]+\.png$'){
                throw "分离精灵路径不合法：$($sprite.file)"
            }
            $file=Join-Path $stage $sprite.file
            if(!(Test-Path $file)){throw "分离精灵文件缺失：$($sprite.file)"}
            $hash=(Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
            if($hash -ne ([string]$sprite.sha256).ToLowerInvariant()){throw "精灵SHA-256错误：$($sprite.file)"}
            $cropped++
        }
    }
    Write-Output ("REFERENCE_ART_VERIFIED originals=$verified sprites=$cropped source=$ZipPath")
    if(!$Apply){
        Write-Output "验证通过但未导入；加入-Apply才会把精灵原件复制到ExternalReferences。"
    }else{
        if(Test-Path (Join-Path $target 'SourceManifest.json')){
            throw '目标ExternalReferences中已有来源清单，拒绝覆盖：'+$target
        }
        if(!(Test-Path $target)){New-Item -Path $target -ItemType Directory -Force|Out-Null}
        foreach($item in (Get-ChildItem $stage -Force)){
            $destination=Join-Path $target $item.Name
            if(Test-Path $destination){throw '拒绝覆盖目标资源：'+$destination}
            Move-Item -LiteralPath $item.FullName -Destination $destination
        }
        Write-Output ('REFERENCE_ART_STAGED_TO_PROJECT '+$target)
        Write-Output '下一步必须用UE5.8官方编辑器执行ImportApprovedAmbientReferenceArtUE58.py，经备份才会调整材质实例。'
    }
}finally{
    if(Test-Path $stage){Remove-Item -LiteralPath $stage -Force -Recurse -ErrorAction SilentlyContinue}
}
