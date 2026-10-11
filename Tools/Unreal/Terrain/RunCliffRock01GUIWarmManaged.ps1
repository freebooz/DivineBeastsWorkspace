#requires -Version 7.0
<#
.SYNOPSIS
  CliffRock01独立GUI D3D12延迟预热审核进程的一次性受控执行。
.DESCRIPTION
  只由本任务启动一个UnrealEditor.exe子进程，不修改其他会话编辑器/系统PATH；
  日志出现真实两帧截图标记并且两图确实存在后，仅结束这个由本脚本创建的子进程。
  此脚本不提交、保存、替换正式Village或改变原材质；每个批次必须为全新输出。
#>
[CmdletBinding()]
param([ValidateSet('lowWarp','baseline','fullWarp')][string]$Variant='lowWarp')
$ErrorActionPreference='Stop'
$project='E:/work/2026/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'
$editor='D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor.exe'
$py='E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/Terrain/RunCliffRock01DelayedGUIWarmGPU.py'
$root=(Get-Location).Path
$output=Join-Path $root 'Game/Saved/Validation/TerrainGaea/CliffRock01LowWarpStreamingOnGUIWarm20261011'
$logFile=Join-Path $root ('Game/Saved/Logs/CliffRock01GUIWarmManaged_'+$Variant+'_20261011.log')
$expectedMarker='CLIFF_GUIWARM_D3D12_QA_CAPTURE_COMPLETED '+$Variant
$photos=@(
    (Join-Path $output ('CliffLowWarp_'+$Variant+'_D3D12_0.png')),
    (Join-Path $output ('CliffLowWarp_'+$Variant+'_D3D12_1.png'))
)
if($photos |Where-Object {Test-Path $_ -PathType Leaf}){
    throw '待审核D3D12原始图已存在，不可重复生成/覆盖'
}
if(Test-Path $logFile){throw '本轮受控GUI原生日志已存在，禁止覆盖'}
$pluginDirs=@(Get-ChildItem 'Game/Plugins' -Recurse -Directory -Filter Win64 -ErrorAction SilentlyContinue |
    Where-Object {$_.FullName -match '\\Binaries\\Win64$'}|
    Select-Object -ExpandProperty FullName -Unique)
if($pluginDirs.Count -lt 10){throw '不能构造仅限当前进程的已编译Win64 DLL搜索路径'}
$psi=[System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName=$editor
$psi.WorkingDirectory=$root
$psi.UseShellExecute=$false
foreach($arg in @(
  $project,('-ExecCmds=py '+$py),'-RenderOffscreen',
  '-unattended','-nop4','-nosplash','-NoSound','-NoLiveCoding',
  '-d3d12','-DisablePlugins=DBAArena,DBAClient',
  '-ini:Monolith:[/Script/MonolithCore.MonolithSettings]:ServerPort=9353',
  ('-abslog='+$logFile)
)){
    [void]$psi.ArgumentList.Add($arg)
}
$psi.Environment['PATH']=($pluginDirs -join ';')+';'+$env:PATH
$psi.Environment['CLIFF_ISOLATED_CAPTURE_VARIANT']=$Variant
$psi.Environment['CLIFF_TEXTURE_STREAMING_MODE']='on'
$psi.Environment['CLIFF_CAPTURE_RUN_TAG']='GUIWarm'
$own=[System.Diagnostics.Process]::Start($psi)
if(!$own){throw 'UE5.8 GUI原生子进程未创建'}
$childPid=$own.Id
Write-Host ('CLIFF_GUIWARM_OWN_EDITOR_LAUNCHED pid='+$childPid+' variant='+$Variant)
$completed=$false
try{
    $start=Get-Date
    while(((Get-Date)-$start).TotalSeconds -lt 175){
        if($own.HasExited){
            Write-Host ('CLIFF_GUIWARM_CHILD_EXIT_EARLY exit='+$own.ExitCode)
            break
        }
        if(Test-Path $logFile){
            $failed=Select-String -Path $logFile -Pattern 'CLIFF_GUIWARM_D3D12_QA_FAILED' -SimpleMatch -Quiet -ErrorAction SilentlyContinue
            if($failed){throw 'GUI材质审核Python执行遇到运行时异常'}
            $completed=Select-String -Path $logFile -Pattern $expectedMarker -SimpleMatch -Quiet -ErrorAction SilentlyContinue
            if($completed){break}
        }
        Start-Sleep -Seconds 3
    }
    if(!$completed){throw '超过审核Shader预热与截图上限，没有获得真D3D12完成标记'}
    foreach($file in $photos){
        if(!(Test-Path $file -PathType Leaf) -or (Get-Item $file).Length -lt 80000){
            throw '真实D3D12审核源文件未生成或长度异常：'+$file
        }
        Write-Host ('CLIFF_GUIWARM_OWN_PNG_VERIFIED '+$file+' bytes='+(Get-Item $file).Length)
    }
    Write-Host ('CLIFF_GUIWARM_MANAGED_CAPTURE_SUCCESS variant='+$Variant+
                ' scene=QA independent and manual quality gate pending')
}finally{
    # 对PID与精确ExecCmds/输出log二次验证，只结束本脚本创建的GUI，不碰Fauna/其他会话。
    if(!$own.HasExited){
        $child=Get-CimInstance Win32_Process -Filter ('ProcessId='+$childPid)
        if(!$child -or !$child.CommandLine.Contains('RunCliffRock01DelayedGUIWarmGPU.py') -or
           !$child.CommandLine.Contains('CliffRock01GUIWarmManaged_'+$Variant+'_20261011.log')){
            throw '子进程身份发生变化，禁止误停其他编辑器'
        }
        $own.Kill($true)
        [void]$own.WaitForExit(18000)
        Write-Host ('CLIFF_GUIWARM_OWN_EDITOR_CLOSED pid='+$childPid)
    }
    $own.Dispose()
}
if(!$completed){exit 1}
