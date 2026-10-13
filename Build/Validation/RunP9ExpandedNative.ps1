# P9未覆盖插件的官方UE原生自动化批次运行器。
# 输入仅允许预审的三个测试命名空间；每次生成独立RunId，保留官方index.json及真实进程日志。
# 只运行既有测试，不修改代码、不接触真实后端/支付、不把用例成功冒充人工验收。
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('GamePlatform', 'DivineBeasts', 'Moba')]
    [string]$Group,
    [ValidateRange(1, 2000)]
    [int]$MinimumTests = 1,
    [switch]$WithRealDefinitionFixtures
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$Project = Join-Path $Root 'Game/DivineBeastsArena.uproject'
$Editor = 'D:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $Project -PathType Leaf) -or
    !(Test-Path -LiteralPath $Editor -PathType Leaf) -or
    !(Split-Path $Root -Leaf).Equals('DivineBeastsWorkspace', [StringComparison]::Ordinal)) {
    throw '未满足锁定项目、真实引擎或工作空间路径'
}
$Id = [guid]::NewGuid().ToString('N')
$Output = Join-Path $Root ('Saved/Validation/PluginP7P9_20261012/ExpandedNative/' + $Group + '_' + $Id)
$Log = Join-Path $Output 'EditorAutomation.log'
New-Item -ItemType Directory -Path $Output -Force | Out-Null
# 记录并检查正式插件真实C++输入：若其他会话同时修改，本次测试报告将明确失效。
$SourceRoot = Join-Path $Root 'Game/Plugins'
$ScopeParts = if ($Group -eq 'GamePlatform') { @('GamePlatform') } elseif ($Group -eq 'Moba') { @('MobaCommon') } else { @('DivineBeasts') }
$Inputs = @{}
foreach ($ScopePart in $ScopeParts) {
    $Dir = Join-Path $SourceRoot $ScopePart
    foreach ($Item in @(Get-ChildItem -LiteralPath $Dir -File -Recurse -ErrorAction Stop |
        Where-Object { $_.Extension -in @('.cpp','.h','.cs','.uplugin') -and
            $_.FullName -notmatch '[\\/](?:Intermediate|Binaries|Saved)[\\/]' })) {
        $Rel = [IO.Path]::GetRelativePath($Root,$Item.FullName).Replace('\','/')
        $Inputs[$Rel] = (Get-FileHash -LiteralPath $Item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$Started = [DateTimeOffset]::Now
$ExecCommands = 'Automation RunTests ' + $Group
$Extra = @()
if ($WithRealDefinitionFixtures) {
    $Extra += '-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1'
    $Extra += '-GamePlatformFlowTestDefinition=GamePlatformDefinition:foundation.flow@1'
}
& $Editor $Project '/Engine/Maps/Entry' '-unattended' '-nop4' '-nullrhi' '-NoSound' '-NoCompile' '-NoCompileEditor' '-nosplash' '-stdout' '-DivineBeastsCharacterTestHero=Hero.Zodiac.Rat' @Extra ("-ExecCmds=" + $ExecCommands) '-TestExit=Automation Test Queue Empty' ("-ReportExportPath=" + $Output) ("-abslog=" + $Log) | Out-Null
$ExitCode = $LASTEXITCODE
$Index = Join-Path $Output 'index.json'
$Tests = @()
$Problems = @()
$Warnings = 0
$Summaries = @{}
if (Test-Path -LiteralPath $Index -PathType Leaf) {
    try {
        $Data = Get-Content -LiteralPath $Index -Raw -Encoding UTF8 | ConvertFrom-Json -Depth 20
        $Tests = @($Data.tests)
        $Summaries['succeeded'] = $Data.succeeded
        $Summaries['failed'] = $Data.failed
        foreach ($Case in $Tests) {
            if ($null -eq $Case -or [string]::IsNullOrWhiteSpace([string]$Case.fullTestPath)) {
                $Problems += '缺少有效用例身份'
                continue
            }
            if ([string]$Case.state -ne 'Success' -or [int]$Case.errors -gt 0) {
                $Problems += ([string]$Case.fullTestPath + ': state=' + $Case.state + ' errors=' + $Case.errors)
            }
            if ([int]$Case.warnings -gt 0) { $Warnings++ }
        }
    } catch {
        $Problems += ('官方JSON报告解析失败: ' + $_.Exception.Message)
    }
} else {
    $Problems += '缺少真实官方自动化 index.json 报告'
}
if ($Tests.Count -lt $MinimumTests) {
    $Problems += ('实际运行数量少于预期: ' + $Tests.Count + '/' + $MinimumTests)
}
if ($ExitCode -ne 0) {
    $Problems += ('编辑器进程退出码不为0: ' + $ExitCode)
}
$SourceChanged = @()
foreach ($Entry in $Inputs.GetEnumerator()) {
    $File = Join-Path $Root $Entry.Key
    if (!(Test-Path -LiteralPath $File) -or
        (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Entry.Value) {
        $SourceChanged += $Entry.Key
    }
}
if ($SourceChanged.Count) {
    $Problems += ('测试执行期间源码已变化：' + ($SourceChanged | Select-Object -First 8) -join ',')
}

$Result = [ordered]@{
    schemaVersion = 1
    group = $Group
    runId = $Id
    started = $Started.ToString('o')
    ended = [DateTimeOffset]::Now.ToString('o')
    editorVersion = '5.8'
    project = 'DivineBeastsWorkspace'
    realDefinitionFixtures = [bool]$WithRealDefinitionFixtures
    sourceInputs = $Inputs
    sourceChanged = $SourceChanged
    processExit = $ExitCode
    total = $Tests.Count
    passed = @($Tests | Where-Object { $_.state -eq 'Success' -and [int]$_.errors -eq 0 }).Count
    warningCases = $Warnings
    failures = $Problems
    report = ('Saved/Validation/PluginP7P9_20261012/ExpandedNative/' + $Group + '_' + $Id + '/index.json')
    log = ('Saved/Validation/PluginP7P9_20261012/ExpandedNative/' + $Group + '_' + $Id + '/EditorAutomation.log')
    humanAcceptance = 'pending'
    success = $Problems.Count -eq 0
}
$ResultPath = Join-Path $Output 'RunSummary.json'
$Result | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $ResultPath -Encoding utf8
Write-Output ('P9_EXPANDED_NATIVE_RESULT ' + $Group + ' pass=' + $Result.passed +
    ' total=' + $Result.total + ' warnings=' + $Warnings +
    ' problems=' + $Problems.Count + ' process=' + $ExitCode +
    ' runId=' + $Id)
foreach ($Issue in @($Problems | Select-Object -First 15)) {
    Write-Output ('P9_EXPANDED_NATIVE_ISSUE ' + $Issue)
}
Write-Output ('P9_EXPANDED_NATIVE_REPORT ' + $Result.report)
if (!$Result.success) { exit 1 }
exit 0
