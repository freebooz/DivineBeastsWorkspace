#requires -Version 7.0
<# T05完整原生门禁：三个实际宿主分组缺一不可；本入口不构建、不Cook，不签人工结论。
   只有Execute才调用既有受控UE进程入口；VerifyOnly仅核对完整Editor源码/模块证据。
   Live使用UnrealEditor.exe -game产生真实Standalone ClientContext，不表示已验证打包Client。 #>
param([switch]$Execute,[switch]$VerifyOnly,[string]$EngineRoot,[string]$BuildResultPath,
    [guid]$RunId=[guid]::NewGuid(),[ValidateRange(1,86400)][int]$TimeoutSeconds=900)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoEvidence.psm1') -Force
if($Execute -eq $VerifyOnly){throw '必须且只能选择Execute或VerifyOnly。'}
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($RunId -eq [guid]::Empty){throw 'RunId不能为零GUID。'}
$directory=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/PresentationNative/$($RunId.ToString('D'))"
if($Execute -and (Test-Path -LiteralPath $directory)){throw '拒绝复用已有T05汇总证据目录；尚未启动UE。'}
$engine=Get-FoundationEngine $EngineRoot
$expected=@('Presentation','PresentationTheme','PresentationLive');$runs=@();$allCases=@()
foreach($suite in $expected){
    $child=[guid]::NewGuid()
    # 子入口exit只退出此独占PowerShell子进程；不停止其他UE/Editor，不把工具输出当测试成功。
    $args=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'TestPluginDemo.ps1'),'-EngineRoot',$engine.Root,'-BuildResultPath',$BuildResultPath,'-Suite',$suite,'-RunId',$child.ToString('D'),'-TimeoutSeconds',"$TimeoutSeconds")
    if($Execute){$args+='-Execute'}else{$args+='-VerifyOnly'}
    & (Join-Path $PSHOME 'pwsh.exe') @args
    if($LASTEXITCODE -ne 0){throw "T05必需分组失败：$suite Exit=$LASTEXITCODE"}
    if($Execute){
        $path="Saved/Validation/FoundationM0/$($child.ToString('D'))/PluginDemoTests/result.json"
        $read=Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine.Root -ResultPath $path -Suite $suite
        $runs+=@{Suite=$suite;RunId=$child.ToString('D');Report=$path;Tests=@($read.Cases.fullTestPath);ValidatedTests=$read.Result.ValidatedTests}
        $allCases+=@($read.Cases)
    }
}
if($VerifyOnly){Write-Output 'T05三个必需分组的完整Editor模块证明通过；UE测试均未执行。';exit 0}
$required=@('DivineBeasts.Demo.Presentation.Contract','DivineBeasts.Demo.Presentation.FullHandle','DivineBeasts.Demo.Presentation.ParameterBoundary',
    'GamePlatform.UI.Theme.SnapshotEmptyAndConflict','DivineBeasts.Demo.Presentation.LiveThemeLifecycle','DivineBeasts.Demo.Presentation.LivePlaybackStopReentry',
    'DivineBeasts.Demo.Presentation.LiveScreenTerminals','DivineBeasts.Demo.Presentation.LiveThemeStopReentry')
foreach($name in $required){$records=@($allCases | Where-Object {$_.fullTestPath -ceq $name});if($records.Count -ne 1 -or $records[0].state -cne 'Success'){throw "T05必需测试缺失/重复/跳过/失败：$name"}}
$record=@{SchemaVersion=1;RunId=$RunId.ToString('D');Status='Passed';RequiredTests=$required;Runs=$runs;BuildExecuted=$false;UEExecuted=$true;Host='Editor plus EditorGameStandaloneClientContext';PackagedClientVerified=$false;VisualAudioHumanAcceptance='Pending'}
Save-PluginDemoEvidenceJson -Root $directory -Relative 'result.json' -Value $record
