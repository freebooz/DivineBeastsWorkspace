#requires -Version 7.0
<# 只验证精确原生门禁集合，合成记录不是UE功能证据。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$prefix='DivineBeasts.PluginDemos.Gameplay.'
$groups=@{
    Gameplay=@('Contract','ParameterBoundary','SourceIsolation','Fix1.RequiredOperations','NativeFix1.RuntimeInputTag')|ForEach-Object{$prefix+$_}
    GameplayLive=@('Live.CombatLifecycle','Live.StopDuringDamage','Live.UniqueCharacterInitialization','Fix1.Live.HeroSelection',
        'Fix1.Live.AbilitySetSelection','Fix1.Live.AISelection','Fix1.Live.ProjectileCases','Fix2.Live.TypedCompletionStop','Fix2.Live.TypedCompletionReset',
        'NativeFix1.Live.TagInput','NativeFix1.Live.EnergyCommit')|ForEach-Object{$prefix+$_}
    GameplayHost=@($prefix+'Fix1.Live.HostLifecycle')
    GameplayHostStop=@($prefix+'Fix1.Live.HostStopReentry')
    PreparingStopLive=@('EventSuccessor','PendingResourceSuccessor','EnteredPrepareRetry','WorldExitPending','WorldExitEntered')|ForEach-Object{'DivineBeasts.Demo.WorldLive.PreparingStop.'+$_}
}
$build=[pscustomobject]@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
$process=[pscustomobject]@{ExitCode=0;ProcessId=123;Cleaned=$true;TimedOut=$false};$checks=0
foreach($suite in $groups.Keys){
    $policy=Get-PluginDemoSuitePolicy $suite
    if(Compare-Object $policy.Tests $groups[$suite]){throw "必需集合不匹配：$suite"}
    if((Get-PluginDemoAutomationFilter $suite) -cne ($policy.Tests -join '+')){throw '原生过滤器不精确。'}
    $valid=@($policy.Tests|ForEach-Object{[pscustomobject]@{fullTestPath=$_;state='Success'}})
    if((Assert-PluginDemoRunResult -ProcessResult $process -Cases $valid -Before $build -After $build -Suite $suite) -ne $valid.Count){throw '正确集合未接受。'};$checks++
    $variants=@(@{Cases=@($valid|Select-Object -Skip 1)},@{Cases=@($valid+$valid[0])},@{Cases=@($valid+[pscustomobject]@{fullTestPath='Unselected.Extra';state='Success'})})
    foreach($state in @('Fail','Skipped','NotRun')){$bad=@($valid|ForEach-Object{[pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state}});$bad[0].state=$state;$variants+=@{Cases=$bad}}
    foreach($variant in $variants){$rejected=$false;try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases @($variant.Cases) -Before $build -After $build -Suite $suite}catch{$rejected=$true};if(!$rejected){throw '门禁接受缺失/重复/外来或非成功项。'};$checks++}
}
$hub='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'
foreach($suite in @('GameplayHost','GameplayHostStop')){
    if((Get-PluginDemoMapURL $suite) -cne ($hub+'?game=/Script/DivineBeastsDemoClient.DivineBeastsDemoGameplayHostMode')){throw '专用玩法Host缺精确URL模式。'}
    foreach($flag in @('-FoundationWorld','-DBAManualWorldHost','-DBAManualGameplayHost','-FoundationGameplayOffline')){if($flag -cnotin @(Get-PluginDemoHostFlags $suite)){throw '玩法Host缺显式旗标。'}};$checks++
}
foreach($suite in @('GameplayLive','PreparingStopLive')){if((Get-PluginDemoMapURL $suite) -cne $hub -or @(Get-PluginDemoHostFlags $suite).Count -ne 0){throw '普通Standalone回归误选世界Host。'};$checks++}
Write-Output "PluginGameplayPolicyCases=$checks Failed=0 UEExecuted=False"
