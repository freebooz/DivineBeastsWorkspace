#requires -Version 7.0
<# SSLM演示测试只读合同；工具层复用Foundation报告校验，不启动进程、不构建、不签人工结论。
   调用方为TestPluginDemo及隔离脚本测试；原始报告/源码指纹由调用者负责真实获取。 #>
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../Validation/FoundationAutomationReport.psm1')
function Get-PluginDemoSuitePolicy {
    <# 返回当前必需完整测试身份和模块；不得以旧二进制仅含的子集覆盖新清单。 #>
    param([ValidateSet('Demo','Review','LaunchPolicy')][string]$Suite='Demo')
    switch ($Suite) {
        Demo { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.';Modules=@('DivineBeastsDemoClient','DivineBeastsFlowRuntime','GamePlatformData','GamePlatformUIClient');Tests=@('Definition','Catalog','Lifecycle','Menu','BirdScenario','Samples','Evidence','LaunchPolicy') | ForEach-Object { 'DivineBeasts.PluginDemos.'+$_ }} }
        Review { [pscustomobject]@{Prefix='GamePlatform.DeveloperTools.';Modules=@('GamePlatformDeveloperTools');Tests=@('ReviewEvidence','Rules','Allowlist','ReportPath','ManualReview','PerformanceBaseline','Naming.BlueprintParentPrefixes','DefinitionIdentityContract') | ForEach-Object { 'GamePlatform.DeveloperTools.'+$_ }} }
        LaunchPolicy { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.';Modules=@('DivineBeastsFlowRuntime');Tests=@('DivineBeasts.PluginDemos.LaunchPolicy')} }
    }
}
function Assert-PluginDemoRunResult {
    <# 纯值门禁：进程必须真实退出，测试须完整成功，测试前后输入/产物必须一致。
       测试替身只在隔离回归中使用；本函数通过不等于产生了真实UE报告。 #>
    param($ProcessResult,$Cases,$Before,$After,[ValidateSet('Demo','Review','LaunchPolicy')][string]$Suite='Demo')
    if ($ProcessResult.ExitCode -ne 0 -or $ProcessResult.TimedOut -ne $false -or $ProcessResult.Cleaned -ne $true -or $ProcessResult.ProcessId -le 0) { throw '原生测试进程失败、超时、未清理或身份缺失。' }
    foreach ($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) {
        if ([string]$Before.$field -cnotmatch '^[A-Fa-f0-9]{64}$' -or $Before.$field -cne $After.$field) { throw "运行期间构建/源码/产物变化：$field" }
    }
    $policy=Get-PluginDemoSuitePolicy $Suite
    Assert-FoundationAutomationReport -Cases @($Cases) -RequiredPaths $policy.Tests -Prefix $policy.Prefix
}
Export-ModuleMember -Function Get-PluginDemoSuitePolicy,Assert-PluginDemoRunResult
