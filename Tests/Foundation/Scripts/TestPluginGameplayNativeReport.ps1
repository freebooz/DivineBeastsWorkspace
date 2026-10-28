#requires -Version 7.0
<# 合成报告验证玩法精确宿主身份门禁；不启动UE，不计为功能运行或人工通过。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$parent=Join-Path $root 'Saved/Validation/PluginManualUI20261027/ScriptContracts'
$temporary=Join-Path $parent ('GameplayReport-'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $temporary 'workspace';$engine=Join-Path $temporary 'engine';$checks=0
function Save([string]$Path,$Value){$null=New-Item -ItemType Directory -Force -Path(Split-Path $Path);[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))}
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message};$script:checks++}
function Clone($Value){$Value|ConvertTo-Json -Depth 24|ConvertFrom-Json -AsHashtable}
try{
    foreach($suite in @('Gameplay','GameplayLive','GameplayHost','GameplayHostStop','PreparingStopLive')){
        $id=[guid]::NewGuid().ToString('D');$directory="Saved/Validation/FoundationM0/$id/PluginDemoTests"
        $policy=Get-PluginDemoSuitePolicy $suite;$isLive=$suite -ne 'Gameplay'
        $fixture=Join-Path $workspace "Saved/Validation/PluginManualUI20261027/Hosts/$id"
        $started=[DateTime]::UtcNow;$build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
        $arguments=@((Join-Path $workspace 'Game/DivineBeastsArena.uproject'),'-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',
            "-FoundationRunId=$id",('-ExecCmds=Automation RunTests '+(Get-PluginDemoAutomationFilter $suite)),"-ReportExportPath=$(Join-Path $workspace "$directory/$suite/Automation")")
        if($isLive){
            $arguments=@($arguments[0],(Get-PluginDemoMapURL $suite),'-game','-RenderOffscreen',
                '-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-GamePlatformManualFixture',
                (Get-PluginDemoNativeFlag $suite),"-UserDir=$fixture")+@($arguments|Select-Object -Skip 1)
        }else{$arguments+='-nullrhi'}
        $arguments+=@(Get-PluginDemoHostFlags $suite)
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
        $nativeRead=& $read
        Require ($nativeRead.Cases.Count -eq $policy.Tests.Count) ("精确原生记录未完整返回：$suite actual=$($nativeRead.Cases.Count) expected=$($policy.Tests.Count)")
        $variants=@()
        if($isLive){
            foreach($flag in @('-game',(Get-PluginDemoNativeFlag $suite),'-RenderOffscreen','-GamePlatformManualFixture')){
                $changed=Clone $process;$changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});$variants+=$changed
            }
            $changed=Clone $process;$changed.Arguments=@($changed.Arguments|Where-Object{$_ -cnotlike '-UserDir=*'});$variants+=$changed
            $changed=Clone $process;$changed.Arguments[1]='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub?game=/Script/Engine.GameModeBase';$variants+=$changed
            $changed=Clone $process;$changed.Arguments+='-nullrhi';$variants+=$changed
            foreach($flag in @('-FoundationWorld','-DBAManualWorldHost','-DBAManualGameplayHost','-FoundationGameplayOffline')){
                $changed=Clone $process
                if($flag -cin @(Get-PluginDemoHostFlags $suite)){
                    $changed.Arguments=@($changed.Arguments|Where-Object{$_ -cne $flag});$variants+=$changed
                    $changed=Clone $process;$changed.Arguments+=$flag;$variants+=$changed
                }else{$changed.Arguments+=$flag;$variants+=$changed}
            }
        }
        foreach($variant in $variants){
            Save $processPath $variant;$rejected=$false;try{$null=& $read}catch{$rejected=$true}
            Require $rejected '实际宿主URL/旗标或独占目录错误仍被接受。';Save $processPath $process
        }
    }
    Write-Output "PluginGameplayNativeReportCases=$checks Failed=0 UEExecuted=False"
}finally{
    $resolved=[IO.Path]::GetFullPath($temporary);$allowed=[IO.Path]::GetFullPath($parent).TrimEnd('\')+'\'
    if(!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw '拒绝界外清理。'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
