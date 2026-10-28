#requires -Version 7.0
<# 世界人工页19条原生门禁：Editor九项与真正Standalone十项必须各出现一次Success。
   不构建/不Cook、不签人工；只调用既有独占进程和完整构建前后门禁。 #>
param([switch]$Execute,[switch]$VerifyOnly,[string]$EngineRoot,[string]$BuildResultPath,
    [guid]$RunId=[guid]::NewGuid(),[ValidateRange(1,86400)][int]$TimeoutSeconds=900)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoValidation.psm1') -Force
if($Execute -eq $VerifyOnly){throw '必须且只能选择Execute或VerifyOnly。'}
if($RunId -eq [guid]::Empty){throw 'RunId不能为零GUID。'}
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$directory=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/WorldNative/$($RunId.ToString('D'))"
if($Execute -and (Test-Path -LiteralPath $directory)){throw '不复用已有世界原生汇总目录。'}
$engine=Get-FoundationEngine $EngineRoot
$required=@((Get-PluginDemoSuitePolicy Demo).Tests|Where-Object{$_.StartsWith('DivineBeasts.PluginDemos.World.',[StringComparison]::Ordinal)})
$required+=@((Get-PluginDemoSuitePolicy WorldLive).Tests)
if($required.Count -ne 19){throw '世界原生门禁必须为九项Editor及十项Standalone。'}
$runs=@();$allCases=@()
foreach($suite in @('Demo','WorldLive')){
    $child=[guid]::NewGuid()
    $arguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'TestPluginDemo.ps1'),'-EngineRoot',$engine.Root,
        '-BuildResultPath',$BuildResultPath,'-Suite',$suite,'-RunId',$child.ToString('D'),'-TimeoutSeconds',"$TimeoutSeconds")
    if($Execute){$arguments+='-Execute'}else{$arguments+='-VerifyOnly'}
    & (Join-Path $PSHOME 'pwsh.exe') @arguments
    if($LASTEXITCODE -ne 0){throw "世界必需分组失败：$suite Exit=$LASTEXITCODE"}
    if($Execute){
        $path="Saved/Validation/FoundationM0/$($child.ToString('D'))/PluginDemoTests/result.json"
        $read=Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine.Root -ResultPath $path -Suite $suite
        $selected=@($read.Cases|Where-Object{$_.fullTestPath -cin $required})
        $runs+=@{Suite=$suite;RunId=$child.ToString('D');Report=$path;WorldTests=@($selected.fullTestPath);ValidatedAllTests=$read.Result.ValidatedTests}
        $allCases+=@($selected)
    }
}
if($VerifyOnly){Write-Output '世界两个分组完整Editor模块证据通过；原生未运行。';exit 0}
foreach($name in $required){
    $records=@($allCases|Where-Object{$_.fullTestPath -ceq $name})
    if($records.Count -ne 1 -or $records[0].state -cne 'Success'){throw "世界必需项缺失/重复/跳过/失败：$name"}
}
$record=@{SchemaVersion=1;RunId=$RunId.ToString('D');Status='Passed';RequiredTests=$required;Runs=$runs;
    BuildExecuted=$false;UEExecuted=$true;Host='Editor plus EditorGameStandaloneClientContext';PackagedClientVerified=$false;
    Scope='WorldFix2_19_exact_native_cases';WorldFixtureHostVerified=$false;RealizedGraphAuthoringVerified=$false;VisualAudioHumanAcceptance='Pending'}
Save-PluginDemoEvidenceJson -Root $directory -Relative 'result.json' -Value $record
