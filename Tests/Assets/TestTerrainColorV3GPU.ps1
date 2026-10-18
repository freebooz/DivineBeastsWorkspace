#requires -Version 5.1
<#
.SYNOPSIS
  SSLM 十层地形V3真实D3D12六态截图质量阻断检测。
.DESCRIPTION
  只读取从UE5.8独立审核地图通过SceneCapture2D得到的六张PNG，统计实际像素明度和文件哈希。
  禁止黑帧、明显无效输出、天气状态重复被报告为通过。截图检查不代表Shader/GPU耗时或人工美术合格。
  默认阻断模式：任一状态未达标返回非零；-ReportOnly只输出缺陷诊断用于审查记录。
  在Saved/Validation写入准确的诊断，不修改任何资产或图片。
#>
[CmdletBinding()]
param([switch]$ReportOnly)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing.Common
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dir=Join-Path $root 'Game/Saved/Validation/TerrainColorV3_GPU_20261018'
$meta=Join-Path $dir 'TerrainPBR_GPU_Evidence.json'
if(!(Test-Path -LiteralPath $meta)){throw '缺失UE5.8真实RHI清单，不能验收'}
$manifest=Get-Content -LiteralPath $meta -Raw -Encoding UTF8 | ConvertFrom-Json
$names=@('Dry','Moss','Wet','LightSnow','HeavySnow','AfterSnow')
if($manifest.origin -ne 'UE5.8 SceneCapture2D' -or $manifest.frames.Count -ne 6 -or @($manifest.frames|Select-Object -ExpandProperty name|Sort-Object -Unique).Count -ne 6){throw 'RHI帧源身份或六态不完整'}
$issues=[System.Collections.Generic.List[string]]::new()
$stats=@()
foreach($name in $names) {
  $f=Join-Path $dir ($name+'_D3D12_1280x720.png')
  if(!(Test-Path -LiteralPath $f)){throw ('截图缺失：'+$f)}
  $entry=@($manifest.frames|Where-Object {$_.name -eq $name})
  if($entry.Count -ne 1 -or ([System.IO.Path]::GetFullPath($entry[0].file) -ne [System.IO.Path]::GetFullPath($f))){throw ('来源清单路径不匹配：'+$name)}
  $img=[System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $f).Path)
  try {
    if($img.Width -ne 1280 -or $img.Height -ne 720){throw ('RHI画面不是1280×720：'+$name)}
    [double]$sum=0;[double]$sum2=0;[int]$black=0;[int]$n=0
    # 中央评审区域，排除可能过亮的远景天空；采用规则网格，不依赖平均图像大小误判。
    for($y=0;$y -lt 46;$y++){for($x=0;$x -lt 78;$x++){
      $px=$img.GetPixel([int](250+($x+.5)*780/78),[int](150+($y+.5)*440/46))
      $brightness=.2126*$px.R+.7152*$px.G+.0722*$px.B
      $sum+=$brightness;$sum2+=$brightness*$brightness
      if($brightness -lt 4){$black++}
      $n++
    }}
    $mean=$sum/$n; $std=[math]::Sqrt([math]::Max(0,$sum2/$n-$mean*$mean));$blackRate=$black/$n
    $sha=(Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash.ToLowerInvariant()
    if($mean -lt 15 -or $std -lt 6 -or $blackRate -gt .6){$issues.Add(('invalid/black-frame:'+ $name+': mean='+[math]::Round($mean,2)+' std='+[math]::Round($std,2)+' blackRatio='+[math]::Round($blackRate,3)))}
    $stats+=[pscustomobject]@{state=$name;sha256=$sha;meanLuma=[math]::Round($mean,3);stdLuma=[math]::Round($std,3);blackRatio=[math]::Round($blackRate,4);bytes=(Get-Item $f).Length}
  }finally{$img.Dispose()}
}
$groups=$stats|Group-Object sha256|Where-Object {$_.Count -gt 1}
foreach($group in $groups){$issues.Add(('duplicate-frame states='+((@($group.Group|Select-Object -ExpandProperty state)) -join ',')+' sha256='+$group.Name))}
$out=[ordered]@{schemaVersion=1;date='2026-10-18';source='UE5.8 SceneCapture2D D3D12';reviewMap=$manifest.landscape;pass=($issues.Count -eq 0);stateCount=$stats.Count;statistics=$stats;issues=@($issues);artReviewStatusZh='未人工签收';conclusionZh=if($issues.Count -eq 0){'六态基础像素门禁通过；仍须GPU性能和人工审核'}else{'截图像素门禁失败，不得声称实际场景色彩已验收'}}
$report=Join-Path $dir 'TerrainColorV3_GPUFrameQA.json'
$out|ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $report -Encoding UTF8
Write-Output ('SSLM_TERRAIN_V3_GPU_FRAME_CHECK pass='+$out.pass+' states='+$stats.Count+' errors='+$issues.Count+' report='+$report)
foreach($i in $issues){Write-Output ('SSLM_TERRAIN_V3_GPU_PROBLEM '+$i)}
if($issues.Count -gt 0 -and -not $ReportOnly){throw '真实D3D12六态审图门禁未通过，禁止验收'}
