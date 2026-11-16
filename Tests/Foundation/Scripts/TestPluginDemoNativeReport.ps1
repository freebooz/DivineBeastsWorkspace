#requires -Version 7.0
<# SSLM原生报告消费合同回归：只在独占临时目录写明确合成JSON夹具，不启动UE、不伪造真实运行证明。
   验证独占路径/PID/启动时间/前后摘要、实际Automation摘要及必需测试；组外项不能污染待审证明。 #>
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('PluginDemoNativeContract-'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $temporary 'workspace'; $engine=Join-Path $temporary 'engine'; $id='20261013-1234-4000-8000-000000000001'
$directory="Saved/Validation/FoundationM0/$id/PluginDemoTests"; $cases=0
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message};$script:cases++}
function Reject([scriptblock]$Action){$caught=$false;try{$null=&$Action}catch{$caught=$true};Require $caught '不完整实际报告合同被接受。'}
function Save-Fixture([string]$Path,$Object){$null=New-Item -ItemType Directory -Force -Path(Split-Path $Path);[IO.File]::WriteAllText($Path,($Object|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))}
try {
    $policy=Get-PluginDemoSuitePolicy Demo
    $tests=@($policy.Tests|ForEach-Object{@{fullTestPath=$_;state='Success'}})
    $build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
    $started=[DateTime]::UtcNow
    $process=@{ProcessId=123;StartTimeUtc=$started.ToString('O');FilePath=(Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe');Arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),'-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',"-FoundationRunId=$id",'-ExecCmds=Automation RunTests DivineBeasts.PluginDemos',"-ReportExportPath=$(Join-Path $workspace "$directory/Demo/Automation")")}
    $result=@{Operation='PluginDemoTests';RunId=$id;Suite='Demo';Status='Passed';ExitCode=0;UEExecuted=$true;BuildExecuted=$false;BuildEvidence=$build;BuildEvidenceAfter=$build;ValidatedTests=$policy.Tests.Count;
        Suites=@(@{Name='Demo';Evidence=(Join-Path $workspace "$directory/Demo");Result=@{ExitCode=0;ProcessId=123;StartTimeTicks=$started.Ticks;TimedOut=$false;Cleaned=$true}})}
    $automationPath=Join-Path $workspace "$directory/Demo/Automation/index.json"; $resultPath=Join-Path $workspace "$directory/result.json"
    Save-Fixture $automationPath @{tests=$tests}
    $result.AutomationReportSHA256=(Get-FileHash -LiteralPath $automationPath -Algorithm SHA256).Hash
    Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $process; Save-Fixture $resultPath $result
    function Read-Fixture{Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine -ResultPath $resultPath -Suite Demo}
    $read=Read-Fixture; Require ($read.Cases.Count -eq $policy.Tests.Count -and $read.Files.Count -eq 3) '完整实际文件链未被消费。'
    # 固定目录有报告不证明它由当前process导出；必须绑定唯一真实ReportExportPath。
    foreach($variant in @('missing','duplicate','other')) {
        $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        $output=@($changed.Arguments|Where-Object{$_ -clike '-ReportExportPath=*'})
        $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cnotlike '-ReportExportPath=*'})
        if($variant -eq 'duplicate'){$changed.Arguments+=@($output[0],$output[0])}
        if($variant -eq 'other'){$changed.Arguments+="-ReportExportPath=$(Join-Path $workspace 'Other/Automation')"}
        Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $changed; Reject {Read-Fixture}
        Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $process
    }
    # 类型不能靠PowerShell隐式转换取得资格；这些值虽可与true/0比较相等，却不是原生JSON事实类型。
    foreach($change in @(@{UEExecuted=1},@{BuildExecuted=0},@{ExitCode='0'},@{ValidatedTests='8'})) {
        $changed=$result|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        foreach($key in $change.Keys){$changed[$key]=$change[$key]}
        Save-Fixture $resultPath $changed; Reject {Read-Fixture}; Save-Fixture $resultPath $result
    }
    $changed=$result|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable; $changed.Suites[0].Result.ProcessId='123'
    Save-Fixture $resultPath $changed; Reject {Read-Fixture}; Save-Fixture $resultPath $result
    Save-Fixture $automationPath @{tests=$tests+@{fullTestPath='Other.Unrelated';state='Fail'}}
    $result.AutomationReportSHA256=(Get-FileHash -LiteralPath $automationPath -Algorithm SHA256).Hash; Save-Fixture $resultPath $result
    Require ((Read-Fixture).Cases.Count -eq $policy.Tests.Count) '组外失败项污染独立Demo证明。'
    foreach($field in @('UEExecuted','ExitCode','Suite','AutomationReportSHA256')) {
        $changed=$result|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        $changed[$field]=switch($field){UEExecuted{$false};ExitCode{6};Suite{'Review'};default{'0'*64}}
        Save-Fixture $resultPath $changed; Reject {Read-Fixture}; Save-Fixture $resultPath $result
    }
    $changed=$result|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable; $changed.BuildEvidenceAfter.InputSHA256='0'*64
    Save-Fixture $resultPath $changed; Reject {Read-Fixture}; Save-Fixture $resultPath $result
    foreach($field in @('ProcessId','StartTimeUtc','FilePath')) {
        $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        $changed[$field]=switch($field){ProcessId{999};StartTimeUtc{$started.AddSeconds(-1).ToString('O')};default{'C:/Other/UnrealEditor-Cmd.exe'}}
        Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $changed; Reject {Read-Fixture}
        Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $process
    }
    $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable; $changed.Arguments=@($changed.Arguments|Where-Object{$_ -ne '-NoCompileEditor'})
    Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $changed; Reject {Read-Fixture}
    Save-Fixture (Join-Path $workspace "$directory/Demo/process.json") $process
    Save-Fixture $automationPath @{tests=@($tests|Select-Object -Skip 1)}
    $result.AutomationReportSHA256=(Get-FileHash -LiteralPath $automationPath -Algorithm SHA256).Hash; Save-Fixture $resultPath $result; Reject {Read-Fixture}
} finally {
    $resolved=[IO.Path]::GetFullPath($temporary)
    if((Test-Path -LiteralPath $resolved) -and $resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -match '^PluginDemoNativeContract-[a-f0-9]{32}$'){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
Write-Output "PluginDemoNativeReportContractCases=$cases Failed=0 UEExecuted=False BuildExecuted=False"
