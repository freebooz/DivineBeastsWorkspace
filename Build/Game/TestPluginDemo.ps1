#requires -Version 7.0
<#
.SYNOPSIS
SSLM演示/审核/启动策略原生测试入口；默认不执行，不自动构建，不修改UE资产。
.DESCRIPTION
Execute与VerifyOnly互斥；两者均要求同源完整Editor构建证据，OnlyModules不得降级使用。
VerifyOnly只核验二进制门禁、不创建UE进程。Execute使用Foundation独占PID管理及新证据目录。
退出0仅表示指定测试组实际成功，1校验/运行失败，2未执行；不代表画面、Cook或人工核验。
#>
param([switch]$Execute,[switch]$VerifyOnly,[string]$EngineRoot,[string]$BuildResultPath,
    [ValidateSet('Demo','Review','LaunchPolicy','Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive')][string]$Suite='Demo',[string]$ReportPath,
    [guid]$RunId=[guid]::NewGuid(),[ValidateRange(1,86400)][int]$TimeoutSeconds=900)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoValidation.psm1') -Force
$context=$null; $code=2; $message=''; $details=@{Suite=$Suite;UEExecuted=$false;BuildExecuted=$false;HumanReview='Deferred';Suites=@()}
try {
    if ($Execute -and $VerifyOnly) { throw 'Execute与VerifyOnly不能同时指定。' }
    if (-not $Execute -and -not $VerifyOnly) { throw [IO.FileNotFoundException]::new('未指定Execute/VerifyOnly；不会启动UE或构建。') }
    $workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $engine=Get-FoundationEngine $EngineRoot
    $policy=Get-PluginDemoSuitePolicy $Suite
    # 此门禁必须早于进程、Suite目录及任何UE动作；本脚本没有构建恢复分支。
    $before=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $policy.Modules
    $details.BuildEvidence=$before
    if ($VerifyOnly) {
        $code=0; $message='当前完整Editor证据通过；未创建UE进程、未运行测试、未签人工结论。'
    } else {
        $context=New-FoundationContext PluginDemoTests $RunId
        $editor=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        Assert-FoundationFile $editor
        $directory=Join-Path $context.Directory $Suite
        $null=New-Item -ItemType Directory -Path $directory
        $automation=Join-Path $directory 'Automation'
        $arguments=@($context.Project,'-unattended','-nop4','-nullrhi','-nosplash','-stdout','-FullStdOutLogOutput',
            # 原生自动化只消费已制作资产；隔离工具插件的单端口/SQLite，不接管交互Editor的Monolith服务。
            '-DisablePlugins=Monolith',"-SessionId=$($context.RunId)",
            '-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',"-FoundationRunId=$($context.RunId)",
            "-abslog=$(Join-Path $directory 'Unreal.log')",("-ExecCmds=Automation RunTests "+(Get-PluginDemoAutomationFilter $Suite)),
            '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$automation")
        if ($Suite -in @('PresentationLive','WorldLive','WorldHostLive','WorldAuthoringLive','EvidenceHashLive')) {
            # 真正-game/Standalone进程，测试按ClientContext注册并通过真实Data请求建立非空主题基线。
            $editor=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
            Assert-FoundationFile $editor
            $arguments=@($arguments | Where-Object { $_ -cne '-nullrhi' })
            $arguments=@($arguments[0],'/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-game')+@($arguments | Select-Object -Skip 1)
            $arguments+=@('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720')
            $arguments+=(Get-PluginDemoNativeFlag $Suite)
            $details.ExecutionHost='EditorGameStandaloneClientContext'
        }
        if ($Suite -in @('Demo','PresentationLive','WorldLive','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive')) {
            # 真实GI服务回归与人工页共用显式离线夹具门禁，写入本次工程内独立用户目录。
            $fixtureRoot=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/Hosts/$($context.RunId)"
            $null=New-Item -ItemType Directory -Path $fixtureRoot -Force
            $arguments+=@('-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub',
                '-GamePlatformManualFixture',"-UserDir=$fixtureRoot")
            $details.ManualFixtureRoot=$fixtureRoot
        }
        if($Suite -eq 'WorldHostLive'){$arguments+=@('-FoundationWorld','-DBAManualWorldHost')}
        $result=Invoke-FoundationProcess -FilePath $editor -Arguments $arguments -WorkingDirectory (Split-Path $editor) -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds
        $details.UEExecuted=$true; $details.Suites=@(@{Name=$Suite;Result=$result;Evidence=$directory})
        # 即使工具退出0仍核对本次完整身份，缺任一项拒绝；结束后再次检查真实源和DLL。
        $index=Join-Path $automation 'index.json'; Assert-FoundationFile $index
        $parsed=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
        $after=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $policy.Modules
        $count=Assert-PluginDemoRunResult -ProcessResult $result -Cases @($parsed.tests) -Before $before -After $after -Suite $Suite
        $details.ValidatedTests=$count; $details.BuildEvidenceAfter=$after
        $details.AutomationReportSHA256=(Get-FileHash -LiteralPath $index -Algorithm SHA256).Hash
        $code=0; $message=if($Suite -in @('PresentationLive','WorldLive','WorldHostLive','WorldAuthoringLive','EvidenceHashLive')){'本组真实Standalone ClientContext原生测试及前后二进制门禁通过；非打包Client，未验收视听。'}else{'本组原生测试及前后二进制门禁通过；NullRHI不证明视觉，未做人工核验。'}
    }
} catch [IO.FileNotFoundException] {
    # 前置缺失才是未执行；UE实际启动后的缺报告（包括崩溃）必须保留失败语义。
    $code=if($details.UEExecuted){1}else{2}; $message=$_.Exception.Message
}
catch { $code=1; $message=$_.Exception.Message }
# 全部请求输出完成后才发布canonical；摘要失败不得留下一份可被提升消费者接受的Passed残留。
$final=Write-PluginDemoTestOutput -Context $context -ExitCode $code -Message $message -Details $details -RunId $RunId -ReportPath $ReportPath
exit $final.ExitCode
