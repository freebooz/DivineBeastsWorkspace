#requires -Version 7.0
<# 只写本工程内独占合成JSON，验证WorldLive报告消费门禁；不启动UE、不生成真实运行证明。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$parent=Join-Path $taskRoot 'Saved/Validation/PluginManualUI20261027/ScriptContracts'
$temporary=Join-Path $parent ('WorldNative-'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $temporary 'workspace';$engine=Join-Path $temporary 'engine'
$id='20261028-1234-4000-8000-000000000001';$directory="Saved/Validation/FoundationM0/$id/PluginDemoTests"
$cases=0
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message};$script:cases++}
function Reject([scriptblock]$Action){$caught=$false;try{$null=&$Action}catch{$caught=$true};Require $caught '不合格WorldLive实际进程合同被接受。'}
function Save([string]$Path,$Object){$null=New-Item -ItemType Directory -Force -Path(Split-Path $Path);[IO.File]::WriteAllText($Path,($Object|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))}
try{
    $policy=Get-PluginDemoSuitePolicy WorldLive
    $tests=@($policy.Tests|ForEach-Object{@{fullTestPath=$_;state='Success'}})
    $build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
    $started=[DateTime]::UtcNow
    $fixture=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/Hosts/$id"
    $arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),'/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub',
        '-game','-RenderOffscreen','-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub',
        '-GamePlatformManualFixture','-WorldManualNative',"-UserDir=$fixture",'-NoLiveCoding','-LiveCoding=false',
        '-NoHotReload','-NoCompile','-NoCompileEditor',"-FoundationRunId=$id",
        ('-ExecCmds=Automation RunTests '+(Get-PluginDemoAutomationFilter WorldLive)),"-ReportExportPath=$(Join-Path $workspace "$directory/WorldLive/Automation")")
    $process=@{ProcessId=123;StartTimeUtc=$started.ToString('O');FilePath=(Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe');Arguments=$arguments}
    $result=@{Operation='PluginDemoTests';RunId=$id;Suite='WorldLive';Status='Passed';ExitCode=0;UEExecuted=$true;BuildExecuted=$false;
        BuildEvidence=$build;BuildEvidenceAfter=$build;ValidatedTests=10;ExecutionHost='EditorGameStandaloneClientContext';ManualFixtureRoot=$fixture;
        Suites=@(@{Name='WorldLive';Evidence=(Join-Path $workspace "$directory/WorldLive");Result=@{ExitCode=0;ProcessId=123;StartTimeTicks=$started.Ticks;TimedOut=$false;Cleaned=$true}})}
    $automation=Join-Path $workspace "$directory/WorldLive/Automation/index.json";$resultPath=Join-Path $workspace "$directory/result.json"
    $processPath=Join-Path $workspace "$directory/WorldLive/process.json"
    Save $automation @{tests=$tests};$result.AutomationReportSHA256=(Get-FileHash $automation -Algorithm SHA256).Hash
    Save $processPath $process;Save $resultPath $result
    function Read{Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine -ResultPath $resultPath -Suite WorldLive}
    Require ((Read).Cases.Count -eq 10) '真实Standalone合同形状未接受。'
    foreach($flag in @('-game','-RenderOffscreen','-WorldManualNative','-DBAPluginDemo','-GamePlatformManualFixture')){
        $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});Save $processPath $changed;Reject {Read};Save $processPath $process
    }
    $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
    $changed.Arguments+='-nullrhi';Save $processPath $changed;Reject {Read};Save $processPath $process
    $changed.FilePath=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe';$changed.Arguments=$process.Arguments
    Save $processPath $changed;Reject {Read};Save $processPath $process
    foreach($variant in @('missing','duplicate','external')){
        $changed=$process|ConvertTo-Json -Depth 20|ConvertFrom-Json -AsHashtable
        $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cnotlike '-UserDir=*'})
        if($variant -eq 'duplicate'){$changed.Arguments+=@("-UserDir=$fixture","-UserDir=$fixture")}
        if($variant -eq 'external'){$changed.Arguments+="-UserDir=$(Join-Path $temporary 'ForeignHost')"}
        Save $processPath $changed;Reject {Read};Save $processPath $process
    }
    Write-Output "PluginWorldNativeReportContractCases=$cases Failed=0 UEExecuted=False BuildExecuted=False"
}finally{
    $resolved=[IO.Path]::GetFullPath($temporary)
    $allowed=[IO.Path]::GetFullPath($parent).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw '拒绝清理界外合成测试目录。'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
