#requires -Version 7.0
<#
.SYNOPSIS
在当前唯一主工程执行插件架构变更的Data/UI/VFX/PCG自动化，并拒绝缺少新增测试的旧二进制。
.DESCRIPTION
必须显式-Execute且先构建当前Editor目标；不自动构建、不修改资产、不停止其他编辑器。
默认顺序执行四组；Suites只用于已通过组以外的定向重验，不改变每组必需身份。
输出为Saved/Validation/FoundationM0/<RunId>/PluginArchitectureTests；拒绝复用证据目录。
0=所选组本次全部必需身份唯一且Success；1=报告或脚本失败；2=未执行；引擎非零退出码原样传播。
Data采用真实开发定义夹具；UI/VFX中的边界替身由各测试说明。NullRHI不证明视觉、PIE或Cook。
#>
param([string]$EngineRoot, [switch]$Execute,
    [ValidateSet('Data','UI','VFX','PCG')][string[]]$Suites = @('Data','UI','VFX','PCG'),
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 900,
    [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Game/FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationAutomationReport.psm1') -Force
$context = $null; $lock = $null; $code = 2; $message = ''; $failedExternal = $false
$details = @{ Suites=@(); Rendering='NullRHI'; Scope='PluginArchitectureAutomation' }
try {
    $context = New-FoundationContext PluginArchitectureTests $RunId
    if (-not $Execute) { throw [IO.FileNotFoundException]::new('需要显式-Execute；本入口不会静默启动编辑器。') }
    if (-not $Suites.Count -or @($Suites | Select-Object -Unique).Count -ne $Suites.Count) { throw 'Suites必须非空且不重复。' }
    $engine = Get-FoundationEngine $EngineRoot
    $editor = Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    Assert-FoundationFile $editor; Assert-FoundationFile $context.Project
    $details.EngineRoot = $engine.Root; $details.EngineVersion = $engine.Version
    # 固定完整身份，数量或其他成功项不能替代新增用例；各组额外失败仍由共用报告门禁拒绝。
    $groups = @{
        Data = @{ Prefix='GamePlatform.Data.'; Required=@(
            'Definition.IdentityAndVersion','Editor.SourceDuplicate','Editor.DependencyGraph',
            'Editor.DependencyDepthLimit','Editor.RegisteredValidatorDispatch','Runtime.ResourceLeases',
            'Runtime.RealAssetLeases','Runtime.RecursiveFailureRollback','Runtime.DeferredRequeueZeroDelta',
            'Runtime.NestedTickerNotification','Index.ColdWarmRepeatedQueries','Index.IncrementalAssetChanges',
            'Index.EditorCandidateReplacement','Index.DiscoveryCancellationAndShutdown',
            'Editor.SourceIndexRegistryEvents','Editor.SourceIndexRegistryShutdown') }
        UI = @{ Prefix='DivineBeasts.UI.'; Required=@(
            'Resources.AbilityProfileAndIcons.HeroSwitchReleasesBoth',
            'Resources.AbilityProfileAndIcons.StaleCompletionAndDestroy',
            'Resources.AbilityProfileAndIcons.MissingResourceFallback',
            'Resources.RootLayout.PendingFailureAndControllerChange',
            'Resources.RootLayout.ReadyExternalInstallAndReentry',
            'Resources.RootLayout.LocalPlayerIsolationAndInvalidClass',
            'AbilityProfile.Validation','Characters.HeroOverview.AbilityDescriptionRequired',
            'Characters.HeroOverview.AbilityDescriptionWhitespaceRejected',
            'Combat.AbilityBarReconstruct','Input.WorldHandoff') }
        VFX = @{ Prefix='GamePlatform.VFX.'; Required=@(
            'Diagnostics.MultiWorldCounts','Diagnostics.WorldSubsystemLifecycle',
            'Diagnostics.ResourcePendingTransitions') }
        PCG = @{ Prefix='GamePlatform.PCG.Editor.GoldAuthoring.'; Required=@(
            'BlueprintRoot.Normalization','BlueprintRoot.InvalidInputs','BlueprintRoot.RequiredStages') }
    }
    # 与统一构建/Cook入口协调同一引擎；不按进程名停止任何既有任务。
    $lock = Enter-FoundationEngineLock $engine.Root
    $code = 0
    foreach ($suite in $Suites) {
        $group = $groups[$suite]
        $directory = Join-Path $context.Directory $suite
        $null = New-Item -ItemType Directory -Path $directory
        $report = Join-Path $directory 'Automation'
        $prefix = $group.Prefix
        $arguments = @($context.Project,'-unattended','-nop4','-nullrhi','-nosplash','-stdout','-FullStdOutLogOutput',
            "-FoundationRunId=$($context.RunId)","-abslog=$(Join-Path $directory 'Unreal.log')",
            '-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1',
            "-ExecCmds=Automation RunTests $($prefix.TrimEnd('.'))",
            '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report")
        $result = Invoke-FoundationProcess -FilePath $editor -Arguments $arguments -WorkingDirectory (Split-Path $editor) -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds
        $details.Suites += @{ Name=$suite; Prefix=$prefix; Result=$result; Evidence=$directory }
        if ($result.ExitCode -ne 0) { $code=$result.ExitCode; $failedExternal=$true; break }
        $index = Join-Path $report 'index.json'
        if (-not (Test-Path -LiteralPath $index -PathType Leaf)) { throw "UE已执行但缺少本次Automation导出：$index" }
        $parsed = Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
        $requiredPaths = @($group.Required | ForEach-Object { $prefix + $_ })
        $count = Assert-FoundationAutomationReport -Cases @($parsed.tests) -RequiredPaths $requiredPaths -Prefix $prefix
        $details.Suites[-1].RequiredPaths = $requiredPaths
        $details.Suites[-1].ValidatedTests = $count
    }
    $message = '只接受本次独占目录的实际导出；每个必需测试身份必须出现一次且Success，NullRHI不替代视觉/PIE/Cook。'
} catch [IO.FileNotFoundException] { $code=2; $message=$_.Exception.Message }
catch { $code=1; $message=$_.Exception.Message }
finally { if ($null -ne $lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
if ($context) {
    $status = if ($code -eq 0) { 'Passed' } elseif ($code -eq 2 -and -not $failedExternal) { 'NotExecuted' } else { 'Failed' }
    Write-FoundationResult $context $code $message $details $status
} else { Write-Host $message }
exit $code
