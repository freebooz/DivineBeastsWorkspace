#requires -Version 5.1
<#
.SYNOPSIS
在没有其他UE5.8编辑器占用DivineBeastsArena时安全修复平台GroundMotion Niagara CustomHLSL。
.DESCRIPTION
不停止其他进程、不改非本任务的资产、不假装0退出等于运行特效成功。
只修改一个确认报错的Particles.Age直接HLSL引用，Python端会对原uasset进行SHA-256备份。
#>
[CmdletBinding()]
param([ValidateRange(0,600)][int]$WaitForSafeWindowSeconds=180)
$ErrorActionPreference='Stop'
$Root=(Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$Engine='D:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Project=Join-Path $Root 'Game/DivineBeastsArena.uproject'
$Source=Join-Path $Root 'Tools/Unreal/VFX/FallingFoliage/RepairFoliageGroundMotionHLSLUE58.py'
$Report=Join-Path $Root 'Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/GroundMotionHLSLFix.json'
$Log=Join-Path $Root 'Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/GroundMotionHLSLFix.log'
$RelativeAsset='Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/FallingFoliage/Niagara/Modules/NM_GP_FoliageGroundMotion.uasset'
if(!(Test-Path -LiteralPath $Project) -or !(Test-Path -LiteralPath $Engine) -or !(Test-Path -LiteralPath $Source)){
    throw 'UE项目、引擎或受控作者工具缺失，拒绝补丁'
}
$dirty=(& git status --porcelain -- $RelativeAsset)
if($LASTEXITCODE -ne 0 -or $dirty){
    throw '目标模块当前存在其他会话未提交修改；拒绝覆盖：'+$dirty
}
$limit=(Get-Date).AddSeconds($WaitForSafeWindowSeconds)
do {
    $sessions=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" -ErrorAction SilentlyContinue |
        Where-Object {$_.CommandLine -match 'DivineBeastsArena[.]uproject'})
    if($sessions.Count -eq 0){break}
    Write-Output ('FOLIAGE_WAIT_OTHER_EDITOR '+($sessions.ProcessId -join ',')+' at '+(Get-Date -Format HH:mm:ss))
    if((Get-Date) -ge $limit){Write-Output 'FOLIAGE_OTHER_EDITOR_BUSY_NO_MODIFICATION';exit 2}
    Start-Sleep -Seconds 8
}while($true)
$dirty=(& git status --porcelain -- $RelativeAsset)
if($dirty){throw '在等待窗口期间其他任务修改了GroundMotion资源；放弃补丁'}
Write-Output ('FOLIAGE_SAFE_WINDOW_ACQUIRED '+(Get-Date -Format s))
& $Engine $Project '-unattended' '-nop4' '-NoLiveCoding' '-NoSound' '-NullRHI' '-nosplash' "-ExecutePythonScript=$Source" "-abslog=$Log"
$exit=$LASTEXITCODE
Write-Output ('FOLIAGE_NIAGARA_HLSL_AUTHOR_EXIT='+$exit)
if($exit -ne 0){exit $exit}
if(!(Test-Path -LiteralPath $Report)){throw 'UE编辑器未生成Niagara修复报告，拒绝宣称完成'}
$state=Get-Content -LiteralPath $Report -Raw -Encoding UTF8|ConvertFrom-Json
if(!$state.saved -or $state.status -notin @('patched','already_patched') -or @($state.errors).Count -gt 0){
    throw ('Niagara模块未通过原位修改并保存，状态='+$state.status)
}
Write-Output ('FOLIAGE_HLSL_FIX_SAVED '+$state.status)
Write-Output ('FOLIAGE_HLSL_BINARY_BACKUP '+$state.backupExists)
exit 0
