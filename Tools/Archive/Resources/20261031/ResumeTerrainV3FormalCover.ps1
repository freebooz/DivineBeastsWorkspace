#requires -Version 5.1
<#
.SYNOPSIS
  SSLM V3.0地形正式十层最后一张Dirt纹理安全补全。
.DESCRIPTION
  检查9/10部分冷重载与12份执行前备份。默认只读；-Apply仅操作Dirt单资产。
  文件锁未释放或另有同工程编辑器时必须失败，不强占、不结束其他进程。
  分两个独立UE5.8进程导入与冷重载；检查实际UE日志标记和Python异常。
  原地图、共享母材质、法线/ORM/Height、PCG与服务器权威均保持不变。
  不声称GPU截图、Cook或人工美术签收已通过。
#>
[CmdletBinding()]
param([switch]$Apply, [string]$EngineRoot = 'D:/UnrealEngine-5.8.0-release')
throw 'Dirt单图恢复已完成，此脚本已退休归档；当前原生完整验证见Resources/ResourceOwnershipMonolith.py及地形现行校验器。'
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$progressPath = Join-Path $root 'Saved/ArtReview/TerrainColorV3FormalCover/ProgressManifest.json'
$reportPath = Join-Path $root 'Saved/ArtReview/TerrainColorV3FormalCover/AppliedManifest.json'
$backupPath = Join-Path $root 'Saved/ArtReview/TerrainColorV3FormalCover/Before_20261018/BackupManifest.json'
$dirt = Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Terrain/Textures/Dirt/T_DBA_Terrain_Dirt_BaseColor.uasset'
$project = Join-Path $root 'Game/DivineBeastsArena.uproject'
$author = Join-Path $root 'Tools/Unreal/Terrain/ApplyTerrainV3ToFormalTexturesUE58.py'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($entry in @($progressPath,$backupPath,$dirt,$project,$author,$editor)) {
    if (!(Test-Path -LiteralPath $entry)) { throw ('正式资源或工具不存在: '+$entry) }
}
$progress=Get-Content -LiteralPath $progressPath -Raw -Encoding UTF8 | ConvertFrom-Json
$backup=Get-Content -LiteralPath $backupPath -Raw -Encoding UTF8 | ConvertFrom-Json
if (Test-Path -LiteralPath $reportPath) { throw '已有完整应用收据；不能重复原位覆盖' }
if ($progress.rows.Count -ne 9 -or $progress.pending.Count -ne 1 -or
    $progress.pending[0].layer -ne 'Dirt' -or !$progress.partialColdVerifyPassed -or
    $progress.coldVerifyPassed -or $progress.humanReviewPassed -or $backup.files.Count -ne 12) {
    throw '必须具有9项UE冷重载、唯一Dirt待办及12份原资产回退记录'
}
$old = @($backup.files | Where-Object {$_.relative -eq 'Terrain/Textures/Dirt/T_DBA_Terrain_Dirt_BaseColor.uasset'})
if ($old.Count -ne 1) { throw '原Dirt纹理回退身份缺失或重复' }
if ((Get-FileHash -LiteralPath $dirt -Algorithm SHA256).Hash.ToLowerInvariant() -ne $old[0].sha256) {
    throw 'Dirt已被外部任务修改，不允许覆盖'
}
$locked = $false
try {
    $stream = [System.IO.File]::Open($dirt,[System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,[System.IO.FileShare]::None)
    $stream.Dispose()
} catch [System.IO.IOException] {
    $locked = $true
}
Write-Output ('SSLM_V3_DIRT_PRECHECK source_backup_match=true file_locked='+$locked)
if (!$Apply) { Write-Output 'SSLM_V3_DIRT_PLAN pending_one=true no_files_modified=true'; return }
if ($locked) { throw 'Dirt文件仍被占用；不结束任何用户编辑器或强行覆盖' }
$editors = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'UnrealEditor.exe' -and $_.CommandLine -match 'DivineBeastsArena\.uproject'
})
if ($editors.Count -gt 0) { throw '同工程UnrealEditor仍运行，先安全关闭该会话' }
$folder = Join-Path $root 'Game/Saved/Validation/TerrainColorV3FormalCover'
if (!(Test-Path $folder)) { New-Item -ItemType Directory -Path $folder -Force | Out-Null }
# 上一轮因55节点旧桥接SHA门禁提前拒绝，错误日志永久保留供追溯；
# 使用专属新日志名恢复，不覆盖失败证据，也不重用任务运行产物。
$applyLog = Join-Path $folder 'FormalDirt_UEApply_AfterBombingBridge.log'
$verifyLog = Join-Path $folder 'Formal10_UEColdVerify_AfterBombingBridge.log'
if ((Test-Path $applyLog) -or (Test-Path $verifyLog)) {
    throw '最终处理日志已经存在，不重用既有运行证据'
}
$env:TERRAIN_V3_FORMAL_MODE='apply'
$env:TERRAIN_V3_FORMAL_APPLY_LAYERS='Dirt'
try {
    & $editor $project ('-ExecutePythonScript='+$author) '-unattended' '-NoSplash' '-NoCompile' '-NoLiveCoding' '-NoHotReload' '-DisablePlugins=Monolith' '-NullRHI' ('-abslog='+$applyLog)
    if (!(Test-Path -LiteralPath $applyLog)) { throw 'UE未生成Dirt原生执行日志' }
    $pass = @(Select-String -LiteralPath $applyLog -Pattern 'SSLM_V3_FORMAL_ASSETS_SAVED_UE count=10' -SimpleMatch)
    $errors = @(Select-String -LiteralPath $applyLog -Pattern 'LogPython: Error:','LogEditorPythonExecuter: Error:','LogSavePackage: Error:' -SimpleMatch)
    if ($pass.Count -ne 1 -or $errors.Count -gt 0 -or !(Test-Path -LiteralPath $reportPath)) {
        throw '原生Dirt保存未真正通过；进程退出0不算成功'
    }
    $env:TERRAIN_V3_FORMAL_MODE='verify'
    Remove-Item Env:TERRAIN_V3_FORMAL_APPLY_LAYERS -ErrorAction SilentlyContinue
    & $editor $project ('-ExecutePythonScript='+$author) '-unattended' '-NoSplash' '-NoCompile' '-NoLiveCoding' '-NoHotReload' '-DisablePlugins=Monolith' '-NullRHI' ('-abslog='+$verifyLog)
    if (!(Test-Path -LiteralPath $verifyLog)) { throw '新进程未生成冷重载日志' }
    $verified = @(Select-String -LiteralPath $verifyLog -Pattern 'SSLM_V3_FORMAL_COLD_VERIFY_PASS' -SimpleMatch)
    $verifyErrors = @(Select-String -LiteralPath $verifyLog -Pattern 'LogPython: Error:','LogEditorPythonExecuter: Error:' -SimpleMatch)
    $final = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($verified.Count -ne 1 -or $verifyErrors.Count -gt 0 -or $final.rows.Count -ne 10 -or
        !$final.coldVerifyPassed -or $final.humanReviewPassed) {
        throw '十层冷重载或原生记录不符合质量门禁'
    }
    Write-Output 'SSLM_V3_FORMAL_10_COLD_VERIFIED human_visual_GPU_cook_pending=true'
} finally {
    Remove-Item Env:TERRAIN_V3_FORMAL_MODE -ErrorAction SilentlyContinue
    Remove-Item Env:TERRAIN_V3_FORMAL_APPLY_LAYERS -ErrorAction SilentlyContinue
}
