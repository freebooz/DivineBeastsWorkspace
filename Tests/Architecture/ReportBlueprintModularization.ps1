#requires -Version 7.0
<#
.SYNOPSIS
SSLM 蓝图模块化静态盘点：文件候选、同名身份、原生离线索引类型、源码字面量消费者。
.DESCRIPTION
只读扫描 `.uasset` 和源码；输出 JSON 报告至 Saved/Validation。
不编译/保存/重命名蓝图，不修改资源；PCG_（程序化图）只作候选，真实类型必须由 UE 确认。
离线 Monolith 索引不保证与运行中编辑器一致；未命中的软/动态引用不能当作无消费者证明。
#>
param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
    [string]$MonolithQueryExe = '',
    [string]$OutputPath = ''
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $WorkspaceRoot).Path
$game = Join-Path $root 'Game'
if (-not (Test-Path (Join-Path $game 'DivineBeastsArena.uproject'))) {
    throw '未找到正式 SSLM UE 工程；拒绝在不明目录运行。'
}
function Get-Package([System.IO.FileInfo]$File) {
    $full = $File.FullName
    $contentRoot = Join-Path $game 'Content'
    if ($full.StartsWith($contentRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        $tail = [IO.Path]::GetRelativePath($contentRoot, $full)
        return '/Game/' + (($tail -replace '\.uasset$', '') -replace '\\', '/')
    }
    $parent = $File.Directory
    while ($parent -and $parent.FullName.StartsWith($game, [StringComparison]::OrdinalIgnoreCase)) {
        if ($parent.Name -eq 'Content') {
            $pluginFolder = $parent.Parent
            if (-not $pluginFolder -or
                -not (Test-Path (Join-Path $pluginFolder.FullName ($pluginFolder.Name + '.uplugin')))) {
                return $null
            }
            $tail = [IO.Path]::GetRelativePath($parent.FullName, $full)
            return '/' + $pluginFolder.Name + '/' +
                (($tail -replace '\.uasset$', '') -replace '\\', '/')
        }
        $parent = $parent.Parent
    }
    return $null
}
# 仅排查常见蓝图命名候选。AssetRegistry 才能确定每份资产的真正 UClass。
$roots = @((Join-Path $game 'Content'), (Join-Path $game 'Plugins')) | Where-Object {Test-Path $_}
$all = @(Get-ChildItem $roots -File -Recurse -Filter '*.uasset' -ErrorAction SilentlyContinue)
$prefix = '^(WBP_|BP_|ABP_|BPI_|BPL_|GA_|GE_|PCG_)'
$candidates = @($all | Where-Object {$_.BaseName -match $prefix})
$groups = @($candidates | Group-Object BaseName | Where-Object {$_.Count -gt 1} |
    Sort-Object Name)
$records = @()
foreach ($group in $groups) {
    $items = @()
    foreach ($file in $group.Group) {
        $pkg = Get-Package $file
        $obj = [ordered]@{
            PackagePath = $pkg
            RelativeFile = [IO.Path]::GetRelativePath($root, $file.FullName)
            Bytes = $file.Length
            AssetClass = '未进行原生类型确认'
            IndexedAt = $null
            LiteralConsumers = @()
        }
        if ($pkg -and $MonolithQueryExe -and (Test-Path $MonolithQueryExe)) {
            try {
                $raw = (& $MonolithQueryExe project get_asset_details $pkg 2>$null) -join [Environment]::NewLine
                $meta = $raw | ConvertFrom-Json
                if ($meta.asset_class) {
                    $obj.AssetClass = $meta.asset_class
                    $obj.IndexedAt = $meta.indexed_at
                }
            } catch {
                $obj.AssetClass = '离线索引未解析；需 UE 编辑器核实'
            }
        }
        $items += $obj
    }
    $records += [ordered]@{Name = $group.Name; Paths = $items}
}
$patterns = @($records | ForEach-Object {$_.Paths} |
    Where-Object { $_.PackagePath } | ForEach-Object {$_.PackagePath})
$sourceFiles = @()
foreach ($dir in @('Game/Source','Game/Plugins')) {
    $sourceDir = Join-Path $root $dir
    if (Test-Path $sourceDir) {
        $sourceFiles += @(Get-ChildItem $sourceDir -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object {$_.Extension -in @('.cpp','.h','.cs','.ini','.json','.py','.ps1') -and
                $_.Length -lt 524288 -and $_.FullName -notmatch '[\\/](Intermediate|Binaries|Saved)[\\/]'})
    }
}
$hits = @()
if ($patterns.Count -gt 0 -and $sourceFiles.Count -gt 0) {
    $hits = @(Select-String -LiteralPath $sourceFiles.FullName -Pattern $patterns -SimpleMatch -ErrorAction SilentlyContinue)
}
foreach ($rec in $records) {
    foreach ($asset in $rec.Paths) {
        $matches = @($hits | Where-Object {
            $_.Line.IndexOf($asset.PackagePath, [StringComparison]::OrdinalIgnoreCase) -ge 0
        } | Select-Object -First 25)
        $asset.LiteralConsumers = @($matches | ForEach-Object {
            [ordered]@{
                File = [IO.Path]::GetRelativePath($root, $_.Path)
                Line = $_.LineNumber
            }
        })
    }
}
$result = [ordered]@{
    GeneratedAt = (Get-Date).ToString('o')
    Project = 'SSLM'
    Scope = 'Game/Content; Game/Plugins'
    AssetFiles = $all.Count
    PrefixCandidates = $candidates.Count
    SameNameGroups = $groups.Count
    Note = '仅源文件与离线索引的静态盘点，不代表全部蓝图父类/Graph/Tick/编译或软引用验证。索引可能过期；未发现字面量消费者不能视为可删除。'
    Groups = $records
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $root 'Saved/Validation/BlueprintModularization/20261025/BlueprintDuplicateAssetAudit.json'
}
$outputDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Output ('资产文件=' + $result.AssetFiles + '；命名前缀候选=' + $result.PrefixCandidates +
    '；同名资源组=' + $result.SameNameGroups)
foreach ($row in $records) {
    Write-Output ($row.Name + ': ' + (($row.Paths | ForEach-Object {
        $_.AssetClass + ' (' + $_.PackagePath + ')' }) -join ' / '))
}
Write-Output ('审计结果：' + $OutputPath)
