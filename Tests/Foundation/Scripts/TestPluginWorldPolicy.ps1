#requires -Version 7.0
<# 世界测试门禁的纯值回归；合成结果仅验证拒绝策略，不是UE执行证据。 #>
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$editorNames=@('ContractRejectsInjectedOperations','FactoryRejectsForeignFeature','FeatureWhitelist','FullRunIdentity',
    'IdentifierAndAssetTypes','InteractionArgumentDefaults','NonFiniteAndRange','RotatedWaterAndClearedMiss','WrongWorldThenStop')
$liveNames=@('DefinitionDependencyRetry','InteractionTargetDefaults','LinkSpawnStopReentry','NavigationProfileOwnership',
    'PCGComponentOwnership','TelemetryExactRestore','WaterQualityRestore','WeatherOwnedCleanup',
    'WeatherPreservesSuccessorProjection','WeatherSpawnStopReentry')
$required=@($liveNames|ForEach-Object{'DivineBeasts.PluginDemos.World.Standalone.'+$_})
$policy=Get-PluginDemoSuitePolicy WorldLive
if($policy.Host -cne 'EditorGameStandaloneClientContext' -or $policy.Tests.Count -ne 10 -or
    (Compare-Object $policy.Tests $required)){throw 'WorldLive必须为精确10项真实Standalone集合。'}
$filter=Get-PluginDemoAutomationFilter WorldLive
if($filter -cne ($required -join '+')){throw 'WorldLive实际过滤器与必需项不相同。'}
$demo=Get-PluginDemoSuitePolicy Demo
foreach($name in $editorNames){if(('DivineBeasts.PluginDemos.World.'+$name) -cnotin $demo.Tests){throw "Demo缺必需World Editor项：$name"}}
$before=[pscustomobject]@{BuildResultSHA256=('a'*64);InputSHA256=('b'*64);ArtifactSHA256=('c'*64)}
$process=[pscustomobject]@{ExitCode=0;TimedOut=$false;Cleaned=$true;ProcessId=123}
$valid=@($required|ForEach-Object{[pscustomobject]@{fullTestPath=$_;state='Success'}})
if((Assert-PluginDemoRunResult -ProcessResult $process -Cases $valid -Before $before -After $before -Suite WorldLive) -ne 10){throw '正确集合未接受。'}
$checks=4
function Reject([object[]]$Cases,$After=$before){
    $caught=$false
    try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases $Cases -Before $before -After $After -Suite WorldLive}catch{$caught=$true}
    if(!$caught){throw '不完整/跳过/旧输入的World门禁被接受。'}
    $script:checks++
}
foreach($name in $required){Reject @($valid|Where-Object fullTestPath -cne $name)}
foreach($status in @('Fail','NotRun','Skipped')){
    $bad=@($valid|ForEach-Object{[pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state}})
    $bad[0].state=$status;Reject $bad
}
Reject ($valid+$valid[0])
Reject @()
foreach($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')){
    $after=[pscustomobject]@{BuildResultSHA256=$before.BuildResultSHA256;InputSHA256=$before.InputSHA256;ArtifactSHA256=$before.ArtifactSHA256}
    $after.$field='d'*64;Reject $valid $after
}
Write-Output "PluginWorldPolicyCases=$checks Failed=0 UEExecuted=False BuildExecuted=False"
