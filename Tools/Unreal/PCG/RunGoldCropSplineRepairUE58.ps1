#requires -Version 5.1
<#
.SYNOPSIS
SSLM Gold农田真实PCG图的样条源迁移工具（UE5.8）。
.DESCRIPTION
Inspect只读；Apply只修改Foundation/Gold_Crop一份开发图，编辑前保存SHA备份；
Verify在新的独立UE进程检查真实GetActorData节点与SplineSampler连接。
既有编辑器/UBT在运行时拒绝启动，禁止覆盖项目正式地图、蓝图或其他PCG图。
#>
[CmdletBinding()]
param(
  [string]$EngineRoot=$env:UE_ROOT,
  [ValidateSet('Inspect','Apply','Verify')][string]$Mode='Inspect'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$script=Join-Path $PSScriptRoot 'RepairGoldCropSplineInputUE58.py'
$graph=Join-Path $root 'Game/Content/Development/Foundation/PCG/Realized/PCG_Gold_Crop.uasset'
$evidence=Join-Path $root ('Saved/Validation/GamePlatformPCG/GoldCropSplineRepair/'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
try {
  if(!$EngineRoot){throw '必须指定锁定UE5.8引擎目录'}
  $engine=(Resolve-Path $EngineRoot).Path
  $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
  if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '仅允许锁定UE5.8'}
  $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
  foreach($path in @($editor,$project,$script,$graph)) {
    if(!(Test-Path $path -PathType Leaf)){throw "工程、脚本或真实UE图缺失：$path"}
  }
  if(@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue).Count) {
    throw '其他UE Editor/UBT仍运行，拒绝同地图热更新'
  }
  $build=@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction SilentlyContinue |
    Where-Object{$_.CommandLine -match 'UnrealBuildTool'})
  if($build.Count){throw '共享UE构建仍在使用引擎，拒绝执行'}
  $before=(Get-FileHash -LiteralPath $graph -Algorithm SHA256).Hash
  # 修复范围只允许一张Gold Crop开发图；其他两张Gold图、金标准地图与正式世界
  # 在原生Editor过程前后必须字节一致，防止编辑器隐式保存或脚本越权修改。
  $protected = @{}
  foreach($relative in @(
    'Game/Content/Development/Foundation/PCG/Realized/PCG_Gold_Canopy.uasset',
    'Game/Content/Development/Foundation/PCG/Realized/PCG_Gold_Rock.uasset',
    'Game/Content/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1.umap',
    'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap',
    'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Maps/L_DBA_MainArena.umap'
  )){
    $absolute=Join-Path $root $relative
    if(!(Test-Path -LiteralPath $absolute -PathType Leaf)){throw ('受保护UE包不存在：'+$relative)}
    $protected[$absolute]=(Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash
  }
  if($Mode -eq 'Apply'){
    $backup=Join-Path $evidence 'PCG_Gold_Crop.before.uasset'
    Copy-Item -LiteralPath $graph -Destination $backup
    if((Get-FileHash $backup -Algorithm SHA256).Hash -ne $before){throw '执行前备份哈希不一致'}
    Write-Output ('SSLM_GOLD_CROP_BACKUP_VERIFIED '+$backup)
  }
  $stdout=Join-Path $evidence ($Mode+'.console.log')
  $stderr=Join-Path $evidence ($Mode+'.stderr.log')
  $native=Join-Path $evidence ($Mode+'.unreal.log')
  $env:SSLM_GOLD_CROP_SPLINE_MODE=$Mode.ToLowerInvariant()
  try{
    $args=@($project,'/Engine/Maps/Entry',("-ExecutePythonScript=$script"),
      '-SSLMGoldCropSplineMigration','-EnablePlugins=PythonScriptPlugin,CommonUI,DBAGameplay,DBAWorlds,DBAWorldPack_Village',
      '-DisablePlugins=Monolith','-NoCompile','-NoCompileEditor',
      '-unattended','-nop4','-nullrhi','-nosplash','-nosound',("-abslog=$native"))
    $process=Start-Process -FilePath $editor -ArgumentList $args -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if(!$process.WaitForExit(420000)){
      $process.Kill();[void]$process.WaitForExit(5000)
      throw '本次独立UE进程超过420秒，仅终止本脚本所有的进程'
    }
    $process.Refresh()
  }finally {Remove-Item Env:SSLM_GOLD_CROP_SPLINE_MODE -ErrorAction SilentlyContinue}
  $body=if(Test-Path $native){Get-Content $native -Raw -Encoding UTF8}else{''}
  $marker=switch($Mode){
    'Inspect'{'SSLM_GOLD_CROP_SPLINE_INSPECT_PASS'}
    'Apply'{'SSLM_GOLD_CROP_SPLINE_REPAIR_PASS'}
    'Verify'{'SSLM_GOLD_CROP_SPLINE_VERIFY_PASS'}
  }
  $ok=($process.ExitCode -eq 0) -and $body.Contains($marker) -and
    !$body.Contains('LogPython: Error:') -and !$body.Contains('Fatal error:')
  $after=(Get-FileHash -LiteralPath $graph -Algorithm SHA256).Hash
  foreach($pair in $protected.GetEnumerator()){
    if((Get-FileHash -LiteralPath $pair.Key -Algorithm SHA256).Hash -ne $pair.Value){
      throw ('Gold Crop迁移越权修改受保护的世界/生成图资源：'+$pair.Key)
    }
  }
  if($Mode -ne 'Apply' -and $before -ne $after){throw '只读操作修改了真实Gold Crop图'}
  if($Mode -eq 'Apply' -and $ok -and $before -eq $after){throw '迁移脚本声明通过但资产字节未变化'}
  # 只写Saved目录中的本次证据，不对正式.uasset/.umap补写虚假验收。
  $receipt=[ordered]@{
    SchemaVersion=1; Mode=$Mode; EngineVersion='5.8'
    Asset='PCG_Gold_Crop.uasset'; BeforeSHA256=$before; AfterSHA256=$after
    OtherApprovedPackagesUnchanged=$true
    UEProcessExitCode=$process.ExitCode; NativeSuccessMarker=$marker
    MarkerAndLogValidated=$ok; NativeLogPath=$native
    ValidatesActualPCGInstances=$false
  }
  $receipt|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
  Write-Output ("SSLM_GOLD_CROP_SPLINE_UE_RESULT mode=$Mode exit=$($process.ExitCode) marker=$ok")
  if(!$ok){throw ('真实UE图迁移失败，见原生日志：'+$native)}
  exit 0
}catch{
  Write-Output ('SSLM_GOLD_CROP_SPLINE_BLOCKED '+$_.Exception.Message)
  exit 1
}finally {
  Write-Output ('SSLM_GOLD_CROP_SPLINE_EVIDENCE '+$evidence)
}
