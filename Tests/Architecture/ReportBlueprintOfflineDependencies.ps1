#requires -Version 7.0
<#
.SYNOPSIS
SSLM（神兽联盟）离线已索引蓝图完整依赖账本。
.DESCRIPTION
逐项读取 Monolith 已保存的硬/软引用信息；不启动/修改Editor（编辑器），
不编译、不运行自动化、不删除蓝图，不使用“反向引用为0”自动判定可删。
动态字符串加载、当前地图外置Actor和引擎未索引的引用仍须Monolith原生确认。
#>
param(
 [string]$WorkspaceRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path,
 [string]$MonolithQueryExe='D:\UnrealEngine-5.8.0-release\Engine\Plugins\Marketplace\Monolith\Binaries\monolith_query.exe',
 [string]$InventoryPath='',
 [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $WorkspaceRoot).Path
if(-not(Test-Path (Join-Path $root 'Game/DivineBeastsArena.uproject'))){throw '需要正式SSLM工程。'}
if(-not(Test-Path $MonolithQueryExe)){throw 'Monolith离线索引不可用。'}
if(-not $InventoryPath){
 $InventoryPath=Join-Path $root 'Saved/Validation/BlueprintModularization/20261026/BlueprintAssetClassInventory_Offline.json'
}
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root 'Saved/Validation/BlueprintModularization/20261026'}
if(-not(Test-Path $InventoryPath)){throw '必须先运行 ReportAllBlueprintAssetsOffline.ps1 得到完整类别清单。'}
$inventory=Get-Content $InventoryPath -Raw|ConvertFrom-Json
$assets=@($inventory.Assets)
if($assets.Count -lt 1){throw '离线资产清单为空。'}
$rows=[Collections.Generic.List[object]]::new()
$failures=[Collections.Generic.List[string]]::new()
foreach($asset in $assets){
 try {
   $path=[string]$asset.PackagePath
   $payload=(& $MonolithQueryExe project find_references $path 2>$null) -join [Environment]::NewLine
   $data=$payload|ConvertFrom-Json -ErrorAction Stop
   if(-not $data.success){throw '离线依赖查询返回失败'}
   $outbound=@($data.depends_on|Where-Object{$null -ne $_}|ForEach-Object{
     [ordered]@{Path=$_.path;Class=$_.class;Kind=$_.type}})
   $inbound=@($data.referenced_by|Where-Object{$null -ne $_}|ForEach-Object{
     [ordered]@{Path=$_.path;Class=$_.class;Kind=$_.type}})
   $rows.Add([pscustomobject]@{
     Name=$asset.Name
     AssetClass=$asset.AssetClass
     PackagePath=$path
     Mount=$asset.Mount
     OfflineNodeCount=$asset.OfflineNodeCount
     OfflineInboundCount=$inbound.Count
     OfflineOutboundCount=$outbound.Count
     OfflineHardInboundCount=@($inbound|Where-Object{$_.Kind -eq 'Hard'}).Count
     OfflineHardOutboundCount=@($outbound|Where-Object{$_.Kind -eq 'Hard'}).Count
     ReferencedBy=$inbound
     DependsOn=$outbound
     LiveGraphVerified=$false
     DynamicConsumersVerified=$false
     SafeToDelete=$false
   })
 }catch{
   $failures.Add([string]$asset.PackagePath+': '+$_.Exception.Message)
 }
}
$rankInbound=@($rows|Sort-Object OfflineInboundCount -Descending|Select-Object -First 25 Name,AssetClass,PackagePath,OfflineInboundCount,OfflineHardInboundCount)
$rankOutbound=@($rows|Sort-Object OfflineOutboundCount -Descending|Select-Object -First 25 Name,AssetClass,PackagePath,OfflineOutboundCount,OfflineHardOutboundCount)
$nonzero=@($rows|Where-Object{$_.OfflineInboundCount -gt 0 -or $_.OfflineOutboundCount -gt 0})
$result=[ordered]@{
 GeneratedAt=(Get-Date).ToString('o')
 RepoHead=(git -C $root rev-parse HEAD)
 Source='Monolith离线索引，不是实时UE AssetRegistry（资产注册表）'
 IndexedBlueprintCount=$assets.Count
 QueriedCount=$rows.Count
 ReferenceFailures=@($failures)
 WithAnyIndexedReferenceCount=$nonzero.Count
 WithoutIndexedReferenceCount=$rows.Count-$nonzero.Count
 TopInbound=$rankInbound
 TopOutbound=$rankOutbound
 AssetReferences=@($rows)
 Warnings=@(
   '0个离线消费者不等于无人使用；字符串软加载、代码间接加载、地图实例以及外部Content可能未入索引',
   '不能单凭此报告自动更改Blueprint父类或删除重定向器',
   '未编译、未运行UE、未核实真实父类/图连线/PIE/Cook'
 )
}
New-Item $OutputDirectory -ItemType Directory -Force|Out-Null
$j=Join-Path $OutputDirectory 'BlueprintReferenceInventory_Offline.json'
$c=Join-Path $OutputDirectory 'BlueprintReferenceInventory_Offline.csv'
$result|ConvertTo-Json -Depth 16|Set-Content -LiteralPath $j -Encoding utf8
$rows|Select-Object Name,AssetClass,PackagePath,Mount,OfflineNodeCount,OfflineInboundCount,OfflineOutboundCount,OfflineHardInboundCount,OfflineHardOutboundCount|
 Export-Csv -NoTypeInformation -Encoding utf8 -Path $c
Write-Output ('离线依赖已检查='+$rows.Count+'/'+$assets.Count+' 异常='+$failures.Count+' 有引用节点='+$nonzero.Count)
Write-Output '主要离线入站使用者节点（至多前12）：'
$rankInbound|Select-Object -First 12|ForEach-Object{
  Write-Output (" "+$_.OfflineInboundCount+" "+$_.PackagePath)
}
Write-Output ('报告：'+$j)
Write-Output ('表格：'+$c)
