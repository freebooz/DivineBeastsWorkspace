#requires -Version 7.0
<#
.SYNOPSIS
SSLM Gold Crop（农作物）修复、编译及真实生成验收的受控续接入口。
.DESCRIPTION
默认Plan只读；显式-Apply且引擎/工作空间独占时，按阶段执行：
Crop Inspect→Apply→Verify→PCG双模块增量编译→UE Editor Automation→
Gold三图GeneratePreview→G09两个独立UE进程确定性比较。
首次失败立即停止并记录证据；不更改正式Village/MainArena地图。
不将Gold开发验收冒充G01～G16全项、Client/Server Cook或3A生产验收。
#>
[CmdletBinding()]
param(
    [string]$EngineRoot=$env:UE_ROOT,
    [switch]$Apply,
    [switch]$AlreadyMigrated
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project=Join-Path $root 'Game/DivineBeastsArena.uproject'
$repair=Join-Path $PSScriptRoot 'RunGoldCropSplineRepairUE58.ps1'
$build=Join-Path $root 'Build/Game/BuildFoundation.ps1'
$probe=Join-Path $PSScriptRoot 'RunGoldSpatialProbe.ps1'
$g09=Join-Path $PSScriptRoot 'RunGoldDeterminism.ps1'
$crop=Join-Path $root 'Game/Content/Development/Foundation/PCG/Realized/PCG_Gold_Crop.uasset'
$run=Join-Path $root ('Saved/Validation/GamePlatformPCG/GoldCropRecovery/'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$steps=[Collections.Generic.List[object]]::new()
$state=[ordered]@{
    SchemaVersion=1; Project=$project
    RequestedMode=$(if($Apply){'Apply'}else{'Plan'})
    AlreadyMigrated=[bool]$AlreadyMigrated
    Status='Started'; ProductionReady=$false; Stages=@()
}
function Save-State {
    $state.Stages=@($steps.ToArray())
    [IO.File]::WriteAllText((Join-Path $run 'result.json'),
        ($state|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
}
function Invoke-CheckedStage {
    param([string]$Name,[scriptblock]$Action,[string]$Marker)
    $path=Join-Path $run ($Name+'.log')
    $status=1
    $messages=@()
    try {
        $messages=@(& $Action 2>&1)
        $status=$LASTEXITCODE
    }catch{
        $messages+=($_.Exception.Message)
        $status=1
    }
    $body=$messages -join [Environment]::NewLine
    [IO.File]::WriteAllText($path,$body,[Text.UTF8Encoding]::new($false))
    $passed=($status -eq 0) -and ($body.Contains($Marker))
    $steps.Add([ordered]@{Name=$Name;ExitCode=$status;Marker=$Marker;Passed=$passed;Log=$path})
    Save-State
    if(!$passed){throw ('失败阶段：'+$Name+'；日志：'+$path)}
    Write-Output ('SSLM_PCG_RECOVERY_STAGE_PASS '+$Name)
}
try {
    if([string]::IsNullOrWhiteSpace($EngineRoot)){throw '必须指定锁定UE5.8安装目录'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw '仅接受UE5.8'}
    $editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach($file in @($editor,$project,$repair,$build,$probe,$g09,$crop)){
        if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw ('缺少必要输入：'+$file)}
    }
    $state.BeforeSHA256=(Get-FileHash $crop -Algorithm SHA256).Hash
    $active=@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue)
    $ubt=@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction SilentlyContinue |
        Where-Object{$_.CommandLine -match 'UnrealBuildTool'})
    $state.ActiveEditorCount=$active.Count
    $state.ActiveUBTCount=$ubt.Count
    if(!$Apply){
        $state.Status=$(if($active.Count -eq 0 -and $ubt.Count -eq 0){'ReadyForApply'}else{'BlockedByActiveEditorOrUBT'})
        Save-State
        Write-Output ("SSLM_GOLD_CROP_RECOVERY_PLAN status="+$state.Status+
            " editors="+$active.Count+" ubt="+$ubt.Count)
        exit 0
    }
    if($active.Count -gt 0 -or $ubt.Count -gt 0){
        throw '其它UE Editor/UBT正在运行；禁止强制关闭或修改其已加载资产'
    }
    if(!$AlreadyMigrated){
        Invoke-CheckedStage '01-Inspect' { & $repair -EngineRoot $engine -Mode Inspect } 'SSLM_GOLD_CROP_SPLINE_UE_RESULT mode=Inspect'
        Invoke-CheckedStage '02-Apply' { & $repair -EngineRoot $engine -Mode Apply } 'SSLM_GOLD_CROP_SPLINE_UE_RESULT mode=Apply'
    }
    Invoke-CheckedStage '03-Verify' { & $repair -EngineRoot $engine -Mode Verify } 'SSLM_GOLD_CROP_SPLINE_UE_RESULT mode=Verify'
    Invoke-CheckedStage '04-Build' {
        & $build -EngineRoot $engine -Editor -OnlyModules @('GamePlatformPCG','GamePlatformPCGEditor') -UsePrecompiled -NoUBA -MaxParallelActions 8 -TimeoutSeconds 3600
    } 'Build: Passed'

    $report=Join-Path $run 'EditorAutomation'
    $unrealLog=Join-Path $run 'EditorAutomation.unreal.log'
    $arguments=@($project,'/Engine/Maps/Entry','-unattended','-nop4','-nullrhi','-NoSound','-NoCompile','-NoCompileEditor',
        '-NoLiveCoding','-DisablePlugins=Monolith','-nosplash',
        '-ExecCmds=Automation RunTests GamePlatform.PCG.Editor',
        '-TestExit=Automation Test Queue Empty',
        ('-ReportExportPath='+$report),('-abslog='+$unrealLog))
    Invoke-CheckedStage '05-EditorAutomation' { & $editor @arguments } 'Automation Test Queue Empty'
    $index=Join-Path $report 'index.json'
    if(!(Test-Path -LiteralPath $index -PathType Leaf)){throw 'UE没有真实导出Automation index.json'}
    $tests=@((Get-Content -LiteralPath $index -Raw -Encoding UTF8|ConvertFrom-Json).tests)
    $required=@('GoldAuthoring.BlueprintRoot.InvalidInputs','GoldAuthoring.BlueprintRoot.Normalization',
        'GoldAuthoring.BlueprintRoot.RequiredStages','DevelopmentGraph',
        'FoundationSubgraphContracts','FoundationTemplateContracts','RealizedGraphMeshSet','SourceFingerprint')
    foreach($name in $required){
        $full='GamePlatform.PCG.Editor.'+$name
        if(@($tests|Where-Object{$_.fullTestPath -eq $full -and $_.state -eq 'Success'}).Count -ne 1){
            throw ('PCG Editor缺少唯一真实Success：'+$full)
        }
    }
    if(@($tests|Where-Object{$_.state -ne 'Success'}).Count -gt 0){
        throw 'PCG Editor自动化包含失败、跳过或未知项'
    }
    Write-Output 'SSLM_PCG_RECOVERY_EDITOR_TESTS_EIGHT_PASS'
    Invoke-CheckedStage '06-GeneratePreview' { & $probe -EngineRoot $engine -Stage GeneratePreview } 'GOLD_SPAWNER_GENERATION_PREVIEW_VALIDATED_IN_UE'
    Invoke-CheckedStage '07-G09' { & $g09 -EngineRoot $engine } 'GOLD_G09_TWO_FRESH_EDITORS_DETERMINISM_PASS'
    $state.AfterSHA256=(Get-FileHash $crop -Algorithm SHA256).Hash
    $state.Status='GoldPreviewAndG09Passed_NotWholeGameProduction'
    Save-State
    Write-Output 'SSLM_GOLD_CROP_RECOVERY_PASS_NOT_G01_G16_OR_DUAL_COOK'
    exit 0
}catch{
    $state.Status='StoppedOnFirstBlocker'
    $state.Blocker=$_.Exception.Message
    Save-State
    Write-Output ('SSLM_GOLD_CROP_RECOVERY_BLOCKED '+$_.Exception.Message)
    exit 2
}finally{
    Write-Output ('SSLM_GOLD_CROP_RECOVERY_EVIDENCE '+$run)
}
