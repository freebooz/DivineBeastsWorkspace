#requires -Version 7.0
<# 合成策略检查：SHA256成功路径须精确出现，不是UE原生证据。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$groups=@{
    EvidenceHash=@('GamePlatform.Core.SHA256','DivineBeasts.PluginDemos.Evidence','GamePlatform.DeveloperTools.ReviewEvidence')
    EvidenceHashLive=@('GamePlatform.Core.SHA256','DivineBeasts.PluginDemos.Evidence')
}
$build=[pscustomobject]@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
$process=[pscustomobject]@{ExitCode=0;ProcessId=123;Cleaned=$true;TimedOut=$false};$checks=0
foreach($suite in $groups.Keys){
    $policy=Get-PluginDemoSuitePolicy $suite
    if(Compare-Object $policy.Tests $groups[$suite]){throw '哈希原生必需集合不匹配。'}
    if((Get-PluginDemoAutomationFilter $suite) -cne ($policy.Tests -join '+')){throw '哈希原生过滤器不精确。'}
    $valid=@($policy.Tests|ForEach-Object{[pscustomobject]@{fullTestPath=$_;state='Success'}})
    if((Assert-PluginDemoRunResult -ProcessResult $process -Cases $valid -Before $build -After $build -Suite $suite) -ne $valid.Count){throw '哈希成功集合未接受。'};$checks++
    $variants=@(@{Cases=@($valid|Select-Object -Skip 1)},@{Cases=@($valid+$valid[0])},@{Cases=@($valid+[pscustomobject]@{fullTestPath='Unselected.Extra';state='Success'})})
    foreach($state in @('Fail','Skipped','NotRun')){$bad=@($valid|ForEach-Object{[pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state}});$bad[0].state=$state;$variants+=@{Cases=$bad}}
    foreach($variant in $variants){$rejected=$false;try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases @($variant.Cases) -Before $build -After $build -Suite $suite}catch{$rejected=$true};if(!$rejected){throw '哈希门禁接受不合格原生集合。'};$checks++}
}
Write-Output "PluginEvidenceHashPolicyCases=$checks Failed=0 UEExecuted=False"
