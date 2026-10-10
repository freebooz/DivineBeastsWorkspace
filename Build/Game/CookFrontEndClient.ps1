#requires -Version 7.0
<#
.SYNOPSIS
显式发布登录、角色预览和Village客户端，并检查实际IoStore中的必需软引用资产。
.DESCRIPTION
只使用锁定UE5.8及现有Client二进制；不构建、不启动后端、不修改资产或复用旧Cook。
输入EngineRoot、-Cook、超时秒数和独占RunId；输出Saved验证目录、真实退出码及包内容报告。
通过专用FrontEndClient配置纳入UI/流程/英雄软引用，避免仅指定地图时发布黑屏客户端。
专用服务器不使用此入口；失败保留独立证据，回退可运行上一包，不删除或覆盖已有产物。
退出2表示未执行，1表示检查失败；引擎非零码原样保留。通过不等于新手村网络准入通过。
#>
param([string]$EngineRoot, [switch]$Cook,
    # 默认发行前端保持开发资源隔离；候选技能必须显式选择用户批准的开发客户端配置。
    [ValidateSet('FrontEndClient','VillageDevelopmentClient')][string]$CustomConfig='FrontEndClient',
    [ValidateRange(1,86400)][int]$TimeoutSeconds=1800, [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context=$null; $lock=$null; $code=2; $message=''; $details=@{Target='Client';CustomConfig=$CustomConfig}
try {
    $context=New-FoundationContext 'Cook-FrontEndClient' $RunId
    if (!$Cook) { throw [IO.FileNotFoundException]::new('需显式指定-Cook。') }
    $engine=Get-FoundationEngine $EngineRoot
    $details.EngineVersion=$engine.Version
    $details.EngineRoot=$engine.Root
    $tool=Get-FoundationManagedTool $engine AutomationTool
    Assert-FoundationFile $context.Project
    Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/DefaultGame.ini")
    $lock=Enter-FoundationEngineLock $engine.Root
    $cookDirectory=Join-Path $context.Directory 'Cooked/WindowsClient'
    $stageDirectory=Join-Path $context.Directory 'Stage'
    $details.CookDirectory=$cookDirectory; $details.StageDirectory=$stageDirectory
    # 三张真实地图与小范围软引用目录共同组成前端回归包；不使用CookAll扩大到全部开发内容。
    $maps=@('/DBAFrontEndPack/Maps/L_DBA_FrontEnd','/DBAFrontEndPack/Maps/L_DBA_CharacterStudio','/DBAWorldPack_Village/Maps/L_Village_Start')
    $arguments=@($tool.Dll,'BuildCookRun',"-project=$($context.Project)",'-target=DivineBeastsArenaClient',
        '-nop4','-unattended','-utf8output','-cook','-stage','-pak','-skipbuild','-skipbuildeditor','-platform=Win64',
        '-clientconfig=Development','-client','-ddc=NoZenLocalFallback',
        "-AdditionalCookerOptions=-CustomConfig=$CustomConfig -ddc=NoZenLocalFallback",
        ('-map='+($maps -join '+')), "-CookOutputDir=$cookDirectory", "-stagingdirectory=$stageDirectory")
    if ($CustomConfig -eq 'VillageDevelopmentClient') {
        # Cook的CustomConfig不决定Stage：Stage仍按目标收据过滤Config/Custom目录。
        # UAT顶层CustomConfig又会被UBT配置缓存读取，影响其他Stage层，因此只精确白名单这两份开发INI。
        # DefaultGame含开关、十二集合映射和客户端UIProfile扫描；GameplayTags含36个独立冷却标签。
        $developmentConfigFiles = @('DefaultGame.ini','DefaultGameplayTags.ini')
        $details.StagedDevelopmentConfigFiles = @()
        foreach ($configFile in $developmentConfigFiles) {
            Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/$configFile")
            $stageConfigPath = "DivineBeastsArena/Config/Custom/$CustomConfig/$configFile"
            $arguments += "-ini:Game:[Staging]:+AllowedConfigFiles=$stageConfigPath"
            $details.StagedDevelopmentConfigFiles += $stageConfigPath
        }
        # 此记录是发布后启动要求，不代表脚本已经运行游戏；textconfig绕过错误层级的BinaryConfig缓存。
        $details.RequiredDevelopmentRuntimeArguments = "-CustomConfig=$CustomConfig -DBADevelopmentSkills -textconfig"
    }
    $tool.Environment.uebp_LogFolder=Join-Path $context.Directory 'UATLogs'
    $tool.Environment.uebp_EngineSavedFolder=Join-Path $context.Directory 'EngineSaved'
    $result=Invoke-FoundationProcess -FilePath $tool.Executable -Arguments $arguments -WorkingDirectory (Split-Path $tool.Dll) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment
    $details.Result=$result; $code=$result.ExitCode
    if ($code -eq 0) {
        $executable=Join-Path $stageDirectory 'WindowsClient/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe'
        $container=Join-Path $stageDirectory 'WindowsClient/DivineBeastsArena/Content/Paks/DivineBeastsArena-WindowsClient.utoc'
        Assert-FoundationFile $executable; Assert-FoundationFile $container
        $details.Executable=$executable
        # 独立读取最终IoStore，不把Cook退出0或磁盘源资产存在当作资源交付成功。
        $inspectionDirectory=Join-Path $context.Directory 'ContainerInspection'
        $null=New-Item -ItemType Directory -Path $inspectionDirectory
        $csv=Join-Path $inspectionDirectory 'ClientContainer.csv'
        $pakTool=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealPak.exe'
        $inspection=Invoke-FoundationProcess -FilePath $pakTool -Arguments @("-ListContainer=$container","-Csv=$csv") -WorkingDirectory (Split-Path $pakTool) -OutputDirectory $inspectionDirectory -TimeoutSeconds 120
        $details.ContainerInspection=$inspection; $code=$inspection.ExitCode
        if ($code -eq 0) {
            $check=Join-Path $context.Workspace 'Tests/Assets/TestFrontEndClientContainer.ps1'
            $gateArguments = @('-NoProfile','-File',$check,'-ContainerCsv',$csv,'-ReportPath',(Join-Path $context.Directory 'ClientContainer.json'))
            if ($CustomConfig -eq 'VillageDevelopmentClient') { $gateArguments += '-DevelopmentSkills' }
            & (Join-Path $PSHOME 'pwsh.exe') @gateArguments
            $code=$LASTEXITCODE
        }
    }
    $message='实际客户端Cook/Stage和IoStore资源门禁结束；仍须验证认证、UI交互与网络准入。'
} catch [IO.FileNotFoundException] { $code=2; $message=$_.Exception.Message }
catch { $code=1; $message=$_.Exception.Message }
finally { if ($lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
if ($context) { Write-FoundationResult $context $code $message $details } else { Write-Host $message }
exit $code
