#requires -Version 7
<#
SSLM牛V6图标临时美术包制作器：
使用已在DBAHeroPack_Ox项目中真实存在的5张1254px牛美术母版，制作8技能24个裁切/构图候选和每技能512/256/64/48纹理。
不冒充本会话生成的其他ImageGen图；不称24个变体为24次独立AI创作；输出保持PENDING_HUMAN_APPROVAL。
仅写入 Saved/ArtReview 目录与一个ZIP供独立UE原生导入，不修改正式技能/原Ox UIProfile。
#>
[CmdletBinding()]
param([switch]$Apply)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$manifest=Get-Content -LiteralPath (Join-Path $root 'Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$design=Get-Content -LiteralPath (Join-Path $root 'Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$hero=@($design.heroes|Where-Object heroId -EQ 'Hero.Zodiac.Ox')
if($hero.Count -ne 1 -or @($manifest.items).Count -ne 8){throw '丑牛V6身份不完整'}
$srcDir=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/SourceArt/UI/SkillIcons'
$staging=Join-Path $root 'Game/Saved/ArtReview/OxV6_Source_20261018/WorkingArtFromProject'
$zipPath=Join-Path $root 'Game/Saved/ArtReview/OxV6_Source_20261018/SSLM_Ox_V6_IconPack_Review.zip'
$sourceNames=@(
  'T_DBA_Ox_BasicAttack_HornStrike_V1.png', # 玄角撞：已有近距离顶击母版
  'T_DBA_Ox_Active01_HornCharge_V1.png',     # 裂角：已有金角横撞母版
  'T_DBA_Ox_Active02_EarthPulse_V1.png',     # 裂地踏：已有岩土冲击母版
  'T_DBA_Ox_Ultimate_MountainRupture_V1.png',# 不动玄门：仅做视觉候选，不声称已经画出原生石门
  'T_DBA_Ox_Active01_HornCharge_V1.png',     # 沉步：旧冲撞形象与V6慢速顶盾差异，待重绘
  'T_DBA_Ox_Passive_Stonehide_V1.png',       # 镇山护壁：已有护甲意象
  'T_DBA_Ox_BasicAttack_HornStrike_V1.png',  # 顶回：正面牛角反冲意象
  'T_DBA_Ox_Ultimate_MountainRupture_V1.png' # 立阵：旧地裂视觉与固定盾阵差异，待重绘
)
function Hash([string]$f){return (Get-FileHash -Algorithm SHA256 -LiteralPath $f).Hash.ToLowerInvariant()}
function Draw-Scaled([Drawing.Image]$source,[int]$size,[double]$zoom,[double]$offsetX,[double]$offsetY,[int]$tint,[string]$dest) {
    $bmp=[System.Drawing.Bitmap]::new($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g=[System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.CompositingQuality=[Drawing.Drawing2D.CompositingQuality]::HighQuality
        $g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([Drawing.Color]::FromArgb(255,10,14,21))
        $side=[int][Math]::Floor([Math]::Min($source.Width,$source.Height)/$zoom)
        $midX=$source.Width/2.0+$offsetX
        $midY=$source.Height/2.0+$offsetY
        $left=[int][Math]::Round([Math]::Max(0,[Math]::Min($source.Width-$side,$midX-$side/2.0)))
        $top=[int][Math]::Round([Math]::Max(0,[Math]::Min($source.Height-$side,$midY-$side/2.0)))
        $g.DrawImage($source,[Drawing.Rectangle]::new(0,0,$size,$size),
            [Drawing.Rectangle]::new($left,$top,$side,$side),[Drawing.GraphicsUnit]::Pixel)
        # 对防御/阵地作轻度色彩区分，不添加低分辨率字体或混乱粒子。
        if($tint -eq 1) {
            $brush=[Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(24,48,132,102))
            try{$g.FillRectangle($brush,0,0,$size,$size)}finally{$brush.Dispose()}
        }
        if($tint -eq 2){
            $brush=[Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(20,25,45,80))
            try{$g.FillRectangle($brush,0,0,$size,$size)}finally{$brush.Dispose()}
        }
    }finally{$g.Dispose()}
    $folder=Split-Path $dest -Parent
    if(!(Test-Path $folder)){New-Item -ItemType Directory -Force -Path $folder|Out-Null}
    $bmp.Save($dest,[Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
foreach($row in $manifest.items){
  $draft=@($hero[0].skills |Where-Object abilityId -EQ $row.abilityId)
  if($draft.Count -ne 1 -or $draft[0].proposedNameZh -ne $row.displayNameZh){throw "V6技能身份不匹配：$($row.abilityId)"}
  $idx=[int]$row.order-1
  $full=Join-Path $srcDir $sourceNames[$idx]
  if(!(Test-Path -LiteralPath $full)){throw "项目现存图片缺失："+$full}
}
if(!$Apply){Write-Output 'SSLM_OX_V6_EXISTING_ART_PREFLIGHT_OK 8 sources=5 quality=REVIEW_ONLY';exit 0}
if(Test-Path -LiteralPath $zipPath){throw "输出ZIP已存在，拒绝覆盖："+$zipPath}
if(Test-Path -LiteralPath $staging){throw "临时输出目录已存在，拒绝覆盖："+$staging}
New-Item -ItemType Directory -Force -Path $staging|Out-Null
$records=@()
for($i=0;$i -lt 8;$i++){
 $row=$manifest.items[$i]
 $sourcePath=Join-Path $srcDir $sourceNames[$i]
 $source=[Drawing.Image]::FromFile($sourcePath)
 try{
    if($source.Width -lt 1024 -or $source.Height -lt 1024){throw "原生美术母版不足1024："+$sourcePath}
    $paths=@{}
    foreach($variant in @('A','B','C')){
       $zoom=switch($variant){A{1.00} B{1.08} C{1.16}}
       $sx=switch($variant){A{0} B{-35} C{45}}
       $sy=switch($variant){A{0} B{25} C{-25}}
       if($i -eq 4){$sx=$sx+70;$sy=$sy+20;$zoom=$zoom+0.10} # 沉步慢进构图
       if($i -eq 6){$sx=$sx-80;$sy=$sy+65;$zoom=$zoom+0.12} # 顶回正面反击构图
       $target=Join-Path $staging $row.candidateFiles[([int][char]$variant-[int][char]'A')]
       $candidateTint=if($i -eq 7){1}else{0}
       Draw-Scaled $source 1024 $zoom $sx $sy $candidateTint $target
       $paths[$variant]=$target
    }
    # 仅用于技术审核，A候选不代表人审通过。
    $prefix=Split-Path $row.preview512 -Parent
    $master1024=Join-Path $staging ($prefix+'/'+$row.artFileName+'_Native1024Review.png')
    Copy-Item -LiteralPath $paths['A'] -Destination $master1024
    $tint=if($i -eq 5 -or $i -eq 7){1}elseif($i -eq 3){2}else{0}
    $zoomFinal=if($i -eq 4){1.28}elseif($i -eq 6){1.21}elseif($i -eq 1){1.13}else{1.02}
    $xFinal=if($i -eq 4){70}elseif($i -eq 6){-80}else{0}
    $yFinal=if($i -eq 4){20}elseif($i -eq 6){65}else{0}
    foreach($entry in @(
      @($row.preview512,512),
      @($row.preview256,256),
      @($row.preview64,64),
      @($row.preview48,48)
    )){
       $dest=Join-Path $staging $entry[0]
       Draw-Scaled $source ([int]$entry[1]) $zoomFinal $xFinal $yFinal $tint $dest
    }    $sha256=Hash (Join-Path $staging $row.preview256)
    $sha48=Hash (Join-Path $staging $row.preview48)
    $records+= [ordered]@{
        order=[int]$row.order
        nameZh=$row.displayNameZh
        slot=$row.slotId
        abilityId=$row.abilityId
        assetName=$row.artFileName
        sourceProjectArt=('DBAHeroPack_Ox/SourceArt/UI/SkillIcons/'+$sourceNames[$i])
        selectionStatus='PROVISIONAL_PROJECT_ART_COMPOSITION_HUMAN_APPROVAL_PENDING'
        sourceSha256=Hash $sourcePath
        ui256Sha256=$sha256
        ui48Sha256=$sha48
    }
    Write-Output ('SSLM_OX_V6_LOCAL_ART_PREPARED '+$row.displayNameZh+' master=1024 runtime=48 sha='+$sha48)
 }finally{$source.Dispose()}
}
$meta=[ordered]@{
  schemaVersion=1
  project='SSLM'
  heroId='Hero.Zodiac.Ox'
  canonicalSource='Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json'
  sourceKind='DERIVED_FROM_EXISTING_PROJECT_ART_NOT_HOST_IMAGEGEN'
  humanReviewRequired=$true
  noteZh='本轮远程运行器不能读取当前会话的生成图ZIP；改用项目已有牛5张高清源图衍生临时纹理，原用户生成图尚待真实上传后替换。候选B/C仅为同一母版的构图变化，不能冒充不同AI创作。未获得人审，不可将V6技能授权上线。'
  entries=$records
}
[IO.File]::WriteAllText((Join-Path $staging 'OxV6_IconManifest.json'),
    ($meta|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
[IO.Compression.ZipFile]::CreateFromDirectory($staging,$zipPath,
    [IO.Compression.CompressionLevel]::Optimal,$false)
Write-Output ('SSLM_OX_V6_PROJECT_REVIEW_ZIP_READY 8 candidates=24 path='+$zipPath+' bytes='+(Get-Item $zipPath).Length)
