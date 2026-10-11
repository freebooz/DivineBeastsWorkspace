#requires -Version 5.1
<#
.DESCRIPTION
《神兽联盟》原PBR图集 → UE5.8 Texture2D/MaterialInstance → 已有Niagara外观实例批次导入。
默认仅核验ZIP与Shader父材质，只有-Apply会真实覆盖已准入的纹理/材质。
所有原资产以哈希与完整.uasset备份到Saved/Validation，不动平台Niagara/服务器。
中文路径仅作为上传文件的原名；工程目录和新资源身份仍遵守ASCII英文约束。

前提：在ChatGPT下载DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip并复制到本脚本自动打开的Incoming。
注意：外部模型生成的PBR图仍需UE5.8 D3D12实际渲染验证，不能把此脚本结果标为ProductionReady。
#>
[CmdletBinding()]
param(
    [switch]$Apply,
    [ValidateRange(0,3)][int]$Batch=0
)
$ErrorActionPreference="Stop"
# 覆盖旧材质必须指定1—3的单个批次，避免误用默认全量批次一次性写入15份资产。
if($Apply -and $Batch -eq 0){throw "-Apply必须同时指定-Batch 1、2或3，禁止单命令无回退点地全量覆盖"}
$root=(Resolve-Path (Join-Path $PSScriptRoot "../../../..")).Path
$incoming=Join-Path $root "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature/Incoming"
$zip=Join-Path $incoming "DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip"
# 允许用户上传/下载后自动生成的(1)副本名称；仍只接受预期的两种固定文件名。
$uploadedZip=Join-Path $incoming "DivineBeasts_NiagaraPBR_AtlasAssets_20261011(1).zip"
if(!(Test-Path $zip) -and (Test-Path $uploadedZip)){$zip=$uploadedZip}
$python="D:\UnrealEngine-5.8.0-release\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
$editor="D:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$uproject=Join-Path $root "Game/DivineBeastsArena.uproject"
$check=Join-Path $root "Tools/Unreal/VFX/FallingFoliage/PreflightPBRAtlases.py"
$importer=Join-Path $root "Tools/Unreal/VFX/FallingFoliage/ImportPBRAtlasAssetsUE58.py"
$log=Join-Path $root ("Saved/Validation/FallingFoliage/PBRAtlas20261011/UEImportBatch"+$Batch+".log")
$batchNames=@(
    @("OsmanthusBlossom","RosePetal","GenericPetalPink","GenericPetalWhite","GenericPetalRed"),
    @("RedSpiderLilyPetal","PearPetal","CherryPetal","PlumPetal","PlaneLeaf"),
    @("WillowLeaf","PoplarLeaf","OakLeaf","BirchLeaf","YellowLeaf")
)
if(!(Test-Path $incoming)){$null=New-Item -Path $incoming -ItemType Directory -Force}
# 用户只需下载到Windows默认Downloads；若工程Incoming为空，先尝试可信SHA源自动接收。
if(!(Test-Path $zip) -and !(Test-Path $uploadedZip)){
    $stageHelper=Join-Path $PSScriptRoot 'StagePBRFromDownloads.ps1'
    if(Test-Path -LiteralPath $stageHelper){
        & $stageHelper
        if($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath (Join-Path $incoming 'DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip'))){
            $zip=Join-Path $incoming 'DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip'
        }
    }
}
if(!(Test-Path $zip)){
    Start-Process explorer.exe -ArgumentList @($incoming)
    Write-Output ("MISSING_PBR_ZIP: "+$zip)
    Write-Output "请先从本对话下载ZIP并放入已打开的Incoming目录，然后重新运行本脚本。"
    exit 2
}
if(!(Test-Path $python) -or !(Test-Path $editor) -or !(Test-Path $uproject)){
    throw "锁定的UE5.8引擎或项目路径不存在；不会自动切换引擎版本"
}
# 多进程同时打开同一虚幻工程并修改.uasset可能产生过期引用/丢失保存。
# 禁止在用户正在运行的UE GUI编辑器或其他命令行Editor内覆盖旧资源；不强制关闭任何进程。
if($Apply){
    $otherEditors=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" |
        Where-Object {$_.CommandLine -match 'DivineBeastsArena[.]uproject'})
    if($otherEditors.Count -gt 0){
        throw ('项目UE5.8编辑器仍在使用中（进程：'+($otherEditors.ProcessId -join ',')+'）。请先保存并正常关闭编辑器，避免并发覆盖资产。')
    }
}
# 当前E盘空间较低，优先将可再生的源纹理暂存与旧.uasset备份放到D盘。
# 真正UE资产路径仍固定在DivineBeastsWorkspace，绝不切换项目或内容包。
$projectFree=(Get-PSDrive -Name E).Free
$cacheDefault=Join-Path $root 'Saved/Validation/FallingFoliage/PBRAtlas20261011'
if($projectFree -lt 3GB -and (Get-PSDrive -Name D).Free -ge 3GB){
    $env:AMBIENT_PBR_CACHE_ROOT='D:\DivineBeastsWorkspace_PBR_WorkCache_20261011'
    $originalEBackup=Join-Path $cacheDefault 'UEBackup'
    if(Test-Path $originalEBackup){throw '检测到E盘历史备份，拒绝在同一次导入中切换备份目录；请先复核之前的资产修改。'}
}else{
    $env:AMBIENT_PBR_CACHE_ROOT=$cacheDefault
}
Write-Output ('PBR_TEMP_CACHE='+$env:AMBIENT_PBR_CACHE_ROOT+' E_FREE_GB='+[math]::Round($projectFree/1GB,2))
Write-Output ("PBR_IMPORT_BATCH="+$Batch+" Apply="+$Apply)
& $python $check "--mode" "inspect" "--file" $zip
if($LASTEXITCODE -ne 0){throw "ZIP完整性校验失败，禁止导入"}
& $python $check "--mode" "extract" "--file" $zip "--allow-existing-stage"
if($LASTEXITCODE -ne 0){throw "安全解包与文件SHA-256校验失败"}
$env:AMBIENT_PBR_APPLY=if($Apply){"approved_apply"}else{"inspect"}
$env:AMBIENT_PBR_BATCH_INDEX=[string]$Batch
if($Batch -gt 0){$env:AMBIENT_PBR_STYLES=$batchNames[$Batch-1] -join ","}
else{$env:AMBIENT_PBR_STYLES=""}
Write-Output ("PBR_IMPORT_MODE="+$env:AMBIENT_PBR_APPLY+" STYLE_FILTER="+$env:AMBIENT_PBR_STYLES)
& $editor $uproject "-unattended" "-nop4" "-NoLiveCoding" "-NoSound" "-NullRHI" "-nosplash" "-ExecutePythonScript=$importer" "-abslog=$log"
$exit=$LASTEXITCODE
$report=Join-Path $root "Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBatchImportReport.json"
if(Test-Path $report){
    $obj=Get-Content $report -Raw -Encoding utf8 | ConvertFrom-Json
    Write-Output ("PBR_UE_IMPORT_RESULT="+$obj.success+" SAVED="+@($obj.editorSavedAssets).Count+" FAILURES="+@($obj.failures).Count)
    foreach($err in @($obj.failures)){Write-Output ("PBR_IMPORT_ERROR="+$err)}
    if(!$obj.success){exit 1}
    # 逐批保留UE材质真实导入报告，避免后续批次覆盖同名JSON证据。
    $batchReport=Join-Path $root ("Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBatchImportReport_Batch"+$Batch+".json")
    Copy-Item -LiteralPath $report -Destination $batchReport -Force -ErrorAction Stop
    Write-Output ("PBR_BATCH_REPORT="+$batchReport)
}else{
    Write-Output "未见官方UE导入报告，不能将引擎进程退出0作为资产成功"
    exit 1
}
exit $exit
