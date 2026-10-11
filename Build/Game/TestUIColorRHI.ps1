#requires -Version 7.0
<#
.SYNOPSIS
通过真实UE5.8 RHI验证神兽联盟原稿到Slate颜色，并生成可供发布入口复查的证据。
.DESCRIPTION
仅验证第三层公共UI，不编辑引擎资产、不登录后端、不操作人工客户端。
VerifyOnly只读核对既有报告、测试结果及当前原图/资产指纹；资源改变后旧报告立即失效。
默认运行独占编辑器命令行进程，未加载测试、NullRHI、缺截图、非零退出或资源并发变化均失败。
保存于Game/Saved的线性浮点截图只编码一次；不调用已知额外Gamma的Monolith widget截图。
#>
param([string]$EngineRoot='D:/UnrealEngine-5.8.0-release', [switch]$VerifyOnly,
    [string]$ReportPath, [ValidateRange(30,3600)][int]$TimeoutSeconds=900,
    [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
$taskWorkspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$ReportPath){$ReportPath=Join-Path $taskWorkspace 'Game/Saved/Validation/UIColorRHI/Latest.json'}

function Get-UIColorInputsHash {
    # 核对完整公共UI包和原稿，不能只验源PNG后漏掉材质、乘色及页面布局变化。
    $pack=Join-Path $taskWorkspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core'
    $inputs=@(Get-ChildItem -LiteralPath (Join-Path $pack 'Content/UI') -Recurse -File)
    $inputs+=@(Get-ChildItem -LiteralPath (Join-Path $pack 'SourceArt/UI') -Recurse -File)
    if(!$inputs.Count){throw '公共UI输入为空，不能生成颜色验收指纹'}
    $records=@($inputs|Sort-Object FullName|ForEach-Object {
        [IO.Path]::GetRelativePath($taskWorkspace,$_.FullName).Replace('\','/')+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($records -join "`n")))
}

function Assert-UIColorReport {
    # 测试实际存在、唯一且成功才接受；布尔空值不等同False，零项测试不等同通过。
    param($Evidence)
    if($Evidence.Passed -ne $true -or $Evidence.NullRHI -ne $false -or $Evidence.TestName -ne 'DivineBeasts.UI.Color.SourceToSlate'){
        throw '缺少有效的真实RHI颜色报告'
    }
    if($Evidence.InputsSHA256 -ne (Get-UIColorInputsHash)){throw 'UI或原图已变化，拒绝沿用旧颜色报告'}
    $index=Get-Content -LiteralPath $Evidence.AutomationReport -Raw|ConvertFrom-Json
    $cases=@($index.tests|Where-Object {$_.fullTestPath -eq $Evidence.TestName})
    if($cases.Count -ne 1 -or $cases[0].state -ne 'Success' -or $index.failed -gt 0){throw 'RHI颜色测试缺失或失败'}
    foreach($capture in $Evidence.Captures){
        if(!(Test-Path -LiteralPath $capture.Path -PathType Leaf) -or
            (Get-FileHash -LiteralPath $capture.Path -Algorithm SHA256).Hash -ne $capture.SHA256){throw '颜色截图缺失或已改变'}
    }
    if(@($Evidence.Captures).Count -ne 5){throw '缺少设置页或四种比例登录页的颜色正确截图'}
}

if($VerifyOnly){
    Assert-UIColorReport (Get-Content -LiteralPath $ReportPath -Raw|ConvertFrom-Json)
    Write-Host 'UI_COLOR_RHI_RELEASE_GATE_PASS'
    exit 0
}
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context=New-FoundationContext 'UIColorRHI' $RunId
$engine=Get-FoundationEngine $EngineRoot
$beforeHash=Get-UIColorInputsHash
$nativeReport=Join-Path $context.Directory 'Report'
$arguments=@($context.Project,'/DBAFrontEndPack/Maps/L_DBA_FrontEnd','-unattended','-RenderOffscreen',
    '-nosound','-nosplash','-NoLiveCoding','-stdout','-FullStdOutLogOutput',
    "-abslog=$(Join-Path $context.Directory 'Unreal.log')",'-ExecCmds=Automation RunTests DivineBeasts.UI.Color.SourceToSlate',
    '-TestExit=Automation Test Queue Empty',"-ReportExportPath=$nativeReport")
$processParameters=@{FilePath=(Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe');
    Arguments=$arguments;WorkingDirectory=$taskWorkspace;OutputDirectory=$context.Directory;TimeoutSeconds=$TimeoutSeconds}
$result=Invoke-FoundationProcess @processParameters
if($result.ExitCode -ne 0){throw "真实RHI颜色进程失败：$($result.ExitCode)"}
if($beforeHash -ne (Get-UIColorInputsHash)){throw '测试期间UI输入改变，报告不能用于发布'}
$captures=@('SettingsLinearVerified.png','Login960_540_LinearVerified.png','Login1920_1080_LinearVerified.png',
    'Login1280_1024_LinearVerified.png','Login2560_1080_LinearVerified.png')|ForEach-Object {
    $path=Join-Path $taskWorkspace "Game/Saved/Validation/UIColorRHI/$_"
    @{Path=$path;SHA256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
}
$evidence=@{Passed=$true;NullRHI=$false;TestName='DivineBeasts.UI.Color.SourceToSlate';InputsSHA256=$beforeHash;
    AutomationReport=(Join-Path $nativeReport 'index.json');Captures=@($captures);RunId=$RunId.ToString();EngineVersion=$engine.Version}
Assert-UIColorReport $evidence
New-Item -ItemType Directory -Path (Split-Path $ReportPath) -Force|Out-Null
$evidence|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $ReportPath -Encoding utf8
Write-Host "UI_COLOR_RHI_VERIFIED $ReportPath"
