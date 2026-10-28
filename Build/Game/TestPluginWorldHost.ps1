#requires -Version 7.0
<# 世界宿主15新增项加既有跨World生命周期回归，共16项真实门禁。 #>
param([switch]$Execute,[switch]$VerifyOnly,[string]$EngineRoot,[string]$BuildResultPath,
    [guid]$RunId=[guid]::NewGuid(),[ValidateRange(1,86400)][int]$TimeoutSeconds=900)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoValidation.psm1') -Force
if($Execute -eq $VerifyOnly){throw '必须且只能指定Execute或VerifyOnly。'}
if($RunId -eq [guid]::Empty){throw 'RunId不能为空。'}
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$directory=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/WorldHostNative/$($RunId.ToString('D'))"
if($Execute -and (Test-Path -LiteralPath $directory)){throw '不复用已有宿主原生汇总目录。'}
$engine=Get-FoundationEngine $EngineRoot
$required=@();$allCases=@();$runs=@()
foreach($suite in @('WorldHostEditor','WorldHostLive','WorldAuthoringLive')){
    $required+=@((Get-PluginDemoSuitePolicy $suite).Tests)
    $child=[guid]::NewGuid()
    $arguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'TestPluginDemo.ps1'),'-Suite',$suite,
        '-EngineRoot',$engine.Root,'-BuildResultPath',$BuildResultPath,'-RunId',$child.ToString('D'),'-TimeoutSeconds',"$TimeoutSeconds")
    $arguments+=if($Execute){'-Execute'}else{'-VerifyOnly'}
    & (Join-Path $PSHOME 'pwsh.exe') @arguments
    if($LASTEXITCODE -ne 0){throw "专用世界必需分组失败：$suite Exit=$LASTEXITCODE"}
    if($Execute){
        $path="Saved/Validation/FoundationM0/$($child.ToString('D'))/PluginDemoTests/result.json"
        $read=Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine.Root -ResultPath $path -Suite $suite
        $allCases+=@($read.Cases)
        $runs+=@{Suite=$suite;RunId=$child.ToString('D');Report=$path;RequiredTests=@((Get-PluginDemoSuitePolicy $suite).Tests);ValidatedTests=$read.Result.ValidatedTests}
    }
}
if($VerifyOnly){Write-Output '三个完整Editor模块门禁通过；未运行UE。';exit 0}
if($required.Count -ne 16 -or $allCases.Count -ne 16){throw '世界宿主门禁必须为精确16项（15新增及既有跨World生命周期）。'}
foreach($path in $required){
    $found=@($allCases|Where-Object{$_.fullTestPath -ceq $path})
    if($found.Count -ne 1 -or $found[0].state -cne 'Success'){throw "世界宿主必需项缺失/重复/失败：$path"}
}
$record=@{SchemaVersion=1;RunId=$RunId.ToString('D');Status='Passed';RequiredTests=$required;Runs=$runs;
    BuildExecuted=$false;UEExecuted=$true;PackagedClientVerified=$false;Scope='WorldHost_15_new_plus_existing_world_lifecycle';
    RealizedSpatialBakeVerified=$false;VisualAudioHumanAcceptance='Pending'}
Save-PluginDemoEvidenceJson -Root $directory -Relative 'result.json' -Value $record
