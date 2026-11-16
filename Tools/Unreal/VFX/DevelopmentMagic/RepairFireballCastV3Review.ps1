# 《神兽联盟》火球V3 P1 GPU修复：只修改已批准的审核 NiagaraScript 和 NiagaraSystem
# 必须连接带 -FireballV3OwnedReview 标识的独占编辑器；不接管其它工作会话。
# 从作者脚本读取唯一 HLSL 真源，拒绝重复创建、不覆盖任何正式火球/战斗资产。
param([int]$Port=9316)
$ErrorActionPreference='Stop'
$base='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/'
$module=$base+'NM_DBA_FireballCast_V3Review'
$system=$base+'NS_DBA_FireballCast_V3Review'
$authorPath=Join-Path $PSScriptRoot 'AuthorFireballCastV3Review.ps1'
$source=Get-Content $authorPath -Raw -Encoding utf8
$hit=[regex]::Match($source,"(?s)\$hlsl=@'\r?\n(.*?)\r?\n'@")
if(-not $hit.Success){throw '缺少冻结的HLSL作者源'}
$hlsl=$hit.Groups[1].Value
if(-not $hlsl.Contains('int uid = Particles.UniqueID;') -or $hlsl.Contains('(Index % 2)')){throw 'GPU作者源未完成修复'}
function CallM([string]$name,[hashtable]$payload){
 $q=@{jsonrpc='2.0';id=1;method='tools/call';params=@{name=$name;arguments=$payload}}|ConvertTo-Json -Depth 35 -Compress
 $resp=Invoke-RestMethod -Uri "http://127.0.0.1:$Port/mcp" -Method Post -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes($q)) -TimeoutSec 110
 if($resp.error -or $resp.result.isError){throw ('Monolith调用失败 '+($resp|ConvertTo-Json -Depth 10 -Compress))}
 $body=($resp.result.content|Where-Object {$_.type -eq 'text'}|Select-Object -First 1).text
 if(-not $body){throw 'Monolith返回空内容'}
 $v=$body|ConvertFrom-Json -Depth 40
 if(($v.PSObject.Properties.Name -contains 'success' -and $v.success -eq $false) -or $v.error){throw ('Monolith拒绝 '+$body)}
 return $v
}
$probe=@'
import unreal,os
assert '-FireballV3OwnedReview' in unreal.SystemLibrary.get_command_line(), '编辑器未持有本次P1独占授权'
assert 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
print('SSLM_P1_EDITOR_ID',os.getpid(),unreal.Paths.get_project_file_path())
'@
CallM 'editor_query' @{action='run_python';params=@{command=$probe;unattended=$true}}|Out-Null
$existing=CallM 'niagara_query' @{action='get_custom_hlsl_text';params=@{script_path=$module}}
if(-not $existing.node_guid){throw '找不到准确的HLSL节点身份'}
$updated=CallM 'niagara_query' @{action='set_custom_hlsl_text';params=@{script_path=$module;node_guid=$existing.node_guid;hlsl=$hlsl}}
Write-Output ('HLSL_UPDATED '+($updated|ConvertTo-Json -Depth 6 -Compress))
$save=@"
import unreal
p='$module'
asset=unreal.load_asset(p)
assert isinstance(asset,unreal.NiagaraScript), '审核脚本类型不符'
assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False), 'NiagaraScript无法保存'
print('SSLM_P1_NIAGARA_SCRIPT_SAVED',p)
"@
$r=CallM 'editor_query' @{action='run_python';params=@{command=$save;unattended=$true}}
Write-Output ('NATIVE_SCRIPT_SAVE '+($r.output|ConvertTo-Json -Depth 8 -Compress))
$diag=CallM 'niagara_query' @{action='get_system_diagnostics';params=@{asset_path=$system;compile_first=$true}}
Write-Output ('NATIVE_NIAGARA_DIAGNOSTICS errors='+$diag.error_count+' warnings='+$diag.warning_count)
if($diag.error_count -or $diag.warning_count){
 $diag.errors|Select-Object -First 8|ForEach-Object {Write-Output ('ERROR '+$_.message)}
 throw 'Niagara翻译仍失败，禁止标记通过'
}
$saveSys=CallM 'niagara_query' @{action='save_system';params=@{asset_path=$system;only_if_dirty=$false}}
Write-Output ('P1_SYSTEM_SAVED '+($saveSys|ConvertTo-Json -Depth 6 -Compress))
Write-Output 'P1_SERVER_TIMING_UNCHANGED'
