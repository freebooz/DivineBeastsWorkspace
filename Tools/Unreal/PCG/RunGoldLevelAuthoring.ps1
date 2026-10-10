#requires -Version 5.1
<#
PCG Gold Level（PCG金标准）UE5.8真实资产创作入口。
先读检查、再显式-Apply，禁止覆盖/删除文件/抢占其他编辑器进程。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT, [switch]$Apply)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project = Join-Path $root 'Game/DivineBeastsArena.uproject'
$evidence = Join-Path $root ('Saved/Validation/GamePlatformPCG/GoldLevelAuthoring/' + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$records = [Collections.Generic.List[object]]::new()
$foundation = Join-Path $root 'Game/Content/Development/Foundation/PCG'
$village = Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/PCG'
$definitions = Join-Path $PSScriptRoot 'AuthorGoldLevelDefinitions.py'
$realizedScript = Join-Path $PSScriptRoot 'AuthorGoldRealizedGraphs.py'
$mapScript = Join-Path $PSScriptRoot 'AuthorGoldLevelMap.py'
$reopenScript = Join-Path $PSScriptRoot 'ValidateGoldLevelAssets.py'

function Assert-Free([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        $current = @(Get-ChildItem -LiteralPath $Path -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in @('.uasset','.umap') })
        if ($current.Count) { throw "PCG资源已存在，拒绝覆盖：$($current[0].FullName)" }
    }
}
function Invoke-UE([string]$Stage,[string[]]$Arguments) {
    $out = Join-Path $evidence "$Stage.out.log"
    $err = Join-Path $evidence "$Stage.err.log"
    $args = @($project) + $Arguments + @('-unattended','-nop4','-nosplash','-nullrhi','-stdout')
    $process = Start-Process -FilePath $editor -ArgumentList $args -Wait -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $records.Add(@{Stage=$Stage;ExitCode=$process.ExitCode;StdOut=$out;StdErr=$err})
    if ($process.ExitCode -ne 0) {
        $tail = @(Get-Content -LiteralPath $out -Tail 18 -ErrorAction SilentlyContinue)
        throw ("UE5.8 " + $Stage + "失败，退出码=" + $process.ExitCode + "。" + [Environment]::NewLine + ($tail -join [Environment]::NewLine))
    }
    Write-Host ("UE5.8成功：" + $Stage)
}

try {
    if (!$EngineRoot) { throw '必须明确传-EngineRoot或设置UE_ROOT。' }
    $engine = (Resolve-Path -LiteralPath $EngineRoot).Path
    $version = Get-Content (Join-Path $engine 'Engine/Build/Build.version') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) { throw '仅接受锁定UE5.8。' }
    $editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $plugin = Join-Path $root 'Game/Plugins/GamePlatform/World/GamePlatformPCG/Binaries/Win64/UnrealEditor-GamePlatformPCGEditor.dll'
    foreach ($item in @($editor,$project,$plugin,$definitions,$realizedScript,$mapScript,$reopenScript)) {
        if (!(Test-Path -LiteralPath $item)) { throw "引擎/PCG编辑器模块/源码前置条件缺失：$item" }
    }
    foreach ($item in @((Join-Path $foundation 'Templates'),(Join-Path $foundation 'Subgraphs'),(Join-Path $foundation 'Definitions'),(Join-Path $village 'Blueprints'))) {
        Assert-Free $item
    }
    # Gold实例独立占用审查；/Profiles可能还含旧版DA_PCG_Cosmetic等开发资产，不属于本次所有权。
    foreach ($name in @('Canopy','Rock','Crop')) {
        foreach ($relative in @("Realized/PCG_Gold_$name.uasset","Profiles/DA_PCGGold_$name.uasset")) {
            $dest = Join-Path $foundation $relative
            if (Test-Path -LiteralPath $dest) { throw "Gold真实图或Profile已存在，禁止覆盖：$dest" }
        }
    }
    if (Test-Path (Join-Path $foundation 'Validation/PCG_GoldLevel_M1.umap')) { throw 'GoldLevel地图已存在，拒绝覆盖。' }
    if (!$Apply) { Write-Host 'PCG资产创作只读预检通过；-Apply才执行真实UE资产保存。'; return }
    if (@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count) {
        throw '其他UE编辑器正在运行，禁止竞争资产写入。'
    }
    $oldDef = $env:PCG_GOLD_DEFINITION_MODE
    $oldRealize = $env:PCG_GOLD_REALIZE_MODE
    $oldMap = $env:PCG_GOLD_MAP_MODE
    $oldReopen = $env:PCG_GOLD_VALIDATE_MODE
    try {
        Invoke-UE '01-FoundationGraphs' @('-run=GamePlatformPCGFoundationTemplates')
        Invoke-UE '02-VillageBlueprints' @('-run=GamePlatformPCGFoundationTemplates','-BlueprintRoot=/DBAWorldPack_Village/PCG/Blueprints/')
        $env:PCG_GOLD_DEFINITION_MODE = 'apply'
        Invoke-UE '03-GoldDefinitions' @("-ExecutePythonScript=$definitions",'-EnablePlugins=PythonScriptPlugin','-EnablePython')
        $env:PCG_GOLD_REALIZE_MODE = 'apply'
        Invoke-UE '04-RealizedSpawnerGraphs' @("-ExecutePythonScript=$realizedScript",'-EnablePlugins=PythonScriptPlugin','-EnablePython')
        $env:PCG_GOLD_MAP_MODE = 'apply'
        Invoke-UE '05-GoldLevelMap' @("-ExecutePythonScript=$mapScript",'-EnablePlugins=PythonScriptPlugin','-EnablePython')
        # 必须启动与创作独立的UE进程重新打开所有资产，复查蓝图/定义/地图的实际反射类别。
        $env:PCG_GOLD_VALIDATE_MODE = 'reopen'
        Invoke-UE '06-FreshEditorReopen' @("-ExecutePythonScript=$reopenScript",'-EnablePlugins=PythonScriptPlugin','-EnablePython')
    } finally {
        $env:PCG_GOLD_DEFINITION_MODE = $oldDef
        $env:PCG_GOLD_REALIZE_MODE = $oldRealize
        $env:PCG_GOLD_MAP_MODE = $oldMap
        $env:PCG_GOLD_VALIDATE_MODE = $oldReopen
    }
    foreach ($item in @(
        @{Label='Templates';Path=(Join-Path $foundation 'Templates');Expected=12},
        @{Label='Subgraphs';Path=(Join-Path $foundation 'Subgraphs');Expected=7},
        @{Label='Definitions';Path=(Join-Path $foundation 'Definitions');Expected=17},
        @{Label='Realized';Path=(Join-Path $foundation 'Realized');Expected=3},
        @{Label='GoldProfiles';Path=(Join-Path $foundation 'Profiles');Filter='DA_PCGGold_*.uasset';Expected=3},
        @{Label='VillageBlueprints';Path=(Join-Path $village 'Blueprints');Expected=11}
    )) {
        $pattern = if ($item.ContainsKey('Filter')) { $item.Filter } else { '*.uasset' }
        $n=@(Get-ChildItem $item.Path -File -Filter $pattern -ErrorAction SilentlyContinue).Count
        if ($n -ne $item.Expected) {throw "$($item.Label)实际资源$n项，不等于$($item.Expected)项。"}
    }
    if (!(Test-Path (Join-Path $foundation 'Validation/PCG_GoldLevel_M1.umap'))) {throw 'UE5.8没有真实保存金标准地图。'}
    Write-Host 'UE5.8真实资产首次创作完成；独立重开、G01–G16、双Cook仍需单独验收。'
} catch {
    Write-Error $_.Exception.Message
    exit 1
} finally {
    ConvertTo-Json -InputObject @($records.ToArray()) -Depth 8 |
        Set-Content (Join-Path $evidence 'steps.json') -Encoding UTF8
    Write-Host "证据目录：$evidence"
}
