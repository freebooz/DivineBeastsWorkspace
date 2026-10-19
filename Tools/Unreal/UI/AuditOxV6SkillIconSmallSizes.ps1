#requires -Version 7
<#
SSLM丑牛V6 64×64审核和48×48运行时图标质量门禁。
依据英雄内容包内16张已存在PNG及真实SHA制作1:1对照图，
测量对比度、动态范围、重复哈希与dHash近似度。
仅允许客观技术检查：不替代美术审核，不改纹理源、Gameplay/Cook或GAS授权。
#>
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$hero=Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/SourceArt/UI/SkillIcons/V6'
$manifestPath=Join-Path $hero 'OxV6_64_48_SourceManifest.json'
$source=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8|ConvertFrom-Json
if($source.heroId -ne 'Hero.Zodiac.Ox' -or @($source.entries).Count -ne 8){throw '丑牛八图标来源清单结构错误'}

function SHA256([string]$path){return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Sample-Icon([string]$path,[int]$expected,[string]$storedHash){
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "PNG缺失：$path"}
    $sha=SHA256 $path
    if($sha -ne $storedHash){throw "PNG SHA256发生变化：$path"}
    $bitmap=[Drawing.Bitmap]::new($path)
    try{
        if($bitmap.Width -ne $expected -or $bitmap.Height -ne $expected){throw "PNG尺寸不正确：$path"}
        $all=@();$badAlpha=0;$white=0;$black=0
        for($y=0;$y -lt $expected;$y++){
           for($x=0;$x -lt $expected;$x++){
               $px=$bitmap.GetPixel($x,$y)
               $luma=0.2126*$px.R + 0.7152*$px.G + 0.0722*$px.B
               $all+= $luma
               if($px.A -ne 255){$badAlpha++}
               if($luma -gt 245){$white++}
               if($luma -lt 20){$black++}
           }
        }
        $num=$all.Count
        $mean=($all|Measure-Object -Average).Average
        $sum=0
        foreach($val in $all){$sum+= [Math]::Pow($val-$mean,2)}
        $std=[Math]::Sqrt($sum/$num)
        # dHash标准9×8缩图，从左到右相邻灰度比较；只用于提示外观近似，不是美术审定。
        $small=[Drawing.Bitmap]::new(9,8)
        $g=[Drawing.Graphics]::FromImage($small)
        try{
          $g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
          $g.DrawImage($bitmap,0,0,9,8)
        }finally{$g.Dispose()}
        $hash=[Collections.Generic.List[int]]::new()
        try{
            for($y=0;$y -lt 8;$y++){
                for($x=0;$x -lt 8;$x++){
                    $a=$small.GetPixel($x,$y);$b=$small.GetPixel($x+1,$y)
                    $la=0.2126*$a.R+0.7152*$a.G+0.0722*$a.B
                    $lb=0.2126*$b.R+0.7152*$b.G+0.0722*$b.B
                    $hash.Add([int]($la -gt $lb))
                }
            }
        }finally{$small.Dispose()}
        return [pscustomobject]@{
            sha256=$sha;size=$expected;meanLuma=[Math]::Round($mean,2);
            lumaStdDev=[Math]::Round($std,2);clippedBrightPct=[Math]::Round(100.0*$white/$num,2);
            deepDarkPct=[Math]::Round(100.0*$black/$num,2);
            nonOpaquePixels=$badAlpha; perceptualBits=@($hash.ToArray())
        }
    }finally{$bitmap.Dispose()}
}

$reportRows=@()
$unique64=[Collections.Generic.HashSet[string]]::new()
$unique48=[Collections.Generic.HashSet[string]]::new()
foreach($row in $source.entries){
    $f64=Join-Path $hero $row.review64
    $f48=Join-Path $hero $row.runtime48
    $a=Sample-Icon $f64 64 $row.sha256Review64
    $b=Sample-Icon $f48 48 $row.sha256Runtime48
    if(!$unique64.Add($a.sha256) -or !$unique48.Add($b.sha256)){throw "不同技能图标PNG字节完全重复：$($row.nameZh)"}
    if($a.lumaStdDev -lt 14 -or $b.lumaStdDev -lt 14){
       throw "缩略图动态范围过低，容易产生一片灰：$($row.nameZh)"
    }
    if($a.deepDarkPct -gt 92 -or $b.deepDarkPct -gt 92){
       throw "缩略图过暗，主体不可见：$($row.nameZh)"
    }
    if($a.clippedBrightPct -gt 55 -or $b.clippedBrightPct -gt 55){
       throw "缩略图高光严重过曝：$($row.nameZh)"
    }
    $reportRows+= [pscustomobject]@{
        order=$row.order;nameZh=$row.nameZh;abilityId=$row.abilityId;slotId=$row.slot;
        review64=$row.review64;runtime48=$row.runtime48;sourceKind=$source.sourceKind;
        metrics64=$a;metrics48=$b;humanSemanticReview='PENDING'
    }
    Write-Output ("SSLM_OX_V6_SMALL_SIZE_TECH_PASS "+$row.order+" "+$row.nameZh+
      " std64="+$a.lumaStdDev+" std48="+$b.lumaStdDev+
      " bright48="+$b.clippedBrightPct+" dark48="+$b.deepDarkPct)
}
$near=@()
for($i=0;$i -lt $reportRows.Count;$i++){
    for($j=$i+1;$j -lt $reportRows.Count;$j++){
        $a=$reportRows[$i].metrics48.perceptualBits
        $b=$reportRows[$j].metrics48.perceptualBits
        $distance=0
        for($k=0;$k -lt 64;$k++){if($a[$k] -ne $b[$k]){$distance++}}
        if($distance -le 12){
            $near+= [pscustomobject]@{left=$reportRows[$i].nameZh;right=$reportRows[$j].nameZh;
                hashDistance=$distance; status='HUMAN_VISUAL_DISTINCTNESS_REVIEW_REQUIRED'}
            Write-Output ('SSLM_OX_V6_NEAR_ICON_RISK '+$reportRows[$i].nameZh+' vs '+$reportRows[$j].nameZh+' dHash='+$distance)
        }
    }
}

# 一张实际像素大小的编辑对照图（不放大两套图标冒充清晰度）；视觉冲突仍由人工审查。
$sheetW=1060
$sheetH=244
$sheet=[Drawing.Bitmap]::new($sheetW,$sheetH,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$canvas=[Drawing.Graphics]::FromImage($sheet)
$font=[Drawing.Font]::new('Microsoft YaHei',11,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$fontSmall=[Drawing.Font]::new('Microsoft YaHei',10,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$white=[Drawing.Brushes]::White
$light=[Drawing.Brushes]::LightGray
try{
    $canvas.Clear([Drawing.Color]::FromArgb(16,23,36))
    $canvas.DrawString('SSLM  丑牛 V6  ·  64×64 / 48×48 原尺寸对照 · 艺术审核待确认',$font,$white,20,9)
    for($i=0;$i -lt 8;$i++){
        $row=$reportRows[$i]
        $x=125+$i*115
        $canvas.DrawString($row.nameZh,$fontSmall,$white,$x-5,41)
        $path64=Join-Path $hero $row.review64
        $path48=Join-Path $hero $row.runtime48
        $image64=[Drawing.Image]::FromFile($path64)
        $image48=[Drawing.Image]::FromFile($path48)
        try{
            $canvas.DrawImageUnscaled($image64,$x,62)
            $canvas.DrawImageUnscaled($image48,$x+8,149)
        }finally{$image64.Dispose();$image48.Dispose()}
    }
    $canvas.DrawString('审核 64×64',$font,$light,16,80)
    $canvas.DrawString('游戏 48×48',$font,$light,16,166)
    $canvas.DrawString('每项对应唯一AbilityId；小图尺寸/哈希/对比度检查并不替代技能含义与美术风格人工验收。',$fontSmall,$light,24,225)
}finally{$fontSmall.Dispose();$font.Dispose();$canvas.Dispose()}
$sheetPath=Join-Path $hero 'OxV6_64_48_ContactSheet.png'
if(Test-Path -LiteralPath $sheetPath){throw '存在旧对照图，拒绝覆盖'}
$sheet.Save($sheetPath,[Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()

# 精简JSON，避免把位图Hash的64bit数组重复塞入交付文件。
$rowsOut=@($reportRows|ForEach-Object{
    [ordered]@{
        order=$_.order;nameZh=$_.nameZh;abilityId=$_.abilityId;slotId=$_.slotId;
        png64=$_.review64;png48=$_.runtime48;
        metrics64=[ordered]@{lumaStdDev=$_.metrics64.lumaStdDev;
            brightPct=$_.metrics64.clippedBrightPct;darkPct=$_.metrics64.deepDarkPct;
            sha256=$_.metrics64.sha256};
        metrics48=[ordered]@{lumaStdDev=$_.metrics48.lumaStdDev;
            brightPct=$_.metrics48.clippedBrightPct;darkPct=$_.metrics48.deepDarkPct;
            sha256=$_.metrics48.sha256}
    }
})
$report=[ordered]@{
    hero='Hero.Zodiac.Ox';technicalSizeGate='PASS';technicalDuplicateByteGate='PASS';
    review64Count=8;runtime48Count=8;artisticApproval='PENDING';
    sourceKind=$source.sourceKind;unique64=$unique64.Count;unique48=$unique48.Count;
    nearAppearanceRisks=$near;rows=$rowsOut;
    humanReviewNoticeZh='基于已有5张Ox原型美术衍生8个图标；技术尺寸/清晰度门禁通过并不证明8技能语义准确，尤其不动玄门、沉步、立阵必须人工重点审核。'
}
$reportPath=Join-Path $hero 'OxV6_SmallSizeTechnicalReview.json'
if(Test-Path -LiteralPath $reportPath){throw '存在旧技术审核报告，拒绝覆盖'}
[IO.File]::WriteAllText($reportPath,($report|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
Write-Output ("SSLM_OX_V6_SMALL_SIZE_TECHNICAL_QA_PASSED 8 64 48 nearPairs="+$near.Count+" contact="+$sheetPath)
