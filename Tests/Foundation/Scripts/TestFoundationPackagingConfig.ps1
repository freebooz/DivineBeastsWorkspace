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
param([string]$EngineRoot = $env:UE_ROOT,
    # 只检查真实命令装配时不加载UBT；输出明确不包含引擎配置解析证据。
    [switch]$ArgumentsOnly)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/FoundationTools.psm1') -Force
$context = New-FoundationContext PackagingConfig ([guid]::NewGuid())
try {
    $engine = Get-FoundationEngine $EngineRoot
    $toolRoot = Join-Path $engine.Root 'Engine/Binaries/DotNET/UnrealBuildTool'
    if (-not $ArgumentsOnly) {
    Add-Type -Path (Join-Path $toolRoot 'EpicGames.Core.dll')
    Add-Type -Path (Join-Path $toolRoot 'EpicGames.Build.dll')
    Add-Type -Path (Join-Path $toolRoot 'UnrealBuildTool.dll')
    # 该公开入口专供引擎目录外的托管宿主；仅影响当前测试进程的目录解析。
    [UnrealBuildBase.Unreal+LocationOverride]::RootDirectory = [EpicGames.Core.DirectoryReference]::new($engine.Root)
    }
} catch { Write-FoundationResult $context 2 $_.Exception.Message; exit 2 }

function Get-DevelopmentStageArgumentProbe([string]$ScriptPath, [string]$Target, [string]$CustomConfig) {
    # 只求值真实脚本的参数表达式；不运行Cook主流程、不加锁、不启动UE。
    # 真源预检由锁定引擎Python只读执行；移除服务器追加或注入客户端都会改变返回参数。
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
    $workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
    $context = @{ Workspace=$workspace; Project=(Join-Path $workspace 'Game/DivineBeastsArena.uproject') }
    $tool = @{ Dll='AutomationTool.dll' }; $details = @{}
    $maps = @('/DBAWorldPack_Village/Maps/L_Village_Start')
    $cookDirectory = 'Probe/Cooked'; $stageDirectory = 'Probe/Stage'
    $serverPresentationPolicy = $null
    if ((Split-Path $ScriptPath -Leaf) -eq 'CookFoundation.ps1') {
        if ($Target -eq 'Server') {
            $python = Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
            $policyTool = Join-Path $workspace 'Build/Game/ServerPresentationPolicy.py'
            $policyOutput = & $python $policyTool check --custom-config $CustomConfig
            if ($LASTEXITCODE -ne 0) { throw '真实服务器策略check失败。' }
            $serverPresentationPolicy = ($policyOutput -join "\n") | ConvertFrom-Json
        }
        $optionAssignment = $tree.Find({
            param($node)
            $node -is [Management.Automation.Language.AssignmentStatementAst] -and
            $node.Left -is [Management.Automation.Language.VariableExpressionAst] -and
            $node.Left.VariablePath.UserPath -eq 'additionalCookerOptions'
        }, $true)
        $optionIf = $tree.Find({
            param($node)
            $node -is [Management.Automation.Language.IfStatementAst] -and
            $node.Extent.Text.Contains('$serverPresentationPolicy.CookerArguments')
        }, $true)
        if (!$optionAssignment -or !$optionIf) { throw '真实CookFoundation缺少服务器策略参数装配。' }
        $additionalCookerOptions = & ([scriptblock]::Create($optionAssignment.Right.Extent.Text))
        . ([scriptblock]::Create($optionIf.Extent.Text))
    }
    $arguments = @(& ([scriptblock]::Create($assignment.Right.Extent.Text)))
    . ([scriptblock]::Create($developmentIf.Extent.Text))
    [pscustomobject]@{ Arguments=$arguments; ServerPolicy=$serverPresentationPolicy }
}

function Assert-ServerCookPolicyArguments($Probe, [string]$Target, [string]$CustomConfig) {
    # 同一真实AdditionalCookerOptions拆为独立token，不能把数组合并为一次Set或逗号串。
    $prefix = '-AdditionalCookerOptions='
    $options = @($Probe.Arguments | Where-Object { $_.StartsWith($prefix) })
    if ($options.Count -ne 1) { throw 'AdditionalCookerOptions必须唯一。' }
    $tokens = @($options[0].Substring($prefix.Length).Split(' ',[StringSplitOptions]::RemoveEmptyEntries))
    $actual = @($tokens | Where-Object { $_.StartsWith('-ini:Game:') })
    $expected = if ($Target -eq 'Server') { @($Probe.ServerPolicy.CookerArguments) } else { @() }
    if ($actual.Count -ne $expected.Count -or
        ($actual.Count -gt 0 -and @(Compare-Object $actual $expected -CaseSensitive).Count -gt 0)) {
        throw "真实Cooker数组追加丢项或泄入客户端：$Target/$CustomConfig"
    }
    if (@($actual | Select-Object -Unique).Count -ne $actual.Count) { throw 'Cooker出现重复追加token。' }
    foreach ($token in $actual) {
        if ($token -cnotmatch '^-ini:Game:\[/Script/UnrealEd.ProjectPackagingSettings\]:\+DirectoriesToNeverCook=\(Path=/[A-Za-z0-9_/]+\)$') {
            throw "非法数组追加token：$token"
        }
    }
    [pscustomobject]@{ Target=$Target; CustomConfig=$CustomConfig; AddedDirectoryCount=$actual.Count; CookerIniArguments=$actual }
}
$argumentCases = @(
    @{ Entry='CookFrontEndClient.ps1'; Target='Client'; Config='FrontEndClient'; Development=$false },
    @{ Entry='CookFrontEndClient.ps1'; Target='Client'; Config='VillageDevelopmentClient'; Development=$true },
    @{ Entry='CookFoundation.ps1'; Target='Client'; Config='FoundationStandalone'; Development=$false },
    @{ Entry='CookFoundation.ps1'; Target='Server'; Config='FoundationStandalone'; Development=$false },
    @{ Entry='CookFoundation.ps1'; Target='Server'; Config='VillageServer'; Development=$false },
    @{ Entry='CookFoundation.ps1'; Target='Server'; Config='VillageDevelopmentServer'; Development=$true }
)
if ($ArgumentsOnly) {
    try {
        $results = foreach ($case in $argumentCases) {
            $probe = Get-DevelopmentStageArgumentProbe (Join-Path $context.Workspace ("Build/Game/" + $case.Entry)) $case.Target $case.Config
            Assert-ServerCookPolicyArguments $probe $case.Target $case.Config
        }
        Write-FoundationResult $context 0 '6组真实脚本参数装配通过；仅ArgumentsOnly，未执行UBT配置解析、Cook或Stage。' @{ ArgumentCases=@($results) }
        exit 0
    } catch { Write-FoundationResult $context 1 $_.Exception.Message; exit 1 }
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
    $serverCookCases = @()
    foreach ($case in $argumentCases) {
        $entryPath = Join-Path $context.Workspace ("Build/Game/" + $case.Entry)
        $probe = Get-DevelopmentStageArgumentProbe $entryPath $case.Target $case.Config
        $probeArguments = @($probe.Arguments)
        $cookArgumentCheck = Assert-ServerCookPolicyArguments $probe $case.Target $case.Config
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
            # 使用真实UBT合并全部独立+Key；只检查参数文本不能证明同名数组未覆盖。
            $cookHierarchy = [UnrealBuildTool.ConfigCache]::ReadHierarchy(
                [UnrealBuildTool.ConfigHierarchyType]::Game,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,
                $case.Config,[string[]]$cookArgumentCheck.CookerIniArguments,$null,$null)
            $cookedExclusions = [Collections.Generic.List[string]]::new()
            $cookedIncluded = [Collections.Generic.List[string]]::new()
            $null = $cookHierarchy.GetArray($section,'DirectoriesToNeverCook',[ref]$cookedExclusions)
            $null = $cookHierarchy.GetArray($section,'DirectoriesToAlwaysCook',[ref]$cookedIncluded)
            $normalizedExclusions = @($cookedExclusions | ForEach-Object { $_.Replace('"','') })
            $expectedPaths = @(Get-Content -LiteralPath (Join-Path $context.Workspace ("Game/Config/Custom/" + $probe.ServerPolicy.Profile + "/DefaultGame.ini")) |
                Where-Object { $_ -cmatch '^\+DirectoriesToNeverCook=' } |
                ForEach-Object { $_.Substring('+DirectoriesToNeverCook='.Length).Replace('"','') })
            foreach ($path in $expectedPaths) {
                if ($path -cnotin $normalizedExclusions) { throw "UBT未保留服务器排除数组项：$($case.Config)/$path" }
            }
            if ($case.Config -eq 'FoundationStandalone' -and ($foundation -in $cookedExclusions -or $foundation -notin $cookedIncluded)) {
                throw '服务器追加破坏了Foundation开发资源许可。'
            }
            $serverCookCases += @{ CustomConfig=$case.Config; Profile=$probe.ServerPolicy.Profile; ExpectedDirectories=$expectedPaths.Count; ActualExcludedCount=$cookedExclusions.Count }
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
    Write-FoundationResult $context 0 '3项既有数组合并、6组真实脚本参数/Stage白名单及3组服务器排除数组UBT回归通过；未执行Cook、Stage或游戏启动。' @{
        StandardExcluded=@($normalExcluded); DevelopmentExcluded=@($devExcluded); DevelopmentIncluded=@($devIncluded)
        SentinelExcluded=@($sentinelExcluded); DevelopmentStageCases=$developmentStageCases; ServerCookCases=$serverCookCases
    }
    exit 0
} catch { Write-FoundationResult $context 1 $_.Exception.Message; exit 1 }
