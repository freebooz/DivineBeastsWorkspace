#requires -Version 7.0
<#
.SYNOPSIS
SSLM（神兽联盟）可复用蓝图候选静态盘点。
.DESCRIPTION
只读扫描项目 .uasset 文件名，并从锁定的Monolith（虚幻编辑器资产索引）离线数据库提取资产类与节点摘要。
结果用于挑选公共基类审查对象，不读取真正的父类、引脚和图连线，不做编译、验证或保存资产。
默认将报告写入 Saved/Validation，不修改任何UE二进制文件、配置或Git版本。
#>
param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
    [string]$MonolithQueryExe = 'D:\UnrealEngine-5.8.0-release\Engine\Plugins\Marketplace\Monolith\Binaries\monolith_query.exe',
    [string]$OutputPath = ''
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $WorkspaceRoot).Path
$game = Join-Path $root 'Game'
if (-not (Test-Path (Join-Path $game 'DivineBeastsArena.uproject'))) {
    throw '未定位SSLM正式工程 Game/DivineBeastsArena.uproject，停止盘点。'
}
function Get-UEPackage([System.IO.FileInfo]$File) {
    $contentRoot = Join-Path $game 'Content'
    if ($File.FullName.StartsWith($contentRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        $relative = [IO.Path]::GetRelativePath($contentRoot, $File.FullName)
        return '/Game/' + (($relative -replace '\.uasset$', '') -replace '\\', '/')
    }
    $dir = $File.Directory
    while ($null -ne $dir -and $dir.FullName.StartsWith($game,
        [StringComparison]::OrdinalIgnoreCase)) {
        if ($dir.Name -eq 'Content') {
            $plugin = $dir.Parent
            if ($null -eq $plugin -or -not (Test-Path (Join-Path $plugin.FullName ($plugin.Name + '.uplugin')))) {
                return $null
            }
            $relative = [IO.Path]::GetRelativePath($dir.FullName, $File.FullName)
            return '/' + $plugin.Name + '/' + (($relative -replace '\.uasset$', '') -replace '\\', '/')
        }
        $dir = $dir.Parent
    }
    return $null
}

$roots = @((Join-Path $game 'Content'), (Join-Path $game 'Plugins')) | Where-Object {Test-Path $_}
$files = @(Get-ChildItem $roots -File -Recurse -Filter '*.uasset' -ErrorAction SilentlyContinue |
    Where-Object {$_.BaseName -match '^(BP_|WBP_|ABP_|BPI_|BPL_|BPF_|BPM_|EUBP_|EUW_)'} |
    Sort-Object FullName)
$offlineEnabled = -not [string]::IsNullOrWhiteSpace($MonolithQueryExe) -and (Test-Path $MonolithQueryExe)
$records = [Collections.Generic.List[object]]::new()
$failed = 0
foreach ($file in $files) {
    $package = Get-UEPackage $file
    $record = [ordered]@{
        Name = $file.BaseName
        PackagePath = $package
        File = [IO.Path]::GetRelativePath($root, $file.FullName)
        Bytes = $file.Length
        AssetClass = '未核实'
        OfflineIndexedAt = $null
        OfflineNodeCount = $null
        OfflineNonEventNodeCount = $null
        OfflineNodeTypeHistogram = @{}
        OfflineNodeNames = @()
        AssetRedirector = $false
        ParentClassVerified = $false
        GraphWiresVerified = $false
        Note = '离线摘要并非UE5.8当前图表/父类验证；名称和字节大小不能确定复杂度'
    }
    if ($offlineEnabled -and $package) {
        try {
            $raw = (& $MonolithQueryExe project get_asset_details $package 2>$null) -join [Environment]::NewLine
            $res = ConvertFrom-Json -InputObject $raw -ErrorAction Stop
            if (-not $res.error -and -not [string]::IsNullOrEmpty($res.asset_class)) {
                $record.AssetClass = $res.asset_class
                $record.OfflineIndexedAt = $res.indexed_at
                $record.AssetRedirector = $res.asset_class -eq 'ObjectRedirector'
                $nodes = @($res.nodes)
                $record.OfflineNodeCount = $nodes.Count
                $nonEvents = @($nodes | Where-Object {
                    $_.node_type -ne 'Event' -and $_.node_class -ne 'K2Node_FunctionEntry'
                })
                $record.OfflineNonEventNodeCount = $nonEvents.Count
                $record.OfflineNodeTypeHistogram = @{}
                foreach($group in @($nodes | Group-Object node_class)){
                    $record.OfflineNodeTypeHistogram[$group.Name] = $group.Count
                }
                $record.OfflineNodeNames = @($nodes | Select-Object -First 35 -ExpandProperty node_name)
            } else {++$failed}
        } catch {++$failed}
    }
    $records.Add([pscustomobject]$record)
}
$classes = @($records |Group-Object AssetClass |Sort-Object Count -Descending|
    ForEach-Object {[ordered]@{Class=$_.Name; Count=$_.Count}})
$mounts = @($records |Group-Object {if($_.PackagePath){($_.PackagePath.TrimStart('/') -split '/')[0]}else{'未挂载'}}|
    Sort-Object Count -Descending|Select-Object -First 30|
    ForEach-Object {[ordered]@{Mount=$_.Name; Count=$_.Count}})
$nonRedirect = @($records | Where-Object {(-not $_.AssetRedirector) -and $null -ne $_.OfflineNodeCount})
$rank = @($nonRedirect |Sort-Object @{Expression='OfflineNonEventNodeCount';Descending=$true},@{Expression='Bytes';Descending=$true} |
    Select-Object -First 18 Name,PackagePath,AssetClass,Bytes,OfflineNodeCount,OfflineNonEventNodeCount,OfflineIndexedAt)
$result = [ordered]@{
    GeneratedAt = (Get-Date).ToString('o')
    Project = 'SSLM'
    EvidenceScope = '仅原始文件名+Monolith离线索引（可能过期），不编译/不调用编辑器/不修改uasset'
    CandidateCount = $records.Count
    OfflineEnabled = $offlineEnabled
    OfflineMisses = $failed
    ClassBreakdown = $classes
    MountBreakdown = $mounts
    IndexedNonEventNodeRank = $rank
    Candidates = @($records)
    Limitations = @(
        '资产真实父类、组件树、硬软引用、所有事件图连线均未核实',
        'EventTick默认节点出现不证明存在Tick逻辑；节点汇总不是复杂度分数',
        '离线索引可能落后于磁盘蓝图，无法作为删除/迁移/编译通过凭据'
    )
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $root 'Saved/Validation/BlueprintModularization/20261025/ReusableBlueprintCandidates.json'
}
New-Item -Path (Split-Path $OutputPath -Parent) -ItemType Directory -Force | Out-Null
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Output "候选文件=$($records.Count)；离线可查询=$offlineEnabled；离线查询未命中=$failed"
$classes | ForEach-Object {Write-Output ("真实类型离线索引 "+$_.Class+"="+$_.Count)}
Write-Output '离线节点摘要优先人工核查的前18项（非复杂度定论）：'
$rank | ForEach-Object {
    Write-Output ("$($_.OfflineNonEventNodeCount)个非事件节点 " + $_.PackagePath)
}
Write-Output ('静态报告：'+$OutputPath)
