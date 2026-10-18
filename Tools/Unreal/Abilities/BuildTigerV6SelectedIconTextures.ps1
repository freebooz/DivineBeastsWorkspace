#requires -Version 7.0
<#
.SYNOPSIS
  寅虎 V6 选中构图源图的高质量 UI 小纹理制作脚本。
.DESCRIPTION
  只在 SourceArt（源艺术资源）生产高清母版、独立64及48像素PNG；
  绝不创建或假冒UE Texture2D/.uasset、写入生产Ability UI Profile或GAS。
  人工优选的构图方向不等同图片成品签收；真实Monolith导入必须独立验收。
.PARAMETER VerifyOnly
  仅检查已存在的源图与两种尺寸，不进行写入。
#>
[CmdletBinding()]
param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$docRoot=Join-Path $repo 'Docs/Implementation/GamePlatformAbilitySystem'
$manifest=Get-Content (Join-Path $docRoot 'TigerV6SkillIconImportManifest_Draft.json') -Raw -Encoding utf8 | ConvertFrom-Json
$candidates=Get-Content (Join-Path $docRoot 'TigerV6IconCandidates_24_20261018.json') -Raw -Encoding utf8 | ConvertFrom-Json
$selections=Get-Content (Join-Path $docRoot 'TigerV6IconSelections_20261018.json') -Raw -Encoding utf8 | ConvertFrom-Json
if(@($manifest.entries).Count -ne 8 -or @($selections.entries).Count -ne 8) {
    throw '寅虎必须有8个稳定技能/8个选择记录。'
}
Add-Type -AssemblyName System.Drawing.Common

# 线性光空间Lanczos3重采样，避免直接对sRGB通道计算均值造成边缘黑化。
# RGB先转线性并与Alpha预乘，横向/纵向独立低通，再复原sRGB与Alpha。
# 本函数仅处理静态UI图源，严禁作为运行时图像处理代码。
$code=@'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class TigerSkillIconResampler
{
    static double Clamp(double value) { return value < 0 ? 0 : value > 1 ? 1 : value; }
    static double ToLinear(double value)
    {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 : Math.Pow((value + 0.055) / 1.055, 2.4);
    }
    static byte ToSrgb(double value)
    {
        value = Clamp(value);
        double gamma = value <= 0.0031308 ? value * 12.92 : 1.055 * Math.Pow(value, 1.0 / 2.4) - 0.055;
        return (byte)Math.Max(0, Math.Min(255, Math.Round(gamma * 255.0)));
    }
    static byte ToAlpha(double value) { return (byte)Math.Max(0, Math.Min(255, Math.Round(Clamp(value) * 255.0))); }
    static double Lanczos3(double x)
    {
        x = Math.Abs(x);
        if (x < 0.0000001) return 1.0;
        if (x >= 3.0) return 0.0;
        double px = Math.PI * x;
        return (Math.Sin(px) / px) * (Math.Sin(px / 3.0) / (px / 3.0));
    }

    // 输入为实际PNG位图，输出为独立的指定整数尺寸；不改变已有输入图。
    public static void Resample(string masterFile, string targetFile, int size)
    {
        using (Bitmap input = new Bitmap(masterFile))
        using (Bitmap bitmap = new Bitmap(input.Width, input.Height, PixelFormat.Format32bppArgb))
        {
            using (Graphics g = Graphics.FromImage(bitmap)) g.DrawImageUnscaled(input, 0, 0);
            int w = bitmap.Width, h = bitmap.Height;
            if (w != h || w < 1024 || (size != 48 && size != 64))
                throw new InvalidOperationException("源图分辨率或目标尺寸不符合游戏规范。");

            Rectangle rect = new Rectangle(0, 0, w, h);
            BitmapData block = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] bgra;
            try
            {
                if (block.Stride <= 0) throw new InvalidOperationException("当前图片缓冲区方向不受支持。");
                bgra = new byte[block.Stride * h];
                Marshal.Copy(block.Scan0, bgra, 0, bgra.Length);
            }
            finally { bitmap.UnlockBits(block); }

            double[] original = new double[w * h * 4];
            for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                int src = y * w * 4 + x * 4;
                int srcBytes = y * w * 4 + x * 4;
                double a = bgra[srcBytes + 3] / 255.0;
                original[src] = ToLinear(bgra[srcBytes + 2]) * a;
                original[src + 1] = ToLinear(bgra[srcBytes + 1]) * a;
                original[src + 2] = ToLinear(bgra[srcBytes]) * a;
                original[src + 3] = a;
            }

            double[] horizontal = new double[size * h * 4];
            double sx = (double)w / size;
            for (int y = 0; y < h; y++)
            for (int nx = 0; nx < size; nx++)
            {
                double center = (nx + .5) * sx - .5;
                int left = (int)Math.Ceiling(center - 3.0 * sx);
                int right = (int)Math.Floor(center + 3.0 * sx);
                double sum = 0.0;
                int resultIndex = (y * size + nx) * 4;
                for (int ix = left; ix <= right; ix++)
                {
                    double weight = Lanczos3((ix - center) / sx);
                    if (weight == 0) continue;
                    int clamped = Math.Max(0, Math.Min(w - 1, ix));
                    int inputIndex = (y * w + clamped) * 4;
                    for (int c = 0; c < 4; c++) horizontal[resultIndex + c] += original[inputIndex + c] * weight;
                    sum += weight;
                }
                for (int c = 0; c < 4; c++) horizontal[resultIndex + c] /= sum;
            }
            double[] vertical = new double[size * size * 4];
            double sy = (double)h / size;
            for (int ny = 0; ny < size; ny++)
            for (int x = 0; x < size; x++)
            {
                double center = (ny + .5) * sy - .5;
                int top = (int)Math.Ceiling(center - 3.0 * sy);
                int bottom = (int)Math.Floor(center + 3.0 * sy);
                double sum = 0.0;
                int resultIndex = (ny * size + x) * 4;
                for (int iy = top; iy <= bottom; iy++)
                {
                    double weight = Lanczos3((iy - center) / sy);
                    if (weight == 0) continue;
                    int clamped = Math.Max(0, Math.Min(h - 1, iy));
                    int inputIndex = (clamped * size + x) * 4;
                    for (int c = 0; c < 4; c++) vertical[resultIndex + c] += horizontal[inputIndex + c] * weight;
                    sum += weight;
                }
                for (int c = 0; c < 4; c++) vertical[resultIndex + c] /= sum;
            }

            using (Bitmap output = new Bitmap(size, size, PixelFormat.Format32bppArgb))
            {
                Rectangle targetRect = new Rectangle(0, 0, size, size);
                BitmapData target = output.LockBits(targetRect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                try
                {
                    byte[] buffer = new byte[target.Stride * size];
                    for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++)
                    {
                        int idx = (y * size + x) * 4;
                        int ofs = y * target.Stride + x * 4;
                        double a = Clamp(vertical[idx + 3]);
                        double invA = a > 0.0001 ? 1.0 / a : 0;
                        buffer[ofs] = ToSrgb(vertical[idx + 2] * invA);
                        buffer[ofs + 1] = ToSrgb(vertical[idx + 1] * invA);
                        buffer[ofs + 2] = ToSrgb(vertical[idx] * invA);
                        buffer[ofs + 3] = ToAlpha(a);
                    }
                    Marshal.Copy(buffer, 0, target.Scan0, buffer.Length);
                }
                finally { output.UnlockBits(target); }
                output.Save(targetFile, ImageFormat.Png);
            }
        }
    }
    // 只用于少数自动生成图中意外烘入的装饰边框；Source原稿留在Candidates目录。
    // S05（扑杀）从1024图四边各裁去约68px，避免将Gold UI框烘入48图标。
    public static void CropBorderToMaster(string inputFile, string masterFile)
    {
        using (Bitmap source = new Bitmap(inputFile))
        using (Bitmap master = new Bitmap(1024, 1024, PixelFormat.Format32bppArgb))
        {
            if (source.Width != 1024 || source.Height != 1024)
                throw new InvalidOperationException("待修正源图尺寸不是1024×1024。");
            using (Graphics g = Graphics.FromImage(master))
            {
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                g.CompositingQuality = System.Drawing.Drawing2D.CompositingQuality.HighQuality;
                g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
                g.DrawImage(source, new Rectangle(0, 0, 1024, 1024),
                    new Rectangle(68, 68, 888, 888), GraphicsUnit.Pixel);
            }
            master.Save(masterFile, ImageFormat.Png);
        }
    }
}
'@
# PowerShell 7.6+ 的Windows GDI+被拆为三个程序集；显式引用而非依赖某个会话已加载状态。
$drawingRoot=Split-Path ([System.Drawing.Bitmap].Assembly.Location)
$references=@('System.Drawing.Common.dll','System.Private.Windows.GdiPlus.dll','System.Private.Windows.Core.dll','System.Drawing.Primitives.dll','System.Runtime.InteropServices.dll') |
    ForEach-Object {Join-Path $drawingRoot $_}
Add-Type -TypeDefinition $code -ReferencedAssemblies $references -ErrorAction Stop

# 仅根据用户指定的八个构图编码制作，绝不选取其他候选或把候选存在冒充人工精修。
$expected=@('S01-A','S02-B','S03-C','S04-A','S05-A','S06-B','S07-B','S08-B')
$seen=@{}
for($i=0;$i -lt 8;$i++) {
    $wanted=$expected[$i]
    $selection=$selections.entries[$i]
    $entry=$manifest.entries[$i]
    $matching=@($candidates.candidates | Where-Object {$_.candidateId -eq $wanted -and $_.abilityId -eq $entry.abilityId})
    if($matching.Count -ne 1) {throw "无法找到指定的候选图：$wanted"}
    if($selection.selectedCandidateId -ne $wanted) {throw "用户优选尚未精确写入记录：$wanted"}
    $src=Join-Path $repo $matching[0].pngFile
    $master=Join-Path $repo $entry.masterFile
    $png48=Join-Path $repo $entry.runtimePngFile
    $png64=Join-Path $repo ($entry.runtimePngFile.Replace('/48/','/64/'))
    if(-not(Test-Path -LiteralPath $src)) {throw "缺少用户已选候选原图：$wanted"}
    $img=[Drawing.Bitmap]::new($src)
    try {
        if($img.Width -ne 1024 -or $img.Height -ne 1024) {throw "源图非1024方形：$wanted"}
    } finally {$img.Dispose()}
    if(-not $VerifyOnly) {
        foreach($dest in @($master,$png64,$png48)) {
            if(Test-Path -LiteralPath $dest) {throw "拒绝覆盖既有正式/审核纹理：$dest"}
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest))|Out-Null
        }
        if($wanted -eq 'S05-A') {
            [TigerSkillIconResampler]::CropBorderToMaster($src,$master)
        } else {
            [IO.File]::Copy($src,$master)
        }
        [TigerSkillIconResampler]::Resample($master,$png64,64)
        [TigerSkillIconResampler]::Resample($master,$png48,48)
    }
    foreach($check in @(@($master,1024),@($png64,64),@($png48,48))) {
        $target=$check[0]
        if(-not(Test-Path -LiteralPath $target)) {throw "缺少已制作纹理：$target"}
        $bmp=[Drawing.Bitmap]::new($target)
        try {
            if($bmp.Width -ne $check[1] -or $bmp.Height -ne $check[1]) {throw "纹理尺寸错误：$target"}
        } finally {$bmp.Dispose()}
    }
    $hash=(Get-FileHash -LiteralPath $png48 -Algorithm SHA256).Hash.ToLowerInvariant()
    if($seen.ContainsKey($hash)) {throw "8技能48px输出重复：$wanted"}
    $seen[$hash]=$true
    Write-Output ('{0} 1024+64+48 通过 SHA256(48)={1}' -f $wanted,$hash)
}
Write-Output '全部8个选中方向：高清母版/64/48静态完整，仍须人工成品审核、Monolith UE纹理导入与HUD实测。'
