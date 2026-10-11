#requires -Version 7.0
<#
.SYNOPSIS
  神兽联盟CliffRock01真正GUI编辑器延迟Shader预热D3D12基准测试。
.DESCRIPTION
  子进程局部PATH增加已存在项目插件Win64 DLL目录；Monolith独占9353端口；
  仅加载独立开发审核UWorld，延迟48s后D3D12截图，主动退出本编辑器进程。
  不覆盖正式Village，不修改其他编辑器/系统PATH/项目配置，不依赖后台未知用户会话。
#>
[CmdletBinding()]
param([ValidateSet('baseline','lowWarp')][string]$Variant='baseline')
$ErrorActionPreference='Stop'
$editor='D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor.exe'
$project='E:/work/2026/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'
$python='E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/Terrain/RunCliffRock01DelayedGUIWarmGPU.py'
$pluginDirs=@(Get-ChildItem Game/Plugins -Recurse -Directory -Filter Win64 -ErrorAction SilentlyContinue|
    Where-Object {$_.FullName -match '\\Binaries\\Win64$'} |
    Select-Object -ExpandProperty FullName -Unique)
if($pluginDirs.Count -lt 10){throw '编译好的项目Win64插件目录不完整'}
$before=$env:PATH
try{
    $env:PATH=($pluginDirs -join ';')+';'+$before
    $env:CLIFF_TEXTURE_STREAMING_MODE='on'
    $env:CLIFF_CAPTURE_RUN_TAG='GUIWarm'
    $env:CLIFF_ISOLATED_CAPTURE_VARIANT=$Variant
    Write-Host ('CLIFF_GUIWARM_PERSISTENT_SUPERVISOR_LAUNCH '+$Variant)
    $args=@(
      $project,
      ('-ExecCmds=py '+$python),
      '-RenderOffscreen','-unattended','-nop4','-nosplash','-NoSound','-NoLiveCoding',
      '-d3d12','-DisablePlugins=DBAArena,DBAClient',
      '-ini:Monolith:[/Script/MonolithCore.MonolithSettings]:ServerPort=9353',
      ('-log=CliffRock01GUIWarmSupervised_'+$Variant+'_20261011.log')
    )
    & $editor @args
    $code=$LASTEXITCODE
    Write-Host ('CLIFF_GUIWARM_SUPERVISED_UE_EXIT '+$Variant+' '+$code)
}finally{
    $env:PATH=$before
    foreach($name in ('CLIFF_TEXTURE_STREAMING_MODE','CLIFF_CAPTURE_RUN_TAG',
                        'CLIFF_ISOLATED_CAPTURE_VARIANT')){
        Remove-Item ('Env:'+$name) -ErrorAction SilentlyContinue
    }
}
$log='Game/Saved/Logs/CliffRock01GUIWarmSupervised_'+$Variant+'_20261011.log'
if(Test-Path $log){
    Select-String $log -SimpleMatch -Pattern 'CLIFF_GUIWARM','CLIFF_LOW_ISOLATED_REAL_D3D12_CAPTURE','LogPython: Error:'|
      Select-Object -Last 25|ForEach-Object{$_.Line}
}
if($code -ne 0){exit $code}
if(!(Test-Path $log) -or !(Select-String $log -SimpleMatch -Pattern 'CLIFF_GUIWARM_D3D12_QA_CAPTURE_COMPLETED' -Quiet)){
    throw '没有真实CliffRock已预热的D3D12帧完成标记，不能宣布GPU测试成功'
}
Write-Host ('CLIFF_GUIWARM_SUPERVISOR_CAPTURE_PROBE_PASS '+$Variant+' actualQualityPending=True')
