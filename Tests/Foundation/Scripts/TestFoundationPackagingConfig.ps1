#requires -Version 7.0
<#
.SYNOPSIS
使用UE自带UBT配置解析器验证发行排除、开发精确撤销和其他数组项保留；不运行构建或Cook。
.DESCRIPTION
EngineRoot或UE_ROOT指定UE5.8；PowerShell的.NET运行时须能加载该引擎UBT程序集。
返回0三项通过，1行为失败，2引擎/程序集环境不可用。真实引擎运行时加载仍需Cook验证。
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
    Write-FoundationResult $context 0 '3项真实UBT配置层级/数组合并行为通过；未执行Cook。' @{ StandardExcluded=@($normalExcluded); DevelopmentExcluded=@($devExcluded); DevelopmentIncluded=@($devIncluded); SentinelExcluded=@($sentinelExcluded) }
    exit 0
} catch { Write-FoundationResult $context 1 $_.Exception.Message; exit 1 }
