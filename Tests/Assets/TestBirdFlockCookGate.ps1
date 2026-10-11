#requires -Version 7.0
<#
.SYNOPSIS
验证正式Cook脚本中新鸟群后置门禁能够建立证据目录、传播失败且不覆盖前置失败。
.DESCRIPTION
从真实CookFrontEndClient脚本AST提取唯一鸟群门禁块，调用真实Foundation进程助手和UE内置Python。
使用临时合成CSV验证连接代码，不执行Cook/Stage、不绕过UI颜色检查进行发布。
只有输出目录属于本测试；不会删除源资产或启动UnrealEditor。测试不是新发布包验收证据。
#>
param([Parameter(Mandatory)][string]$EngineRoot)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $workspace 'Build/Game/FoundationTools.psm1') -Force
$entry=Join-Path $workspace 'Build/Game/CookFrontEndClient.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($entry,[ref]$tokens,[ref]$errors)
if($errors.Count){throw '正式Cook脚本语法错误'}
$blocks=@($ast.FindAll({param($node)
    $node -is [Management.Automation.Language.IfStatementAst] -and
    $node.Extent.Text.Contains('$birdPython=') -and
    $node.Extent.Text.Contains('ValidateBirdFlockContainer.py')
},$true))
if($blocks.Count -ne 1){throw '未定位唯一鸟群门禁，拒绝执行其他Cook代码'}
$block=[ScriptBlock]::Create($blocks[0].Extent.Text)
$gp='../../../DivineBeastsArena/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock'
$dba='../../../DivineBeastsArena/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock'
$required=@("$gp/Materials/M_GP_BirdFlock_Master.uasset","$gp/Niagara/NS_GP_BirdFlock_Base.uasset","$gp/EffectTypes/NET_GP_BirdFlock_Ambient.uasset","$dba/Blueprints/BP_DBA_BirdFlockAmbient.uasset")
foreach($species in @('Seagull','Swallow','Eagle')){
    $required+=@("$dba/Meshes/Flight/SM_DBA_Bird_$species.uasset","$dba/Textures/T_DBA_Bird_${species}_Albedo.uasset","$dba/Materials/MI_DBA_Bird_$species.uasset","$dba/Niagara/NS_DBA_BirdFlock_$species.uasset","$dba/Definitions/DA_DBA_VFX_BirdFlock_$species.uasset")
}
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('BirdCookGate-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $testRoot|Out-Null
function Invoke-BirdGateCase {
    # 在函数局部作用域执行原门禁，保留真实$code及结果，永不执行UAT或前置UI逻辑。
    param([string]$Name,[string[]]$Files,[int]$InitialCode,[int]$ExpectedCode)
    $directory=Join-Path $testRoot $Name
    New-Item -ItemType Directory $directory|Out-Null
    $csv=Join-Path $directory 'Container.csv'
    $Files|ForEach-Object{[pscustomobject]@{Filename=$_;Platform='WindowsClient'}}|Export-Csv $csv -NoTypeInformation -Encoding utf8
    $context=[pscustomobject]@{Workspace=$workspace;Directory=$directory}
    $engine=[pscustomobject]@{Root=$EngineRoot}
    $details=@{};$code=$InitialCode
    . $block
    if($code -ne $ExpectedCode){throw "$Name 应返回$ExpectedCode，实际$code"}
    $gateReport=Join-Path $directory 'BirdFlockContainer.json'
    if($InitialCode -eq 0){
        if(!(Test-Path $gateReport)){throw "$Name 没有生成真实子进程报告"}
        $report=Get-Content $gateReport -Raw|ConvertFrom-Json
        if(($ExpectedCode -eq 0) -ne ($report.status -eq 'passed')){throw "$Name 报告和退出码不一致"}
    } elseif(Test-Path $gateReport){throw '前置失败后不应该继续执行鸟群门禁'}
    Write-Output "BIRD_COOK_GATE_CASE_PASSED $Name exit=$code"
}
try{
    Invoke-BirdGateCase 'Clean' $required 0 0
    Invoke-BirdGateCase 'Legacy' ($required+"$dba/Meshes/SM_DBA_Bird_Seagull_Cm.uasset") 0 1
    Invoke-BirdGateCase 'KeepPriorFailure' $required 7 7
    Write-Output 'BIRD_COOK_GATE_TESTS_PASSED 3'
}finally{
    # 仅删除本次GUID创建的临时CSV/日志，不接触项目Saved或任何用户资产。
    if(Test-Path $testRoot){Remove-Item $testRoot -Recurse -Force}
}
