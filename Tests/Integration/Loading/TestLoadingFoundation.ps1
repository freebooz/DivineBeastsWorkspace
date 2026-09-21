#requires -Version 7.0
<#
.SYNOPSIS
使用正式工程运行Loading真实实例服务与Data租约自动化；不以日志关键字代替测试报告。
.DESCRIPTION
复用Foundation进程所有权、超时和报告解析工具。此入口不等同三维Foundation场景、多PIE或Cook验收。
#>
param([string]$EngineRoot,[switch]$Execute,[ValidateRange(1,1800)][int]$TimeoutSeconds=180,[guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
Import-Module (Join-Path $root 'Build/Game/FoundationTools.psm1') -Force
Import-Module (Join-Path $root 'Build/Validation/FoundationAutomationReport.psm1') -Force
$context=New-FoundationContext 'LoadingUnrealTests' $RunId
$code=2;$status='未执行';$details=@{}
try {
    if (-not $Execute) { throw [IO.FileNotFoundException]::new('需显式Execute；尚未运行不能通过。') }
    $engine=Get-FoundationEngine $EngineRoot
    $editor=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    Assert-FoundationFile $editor;Assert-FoundationFile $context.Project
    foreach($descriptor in Get-ChildItem (Join-Path $root 'Game/Plugins') -Recurse -Filter '*.uplugin' -File) {
        $null=Get-Content -LiteralPath $descriptor.FullName -Raw -Encoding utf8 | ConvertFrom-Json -ErrorAction Stop
    }
    # 实际测试由主资产身份查找；不生成、替换或跳过缺失定义。
    $details.RequiredDefinitions=@('GamePlatformDefinition:foundation.probe@1','GamePlatformDefinition:foundation.flow@1')
    $report=Join-Path $context.Directory 'Automation'
    $result=Invoke-FoundationProcess -FilePath $editor -Arguments @($context.Project,'-unattended','-nop4','-nullrhi','-nosplash','-stdout',
        '-ExecCmds=Automation RunTests GamePlatform.Loading','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$report", "-abslog=$(Join-Path $context.Directory 'Unreal.log')") `
        -WorkingDirectory (Split-Path $editor) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds
    $details.Process=$result
    $code=$result.ExitCode;$status=if($code -eq 0){'通过'}else{'失败'}
    if ($code -ne 0) { throw "UE进程失败：$code" }
    $index=Join-Path $report 'index.json';Assert-FoundationFile $index
    $parsed=Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
    $details.Tests=Assert-FoundationAutomationReport -Cases @($parsed.tests) -RequiredPaths @('GamePlatform.Loading.Service.Lifecycle','GamePlatform.Loading.Data.RealLeases') -Prefix 'GamePlatform.Loading.'
    $message='只证明所列真实UE自动化；完整场景、双PIE、Cook与Session未执行。'
} catch [IO.FileNotFoundException] { $message=$_.Exception.Message;$code=2;$status='未执行' }
catch { $message=$_.Exception.Message;if($code -eq 0 -or $code -eq 2){$code=1};$status='失败' }
Write-FoundationResult $context $code $message $details $status
exit $code
