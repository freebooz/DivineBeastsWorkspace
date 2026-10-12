#requires -Version 5.1
<#
.SYNOPSIS
神兽联盟PCG无Gaea独立开发资源制作与真实编辑器验证入口。
.DESCRIPTION
默认Plan只审查来源；-Apply严格顺序完成六技术网格真实导入/独立重开、技术地图保存/重开、
Gold空间探针、三图真实Spawner预览、G09双进程对比；-NativeTests只在最后触发原生测试。
共享UE Editor/UBT占用时拒绝执行。每个阶段有独立退出码、日志和总报告。
全部技术阶段通过仍不等于G01-G16或发行客户端/专服Cook通过；无Gaea不阻止开发验证。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT,
      [switch]$Apply,
      [switch]$NativeTests)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$evidence=Join-Path $root ('Saved/Validation/GamePlatformPCG/NoGaeaDevelopment/'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $evidence -Force|Out-Null
$results=[Collections.Generic.List[object]]::new()
$overall='preflight_only'
$lastError=$null
$package='Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Development/PCG'
$meshFolder=Join-Path $root ($package+'/Meshes')
$map=Join-Path $root ($package+'/Maps/L_DBA_PCG_TechMeshReview.umap')
$roles=@('ForestCanopy','RockScatter','AgriCrop','FieldFence','FarmGate','WoodBridge')
$author=Join-Path $PSScriptRoot 'RunVillagePrototypeImport.ps1'
$probe=Join-Path $PSScriptRoot 'RunGoldSpatialProbe.ps1'
$g09=Join-Path $PSScriptRoot 'RunGoldDeterminism.ps1'
$verify=Join-Path $root 'Build/Validation/VerifyPCG.ps1'

function Invoke-Step([string]$Name,[scriptblock]$Command) {
    $out=Join-Path $evidence ($Name+'.log')
    Write-Output ('PCG_NO_GAEA_STAGE_START '+$Name)
    $lines=@(& $Command 2>&1)
    $code=$LASTEXITCODE
    if($null -eq $code){$code=1}
    @($lines)|ForEach-Object {"$_"}|Set-Content -LiteralPath $out -Encoding UTF8
    $results.Add(@{Name=$Name;ExitCode=$code;Evidence=$out;Passed=($code -eq 0)})
    Write-Output ('PCG_NO_GAEA_STAGE_END '+$Name+' Exit='+$code)
    if($code -ne 0){
        $tail=($lines |Select-Object -Last 12|ForEach-Object {"$_"})-join [Environment]::NewLine
        throw ('本阶段失败且停止后续操作：'+$Name+' '+$tail)
    }
}

try {
    foreach($f in @($author,$probe,$g09,$verify)){
        if(!(Test-Path -LiteralPath $f)){throw ('本工单已批准入口缺失：'+$f)}
    }
    $config=Get-Content (Join-Path $root 'Game/Config/DefaultGame.ini') -Raw -Encoding UTF8
    if(!$config.Contains('+DirectoriesToNeverCook=(Path="/DBAWorldPack_Village/Development/PCG")')){
        throw '技术原型没有明确从Client/Server Cook排除'
    }
    Invoke-Step '00_TechSource' { & python (Join-Path $PSScriptRoot 'GenerateVillagePrototypeMeshes.py') '--verify' }
    Invoke-Step '01_FoundationFiles' { & python (Join-Path $PSScriptRoot 'InventoryFoundationAssets.py') }
    Invoke-Step '02_GoldFiles' { & python (Join-Path $PSScriptRoot 'InventoryGoldAssets.py') }
    if(!$Apply){
        Write-Output 'PCG_NO_GAEA_DEVELOPMENT_PLAN_ONLY_NO_UE_ASSETS_MODIFIED'
        $overall='plan_only'
        return
    }
    if(!$EngineRoot){throw 'Apply必须提供锁定UE5.8引擎根路径'}
    $engine=(Resolve-Path -LiteralPath $EngineRoot).Path
    $version=Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8|ConvertFrom-Json
    if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){
        throw '只接受神兽联盟锁定的UE5.8引擎'
    }
    if(@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count){
        throw '已有其他编辑器占用，禁止重复加载同一游戏资源'
    }
    if(@(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
          Where-Object{$_.CommandLine -match 'UnrealBuildTool'}).Count){
        throw '其他会话UBT构建占用共享引擎，拒绝抢锁或终止其任务'
    }
    # 只有六项全部缺失时首次导入；只有六项全存在时只读回读。
    $meshExists=@($roles|Where-Object{
        Test-Path -LiteralPath (Join-Path $meshFolder ('SM_DBA_PCG_'+$_+'_Tech.uasset'))
    })
    if($meshExists.Count -eq 0){
        Invoke-Step '03_TechMeshImport' { & $author -EngineRoot $engine -Mode Apply }
    } elseif($meshExists.Count -ne $roles.Count) {
        throw ('六项技术StaticMesh导入到一半，拒绝覆盖或跳过回读，已存在：'+$meshExists.Count)
    }
    Invoke-Step '04_TechMeshReopen' { & $author -EngineRoot $engine -Mode Verify }
    if(!(Test-Path -LiteralPath $map)){
        Invoke-Step '05_TechMapCreate' { & $author -EngineRoot $engine -Mode ReviewApply }
    }
    Invoke-Step '06_TechMapReopen' { & $author -EngineRoot $engine -Mode ReviewVerify }

    # 源/资产制作结束后才运行真实Gold空间审查、Spawner和双进程确定性。
    Invoke-Step '07_GoldSpatialProbe' { & $probe -EngineRoot $engine -Stage SpatialProbe }
    Invoke-Step '08_GoldGeneratePreview' { & $probe -EngineRoot $engine -Stage GeneratePreview }
    Invoke-Step '09_GoldG09Determinism' { & $g09 -EngineRoot $engine }
    if($NativeTests){
        # 原VerifyPCG.ps1即使Native通过也可能退出2表示“整体UE验收待完成”；不覆盖其原始退出码。
        Invoke-Step '10_NativePCGRegression' {
            & $verify -EngineRoot $engine -NativeTests -TimeoutSeconds 600
        }
    }
    $overall='development_scope_stages_pass_g01_g16_cook_pending'
    Write-Output 'PCG_NO_GAEA_TECHNICAL_DEVELOPMENT_STAGES_SUCCEEDED_NOT_PRODUCTION'
}
catch {
    $lastError=$_.Exception.Message
    $overall='failed_or_unsafe_environment'
    Write-Output ('PCG_NO_GAEA_BLOCKED '+$lastError)
}
finally {
    $report=@{
        SchemaVersion=1;RunUtc=[DateTime]::UtcNow.ToString('o')
        Status=$overall;LastError=$lastError;Results=$results.ToArray()
        GaeaInputRequired=$false
        NotYetAccepted=@('G01～G16完整矩阵','正式AAA美术','最终Gaea地貌','DedicatedServer/Client双端Cook','人工视觉与性能签收')
    }
    $report|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
    Write-Output ('PCG_NO_GAEA_EVIDENCE '+$evidence)
}
if($overall -eq 'failed_or_unsafe_environment'){exit 1}
if($overall -eq 'development_scope_stages_pass_g01_g16_cook_pending'){exit 2}
exit 0
