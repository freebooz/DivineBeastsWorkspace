#requires -Version 7.0
<#
.SYNOPSIS
显式-Cook执行Development开发地图Cook及Stage；不触发C++构建或启动服务器。
.DESCRIPTION
Target为Client或Server，默认Client。Maps默认两张Foundation开发地图。
仅使用UE5.8真实BuildCookRun参数：-target、-client或-server -noclient、-CookOutputDir、-stagingdirectory。
独立新目录代替清理旧Cook；默认不迭代。不包含-build/-run/-clean。
返回0成功，1脚本失败，2未执行；外部非零退出码原样保留。
#>
param([string]$EngineRoot, [switch]$Cook, [ValidateSet('Client','Server')][string]$Target = 'Client',
    # 保留Foundation默认行为；新手村开发验证必须显式选配置，不在发行默认中启用候选技能。
    [ValidateSet('FoundationStandalone','VillageServer','VillageDevelopmentServer')][string]$CustomConfig='FoundationStandalone',
    [switch]$Pak,
    [string[]]$Maps = @('/Game/Development/Foundation/Maps/L_FoundationBootstrap','/Game/Development/Foundation/Maps/L_FoundationSandbox'),
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 1800, [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context = $null; $lock = $null; $code = 2; $message = ''; $details = @{ Target=$Target; Maps=$Maps; Configuration='Development'; Platform='Win64'; CustomConfig=$CustomConfig; Pak=[bool]$Pak }
try {
    $context = New-FoundationContext "Cook-$Target" $RunId
    if (-not $Cook) { throw [IO.FileNotFoundException]::new('需显式指定-Cook。') }
    if ($CustomConfig -eq 'VillageDevelopmentServer' -and $Target -ne 'Server') {
        throw 'VillageDevelopmentServer只允许专用服务器目标；客户端开发验证使用CookFrontEndClient。'
    }
    $engine = Get-FoundationEngine $EngineRoot
    $details.EngineVersion = $engine.Version; $details.EngineRoot = $engine.Root
    Assert-FoundationFile $context.Project
    Assert-FoundationFile (Join-Path $context.Workspace "Game/Source/DivineBeastsArena$Target.Target.cs")
    # Foundation地图白名单不能隐式扩大；只有Village专服配置可显式发布这一张已存在的正式村庄地图。
    # 不接受任意插件路径或组合地图，防止测试入口变成无边界CookAll；其余配置沿用原Foundation门禁。
    if ($Target -eq 'Server' -and $CustomConfig -in @('VillageServer','VillageDevelopmentServer') -and
        $Maps.Count -eq 1 -and $Maps[0] -ceq '/DBAWorldPack_Village/Maps/L_Village_Start') {
        Assert-FoundationFile (Join-Path $context.Workspace 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap')
    } else {
        Assert-FoundationMaps $context $Maps
    }
    Assert-FoundationFile (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
    $tool = Get-FoundationManagedTool $engine AutomationTool
    $lock = Enter-FoundationEngineLock $engine.Root
    # UE单平台-outputdir不自动附加平台；Stage仅在目录末段匹配CookPlatform时使用原路径。
    $cookDirectory = Join-Path $context.Directory "Cooked/Windows$Target"
    $stageDirectory = Join-Path $context.Directory 'Stage'
    $details.CookDirectory = $cookDirectory; $details.StageDirectory = $stageDirectory
    # Cook只由AdditionalCookerOptions选择角色/开发配置；不向UAT顶层传CustomConfig。
    # UBT ConfigCache仍会读取全局CustomConfig并改变Stage配置层；专服必须保留收据DedicatedServer剥离规则。
    $arguments = @($tool.Dll,'BuildCookRun',"-project=$($context.Project)","-target=DivineBeastsArena$Target",'-nop4','-unattended','-utf8output','-cook','-stage','-skipbuild','-skipbuildeditor','-platform=Win64','-clientconfig=Development','-serverconfig=Development',"-AdditionalCookerOptions=-CustomConfig=$CustomConfig -ddc=NoZenLocalFallback",'-ddc=NoZenLocalFallback',('-map=' + ($Maps -join '+')),"-CookOutputDir=$cookDirectory", "-stagingdirectory=$stageDirectory")
    if ($CustomConfig -eq 'VillageDevelopmentServer') {
        # Stage的AllowedConfigFiles先于收据CustomConfig目录过滤；只为显式开发发布加入两份运行INI。
        # 不改变正式Target、DedicatedServer的Pak规则或默认发行关闭状态；其他配置没有这些白名单。
        $developmentConfigFiles = @('DefaultGame.ini','DefaultGameplayTags.ini')
        $details.StagedDevelopmentConfigFiles = @()
        foreach ($configFile in $developmentConfigFiles) {
            Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/$configFile")
            $stageConfigPath = "DivineBeastsArena/Config/Custom/$CustomConfig/$configFile"
            $arguments += "-ini:Game:[Staging]:+AllowedConfigFiles=$stageConfigPath"
            $details.StagedDevelopmentConfigFiles += $stageConfigPath
        }
        # 发布后启动还须指定这组参数；textconfig令实际进程重新读取已Stage的自定义INI层。
        $details.RequiredDevelopmentRuntimeArguments = "-CustomConfig=$CustomConfig -DBADevelopmentSkills -textconfig"
    }
    if ($Pak) { $arguments += '-pak' }
    if ($Target -eq 'Server') { $arguments += @('-server','-noclient','-serverplatform=Win64') } else { $arguments += '-client' }
    $tool.Environment.uebp_LogFolder = Join-Path $context.Directory 'UATLogs'
    $tool.Environment.uebp_EngineSavedFolder = Join-Path $context.Directory 'EngineSaved'
    $result = Invoke-FoundationProcess -FilePath $tool.Executable -Arguments $arguments -WorkingDirectory (Split-Path $tool.Dll) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment
    $code = $result.ExitCode; $details.Result = $result
    if ($code -eq 0) {
        # 工具退出0仍需核验实际Stage可执行文件，防止空输出误报成功。
        $executables = @(Get-ChildItem -LiteralPath $stageDirectory -Recurse -File -Filter "DivineBeastsArena$Target.exe" -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq 'Win64' })
        if ($executables.Count -ne 1) { throw "Stage目标程序数量应为1，实际$($executables.Count)。" }
        $details.Executable = $executables[0].FullName
    }
    $message = '实际UAT开发地图Cook/Stage结束；不代表运行或服务器资源剥离审计通过。'
} catch [IO.FileNotFoundException] { $code = 2; $message = $_.Exception.Message }
catch { $code = 1; $message = $_.Exception.Message }
finally { if ($null -ne $lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
$status = if ($details.ContainsKey('Result') -and $details.Result.ExitCode -ne 0) { 'Failed' } else { '' }
if ($context) { Write-FoundationResult $context $code $message $details $status } else { Write-Host $message }
exit $code
