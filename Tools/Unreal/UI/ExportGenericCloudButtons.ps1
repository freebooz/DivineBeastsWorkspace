param([switch]$ReviewOnly)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$buttonRoot=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$buttonConfig=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'GenericCloudButtons.json') -Raw | ConvertFrom-Json
$buttonArt=Join-Path $buttonRoot 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Buttons'
$buttonEvidence=Join-Path $buttonRoot 'Saved/Validation/GenericCloudButtons20261103'
New-Item -ItemType Directory -Path $buttonEvidence -Force | Out-Null
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
public static class CloudButtonAlphaBounds {
 public static Rectangle Get(Bitmap image) {
  int minX=image.Width,minY=image.Height,maxX=-1,maxY=-1;
  for(int y=0;y<image.Height;y++) for(int x=0;x<image.Width;x++) if(image.GetPixel(x,y).A>=32) {
   minX=Math.Min(minX,x); minY=Math.Min(minY,y); maxX=Math.Max(maxX,x); maxY=Math.Max(maxY,y);
  }
  if(maxX<0) throw new Exception("Empty alpha image");
  return Rectangle.FromLTRB(Math.Max(0,minX-2),Math.Max(0,minY-2),Math.Min(image.Width,maxX+3),Math.Min(image.Height,maxY+3));
 }
}
'@ -ReferencedAssemblies @([System.Drawing.Bitmap].Assembly.Location,[System.Drawing.Rectangle].Assembly.Location)
$buttonReport=@()
if(-not $ReviewOnly){
New-Item -ItemType Directory -Path $buttonArt -Force | Out-Null
foreach($item in $buttonConfig.generated){
 $sourcePath=if([System.IO.Path]::IsPathRooted($item.source)){$item.source}else{Join-Path $buttonRoot $item.source}
 $source=[System.Drawing.Bitmap]::FromFile($sourcePath)
 $target=[System.Drawing.Bitmap]::new(640,180,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $g=[System.Drawing.Graphics]::FromImage($target)
 try {
  $bounds=[CloudButtonAlphaBounds]::Get($source)
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.CompositingQuality=[System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($source,[System.Drawing.RectangleF]::new(2,2,636,176),[System.Drawing.RectangleF]::new($bounds.X,$bounds.Y,$bounds.Width,$bounds.Height),[System.Drawing.GraphicsUnit]::Pixel)
  $dest=Join-Path $buttonArt ('T_DBA_UI_CloudButton_'+$item.id+'.png')
  $target.Save($dest,[System.Drawing.Imaging.ImageFormat]::Png)
  $zero=0;$opaque=0
  for($y=0;$y -lt 180;$y++){for($x=0;$x -lt 640;$x++){ $a=$target.GetPixel($x,$y).A; if($a -eq 0){$zero++}elseif($a -ge 250){$opaque++} }}
  if($zero -lt 2000 -or $opaque -lt 40000){throw 'Button alpha validation failed'}
  $buttonReport += [pscustomobject]@{Family=$item.id;Source=$dest;Size=@(640,180);AlphaZero=$zero;AlphaOpaque=$opaque;AlphaPassed=$true;SHA256=(Get-FileHash -LiteralPath $dest).Hash.ToLowerInvariant()}
 } finally {$g.Dispose();$source.Dispose();$target.Dispose()}
}
}else{
 $buttonReport=@(Get-Content -LiteralPath (Join-Path $buttonEvidence 'SourceArt.json') -Raw | ConvertFrom-Json)
}
$w=[int]$buttonConfig.export_size[0];$h=[int]$buttonConfig.export_size[1]
$halfFolder=Join-Path $buttonEvidence ('Transparent'+$w+'x'+$h)
New-Item -ItemType Directory -Path $halfFolder -Force | Out-Null
$halfReport=@()
foreach($item in $buttonReport){
 $source=[System.Drawing.Bitmap]::FromFile($item.Source)
 $target=[System.Drawing.Bitmap]::new($w,$h,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $g=[System.Drawing.Graphics]::FromImage($target)
 try{
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($source,[System.Drawing.Rectangle]::new(0,0,$w,$h),0,0,$source.Width,$source.Height,[System.Drawing.GraphicsUnit]::Pixel)
  $dest=Join-Path $halfFolder ('SSLM_Button_'+$item.Family+'_'+$w+'x'+$h+'.png')
  $target.Save($dest,[System.Drawing.Imaging.ImageFormat]::Png)
  $zero=0;$opaque=0
  for($y=0;$y -lt $h;$y++){for($x=0;$x -lt $w;$x++){ $a=$target.GetPixel($x,$y).A; if($a -eq 0){$zero++}elseif($a -ge 250){$opaque++} }}
  if($zero -lt $w*$h*0.06 -or $opaque -lt $w*$h*0.65){throw 'Button export alpha validation failed'}
  $halfReport += [pscustomobject]@{Family=$item.Family;Path=$dest;Size=@($w,$h);AlphaZero=$zero;AlphaOpaque=$opaque;SHA256=(Get-FileHash -LiteralPath $dest).Hash.ToLowerInvariant()}
 }finally{$g.Dispose();$target.Dispose();$source.Dispose()}
}
$halfReport | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $halfFolder 'Manifest.json') -Encoding utf8
$buttonPreview=[System.Drawing.Bitmap]::new(760,440,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($buttonPreview)
try {
 $g.Clear([System.Drawing.Color]::FromArgb(35,63,58))
 for($i=0;$i -lt 2;$i++){
  $b=[System.Drawing.Image]::FromFile($buttonReport[$i].Source)
  try {$g.DrawImage($b,60,25+$i*210,640,180)} finally {$b.Dispose()}
 }
 $buttonPreview.Save((Join-Path $buttonEvidence 'SourcePreview.png'),[System.Drawing.Imaging.ImageFormat]::Png)
} finally {$g.Dispose();$buttonPreview.Dispose()}
if(-not $ReviewOnly){$buttonReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $buttonEvidence 'SourceArt.json') -Encoding utf8}
$halfReport | Select-Object Family,Size,AlphaZero,AlphaOpaque

