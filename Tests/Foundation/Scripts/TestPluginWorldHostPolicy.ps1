#requires -Version 7.0
<# 纯合同回归；不运行UE。混合命名空间要求精确选取，不能丢失平台测试。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$groups=@{
    WorldHostEditor=@('DivineBeasts.Demo.WorldHost.ExplicitSelection','GamePlatform.PCG.Editor.ManualRealizedOutputScope',
        'GamePlatform.World.Lifecycle.ReadinessNotifications','GamePlatform.World.Lifecycle.ReadinessReentrantUnsubscribe',
        'GamePlatform.World.Lifecycle.ReadinessWeakOwnerAndWorldExit','DivineBeasts.Foundation.World.ManualHostExclusion',
        'GamePlatform.World.Lifecycle.ReadinessNestedExitTerminal','GamePlatform.World.Lifecycle.ReadinessNestedExitUnsubscribe',
        'DivineBeasts.PluginDemos.World.RealizedProjectOutputScope','GamePlatform.World.Lifecycle.GenerationAndTeardown')
    WorldHostLive=@('DivineBeasts.Demo.WorldLive.ManualHostProviderHandoff','DivineBeasts.Demo.WorldLive.ManualHostPreparingStopAndSuccessor','DivineBeasts.Demo.WorldLive.ManualHostReadyAndNoReinitialize')
    WorldAuthoringLive=@('DivineBeasts.Demo.WorldLive.RealizedAuthoringLeaseAndSave','DivineBeasts.Demo.WorldLive.RealizedAuthoringStopDuringAssetCreated','DivineBeasts.Demo.WorldLive.RealizedProjectRejectBeforeData')
}
$before=[pscustomobject]@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
$process=[pscustomobject]@{ExitCode=0;TimedOut=$false;Cleaned=$true;ProcessId=123}
$checks=0
foreach($suite in $groups.Keys){
    $policy=Get-PluginDemoSuitePolicy $suite
    if(Compare-Object $groups[$suite] $policy.Tests){throw "精确必需集合不匹配：$suite"}
    if((Get-PluginDemoAutomationFilter $suite) -cne ($policy.Tests -join '+')){throw '过滤器不精确'}
    $valid=@($policy.Tests|ForEach-Object{[pscustomobject]@{fullTestPath=$_;state='Success'}})
    if((Assert-PluginDemoRunResult -ProcessResult $process -Cases $valid -Before $before -After $before -Suite $suite) -ne $valid.Count){throw '跨命名空间必需项丢失'}
    $checks++
    $variants=@(@{Cases=@($valid|Select-Object -Skip 1)},@{Cases=@($valid+$valid[0])},@{Cases=@($valid+[pscustomobject]@{fullTestPath='Foreign.Unselected';state='Success'})})
    foreach($state in @('Fail','Skipped','NotRun')){
        $bad=@($valid|ForEach-Object{[pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state}});$bad[0].state=$state
        $variants+=@{Cases=$bad}
    }
    foreach($variant in $variants){
        $rejected=$false;try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases @($variant.Cases) -Before $before -After $before -Suite $suite}catch{$rejected=$true}
        if(!$rejected){throw "组接受缺失/重复/外来/非成功项：$suite"};$checks++
    }
}
Write-Output "PluginWorldHostPolicyCases=$checks Failed=0 UEExecuted=False"
