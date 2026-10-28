#requires -Version 7.0
<# SSLM演示测试只读合同；工具层复用Foundation报告校验，不启动进程、不构建、不签人工结论。
   调用方为TestPluginDemo及隔离脚本测试；原始报告/源码指纹由调用者负责真实获取。 #>
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../Validation/FoundationAutomationReport.psm1')
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1')
function Get-PluginDemoSuitePolicy {
    <# 返回当前必需完整测试身份和模块；不得以旧二进制仅含的子集覆盖新清单。 #>
    param([ValidateSet('Demo','Review','LaunchPolicy','Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive','Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive','EditorManual')][string]$Suite='Demo')
    switch ($Suite) {
        Demo { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformWaterRuntime');Tests=@('Definition','Catalog','Lifecycle','Menu','BirdScenario','Samples','Evidence','LaunchPolicy','World.ContractRejectsInjectedOperations','World.FactoryRejectsForeignFeature','World.FeatureWhitelist','World.FullRunIdentity','World.IdentifierAndAssetTypes','World.InteractionArgumentDefaults','World.NonFiniteAndRange','World.RotatedWaterAndClearedMiss','World.WrongWorldThenStop') | ForEach-Object { 'DivineBeasts.PluginDemos.'+$_ }} }
        Review { [pscustomobject]@{Prefix='GamePlatform.DeveloperTools.';Modules=@('GamePlatformDeveloperTools');Tests=@('ReviewEvidence','Rules','Allowlist','ReportPath','ManualReview','PerformanceBaseline','Naming.BlueprintParentPrefixes','DefinitionIdentityContract') | ForEach-Object { 'GamePlatform.DeveloperTools.'+$_ }} }
        Presentation { [pscustomobject]@{Prefix='DivineBeasts.Demo.Presentation.';Host='Editor';Modules=@('DivineBeastsDemoClient','GamePlatformSFXClient','GamePlatformUIClient');Tests=@('Contract','FullHandle','ParameterBoundary') | ForEach-Object { 'DivineBeasts.Demo.Presentation.'+$_ }} }
        PresentationTheme { [pscustomobject]@{Prefix='GamePlatform.UI.Theme.';Host='Editor';Modules=@('GamePlatformUIClient');Tests=@('GamePlatform.UI.Theme.SnapshotEmptyAndConflict')} }
        PresentationLive { [pscustomobject]@{Prefix='DivineBeasts.Demo.Presentation.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformPresentationCore','GamePlatformPresentationClient','GamePlatformVFXClient','GamePlatformSFXClient','GamePlatformCameraClient','GamePlatformSurfaceClient','DBAWorldsRuntime');Tests=@('LiveThemeLifecycle','LivePlaybackStopReentry','LiveScreenTerminals','LiveThemeStopReentry') | ForEach-Object { 'DivineBeasts.Demo.Presentation.'+$_ }} }
        WorldLive { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.World.Standalone.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformWorld','GamePlatformWeatherRuntime','GamePlatformWeatherClient','GamePlatformWaterRuntime','GamePlatformWaterClient','GamePlatformPCG','GamePlatformPCGEditor','GamePlatformNavigation','GamePlatformNavigationServer','GamePlatformInteraction','GamePlatformTelemetry','GamePlatformSurfaceClient','DBAWorldsRuntime');Tests=@('DefinitionDependencyRetry','InteractionTargetDefaults','LinkSpawnStopReentry','NavigationProfileOwnership','PCGComponentOwnership','TelemetryExactRestore','WaterQualityRestore','WeatherOwnedCleanup','WeatherPreservesSuccessorProjection','WeatherSpawnStopReentry') | ForEach-Object { 'DivineBeasts.PluginDemos.World.Standalone.'+$_ }} }
        WorldHostEditor { [pscustomobject]@{Prefix='DivineBeasts.Demo.WorldLive.';Host='Editor';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','DivineBeastsArena','GamePlatformWorld','GamePlatformPCG','GamePlatformPCGEditor','GamePlatformData','GamePlatformUIClient');Tests=@('DivineBeasts.Demo.WorldHost.ExplicitSelection','GamePlatform.PCG.Editor.ManualRealizedOutputScope','GamePlatform.World.Lifecycle.ReadinessNotifications','GamePlatform.World.Lifecycle.ReadinessReentrantUnsubscribe','GamePlatform.World.Lifecycle.ReadinessWeakOwnerAndWorldExit','DivineBeasts.Foundation.World.ManualHostExclusion','GamePlatform.World.Lifecycle.ReadinessNestedExitTerminal','GamePlatform.World.Lifecycle.ReadinessNestedExitUnsubscribe','DivineBeasts.PluginDemos.World.RealizedProjectOutputScope','GamePlatform.World.Lifecycle.GenerationAndTeardown')} }
        WorldHostLive { [pscustomobject]@{Prefix='DivineBeasts.Demo.WorldLive.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','DivineBeastsArena','GamePlatformWorld','GamePlatformPCG','GamePlatformPCGEditor','GamePlatformData','GamePlatformUIClient');Tests=@('DivineBeasts.Demo.WorldLive.ManualHostProviderHandoff','DivineBeasts.Demo.WorldLive.ManualHostPreparingStopAndSuccessor','DivineBeasts.Demo.WorldLive.ManualHostReadyAndNoReinitialize')} }
        WorldAuthoringLive { [pscustomobject]@{Prefix='DivineBeasts.Demo.WorldLive.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','DivineBeastsArena','GamePlatformWorld','GamePlatformPCG','GamePlatformPCGEditor','GamePlatformData','GamePlatformUIClient');Tests=@('DivineBeasts.Demo.WorldLive.RealizedAuthoringLeaseAndSave','DivineBeasts.Demo.WorldLive.RealizedAuthoringStopDuringAssetCreated','DivineBeasts.Demo.WorldLive.RealizedProjectRejectBeforeData')} }
        EvidenceHash { [pscustomobject]@{Prefix='GamePlatform.Core.';Host='Editor';Modules=@('GamePlatformCore','DivineBeastsDemoClient','GamePlatformDeveloperTools');Tests=@('GamePlatform.Core.SHA256','DivineBeasts.PluginDemos.Evidence','GamePlatform.DeveloperTools.ReviewEvidence')} }
        EvidenceHashLive { [pscustomobject]@{Prefix='GamePlatform.Core.';Host='EditorGameStandaloneClientContext';Modules=@('GamePlatformCore','DivineBeastsDemoClient');Tests=@('GamePlatform.Core.SHA256','DivineBeasts.PluginDemos.Evidence')} }
        Gameplay { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.Gameplay.';Host='Editor';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformGameplay','GamePlatformCharacter','GamePlatformAbilitySystem','GamePlatformCombat','GamePlatformAnimationClient','GamePlatformAI','GamePlatformQuest','GamePlatformQuestClient','DivineBeastsCharactersRuntime','DBAWorldsRuntime','GamePlatformWorld','GamePlatformAIServer','GamePlatformQuestServer');Tests=@('Contract','ParameterBoundary','SourceIsolation','Fix1.RequiredOperations','NativeFix1.RuntimeInputTag') | ForEach-Object { 'DivineBeasts.PluginDemos.Gameplay.'+$_ } } }
        GameplayLive { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.Gameplay.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformGameplay','GamePlatformCharacter','GamePlatformAbilitySystem','GamePlatformCombat','GamePlatformAnimationClient','GamePlatformAI','GamePlatformQuest','GamePlatformQuestClient','DivineBeastsCharactersRuntime','DBAWorldsRuntime','GamePlatformWorld');Tests=@('Live.CombatLifecycle','Live.StopDuringDamage','Live.UniqueCharacterInitialization','Fix1.Live.HeroSelection','Fix1.Live.AbilitySetSelection','Fix1.Live.AISelection','Fix1.Live.ProjectileCases','Fix2.Live.TypedCompletionStop','Fix2.Live.TypedCompletionReset','NativeFix1.Live.TagInput','NativeFix1.Live.EnergyCommit') | ForEach-Object { 'DivineBeasts.PluginDemos.Gameplay.'+$_ } } }
        GameplayHost { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.Gameplay.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformGameplay','GamePlatformCharacter','GamePlatformAbilitySystem','GamePlatformCombat','GamePlatformAnimationClient','GamePlatformAI','GamePlatformQuest','GamePlatformQuestClient','DivineBeastsCharactersRuntime','DBAWorldsRuntime','GamePlatformWorld');Tests=@('DivineBeasts.PluginDemos.Gameplay.Fix1.Live.HostLifecycle')} }
        GameplayHostStop { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.Gameplay.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformGameplay','GamePlatformCharacter','GamePlatformAbilitySystem','GamePlatformCombat','GamePlatformAnimationClient','GamePlatformAI','GamePlatformQuest','GamePlatformQuestClient','DivineBeastsCharactersRuntime','DBAWorldsRuntime','GamePlatformWorld');Tests=@('DivineBeasts.PluginDemos.Gameplay.Fix1.Live.HostStopReentry')} }
        PreparingStopLive { [pscustomobject]@{Prefix='DivineBeasts.Demo.WorldLive.PreparingStop.';Host='EditorGameStandaloneClientContext';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformCore','GamePlatformWorld','GamePlatformUIClient');Tests=@('EventSuccessor','PendingResourceSuccessor','EnteredPrepareRetry','WorldExitPending','WorldExitEntered') | ForEach-Object { 'DivineBeasts.Demo.WorldLive.PreparingStop.'+$_ } } }
        EditorManual { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.Editor.';Host='EditorPIE';Modules=@('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient','GamePlatformCore','GamePlatformDeveloperTools','BlueprintAutoLayout','GlooPrint','GamePlatformPCGEditor','GamePlatformPCG','GamePlatformSurfaceEditor','GamePlatformSurfaceClient','GamePlatformWorld','DBAWorldsRuntime');Tests=@('ContractClosed','PIE.CentralRunnerRetainedCleanup','PIE.HostEligibilityAndLease','PIE.NineVendorLifecycle','PIE.StopReentryAndWorldExit','PIE.TwoEngineGameInstancesExclusive','Vendor.OwnedGraphAndConflict') | ForEach-Object { 'DivineBeasts.PluginDemos.Editor.'+$_ }} }
        LaunchPolicy { [pscustomobject]@{Prefix='DivineBeasts.PluginDemos.';Modules=@('DivineBeastsApplicationFlowClient');Tests=@('DivineBeasts.PluginDemos.LaunchPolicy')} }
    }
}
function Get-PluginDemoNativeFlag {
    <# 精确原生宿主使用自己的显式夹具旗标；普通合同组拒绝调用。 #>
    param([string]$Suite)
    switch($Suite){
        PresentationLive {'-PresentationManualNative'}
        {$_ -in @('WorldLive','WorldHostLive','WorldAuthoringLive')} {'-WorldManualNative'}
        EvidenceHashLive {'-EvidenceManualNative'}
        {$_ -in @('GameplayLive','GameplayHost','GameplayHostStop')} {'-GameplayManualNative'}
        PreparingStopLive {'-WorldManualNative'}
        EditorManual {'-GamePlatformManualEditorFixture'}
        default {throw '该Suite不是已登记Live宿主。'}
    }
}
function Get-PluginDemoMapURL {
    <# 固定开发地图；Gameplay专用宿主通过显式URL类覆盖，不修改Hub资产或正式GameMode。 #>
    param([string]$Suite)
    $null=Get-PluginDemoSuitePolicy $Suite
    $map='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'
    if($Suite -in @('GameplayHost','GameplayHostStop')){return $map+'?game=/Script/DivineBeastsDemoClient.DivineBeastsDemoGameplayHostMode'}
    return $map
}
function Get-PluginDemoHostFlags {
    <# 专用离线宿主唯一参数合同，普通Live组不隐式初始化World或Experience。 #>
    param([string]$Suite)
    $null=Get-PluginDemoSuitePolicy $Suite
    switch($Suite){
        WorldHostLive { '-FoundationWorld';'-DBAManualWorldHost' }
        {$_ -in @('GameplayHost','GameplayHostStop')} { '-FoundationWorld';'-DBAManualWorldHost';'-DBAManualGameplayHost';'-FoundationGameplayOffline' }
    }
}
function Get-PluginDemoAutomationFilter {
    param([ValidateSet('Demo','Review','LaunchPolicy','Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive','Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive','EditorManual')][string]$Suite='Demo')
    $policy=Get-PluginDemoSuitePolicy $Suite
    if($Suite -in @('Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive','Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive','EditorManual')) { return ($policy.Tests -join '+') }
    $policy.Prefix.TrimEnd('.')
}
function Assert-PluginDemoRunResult {
    <# 纯值门禁：进程必须真实退出，测试须完整成功，测试前后输入/产物必须一致。
       测试替身只在隔离回归中使用；本函数通过不等于产生了真实UE报告。 #>
    param($ProcessResult,$Cases,$Before,$After,[ValidateSet('Demo','Review','LaunchPolicy','Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive','Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive','EditorManual')][string]$Suite='Demo')
    if ($ProcessResult.ExitCode -ne 0 -or $ProcessResult.TimedOut -ne $false -or $ProcessResult.Cleaned -ne $true -or $ProcessResult.ProcessId -le 0) { throw '原生测试进程失败、超时、未清理或身份缺失。' }
    foreach ($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) {
        if ([string]$Before.$field -cnotmatch '^[A-Fa-f0-9]{64}$' -or $Before.$field -cne $After.$field) { throw "运行期间构建/源码/产物变化：$field" }
    }
    $policy=Get-PluginDemoSuitePolicy $Suite
    if($Suite -in @('WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive','Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive','EditorManual')) {
        # 精确过滤器跨平台/项目命名空间；拒绝额外、重复、缺失及任何非Success记录。
        if(@($Cases).Count -ne $policy.Tests.Count){throw '专用World组实际记录数与精确集合不匹配。'}
        foreach($path in $policy.Tests){
            $found=@($Cases|Where-Object{$_.fullTestPath -ceq $path})
            if($found.Count -ne 1 -or $found[0].state -cne 'Success'){throw "专用World必需项缺失/重复/失败：$path"}
        }
        return $policy.Tests.Count
    }
    Assert-FoundationAutomationReport -Cases @($Cases) -RequiredPaths $policy.Tests -Prefix $policy.Prefix
}
function Assert-PluginDemoNoEngineFailure {
    <# Automation单项成功不能覆盖模块启动或运行中的引擎Ensure/断言/致命错误。
       只匹配引擎错误通道，不把测试描述/刻意负向数据校验的一般Error当引擎故障。 #>
    param([AllowEmptyCollection()][string[]]$Lines)
    if(-not $Lines -or $Lines.Count -eq 0){throw '原生引擎日志为空。'}
    foreach($line in $Lines){
        if($line -match 'Log(?:OutputDevice|Windows): (?:Error|Fatal): .*(?:Ensure condition failed:|Assertion failed:|Fatal error:|LowLevelFatalError)'){
            throw "原生引擎日志存在Ensure/断言/致命错误；单项Success不具备干净运行资格：$line"
        }
    }
    return $Lines.Count
}
function Write-PluginDemoTestOutput {
    <# 工具层最终收口：先完成调用者明确请求的追加摘要，再发布唯一canonical结果。
       摘要冲突/写失败令最终结果Failed，旧摘要不覆盖；不启动进程、不构建，不改真实原生用例状态。
       Context仅允许调用者本次独占Foundation上下文；空Context用于未执行/VerifyOnly信息，不伪造canonical。 #>
    param($Context,[int]$ExitCode,[string]$Message,[hashtable]$Details,[guid]$RunId,[string]$ReportPath)
    $finalCode=$ExitCode; $finalMessage=$Message
    if ($ReportPath) {
        try {
            $full=[IO.Path]::GetFullPath($ReportPath)
            if ($Context -and $full -ieq [IO.Path]::GetFullPath((Join-Path $Context.Directory 'result.json'))) { throw '追加摘要不能占用本次canonical结果路径。' }
            $stream=[IO.File]::Open($full,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try {
                $bytes=[Text.UTF8Encoding]::new($false).GetBytes((@{RunId=$RunId.ToString('D');ExitCode=$finalCode;Message=$finalMessage;Details=$Details} | ConvertTo-Json -Depth 12))
                $stream.Write($bytes,0,$bytes.Length)
            } finally { $stream.Dispose() }
        } catch { $finalCode=1; $finalMessage="请求摘要未写入；本次工具结果失败：$($_.Exception.Message)" }
    }
    if ($Context) { Write-FoundationResult $Context $finalCode $finalMessage $Details }
    else { Write-Host $finalMessage }
    [pscustomobject]@{ExitCode=$finalCode;Message=$finalMessage}
}
Export-ModuleMember -Function Get-PluginDemoSuitePolicy,Get-PluginDemoAutomationFilter,Get-PluginDemoNativeFlag,Get-PluginDemoMapURL,Get-PluginDemoHostFlags,Assert-PluginDemoRunResult,Assert-PluginDemoNoEngineFailure,Write-PluginDemoTestOutput
