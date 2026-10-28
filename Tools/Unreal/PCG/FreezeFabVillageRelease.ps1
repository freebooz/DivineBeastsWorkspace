# 只读冻结本次完整构建输入和服务器权威资产，供客户端/服务器版本门禁使用。
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
$taskAllowed=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Saved/Validation/FabVillageGrass'))
if(!$taskOutput.StartsWith($taskAllowed+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '证据必须位于工程内指定目录'}
if(Test-Path -LiteralPath $taskOutput){throw '证据目录已存在'}
$null=New-Item -ItemType Directory -Path $taskOutput
Import-Module (Join-Path $taskRoot 'Build/Game/FoundationBuildEvidence.psm1') -Force
$inventory=Get-FoundationCompileInputInventory $taskRoot
$inventory | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $taskOutput 'CompileInventory.before.json') -Encoding utf8
$entries=@($inventory.Entries | Where-Object {[IO.Path]::GetExtension($_.Path) -in @('.cpp','.h','.cs','.proto','.ini','.json','.uplugin','.uproject')})
$entries | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $taskOutput 'CompileInputs.before.json') -Encoding utf8
$roots=@('Game/Plugins/DivineBeasts/DBAGameplay/Content','Game/Plugins/DivineBeasts/DBAWorlds/Content','Game/Plugins/DivineBeasts/DBAServer/Content','Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps','Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Maps','Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Definitions','Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Definitions')
$assets=@(foreach($root in $roots){$path=Join-Path $taskRoot $root; if(Test-Path -LiteralPath $path){Get-ChildItem -LiteralPath $path -File -Recurse|ForEach-Object {@{Path=[IO.Path]::GetRelativePath($taskRoot,$_.FullName).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}}})
$assets | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $taskOutput 'AuthorityAssets.before.json') -Encoding utf8
@{CompileInputs=$entries.Count;AuthorityAssets=$assets.Count;Output=$taskOutput} | ConvertTo-Json -Compress
