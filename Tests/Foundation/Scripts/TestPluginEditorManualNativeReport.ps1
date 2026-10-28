#requires -Version 7.0
<# 正常Editor/PIE身份门禁；只写工程内合成夹具，不启动UE。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$parent=Join-Path $root 'Saved/Validation/PluginManualUI20261027/ScriptContracts'
$temporary=Join-Path $parent ('EditorReport-'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $temporary 'workspace';$engine=Join-Path $temporary 'engine';$checks=0
function Save([string]$Path,$Value){$null=New-Item -ItemType Directory -Force -Path(Split-Path $Path);[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))}
function Clone($Value){$Value|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable}
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message};$script:checks++}
try{
    $suite='EditorManual';$id=[guid]::NewGuid().ToString('D');$directory="Saved/Validation/FoundationM0/$id/PluginDemoTests"
    $policy=Get-PluginDemoSuitePolicy $suite;$fixture=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/Hosts/$id"
    $started=[DateTime]::UtcNow;$build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
    $arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),(Get-PluginDemoMapURL $suite),'-RenderOffscreen',
        '-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-GamePlatformManualFixture',
        '-GamePlatformManualEditorFixture',"-UserDir=$fixture",'-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',
        "-FoundationRunId=$id",('-ExecCmds=Automation RunTests '+(Get-PluginDemoAutomationFilter $suite)),"-ReportExportPath=$(Join-Path $workspace "$directory/$suite/Automation")")
    $process=@{ProcessId=123;StartTimeUtc=$started.ToString('O');FilePath=(Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe');Arguments=$arguments}
    $result=@{Operation='PluginDemoTests';RunId=$id;Suite=$suite;Status='Passed';ExitCode=0;UEExecuted=$true;BuildExecuted=$false;
        BuildEvidence=$build;BuildEvidenceAfter=$build;ValidatedTests=$policy.Tests.Count;ExecutionHost='EditorPIE';ManualFixtureRoot=$fixture;
        Suites=@(@{Name=$suite;Evidence=(Join-Path $workspace "$directory/$suite");Result=@{ExitCode=0;ProcessId=123;StartTimeTicks=$started.Ticks;TimedOut=$false;Cleaned=$true}})}
    $automation=Join-Path $workspace "$directory/$suite/Automation/index.json"
    $resultPath=Join-Path $workspace "$directory/result.json";$processPath=Join-Path $workspace "$directory/$suite/process.json"
    $engineLog=Join-Path $workspace "$directory/$suite/Unreal.log"
    $null=New-Item -ItemType Directory -Force -Path (Split-Path $engineLog)
    [IO.File]::WriteAllText($engineLog,'LogAutomationController: Display: Success',[Text.UTF8Encoding]::new($false))
    $result.EngineLogSHA256=(Get-FileHash -LiteralPath $engineLog -Algorithm SHA256).Hash;$result.EngineLogLines=1
    Save $automation @{tests=@($policy.Tests|ForEach-Object{@{fullTestPath=$_;state='Success'}})}
    $result.AutomationReportSHA256=(Get-FileHash $automation -Algorithm SHA256).Hash
    Save $processPath $process;Save $resultPath $result
    $read={Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine -ResultPath $resultPath -Suite $suite}
    Require ((& $read).Cases.Count -eq 7) '真实Editor身份的完整集合未接受。'
    $variants=@()
    foreach($flag in @('-RenderOffscreen','-DBAPluginDemo','-GamePlatformManualFixture','-GamePlatformManualEditorFixture')){
        $changed=Clone $process;$changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});$variants+=$changed
    }
    foreach($flag in @('-game','-GAME','-server','-nullrhi','-NullRHI','-WorldManualNative','-GameplayManualNative','-FoundationWorld','-foundationworld','-DBAManualWorldHost','-DBAManualGameplayHost','-FoundationGameplayOffline')){
        $changed=Clone $process;$changed.Arguments+=$flag;$variants+=$changed
    }
    $changed=Clone $process;$changed.FilePath=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe';$variants+=$changed
    $changed=Clone $process;$changed.Arguments[1]+='?game=/Script/Engine.GameModeBase';$variants+=$changed
    $changed=Clone $process;$changed.Arguments=@($changed.Arguments|Where-Object{$_ -cnotlike '-UserDir=*'});$variants+=$changed
    $changed=Clone $process;$changed.Arguments+="-UserDir=$fixture";$variants+=$changed
    foreach($variant in $variants){Save $processPath $variant;$rejected=$false;try{$null=& $read}catch{$rejected=$true};Require $rejected '错误/混合Editor宿主身份仍接受。';Save $processPath $process}
    $wrong=Clone $result;$wrong.ExecutionHost='EditorGameStandaloneClientContext';Save $resultPath $wrong
    $rejected=$false;try{$null=& $read}catch{$rejected=$true};Require $rejected 'Editor误记Standalone仍接受。'
    $wrong=Clone $result;$wrong.Remove('EngineLogSHA256');Save $resultPath $wrong
    $rejected=$false;try{$null=& $read}catch{$rejected=$true};Require $rejected 'Editor新组缺引擎日志仍被当历史兼容接受。'
    Write-Output "PluginEditorManualNativeReportCases=$checks Failed=0 UEExecuted=False"
}finally{
    $resolved=[IO.Path]::GetFullPath($temporary);$allowed=[IO.Path]::GetFullPath($parent).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw '拒绝界外清理。'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
