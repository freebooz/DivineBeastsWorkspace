#requires -Version 5.1
<#
.SYNOPSIS
  CliffRock01基线/低扰动两个独立UE5.8地图及D3D12截图自动复验。
.DESCRIPTION
  只读原L_DBA_CliffRock01MacroReview，两张新开发地图各独立启动UE Editor配置；
  两次GPU采集独立进程，避免同一帧内SetMaterial导致图片不刷新；
  输出原生UE日志、截图，无用户人工签收结果。
  仅为此PowerShell子进程设置已存在Game/Plugins DLL搜索路径，不改工程配置。
#>
[CmdletBinding()]
param([switch]$ValidateOnly)
$ErrorActionPreference='Stop'
$project='E:/work/2026/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'
$editor='D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$author='E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/Terrain/AuthorCliffRock01LowWarpIsolatedMaps.py'
$capture='E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/Terrain/CaptureCliffRock01LowWarpIsolatedGPU.py'
$dir=Join-Path (Get-Location).Path 'Game/Saved/Validation/TerrainGaea/CliffRock01LowWarpIsolated20261011'
$logBase='CliffRockLowWarpIsolated20261011'
$binaryDirs=@(Get-ChildItem 'Game/Plugins' -Recurse -Directory -Filter Win64 -ErrorAction SilentlyContinue |
    Where-Object {$_.FullName -match '\\Binaries\\Win64$'}|Select-Object -ExpandProperty FullName -Unique)
if($binaryDirs.Count -lt 10){throw '未找到必要Game插件模块Binaries，无法在临时进程补充搜索路径'}
$oldPath=$env:PATH
try{
  $env:PATH=($binaryDirs -join ';')+';'+$env:PATH
  $argsBase=@($project,'-DisablePlugins=DBAArena,DBAClient','-unattended','-nop4','-nosplash','-NoSound')
  function RunUE([string]$mode,[string]$variant,[string]$script,[bool]$gpu,[string]$marker){
    $env:CLIFF_ISOLATED_MAP_MODE=$mode
    $env:CLIFF_ISOLATED_MAP_TYPE=$variant
    $env:CLIFF_ISOLATED_CAPTURE_VARIANT=$variant
    $label='CliffRockLowWarp_'+$variant+'_'+$mode+'_20261011'
    $log='Game/Saved/Logs/'+$label+'.log'
    Write-Host ("CLIFF_ISOLATED_UE_START "+$label)
    $cmd=@($project,('-ExecutePythonScript='+$script),'-DisablePlugins=DBAArena,DBAClient',
            '-unattended','-nop4','-nosplash','-NoSound',('-log='+$label+'.log'))
    if($gpu){$cmd+='-d3d12'}else{$cmd+='-NullRHI'}
    & $editor @cmd
    $code=$LASTEXITCODE
    Write-Host ('CLIFF_ISOLATED_UE_EXIT '+$label+' code='+$code)
    if($code -ne 0 -or !(Test-Path $log)){
      if(Test-Path $log){
        Select-String $log -Pattern 'LogPython: Error:','LogPluginManager: Error:','Error:'|
          Select-Object -Last 9|ForEach-Object{$_.Line}
      }
      throw ('UE未正常结束QA操作 '+$label+' code='+$code)
    }
    if(!(Select-String -Path $log -Pattern $marker -SimpleMatch -Quiet)){
      throw ("本阶段没有原生成功日志[$marker]："+$label)
    }
  }
  foreach($name in @('baseline','lowWarp')){
    $path='Game/Content/Development/Terrain/Maps/L_DBA_CliffRock01LowWarp'+$(if($name -eq 'baseline'){'Baseline'}else{'Variant'})+'Review.umap'
    if(!(Test-Path $path) -and !$ValidateOnly){
      RunUE 'clone' $name $author $false 'CLIFF_LOW_ISOLATED_WORLD_CLONE_SAVED'
    }
    if(!$ValidateOnly){
      RunUE 'configure' $name $author $false 'CLIFF_LOW_ISOLATED_WORLD_REAL_UE_VERIFY_PASS'
    }else{
      RunUE 'verify' $name $author $false 'CLIFF_LOW_ISOLATED_WORLD_REAL_UE_VERIFY_PASS'
    }
  }
  if(!$ValidateOnly){
    foreach($name in @('baseline','lowWarp')){
      RunUE 'capture' $name $capture $true 'CLIFF_LOW_ISOLATED_GPU_TWO_FRAMES_PASS'
    }
  }
  Write-Host 'CLIFF_LOW_ISOLATED_ALL_EDITOR_STAGES_COMPLETE (analysis not yet evaluated)'
}finally{
  $env:PATH=$oldPath
  Remove-Item Env:CLIFF_ISOLATED_MAP_MODE -ErrorAction SilentlyContinue
  Remove-Item Env:CLIFF_ISOLATED_MAP_TYPE -ErrorAction SilentlyContinue
  Remove-Item Env:CLIFF_ISOLATED_CAPTURE_VARIANT -ErrorAction SilentlyContinue
}
