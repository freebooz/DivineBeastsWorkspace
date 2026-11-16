#requires -Version 5.1
<#
.SYNOPSIS
PCG三世界文件覆盖率检查只读回归。
.DESCRIPTION
验证报告角色、资产枚举和严格发布门禁；不生成或修改UE原生资产。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)
$ErrorActionPreference='Stop'
if([string]::IsNullOrWhiteSpace($WorkspaceRoot)){$WorkspaceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path}
$root=(Resolve-Path $WorkspaceRoot).Path
$audit=Join-Path $root 'Build/Validation/VerifyPCGGameCoverage.ps1'
$report=Join-Path $root 'Saved/Validation/GamePlatformPCG/PCGWholeGameCoverage_Test.json'
& $audit -WorkspaceRoot $root -ReportPath $report|Out-Null
if($LASTEXITCODE -ne 0){throw "PCG基础审计应完成，退出码=$LASTEXITCODE"}
$data=Get-Content -LiteralPath $report -Raw -Encoding UTF8|ConvertFrom-Json
if($data.SchemaVersion -ne 1 -or $data.InspectionType -ne 'FilesystemAndRegistryOnly'){throw '报告版本/证据类型错误'}
$roles=@($data.WorldRoles|ForEach-Object{$_.Name})
if(@(Compare-Object $roles @('Village','OpenWorld','MainArena')).Count -ne 0){throw '三个正式世界角色不完整'}
if(@($data.AssetFacts).Count -lt 60){throw '未完整盘点模板、图和新手村蓝图'}
if($data.ProductionReady -ne $false -or @($data.PendingUEAcceptance).Count -lt 7){throw '静态审计不得冒充生产通过'}
& $audit -WorkspaceRoot $root -ReportPath $report -RequireProductionReady|Out-Null
if($LASTEXITCODE -ne 2){throw "发布门禁必须失败关闭，实际=$LASTEXITCODE"}
# 静态生成计划必须比较实际Graph绑定，不能只检查软引用是否填写。
$director=Get-Content -LiteralPath (Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Actors/GamePlatformPCGSpatialDirector.cpp') -Raw -Encoding UTF8
foreach($token in @('Graph.LoadSynchronous()','PCGComponent->GetGraph()','PCGComponent->Seed','EPCGComponentGenerationTrigger::GenerateOnDemand')) {
  if(!$director.Contains($token)){throw "静态执行安全检查缺失：$token"}
}
# UE5.8世界冷重载：校验器不得先LoadAsset(UWorld)再EditorLevelLibrary.load_level。
$reopen=Get-Content -LiteralPath (Join-Path $root 'Tools/Unreal/PCG/ValidateGoldLevelAssets.py') -Raw -Encoding UTF8
$guard=$reopen.IndexOf('if class_name == "World":')
$rawLoad=$reopen.IndexOf('obj = assets.load_asset(path)')
$levelSwitch=$reopen.IndexOf('unreal.EditorLevelLibrary.load_level(GOLD_MAP)')
if($guard -lt 0 -or $rawLoad -lt 0 -or $levelSwitch -lt 0 -or $guard -gt $rawLoad -or $rawLoad -gt $levelSwitch) {
  throw 'Gold资产冷重开校验器可能提前将地图作为standalone World加载'
}
# PCG Python编辑器校验必须防止空OutError被误判，并阻止Python异常被0退出码掩盖。
if(!$reopen.Contains('PCG_REOPEN_DIRECTOR_VALIDATED') -or !$reopen.Contains('isinstance(result, str)')){
  throw 'PCG Gold校验器未兼容UE5.8反射OutError返回'
}
$authoring=Get-Content -LiteralPath (Join-Path $root 'Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1') -Raw -Encoding UTF8
foreach($marker in @('PCG_GOLD_DEFINITIONS_SAVED_TOTAL 17','PCG_GOLD_REALIZED_COMPLETED 3','UE_PCG_GOLD_LEVEL_MAP_SAVED','PCG_REOPEN_BASIC_CLASSES_PASS','LogPython: Error:')) {
  if(!$authoring.Contains($marker)){throw "PCG UE Python阶段缺少日志成功门禁：$marker"}
}
Write-Output "PCG_GAME_COVERAGE_TEST_PASS: worlds=3 packages=$(@($data.AssetFacts).Count) strict=blocked"
exit 0
