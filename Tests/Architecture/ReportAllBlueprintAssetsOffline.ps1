#requires -Version 7.0
<#
.SYNOPSIS
SSLM（神兽联盟）所有已离线索引蓝图的只读盘点。
.DESCRIPTION
只读取项目 Content（资源目录）与 Monolith（编辑器离线项目数据库）的现有索引，
不启动编辑器、不编译、不保存UE .uasset、不修改当前工程配置/版本。
离线节点摘要和软硬引用不能代替真实UE5.8 ParentClass（父类）、Graph（图连线）及编译。
输出在 Saved/Validation 下，仅用于制定逐资源重构清单。
#>
param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
    [string]$MonolithQueryExe = 'D:\UnrealEngine-5.8.0-release\Engine\Plugins\Marketplace\Monolith\Binaries\monolith_query.exe',
    [string]$OutputDirectory = '',
    [switch]$SkipNodeDetails
)
$ErrorActionPreference = 'Stop'
$root=(Resolve-Path $WorkspaceRoot).Path
$game=Join-Path $root 'Game'
if(-not (Test-Path (Join-Path $game 'DivineBeastsArena.uproject'))) {
    throw '未定位SSLM正式工程。'
}
if(-not (Test-Path $MonolithQueryExe)) {
    throw 'Monolith离线资产索引程序不存在；无法假装扫描全部UE蓝图。'
}

function PackageName([System.IO.FileInfo]$Item) {
    $content = Join-Path $game 'Content'
    if($Item.FullName.StartsWith($content+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
        $r=[IO.Path]::GetRelativePath($content,$Item.FullName)
        return '/Game/'+(($r -replace '\.uasset$','') -replace '\\','/')
    }
    $parent=$Item.Directory
    while($parent -and $parent.FullName.StartsWith($game,[StringComparison]::OrdinalIgnoreCase)){
        if($parent.Name -eq 'Content'){
            $plugin=$parent.Parent
            if($plugin -and (Test-Path (Join-Path $plugin.FullName ($plugin.Name+'.uplugin')))){
                $r=[IO.Path]::GetRelativePath($parent.FullName,$Item.FullName)
                return '/'+$plugin.Name+'/'+(($r -replace '\.uasset$','') -replace '\\','/')
            }
            return $null
        }
        $parent=$parent.Parent
    }
    return $null
}
$disk=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
$diskPaths=@((Join-Path $game 'Content'),(Join-Path $game 'Plugins'))|Where-Object{Test-Path $_}
foreach($asset in @(Get-ChildItem $diskPaths -Recurse -File -Filter '*.uasset' -ErrorAction SilentlyContinue)){
    $name=PackageName $asset
    if($name){$disk[$name]=[pscustomobject]@{Bytes=$asset.Length;RelativePath=[IO.Path]::GetRelativePath($root,$asset.FullName)}}
}

$assets=[Collections.Generic.List[object]]::new()
$classCounts=[ordered]@{}
$errors=[Collections.Generic.List[string]]::new()
foreach($class in @('Blueprint','WidgetBlueprint','AnimBlueprint')){
    $payload=(& $MonolithQueryExe project find_by_type $class --limit=1000 2>$null) -join [Environment]::NewLine
    $response=$payload|ConvertFrom-Json -ErrorAction Stop
    if(-not $response.success){throw "Monolith离线蓝图类别查询失败：$class"}
    $all=@($response.results)
    $classCounts[$class]=$all.Count
    foreach($item in $all){
        $package=[string]$item.package_path
        $diskItem=$null
        if($disk.ContainsKey($package)){$diskItem=$disk[$package]}
        $row=[ordered]@{
            AssetClass=$class
            Name=$item.asset_name
            PackagePath=$package
            Mount=$item.module_name
            DiskExists=($null -ne $diskItem)
            DiskBytes=if($diskItem){$diskItem.Bytes}else{$null}
            RelativeFile=if($diskItem){$diskItem.RelativePath}else{$null}
            OfflineIndexedAt=$null
            OfflineNodeCount=$null
            OfflineNonEventNodes=$null
            OfflineNodesByType=@{}
            ParentClassVerified=$false
            GraphWiresVerified=$false
            CurrentEditorVerified=$false
            DynamicReferencesVerified=$false
        }
        if(-not $SkipNodeDetails) {
            try{
                $json=(& $MonolithQueryExe project get_asset_details $package 2>$null)-join [Environment]::NewLine
                $details=$json|ConvertFrom-Json -ErrorAction Stop
                if($details.asset_class -eq $class){
                    $row.OfflineIndexedAt=$details.indexed_at
                    $nodes=@($details.nodes)
                    $row.OfflineNodeCount=$nodes.Count
                    $row.OfflineNonEventNodes=@($nodes|Where-Object{
                        $_.node_type -ne 'Event' -and $_.node_class -ne 'K2Node_FunctionEntry'
                    }).Count
                    foreach($g in @($nodes|Group-Object node_class)){$row.OfflineNodesByType[$g.Name]=$g.Count}
                }else{
                    $errors.Add("$package ：索引详情类型与清单不一致，可能已过期")
                }
            }catch{$errors.Add("$package ：离线详情不可用")}
        }
        $assets.Add([pscustomobject]$row)
    }
}
$missing=@($assets|Where-Object {-not $_.DiskExists})
$interesting=@($assets|Where-Object {$_.AssetClass -in @('Blueprint','WidgetBlueprint','AnimBlueprint') -and $_.OfflineNonEventNodes -gt 0}|
    Sort-Object -Property @{Expression='OfflineNonEventNodes';Descending=$true},@{Expression='DiskBytes';Descending=$true}|
    Select-Object -First 40 Name,AssetClass,PackagePath,OfflineNodeCount,OfflineNonEventNodes,OfflineIndexedAt)
$mounts=@($assets|Group-Object Mount|Sort-Object Count -Descending|ForEach-Object {
    [ordered]@{Mount=$_.Name;Count=$_.Count}})
$prefix=@($assets|Group-Object {if($_.Name -match '^([A-Za-z]+)_'){$Matches[1]}else{'其他'}}|
    Sort-Object Count -Descending|ForEach-Object{[ordered]@{Prefix=$_.Name;Count=$_.Count}})
$result=[ordered]@{
    GeneratedAt=(Get-Date).ToString('o')
    RepositoryHead=(git -C $root rev-parse HEAD)
    Source='Monolith离线索引 + 当前磁盘资产路径；不是UE Editor实时资产注册表'
    BlueprintCounts=$classCounts
    IndexedBlueprintTotal=$assets.Count
    DiskAssetTotal=$disk.Count
    OfflineOnlyCount=$missing.Count
    OfflineInconsistencies=$errors.ToArray()
    OfflineOnlyAssets=@($missing|Select-Object Name,PackagePath,AssetClass)
    PrefixBreakdown=$prefix
    MountBreakdown=$mounts
    TopNodeAuditCandidates=$interesting
    Assets=@($assets)
    Limitations=@(
        '不确认二进制资源真实父类、重定向器目标、依赖和引脚连线',
        '索引可能晚于或早于当前文件；节点计数不等于逻辑复杂度或已连接的Tick',
        '本命令未编译、未运行引擎/自动化、未Cook或验证网络'
    )
}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root 'Saved/Validation/BlueprintModularization/20261026'}
New-Item -Path $OutputDirectory -ItemType Directory -Force|Out-Null
$jsonFile=Join-Path $OutputDirectory 'BlueprintAssetClassInventory_Offline.json'
$csvFile=Join-Path $OutputDirectory 'BlueprintAssetClassInventory_Offline.csv'
$result|ConvertTo-Json -Depth 14|Set-Content $jsonFile -Encoding utf8
$assets|Select-Object Name,AssetClass,PackagePath,Mount,DiskExists,DiskBytes,OfflineIndexedAt,OfflineNodeCount,OfflineNonEventNodes|
    Export-Csv -NoTypeInformation -Encoding utf8 -Path $csvFile
Write-Output ('索引蓝图='+$assets.Count+' 磁盘资产='+$disk.Count+' 只有离线索引='+$missing.Count+' 详情异常='+$errors.Count)
$prefix|ForEach-Object{Write-Output ("前缀 "+$_.Prefix+"="+$_.Count)}
Write-Output '索引节点优先复核（前10，非真实复杂度分数）：'
$interesting|Select-Object -First 10|ForEach-Object{Write-Output (" "+$_.OfflineNonEventNodes+" "+$_.PackagePath)}
Write-Output '报告：'
Write-Output $jsonFile
Write-Output $csvFile
