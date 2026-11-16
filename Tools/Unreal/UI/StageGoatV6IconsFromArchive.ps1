#requires -Version 7
<#
.SYNOPSIS
    SSLM 未羊 V6 图标源包安全接收工具（不创建虚幻资产）。
.DESCRIPTION
    本脚本属于第三层英雄内容包的编辑器外围交付工具。
    inspect只审查已冻结的身份、压缩包内容、尺寸与SHA-256。
    stage在完整通过预检后复制八套1254母版、64审核图和48运行图，
    并为既有Monolith工具写IntegrationPlan和ProtectedFiles，不修改GAS。
    未经Monolith操作不会生成Texture2D、UIProfile或HUD。
#>
[CmdletBinding()]
param(
    [ValidateSet('inspect','stage')][string]$Mode='inspect',
    [string]$ArchivePath='artifacts/imports/SSLM_GoatV6_EightSkillIconAssets_20261019.zip'
)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$Pack='Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat'
$FreezePath=Join-Path $Root "$Pack/Docs/GoatV6_ArtFreeze_20261020.json"
$Evidence=Join-Path $Root 'Saved/Validation/GoatV6IconImport20261020'
$Freeze=Get-Content -LiteralPath $FreezePath -Raw -Encoding UTF8 | ConvertFrom-Json
$Slots=@('Primary','Slot1','Slot2','Slot4','Slot3','Slot5','Slot6','Slot7')
function Assert-True([bool]$Ok,[string]$Message){
    if(-not $Ok){ throw "SSLM_GOAT_V6_GATE_FAILED: $Message" }
}
function Sha-File([string]$Path){
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Png-Size([IO.Stream]$Stream){
    $head=New-Object byte[] 24
    $n=$Stream.Read($head,0,24)
    Assert-True ($n -eq 24 -and [Convert]::ToHexString($head[0..7]) -eq '89504E470D0A1A0A') 'PNG签名错误'
    $w=($head[16] -shl 24) -bor ($head[17] -shl 16) -bor ($head[18] -shl 8) -bor $head[19]
    $h=($head[20] -shl 24) -bor ($head[21] -shl 16) -bor ($head[22] -shl 8) -bor $head[23]
    return @([int]$w,[int]$h)
}
Assert-True ($Freeze.heroId -eq 'Hero.Zodiac.Goat' -and $Freeze.skills.Count -eq 8) '英雄冻结合同错误'
Assert-True ((@($Freeze.skills | ForEach-Object {$_.slotId}) -join ',') -eq ($Slots -join ',')) '技能槽序不符合八槽合同'
Assert-True ((@($Freeze.skills.abilityId | Select-Object -Unique)).Count -eq 8) '稳定技能ID重复'
$Authority=Get-Content -LiteralPath (Join-Path $Root 'Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$Goat=@($Authority.heroes | Where-Object {$_.heroId -eq 'Hero.Zodiac.Goat'})
Assert-True ($Goat.Count -eq 1) 'V6权威技能数据未找到唯一羊'
for($i=0;$i -lt 8;$i++){
    $x=$Freeze.skills[$i]; $a=$Goat[0].skills[$i]
    Assert-True ($x.abilityId -eq $a.abilityId -and $x.v6DisplayNameZh -eq $a.proposedNameZh -and $x.productionV5NameZh -eq $a.currentApprovedV5NameZh) "第$($i+1)项与仓库V6/V5身份不符"
}
$ArchiveFull=if([IO.Path]::IsPathRooted($ArchivePath)){$ArchivePath}else{Join-Path $Root $ArchivePath}
if(-not (Test-Path -LiteralPath $ArchiveFull -PathType Leaf)){
    if($Mode -eq 'stage'){throw "未羊V6原始ZIP未传入Windows工作区：$ArchiveFull"}
    [pscustomobject]@{heroId='Hero.Zodiac.Goat';frozenSkills=8;archive='MISSING';status='AWAITING_SOURCE_ARCHIVE';texture2DImported=$false;hudVerified=$false}|ConvertTo-Json -Compress
    return
}
Add-Type -AssemblyName System.IO.Compression
$Zip=[IO.Compression.ZipFile]::OpenRead($ArchiveFull)
try {
    $Lookup=@{}
    foreach($item in $Zip.Entries){
        if(-not $item.Name){continue}
        Assert-True ($item.FullName.StartsWith("$Pack/",[StringComparison]::Ordinal) -and -not $item.FullName.Contains('..') -and -not $item.FullName.Contains('\')) "压缩包路径越界：$($item.FullName)"
        Assert-True (-not $Lookup.ContainsKey($item.FullName)) "压缩包重复文件：$($item.FullName)"
        $Lookup[$item.FullName]=$item
    }
    $Required=[System.Collections.Generic.List[object]]::new()
    foreach($skill in $Freeze.skills){
        foreach($spec in @(
            @{name=$skill.sourceFile;size=1254;sha=$skill.sha256.source},
            @{name=$skill.png64;size=64;sha=$skill.sha256.png64},
            @{name=$skill.png48;size=48;sha=$skill.sha256.png48}
        )){
            Assert-True ($Lookup.ContainsKey($spec.name)) "压缩包缺少正式图源：$($spec.name)"
            $entry=$Lookup[$spec.name]
            $stream=$entry.Open()
            try{
                $shape=Png-Size $stream
                Assert-True ($shape[0] -eq $spec.size -and $shape[1] -eq $spec.size) "PNG真实像素尺寸不符：$($spec.name)"
            }finally{$stream.Dispose()}
            $stream=$entry.Open()
            try{$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant()}
            finally{$stream.Dispose()}
            Assert-True ($hash -eq $spec.sha) "源文件SHA-256不符：$($spec.name)"
            $dest=Join-Path $Root $spec.name
            if(Test-Path -LiteralPath $dest -PathType Leaf){
                Assert-True ((Sha-File $dest) -eq $hash) "已有工程文件与冻结源不同，拒绝覆盖：$($spec.name)"
            }
            $Required.Add([pscustomobject]@{entry=$entry;relative=$spec.name;sha=$hash;dest=$dest})
        }
    }
    if($Mode -eq 'inspect'){
        [pscustomobject]@{status='ARCHIVE_VERIFIED';skillCount=8;pngCount=$Required.Count;missingInProject=@($Required|Where-Object {-not (Test-Path -LiteralPath $_.dest)}).Count;ueImported=$false;hudVerified=$false}|ConvertTo-Json -Compress
        return
    }
    # 只有全部24张图在压缩包中完成预检，才允许增量复制；已有相同SHA时保持原文件不变。
    foreach($file in $Required){
        if(Test-Path -LiteralPath $file.dest){continue}
        $dir=Split-Path -Parent $file.dest
        [IO.Directory]::CreateDirectory($dir)|Out-Null
        $inputStream=$file.entry.Open()
        try{
            $outputStream=[IO.File]::Open($file.dest,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try{$inputStream.CopyTo($outputStream)}finally{$outputStream.Dispose()}
        }finally{$inputStream.Dispose()}
        Assert-True ((Sha-File $file.dest) -eq $file.sha) "复制后SHA-256校验失败：$($file.relative)"
    }
    # 正式Profile显示仍沿用V5已批准名称，V6名称只在美术评审清单中展示。
    $ProfilePath='/DBAHeroPack_Goat/UI/Abilities/DA_DBA_Goat_PlaceholderUIProfile'
    $ProfileFile="$Pack/Content/UI/Abilities/DA_DBA_Goat_PlaceholderUIProfile.uasset"
    $Rows=@($Freeze.skills | ForEach-Object {
        [ordered]@{order=$_.displayOrder;slotId=$_.slotId;abilityId=$_.abilityId;
            displayNameZh=$_.productionV5NameZh;v6ArtReviewNameZh=$_.v6DisplayNameZh;
            descriptionZh=('【V6仅美术预览，玩法待审】'+$_.descriptionZh);
            texturePath=$_.texturePath;png48=$_.png48;png64=$_.png64;
            sha48=$_.sha256.png48;sha64=$_.sha256.png64}
    })
    [IO.Directory]::CreateDirectory($Evidence)|Out-Null
    $Plan=[ordered]@{version=1;mode='UNAPPROVED_READ_ONLY_ART';heroes=@([ordered]@{
        hero='Goat';heroId='Hero.Zodiac.Goat';profilePath=$ProfilePath;profileFile=$ProfileFile;entries=$Rows
    })}
    $Plan|ConvertTo-Json -Depth 20|Set-Content -LiteralPath (Join-Path $Evidence 'IntegrationPlan.json') -Encoding UTF8
    # 保护V1纹理不变；唯一Profile允许通用工具在备份后增补只读条目，
    # 因此不能把允许变化的Profile文件加入操作前后都固定SHA的ProtectedFiles。
    Assert-True (Test-Path -LiteralPath (Join-Path $Root $ProfileFile) -PathType Leaf) '唯一UIProfile缺失'
    $Protected=[ordered]@{}
    $ProtectedFiles=@(Get-ChildItem -LiteralPath (Join-Path $Root "$Pack/Content/UI/Abilities") -File -Filter 'T_DBA_Goat_*_V1.uasset'|ForEach-Object{[IO.Path]::GetRelativePath($Root,$_.FullName).Replace('\','/')})
    foreach($rel in $ProtectedFiles){$abs=Join-Path $Root $rel;Assert-True (Test-Path $abs) "原有资产缺失：$rel";$Protected[$rel]=Sha-File $abs}
    $Protected|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $Evidence 'ProtectedFiles.json') -Encoding UTF8
    [pscustomobject]@{status='ART_SOURCES_STAGED';pngCount=$Required.Count;profilePlan='Saved/Validation/GoatV6IconImport20261020/IntegrationPlan.json';ueImported=$false;profileModified=$false;hudVerified=$false}|ConvertTo-Json -Compress
}finally{$Zip.Dispose()}
