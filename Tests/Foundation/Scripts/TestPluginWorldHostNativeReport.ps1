#requires -Version 7.0
<# 合成报告仅验证三个新Suite身份门禁，不是UE运行证据。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$parent=Join-Path $root 'Saved/Validation/PluginManualUI20261027/ScriptContracts'
$temporary=Join-Path $parent ('WorldHostReport-'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $temporary 'workspace';$engine=Join-Path $temporary 'engine';$checks=0
function Save([string]$Path,$Value){$null=New-Item -ItemType Directory -Force -Path(Split-Path $Path);[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))}
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message};$script:checks++}
try{
    foreach($suite in @('WorldHostEditor','WorldHostLive','WorldAuthoringLive')){
        $id=[guid]::NewGuid().ToString('D');$directory="Saved/Validation/FoundationM0/$id/PluginDemoTests"
        $policy=Get-PluginDemoSuitePolicy $suite;$isLive=$suite -ne 'WorldHostEditor'
        $fixture=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/Hosts/$id"
        $started=[DateTime]::UtcNow;$build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
        $arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),'-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',
            "-FoundationRunId=$id",('-ExecCmds=Automation RunTests '+(Get-PluginDemoAutomationFilter $suite)),"-ReportExportPath=$(Join-Path $workspace "$directory/$suite/Automation")")
        if($isLive){
            $arguments=@($arguments[0],'/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-game','-RenderOffscreen',
                '-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-GamePlatformManualFixture','-WorldManualNative',"-UserDir=$fixture")+@($arguments|Select-Object -Skip 1)
        }else{$arguments+='-nullrhi'}
        if($suite -eq 'WorldHostLive'){$arguments+=@('-FoundationWorld','-DBAManualWorldHost')}
        $exe=if($isLive){'UnrealEditor.exe'}else{'UnrealEditor-Cmd.exe'}
        $process=@{ProcessId=123;StartTimeUtc=$started.ToString('O');FilePath=(Join-Path $engine "Engine/Binaries/Win64/$exe");Arguments=$arguments}
        $result=@{Operation='PluginDemoTests';RunId=$id;Suite=$suite;Status='Passed';ExitCode=0;UEExecuted=$true;BuildExecuted=$false;
            BuildEvidence=$build;BuildEvidenceAfter=$build;ValidatedTests=$policy.Tests.Count;ExecutionHost=$policy.Host;ManualFixtureRoot=$fixture;
            Suites=@(@{Name=$suite;Evidence=(Join-Path $workspace "$directory/$suite");Result=@{ExitCode=0;ProcessId=123;StartTimeTicks=$started.Ticks;TimedOut=$false;Cleaned=$true}})}
        $automation=Join-Path $workspace "$directory/$suite/Automation/index.json"
        $resultPath=Join-Path $workspace "$directory/result.json";$processPath=Join-Path $workspace "$directory/$suite/process.json"
        Save $automation @{tests=@($policy.Tests|ForEach-Object{@{fullTestPath=$_;state='Success'}})}
        $result.AutomationReportSHA256=(Get-FileHash $automation -Algorithm SHA256).Hash
        Save $processPath $process;Save $resultPath $result
        $read={Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine -ResultPath $resultPath -Suite $suite}
        Require ((& $read).Cases.Count -eq $policy.Tests.Count) '跨命名空间精确记录未完整返回。'
        $variants=@()
        if($isLive){
            foreach($flag in @('-game','-WorldManualNative','-RenderOffscreen','-GamePlatformManualFixture')){
                $changed=$process|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable
                $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});$variants+=@{Process=$changed}
            }
            $changed=$process|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable
            $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cnotlike '-UserDir=*'});$variants+=@{Process=$changed}
            if($suite -eq 'WorldHostLive'){
                foreach($flag in @('-FoundationWorld','-DBAManualWorldHost')){
                    $changed=$process|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable
                    $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});$variants+=@{Process=$changed}
                }
            }else{
                foreach($flag in @('-FoundationWorld','-DBAManualWorldHost')){
                    $changed=$process|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable
                    $changed.Arguments+=$flag;$variants+=@{Process=$changed}
                }
            }
        }
        foreach($variant in $variants){
            Save $processPath $variant.Process;$rejected=$false;try{$null=& $read}catch{$rejected=$true}
            Require $rejected '专用世界宿主实际进程标志不符仍被接受。';Save $processPath $process
        }
    }
    Write-Output "PluginWorldHostNativeReportCases=$checks Failed=0 UEExecuted=False"
}finally{
    $resolved=[IO.Path]::GetFullPath($temporary);$allowed=[IO.Path]::GetFullPath($parent).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw '拒绝界外清理。'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
