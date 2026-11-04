$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$buttonRoot=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$buttonOut=Join-Path $buttonRoot 'Saved/Validation/GenericCloudButtons20261103'
$buttonCapture=Get-Content -LiteralPath (Join-Path $buttonOut 'NativeCapture.json') -Raw | ConvertFrom-Json
$buttonImage=Join-Path $buttonOut $buttonCapture.Image
if((Get-FileHash -LiteralPath $buttonImage).Hash.ToLowerInvariant() -ne $buttonCapture.PNG_SHA256){throw 'Native screenshot fingerprint changed'}
function ToLinear([double]$v){$v/=255.0;if($v -le 0.04045){return $v/12.92};return [Math]::Pow(($v+0.055)/1.055,2.4)}
function NineSliceUV([double]$position,[double]$size,[double]$margin){
 # Slate Box九宫格顶点会像素对齐；UV边界仍是原始margin。不能用整图等比UV替代。
 $left=[Math]::Floor($size*$margin+0.5);$right=[Math]::Floor($size*(1-$margin)+0.5)
 if($position -lt $left){return $position/$left*$margin}
 if($position -gt $right){return 1-$margin+($position-$right)/($size-$right)*$margin}
 return $margin+($position-$left)/($right-$left)*(1-2*$margin)
}
function SourceSample($bitmap,[double]$u,[double]$v){
 $x=$u*$bitmap.Width-0.5;$y=$v*$bitmap.Height-0.5
 $ix=[int][Math]::Floor($x);$iy=[int][Math]::Floor($y);$fx=$x-$ix;$fy=$y-$iy
 $r=0.0;$g=0.0;$b=0.0;$a=0.0
 for($dy=0;$dy -lt 2;$dy++){for($dx=0;$dx -lt 2;$dx++){
  $p=$bitmap.GetPixel([Math]::Clamp($ix+$dx,0,$bitmap.Width-1),[Math]::Clamp($iy+$dy,0,$bitmap.Height-1))
  $wx=if($dx){$fx}else{1-$fx};$wy=if($dy){$fy}else{1-$fy};$w=$wx*$wy
  $r+=$w*(ToLinear $p.R);$g+=$w*(ToLinear $p.G);$b+=$w*(ToLinear $p.B);$a+=$w*$p.A/255.0
 }}
 return @($r,$g,$b,$a)
}
$shot=[System.Drawing.Bitmap]::FromFile($buttonImage)
$checks=@()
try{
 $backgroundPixel=$shot.GetPixel(5,5)
 $background=@((ToLinear $backgroundPixel.R),(ToLinear $backgroundPixel.G),(ToLinear $backgroundPixel.B))
 foreach($family in @('Primary','Secondary')){
  $sourcePath=Join-Path $buttonRoot ('Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Buttons/T_DBA_UI_CloudButton_'+$family+'.png')
  $source=[System.Drawing.Bitmap]::FromFile($sourcePath)
  try{
   $top=if($family -eq 'Primary'){110}else{206}
   foreach($point in @(@(50,30),@(55,20),@(160,30),@(165,45),@(100,10))){
    $x=[int]$point[0];$y=[int]$point[1]
    $s=SourceSample $source (NineSliceUV ($x+0.5) 213 0.16) (NineSliceUV ($y+0.5) 60 0.44)
    $pixel=$shot.GetPixel(136+$x,$top+$y)
    $actual=@((ToLinear $pixel.R),(ToLinear $pixel.G),(ToLinear $pixel.B));$expected=@();$delta=@()
    for($c=0;$c -lt 3;$c++){$expected+=($s[$c]*$s[3]+$background[$c]*(1-$s[3]));$delta += [Math]::Abs($actual[$c]-$expected[$c])}
    $max=($delta|Measure-Object -Maximum).Maximum
    $checks += [pscustomobject]@{Family=$family;LocalPixel=@($x,$y);ExpectedLinear=$expected;ActualLinear=$actual;MaxError=$max;Passed=($max -le 0.008)}
   }
  }finally{$source.Dispose()}
 }
}finally{$shot.Dispose()}
$failed=@($checks|Where-Object {-not $_.Passed})
@{Passed=($failed.Count -eq 0);Samples=$checks;Tolerance=0.008;Comparison='Slate pixel-snapped nine-slice UV; source sRGB decode and bilinear sampling + source alpha compositing versus native RHI PNG sRGB decode';PNG_SHA256=$buttonCapture.PNG_SHA256}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $buttonOut 'SourceToRHIColor.json') -Encoding utf8
$checks | Select-Object Family,LocalPixel,MaxError,Passed | Format-Table
if($failed.Count){throw 'Source-to-RHI color comparison failed'}
