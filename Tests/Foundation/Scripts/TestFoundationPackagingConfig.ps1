#requires -Version 7.0
<#
.SYNOPSIS
使用UE自带UBT配置解析器验证发行排除、开发精确撤销和其他数组项保留；不运行构建或Cook。
.DESCRIPTION
EngineRoot或UE_ROOT指定UE5.8；PowerShell的.NET运行时须能加载该引擎UBT程序集。
还从两个实际Cook脚本提取参数表达式，以真实UBT解析验证开发Stage白名单与专服Pak规则保留。
测试仅求值参数及配置，不调用Cook脚本主流程、不运行构建、编辑器或发布游戏。
返回0检查通过，1行为失败，2引擎/程序集环境不可用；实际Stage和运行时读取仍须独立验证。
#>
param([string]$EngineRoot = $env:UE_ROOT)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/FoundationTools.psm1') -Force
$context = New-FoundationContext PackagingConfig ([guid]::NewGuid())
try {
    $engine = Get-FoundationEngine $EngineRoot
    $toolRoot = Join-Path $engine.Root 'Engine/Binaries/DotNET/UnrealBuildTool'
    Add-Type -Path (Join-Path $toolRoot 'EpicGames.Core.dll')
    Add-Type -Path (Join-Path $toolRoot 'EpicGames.Build.dll')
    Add-Type -Path (Join-Path $toolRoot 'UnrealBuildTool.dll')
    # 该公开入口专供引擎目录外的托管宿主；仅影响当前测试进程的目录解析。
    [UnrealBuildBase.Unreal+LocationOverride]::RootDirectory = [EpicGames.Core.DirectoryReference]::new($engine.Root)
} catch { Write-FoundationResult $context 2 $_.Exception.Message; exit 2 }

function Get-DevelopmentStageArgumentProbe([string]$ScriptPath, [string]$Target, [string]$CustomConfig) {
    # 解析真实脚本而非复制一份期望参数；只求值构造数组和开发白名单if，不调用外部进程或获取引擎锁。
    # 删除白名单、取消条件或添加顶层CustomConfig将改变实际结果，回归应因此失败。
    $parseTokens = $null; $parseErrors = $null
    $tree = [Management.Automation.Language.Parser]::ParseFile($ScriptPath,[ref]$parseTokens,[ref]$parseErrors)
    if ($parseErrors.Count -gt 0) { throw "Cook脚本语法错误：$ScriptPath" }
    $assignment = $tree.Find({
        param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and
        $node.Left -is [Management.Automation.Language.VariableExpressionAst] -and
        $node.Left.VariablePath.UserPath -eq 'arguments'
    }, $true)
    $developmentIf = $tree.Find({
        param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and
        $node.Extent.Text.Contains('AllowedConfigFiles=')
    }, $true)
    if (!$assignment -or !$developmentIf) { throw '未找到真实UAT参数数组或条件开发Stage白名单。' }
    # 夹具仅提供参数表达式所需的普通字符串；真实配置文件仍由既有Assert-FoundationFile做只读核对。
    $workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $context = @{ Workspace=$workspace; Project=(Join-Path $workspace 'Game/DivineBeastsArena.uproject') }
    $tool = @{ Dll='AutomationTool.dll' }; $details = @{}
    $maps = @('/DBAWorldPack_Village/Maps/L_Village_Start')
    $cookDirectory = 'Probe/Cooked'; $stageDirectory = 'Probe/Stage'
    $arguments = @(& ([scriptblock]::Create($assignment.Right.Extent.Text)))
    # 与源脚本相同作用域求值+=；创建子作用域会从空局部变量开始，不能据此误判源脚本参数。
    . ([scriptblock]::Create($developmentIf.Extent.Text))
    $arguments
}
try {
    $baselinePath = Join-Path $context.Workspace 'Game/Config/DefaultGame.ini'
    $customPath = Join-Path $context.Workspace 'Game/Config/Custom/FoundationStandalone/DefaultGame.ini'
    Assert-FoundationFile $baselinePath; Assert-FoundationFile $customPath
    $baseline = [UnrealBuildTool.ConfigFile]::new([EpicGames.Core.FileReference]::new($baselinePath),[UnrealBuildTool.ConfigLineAction]::Set)
    $custom = [UnrealBuildTool.ConfigFile]::new([EpicGames.Core.FileReference]::new($customPath),[UnrealBuildTool.ConfigLineAction]::Set)
    # 仅Saved夹具加入一个不相关排除项；真实工程配置保持不变。
    $sentinelPath = Join-Path $context.Directory 'OtherExclusion.ini'
    [IO.File]::WriteAllText($sentinelPath,"[/Script/UnrealEd.ProjectPackagingSettings]`n+DirectoriesToNeverCook=(Path=`"/Game/OtherDevelopmentSentinel`")`n")
    $sentinel = [UnrealBuildTool.ConfigFile]::new([EpicGames.Core.FileReference]::new($sentinelPath),[UnrealBuildTool.ConfigLineAction]::Set)
    $sourceFiles = [EpicGames.Core.FileReference[]]@()
    $project = [EpicGames.Core.DirectoryReference]::new((Join-Path $context.Workspace 'Game'))
    $standard = [UnrealBuildTool.ConfigCache]::ReadHierarchy([UnrealBuildTool.ConfigHierarchyType]::Game,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,'',[string[]]@(),$null,$null)
    $development = [UnrealBuildTool.ConfigCache]::ReadHierarchy([UnrealBuildTool.ConfigHierarchyType]::Game,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,'FoundationStandalone',[string[]]@(),$null,$null)
    $sentinelMerge = [UnrealBuildTool.ConfigHierarchy]::new([UnrealBuildTool.ConfigFile[]]@($baseline,$sentinel,$custom),$sourceFiles)
    $section = '/Script/UnrealEd.ProjectPackagingSettings'
    $normalExcluded = [Collections.Generic.List[string]]::new()
    $devExcluded = [Collections.Generic.List[string]]::new()
    $devIncluded = [Collections.Generic.List[string]]::new()
    $sentinelExcluded = [Collections.Generic.List[string]]::new()
    $null = $standard.GetArray($section,'DirectoriesToNeverCook',[ref]$normalExcluded)
    $null = $development.GetArray($section,'DirectoriesToNeverCook',[ref]$devExcluded)
    $null = $development.GetArray($section,'DirectoriesToAlwaysCook',[ref]$devIncluded)
    $null = $sentinelMerge.GetArray($section,'DirectoriesToNeverCook',[ref]$sentinelExcluded)
    $foundation = '(Path="/Game/Development/Foundation")'
    $other = '(Path="/Game/OtherDevelopmentSentinel")'
    if ($foundation -notin $normalExcluded) { throw '发行默认没有排除Foundation。' }
    if ($foundation -in $devExcluded -or $foundation -notin $devIncluded) { throw '开发配置没有精确解除排除并加入开发内容。' }
    if ($other -notin $sentinelExcluded) { throw '不相关排除项被清空。' }
    $developmentStageCases = @()
    foreach ($case in @(
        @{ Entry='CookFrontEndClient.ps1'; Target='Client'; Config='FrontEndClient'; Development=$false },
        @{ Entry='CookFrontEndClient.ps1'; Target='Client'; Config='VillageDevelopmentClient'; Development=$true },
        @{ Entry='CookFoundation.ps1'; Target='Server'; Config='FoundationStandalone'; Development=$false },
        @{ Entry='CookFoundation.ps1'; Target='Server'; Config='VillageServer'; Development=$false },
        @{ Entry='CookFoundation.ps1'; Target='Server'; Config='VillageDevelopmentServer'; Development=$true }
    )) {
        $entryPath = Join-Path $context.Workspace ("Build/Game/" + $case.Entry)
        $probeArguments = @(Get-DevelopmentStageArgumentProbe $entryPath $case.Target $case.Config)
        if (@($probeArguments | Where-Object { $_ -like '-CustomConfig=*' }).Count -ne 0) {
            throw "UAT顶层CustomConfig会改变专服Stage配置层：$($case.Entry)"
        }
        if (@($probeArguments | Where-Object { $_ -like "-AdditionalCookerOptions=-CustomConfig=$($case.Config)*" }).Count -ne 1) {
            throw "Cook没有保留所选开发/角色配置：$($case.Entry)/$($case.Config)；实际参数=$($probeArguments -join ' | ')"
        }
        $iniArguments = [string[]]@($probeArguments | Where-Object { $_ -like '-ini:Game:[[]Staging[]]:+AllowedConfigFiles=*' })
        $expectedStageFiles = @()
        if ($case.Development) {
            $expectedStageFiles = @(
                "DivineBeastsArena/Config/Custom/$($case.Config)/DefaultGame.ini",
                "DivineBeastsArena/Config/Custom/$($case.Config)/DefaultGameplayTags.ini"
            )
        }
        if ($iniArguments.Count -ne $expectedStageFiles.Count) { throw "开发白名单数量错误：$($case.Entry)/$($case.Config)" }
        $receiptCustomConfig = if ($case.Target -eq 'Server') { 'DedicatedServer' } else { '' }
        $stageHierarchy = [UnrealBuildTool.ConfigCache]::ReadHierarchy(
            [UnrealBuildTool.ConfigHierarchyType]::Game,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,
            $receiptCustomConfig,$iniArguments,$null,$null)
        $allowedFiles = [Collections.Generic.List[string]]::new()
        $null = $stageHierarchy.GetArray('Staging','AllowedConfigFiles',[ref]$allowedFiles)
        foreach ($expectedFile in $expectedStageFiles) {
            if ($expectedFile -notin $allowedFiles) { throw "真实UBT解析未保留白名单：$expectedFile" }
        }
        $stagedDevelopmentFiles = @($allowedFiles | Where-Object { $_ -like 'DivineBeastsArena/Config/Custom/VillageDevelopment*/*' })
        if ($stagedDevelopmentFiles.Count -ne $expectedStageFiles.Count) { throw '默认路径被注入开发配置或额外自定义文件。' }
        $developmentEnabled = $false
        $null = $stageHierarchy.GetBool('DivineBeasts.Abilities','bAllowDevelopmentAbilitySets',[ref]$developmentEnabled)
        if ($developmentEnabled) { throw 'Stage白名单改变了正式默认开发技能关闭状态。' }
        if ($case.Target -eq 'Server') {
            $pakHierarchy = [UnrealBuildTool.ConfigCache]::ReadHierarchy(
                [UnrealBuildTool.ConfigHierarchyType]::PakFileRules,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,
                'DedicatedServer',$iniArguments,$null,$null)
            $serverStripping = $false
            if (!$pakHierarchy.GetBool('ExcludeProjectClientPresentationFromDedicatedServer','bExcludeFromPaks',[ref]$serverStripping) -or !$serverStripping) {
                throw '开发白名单导致DedicatedServer表现剥离规则丢失。'
            }
        }
        $developmentStageCases += @{ Entry=$case.Entry; Target=$case.Target; CustomConfig=$case.Config; AllowedDevelopmentFiles=$stagedDevelopmentFiles; DefaultDevelopmentEnabled=$developmentEnabled }
    }
    Write-FoundationResult $context 0 '3项既有数组合并及5组真实脚本参数/UBT开发Stage白名单回归通过；未执行Cook、Stage或游戏启动。' @{
        StandardExcluded=@($normalExcluded); DevelopmentExcluded=@($devExcluded); DevelopmentIncluded=@($devIncluded)
        SentinelExcluded=@($sentinelExcluded); DevelopmentStageCases=$developmentStageCases
    }
    exit 0
} catch { Write-FoundationResult $context 1 $_.Exception.Message; exit 1 }
