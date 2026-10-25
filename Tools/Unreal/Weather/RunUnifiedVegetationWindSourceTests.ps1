#requires -Version 7.0
<#
只运行统一风场受影响的十三项原生Automation，不构建、不改资产、不操作现有UE/游戏窗口。
必须传入成功完整Editor证据并显式Execute；前后校验源码/DLL/receipt，复用统一引擎锁。
NullRHI报告不证明材质Shader、GPU、Cook、服务器准入或人工验收。
#>
param([string]$EngineRoot, [string]$EditorBuildResult, [switch]$Execute,
    [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
$windRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $windRoot 'Build/Game/FoundationTools.psm1') -Force
Import-Module (Join-Path $windRoot 'Build/Game/FoundationBuildEvidence.psm1') -Force
Import-Module (Join-Path $windRoot 'Build/Validation/FoundationAutomationReport.psm1') -Force
$context=$null; $lock=$null; $code=2; $details=@{}; $message=''
try {
    $context=New-FoundationContext WindSourceTests $RunId
    if(!$Execute){throw [IO.FileNotFoundException]::new('需要显式Execute；不自动构建或启动编辑器。')}
    $engine=Get-FoundationEngine $EngineRoot
    $modules=@('GamePlatformWeatherRuntime','GamePlatformWeatherClient','GamePlatformSurfaceClient','GamePlatformSurfaceEditor','GamePlatformGameplay')
    $lock=Enter-FoundationEngineLock $engine.Root
    $proof=Assert-FoundationEditorBuildResult -Workspace $windRoot -Engine $engine -BuildResultPath $EditorBuildResult -RequiredModules $modules -AllowCookFileOrderChanges
    $required=@(
        'GamePlatform.Weather.Runtime.QuantizationAndTransition',
        'GamePlatform.Weather.Runtime.ReentrantScheduleReplacement',
        'GamePlatform.Weather.Runtime.WindDirectionAndPhase',
        'GamePlatform.Weather.Runtime.SceneWindSourceInitialOnly',
        'GamePlatform.Weather.Client.WindRefreshRejectsReentry',
        'GamePlatform.Weather.Client.WindClockCorrectionAndOldSource',
        'GamePlatform.Weather.Client.NativeWindProjectionAndCleanup',
        'GamePlatform.Surface.State.ClampAndCompare',
        'GamePlatform.Surface.Binding.StickyFailureAndOldGeneration',
        'GamePlatform.Surface.Wind.StateValidationAndMerge',
        'GamePlatform.Surface.Wind.BindingAndWorldIsolation',
        'GamePlatform.Surface.Editor.MPCContract',
        'GamePlatform.Surface.Editor.WindPreviewIsolation')
    $editor=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $report=Join-Path $context.Directory 'Automation'
    $arguments=@($context.Project,'/Engine/Maps/Entry','-unattended','-nop4','-nullrhi','-nosplash','-stdout',
        '-DisablePlugins=Monolith','-NoCompile','-NoCompileEditor','-NoLiveCoding','-LiveCoding=false',
        "-SessionId=$RunId",("-ExecCmds=Automation RunTests "+($required -join '+')),
        '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report",("-abslog="+(Join-Path $context.Directory 'Unreal.log')))
    $result=Invoke-FoundationProcess -FilePath $editor -Arguments $arguments -WorkingDirectory (Split-Path $editor) -OutputDirectory $context.Directory -TimeoutSeconds 600
    $details.Process=$result; $details.BuildEvidence=$proof; $details.RequiredPaths=$required
    if($result.ExitCode -ne 0){throw "原生测试进程失败：$($result.ExitCode)"}
    $parsed=Get-Content -LiteralPath (Join-Path $report 'index.json') -Raw | ConvertFrom-Json
    $details.ValidatedTests=Assert-FoundationAutomationReport -Cases @($parsed.tests) -RequiredPaths $required -Prefix 'GamePlatform.'
    $after=Assert-FoundationEditorBuildResult -Workspace $windRoot -Engine $engine -BuildResultPath $EditorBuildResult -RequiredModules $modules -AllowCookFileOrderChanges
    if($after.BuildResultSHA256 -ne $proof.BuildResultSHA256){throw '测试期间构建证据变化'}
    $details.BuildEvidenceRecheckedAfterTests=$true
    $code=0; $message='十三项受影响原生回归实际执行通过；NullRHI不代表植物渲染。'
} catch [IO.FileNotFoundException] {$code=2; $message=$_.Exception.Message}
catch {$code=1; $message=$_.Exception.Message}
finally {if($lock){$lock.ReleaseMutex();$lock.Dispose()}}
if($context){Write-FoundationResult $context $code $message $details}else{Write-Host $message}
exit $code
