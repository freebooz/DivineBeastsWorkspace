# 将图像生成母版整理为 UE 移动端运行纹理。
# 数据纹理强制使用灰度、无 Alpha；需要平铺的纹理通过镜像象限保证四边连续。
$ErrorActionPreference = 'Stop'
$GeneratedRoot = $args[0]
if ([string]::IsNullOrWhiteSpace($GeneratedRoot)) {
    throw 'GeneratedRoot is required.'
}
$chapterRoot = $PSScriptRoot
$masterRoot = Join-Path $chapterRoot 'Generated_Source'
$runtimeRoot = Join-Path $chapterRoot 'png'
$importRoot = Join-Path $chapterRoot 'Import_Ready'
New-Item -ItemType Directory -Force -Path $masterRoot, $runtimeRoot, $importRoot | Out-Null

$sourceMap = [ordered]@{
    'T_VFX_Energy_Noise_01' = 'exec-b69457ff-0534-467e-9ca0-682fe0012890.png'
    'T_VFX_Energy_Noise_02' = 'exec-f81f8c7c-e087-4b3b-b6c5-6241feff68f8.png'
    'T_VFX_Energy_CloudNoise_01' = 'exec-782c5f8f-0792-4357-a60d-834e7a295d62.png'
    'T_VFX_Energy_Ring_01' = 'exec-5caaa9c0-1eb6-4ad3-b5c6-41ea663cb046.png'
    'T_VFX_Energy_RingSoft_01' = 'exec-a32c2a0f-32a0-44e9-8a03-e32f28d00e73.png'
    'T_VFX_Energy_Ripple_01_Keyed' = 'exec-669aaf97-a71b-4a2f-92b0-e359aaa61a1f.png'
    'T_VFX_Energy_Gradient_01' = 'exec-0b0aa545-facb-4347-9006-3b49578085dd.png'
    'T_VFX_Common_FresnelBreakup_01' = 'exec-9e11c32c-1ead-48da-93f7-b17aaead0b63.png'
}

foreach ($entry in $sourceMap.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $GeneratedRoot $entry.Value) -Destination (Join-Path $masterRoot ($entry.Key + '.png')) -Force
}

function Write-SeamlessGrayscale {
    param([string]$Name)
    $source = Join-Path $masterRoot ($Name + '.png')
    $target = Join-Path $runtimeRoot ($Name + '.png')
    # Mirror a 256 tile into a 512 image so opposite borders remain continuous.
    $arguments = @(
        $source, '-colorspace', 'Gray', '-resize', '256x256!', '-write', 'mpr:tile', '+delete',
        '(', 'mpr:tile', '-flop', ')', '(', 'mpr:tile', ')', '+append', '-write', 'mpr:top', '+delete',
        '(', 'mpr:top', '-flip', ')', '(', 'mpr:top', ')', '-append', '-depth', '8', '-alpha', 'off', '-type', 'Grayscale', $target
    )
    & magick @arguments
}

foreach ($name in @('T_VFX_Energy_Noise_01', 'T_VFX_Energy_Noise_02', 'T_VFX_Energy_CloudNoise_01', 'T_VFX_Common_FresnelBreakup_01')) {
    Write-SeamlessGrayscale -Name $name
}

foreach ($name in @('T_VFX_Energy_Ring_01', 'T_VFX_Energy_RingSoft_01', 'T_VFX_Energy_Gradient_01')) {
    $source = Join-Path $masterRoot ($Name + '.png')
    $target = Join-Path $runtimeRoot ($Name + '.png')
    # Masks and gradients stay grayscale without meaningless alpha.
    & magick $source -colorspace Gray -resize '512x512!' -depth 8 -alpha off -type Grayscale $target
}

$rippleSource = Join-Path $masterRoot 'T_VFX_Energy_Ripple_01_Keyed.png'
$rippleTarget = Join-Path $runtimeRoot 'T_VFX_Energy_Ripple_01.png'
# Convert magenta key to alpha, then force white RGB to prevent color fringes.
& magick $rippleSource -resize '512x512!' -fuzz '12%' -transparent '#FF00FF' `
    -channel A -blur '0x0.7' -channel RGB -fill white -colorize 100 -channel RGBA -define png:color-type=6 -depth 8 $rippleTarget

foreach ($file in Get-ChildItem -LiteralPath $runtimeRoot -Filter '*.png') {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $importRoot $file.Name) -Force
}

# Print lightweight QA metadata.
Get-ChildItem -LiteralPath $runtimeRoot -Filter '*.png' | Sort-Object Name | ForEach-Object {
    $info = magick identify -format '%f|%wx%h|%[channels]|%[pixel:p{0,0}]|%[pixel:p{511,511}]' $_.FullName
    Write-Output $info
}
