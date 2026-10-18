#requires -Version 5.1
<#
.SYNOPSIS
  SSLM 十层地形V3配色派生源图生成及离线人工审稿板制作。
.DESCRIPTION
  读取V3全局参考色板与正式CC0 PBR清单。仅从原始BaseColor派生10张新纹理；
  不修改原图、Normal、ORM、Height或已保存UE二进制资源，只有UE单独导入后才存在正式Texture2D。
  使用线性光域logit曲线按通道校准色彩均值、保留局部纹理对比和源图细节；
  参考HEX属于屏幕sRGB初始目标，绝非正式UE材质最终屏幕颜色。
  -VerifyOnly仅验证已生成文件的哈希和归属，不改动任何文件。
  输出独立审稿对照图至Saved，正式质量仍需真实UE客户端/人工检查。
#>
[CmdletBinding()]
param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$mappingPath=Join-Path $PSScriptRoot 'TerrainV3ColorMapping.json'
$mapping=Get-Content -LiteralPath $mappingPath -Raw -Encoding UTF8 | ConvertFrom-Json
$palette=Get-Content -LiteralPath (Join-Path $root $mapping.referencePalette) -Raw -Encoding UTF8 | ConvertFrom-Json
$production=Get-Content -LiteralPath (Join-Path $root $mapping.baselinePBRManifest) -Raw -Encoding UTF8 | ConvertFrom-Json
if($mapping.schemaVersion -ne 1 -or $mapping.layers.Count -ne 10){throw 'V3地形派生合同必须恰好包含10层'}
if($palette.items.Count -ne 41){throw 'V3色板数量异常，拒绝派生'}
if($production.files.Count -ne 40){throw '正式PBR来源合同不完整'}
$review=Join-Path $root 'Saved/ArtReview/TerrainColorV3'
$reportPath=Join-Path $review 'GenerationManifest.json'
$sourceRoot=Join-Path $root $mapping.outputSourceRoot
$written=@()
$sourceCount=0

if($VerifyOnly){
 if(!(Test-Path -LiteralPath $reportPath)){throw "还没有派生清单：$reportPath"}
 $previous=Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
 if($previous.layers.Count -ne 10){throw '已存V3清单不完整'}
 $palHash=(Get-FileHash (Join-Path $root $mapping.referencePalette) -Algorithm SHA256).Hash.ToLowerInvariant()
 if($previous.paletteSha256 -ne $palHash){throw '权威色板已变更，禁止使用旧派生'}
 foreach($row in $previous.layers){
   $src=Join-Path $root $row.originalSourcePath
   $derived=Join-Path $root $row.derivedSourcePath
   if(!(Test-Path -LiteralPath $src) -or !(Test-Path -LiteralPath $derived)){throw "缺少V3图：$($row.layer)"}
   if((Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.originalSha256){throw "原始来源被替换：$src"}
   if((Get-FileHash $derived -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.derivedSha256){throw "派生文件已变化：$derived"}
 }
 Write-Output 'SSLM_TERRAIN_COLOR_V3_SOURCE_VERIFY_PASS count=10 original_untouched=true color_policy_valid=true'
 return
}
if(Test-Path -LiteralPath $reportPath){throw 'V3派生清单已存在。请运行-VerifyOnly，不覆盖或重写已审稿文件'}
foreach($layer in $mapping.layers){
 $original= @($production.files | Where-Object { $_.layer -eq $layer.layer -and $_.role -eq 'BaseColor' })
 $ref= @($palette.items | Where-Object { $_.semanticId -eq $layer.semanticId })
 if($original.Count -ne 1 -or $ref.Count -ne 1){throw "来源或唯一色板缺失：$($layer.layer)"}
 $source=Join-Path $root $original[0].sourcePath
 $dest=Join-Path $sourceRoot ('T_DBA_Terrain_'+$layer.layer+'_BaseColor_V3_Source.png')
 if(!(Test-Path -LiteralPath $source) -or (Test-Path -LiteralPath $dest)){throw "源缺失或目标已存在，不允许覆盖：$dest"}
 $digest=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()
 if($digest -ne $original[0].sha256){throw "原始CC0基础色哈希不一致：$source"}
 $target=[string]$ref[0].referenceSRGBHex
 if($target -cnotmatch '^#[0-9A-F]{6}$' -or $layer.strength -le 0 -or $layer.strength -gt 1){throw "无效参考色或校准强度：$($layer.layer)"}
}
# 仅一次编译C#供全纹理使用：锁定位图内存即可批量处理，比逐像素PowerShell/外部在线服务稳定。
$sourceCode=@'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.IO;
using System.Collections.Generic;
public static class SSLMTerrainColorV3Transfer {
  static double Linear(double v) { return v <= 0.04045 ? v / 12.92 : Math.Pow((v+0.055)/1.055,2.4); }
  static double Encode(double v) { return v <= 0.0031308 ? 12.92*v : 1.055*Math.Pow(v,1.0/2.4)-0.055; }
  static int Change(int old, double shift) {
    if(old==0 || old==255)return old;
    double value=Linear(old/255.0);
    value=Math.Max(1e-9,Math.Min(1-1e-9,value));
    double odds=Math.Log(value/(1-value))+shift;
    double dst=1/(1+Math.Exp(-odds));
    return Math.Max(0,Math.Min(255,(int)Math.Round(Encode(dst)*255)));
  }
  static double Mean(int[] hist,double delta,int count){
    double v=0;
    for(int i=0;i<256;i++) if(hist[i]>0) v+=hist[i]*Change(i,delta);
    return v/count;
  }
  static double Shift(int[] hist, double target, int count){
    double lo=-10,hi=10;
    for(int i=0;i<29;i++){
      double mid=(lo+hi)/2;
      if(Mean(hist,mid,count)<target)lo=mid;else hi=mid;
    }
    return (lo+hi)/2;
  }
  public static string Make(string input,string output,int r,int g,int b,double strength,double chromaControl,double detailContrast){
    using(Bitmap original=new Bitmap(input))
    using(Bitmap bitmap=new Bitmap(original.Width,original.Height,PixelFormat.Format32bppArgb)){
      using(Graphics gfx=Graphics.FromImage(bitmap)){
        gfx.CompositingMode=System.Drawing.Drawing2D.CompositingMode.SourceCopy;
        gfx.DrawImage(original,0,0,original.Width,original.Height);
      }
      Rectangle rect=new Rectangle(0,0,bitmap.Width,bitmap.Height);
      BitmapData data=bitmap.LockBits(rect,ImageLockMode.ReadWrite,PixelFormat.Format32bppArgb);
      byte[] bytes=new byte[Math.Abs(data.Stride)*data.Height];
      try {
        Marshal.Copy(data.Scan0,bytes,0,bytes.Length);
        int[][] hist={new int[256],new int[256],new int[256]};
        int count=bitmap.Width*bitmap.Height;
        for(int y=0;y<bitmap.Height;y++)for(int x=0;x<bitmap.Width;x++){
          int i=y*data.Stride+x*4;
          hist[0][bytes[i+2]]++;hist[1][bytes[i+1]]++;hist[2][bytes[i]]++;
        }
        int[] tgt={r,g,b};int[] old=new int[3];int[] fresh=new int[3];
        byte[][] table={new byte[256],new byte[256],new byte[256]};
        for(int c=0;c<3;c++){
          double mean=Mean(hist[c],0,count);
          old[c]=(int)Math.Round(mean);
          double dest=mean+strength*(tgt[c]-mean);
          double shift=Shift(hist[c],dest,count);
          for(int i=0;i<256;i++)table[c][i]=(byte)Change(i,shift);
          fresh[c]=(int)Math.Round(Mean(hist[c],shift,count));
        }
        double[] avgLin=new double[3];
        for(int c=0;c<3;c++){
          for(int k=0;k<256;k++)avgLin[c]+=(double)hist[c][k]*Linear(table[c][k]/255.0)/count;
        }
        double[] goalLin={Linear(r/255.0),Linear(g/255.0),Linear(b/255.0)};
        double goalLum=0.2126*goalLin[0]+0.7152*goalLin[1]+0.0722*goalLin[2];
        if(goalLum<=0)throw new ArgumentException("色板参考色亮度非法");
        double[] gain={goalLin[0]/goalLum,goalLin[1]/goalLum,goalLin[2]/goalLum};
        double[] after={0,0,0};
        for(int y=0;y<bitmap.Height;y++)for(int x=0;x<bitmap.Width;x++){
          int i=y*data.Stride+x*4;
          double[] c={
            Linear(table[0][bytes[i+2]]/255.0),
            Linear(table[1][bytes[i+1]]/255.0),
            Linear(table[2][bytes[i]]/255.0)
          };
          for(int channel=0;channel<3;channel++){
            c[channel]=Math.Max(0,Math.Min(1,avgLin[channel]+detailContrast*(c[channel]-avgLin[channel])));
          }
          double luma=0.2126*c[0]+0.7152*c[1]+0.0722*c[2];
          for(int channel=0;channel<3;channel++){
            c[channel]=Math.Max(0,Math.Min(1,(1-chromaControl)*c[channel]+chromaControl*luma*gain[channel]));
            after[channel]+=Encode(c[channel])*255.0;
          }
          bytes[i+2]=(byte)Math.Round(Encode(c[0])*255.0);
          bytes[i+1]=(byte)Math.Round(Encode(c[1])*255.0);
          bytes[i]=(byte)Math.Round(Encode(c[2])*255.0);
        }
        for(int channel=0;channel<3;channel++)fresh[channel]=(int)Math.Round(after[channel]/count);
        Marshal.Copy(bytes,0,data.Scan0,bytes.Length);
        bitmap.UnlockBits(data);
        data=null;
        bitmap.Save(output,ImageFormat.Png);
        return string.Format("#{0:X2}{1:X2}{2:X2}|#{3:X2}{4:X2}{5:X2}",old[0],old[1],old[2],fresh[0],fresh[1],fresh[2]);
      } finally { if(data!=null) bitmap.UnlockBits(data); }
    }
  }
  public static void MakeReview(string[] original,string[] derived,string[] layerNames,int[] colors,string output){
    int width=860,row=185;
    using(Bitmap board=new Bitmap(width,row*layerNames.Length+46)){
      using(Graphics g=Graphics.FromImage(board))
      using(Font font=new Font("Arial",13,FontStyle.Bold))
      using(Font small=new Font("Arial",11,FontStyle.Regular)){
        g.Clear(Color.FromArgb(21,29,31));
        using(Brush light=new SolidBrush(Color.White)){
          g.DrawString("SSLM Terrain V3 (before / corrected / sRGB reference)",font,light,16,10);
          for(int k=0;k<layerNames.Length;k++){
            int y=46+k*row;
            using(Bitmap a=new Bitmap(original[k]))
            using(Bitmap b=new Bitmap(derived[k])){
              g.DrawImage(a,new Rectangle(16,y+23,288,150),new Rectangle(0,0,a.Width,a.Height),GraphicsUnit.Pixel);
              g.DrawImage(b,new Rectangle(322,y+23,288,150),new Rectangle(0,0,b.Width,b.Height),GraphicsUnit.Pixel);
            }
            using(Brush swatch=new SolidBrush(Color.FromArgb(colors[k]))){
              g.FillRectangle(swatch,625,y+23,218,150);
            }
            g.DrawString(layerNames[k]+"  ORIGINAL          V3 DERIVED                     REFERENCE",small,light,16,y+3);
          }
        }
      }
      board.Save(output,ImageFormat.Png);
    }
  }
}
'@
Add-Type -AssemblyName System.Drawing.Common -ErrorAction Stop
$drawingRuntime=Split-Path ([System.Drawing.Bitmap].Assembly.Location) -Parent
$graphicsAssemblies=@('System.Drawing.Common.dll','System.Drawing.Primitives.dll','System.Private.Windows.GdiPlus.dll','System.Private.Windows.Core.dll') | ForEach-Object {Join-Path $drawingRuntime $_}
Add-Type -TypeDefinition $sourceCode -ReferencedAssemblies $graphicsAssemblies -ErrorAction Stop
New-Item -ItemType Directory -Force -Path $sourceRoot,$review | Out-Null
$picsOriginal=[System.Collections.Generic.List[string]]::new()
$picsDerived=[System.Collections.Generic.List[string]]::new()
$names=[System.Collections.Generic.List[string]]::new()
$colors=[System.Collections.Generic.List[int]]::new()
foreach($layer in $mapping.layers){
 $original= @($production.files | Where-Object { $_.layer -eq $layer.layer -and $_.role -eq 'BaseColor' })[0]
 $ref= @($palette.items | Where-Object { $_.semanticId -eq $layer.semanticId })[0]
 $source=Join-Path $root $original.sourcePath
 $filename='T_DBA_Terrain_'+$layer.layer+'_BaseColor_V3_Source.png'
 $dest=Join-Path $sourceRoot $filename
 $target=[string]$ref.referenceSRGBHex
 $r=[Convert]::ToInt32($target.Substring(1,2),16)
 $g=[Convert]::ToInt32($target.Substring(3,2),16)
 $b=[Convert]::ToInt32($target.Substring(5,2),16)
 $result=[SSLMTerrainColorV3Transfer]::Make($source,$dest,$r,$g,$b,[double]$layer.strength,[double]$layer.chromaControl,[double]$layer.detailContrast)
 $picsOriginal.Add($source);$picsDerived.Add($dest);$names.Add([string]$layer.layer)
 $colors.Add([System.Drawing.Color]::FromArgb(255,$r,$g,$b).ToArgb())
 $written += [pscustomobject]@{
   layer=$layer.layer;semanticId=$layer.semanticId;targetSRGB=$target;transferStrength=$layer.strength;chromaControl=$layer.chromaControl;detailContrast=$layer.detailContrast;meanBeforeAfter=$result;
   originalSourcePath=$original.sourcePath;originalSha256=$original.sha256;
   derivedSourcePath=($mapping.outputSourceRoot+'/'+$filename);
   derivedSha256=(Get-FileHash $dest -Algorithm SHA256).Hash.ToLowerInvariant();
   futureUnrealAsset=$mapping.outputAssetRoot+'/'+$filename.Replace('_Source.png','');
   statusZh='源PNG已派生；UE资产未因本步骤而自动导入'
 }
 Write-Output ('SSLM_TERRAIN_COLOR_V3_SOURCE '+$layer.layer+' '+$result+' target='+$target)
}
$comparison=Join-Path $review 'TerrainV3_BeforeAfterReview.png'
[SSLMTerrainColorV3Transfer]::MakeReview($picsOriginal.ToArray(),$picsDerived.ToArray(),$names.ToArray(),$colors.ToArray(),$comparison)
$final=[ordered]@{schemaVersion=1;date='2026-10-18';algorithm=$mapping.algorithm;
paletteSha256=(Get-FileHash (Join-Path $root $mapping.referencePalette) -Algorithm SHA256).Hash.ToLowerInvariant();
productionManifestSha256=(Get-FileHash (Join-Path $root $mapping.baselinePBRManifest) -Algorithm SHA256).Hash.ToLowerInvariant();
originZh='原始Poly Haven CC0扫描贴图未经覆盖';reviewImage=$comparison.Replace($root+'\','').Replace('\','/');humanReviewStatusZh='待审核';layers=$written}
$final | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output 'SSLM_TERRAIN_COLOR_V3_GENERATED count=10 originals_untouched=true preview_generated=true unreal_import_required=true'