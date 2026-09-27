#requires -Version 7.0
<#
.SYNOPSIS
使用UE5.8 UBT验证Editor与EditorPerProjectUserSettings配置归属和合并后的默认值。
.DESCRIPTION
EngineRoot或UE_ROOT指定实际引擎；只读项目默认配置，不覆盖用户Saved偏好。
检查配置层级选中了项目文件，不以任意ConfigFile单文件解析冒充真实加载层。
返回0通过、1行为失败、2前置不可用；不运行UObject加载、编辑器或Cook。
#>
param([string]$EngineRoot = $env:UE_ROOT)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/FoundationTools.psm1') -Force
$context = New-FoundationContext EditorConfig ([guid]::NewGuid())
try {
    $engine = Get-FoundationEngine $EngineRoot
    $toolRoot = Join-Path $engine.Root 'Engine/Binaries/DotNET/UnrealBuildTool'
    foreach ($assembly in @('EpicGames.Core.dll','EpicGames.Build.dll','UnrealBuildTool.dll')) {
        Add-Type -Path (Join-Path $toolRoot $assembly)
    }
    [UnrealBuildBase.Unreal+LocationOverride]::RootDirectory = [EpicGames.Core.DirectoryReference]::new($engine.Root)
} catch { Write-FoundationResult $context 2 $_.Exception.Message; exit 2 }
try {
    $project = [EpicGames.Core.DirectoryReference]::new((Join-Path $context.Workspace 'Game'))
    $results = @()
    foreach ($kind in @('Editor','EditorPerProjectUserSettings')) {
        $type = [UnrealBuildTool.ConfigHierarchyType]$kind
        $expectedFile = Join-Path $project.FullName "Config/Default$kind.ini"
        Assert-FoundationFile $expectedFile
        $locations = @([UnrealBuildTool.ConfigHierarchy]::EnumerateConfigFileLocations(
            $type,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,'',$null) | ForEach-Object { $_.FullName })
        if ([IO.Path]::GetFullPath($expectedFile) -notin $locations) { throw "$kind 配置层未枚举项目默认文件" }
        $hierarchy = [UnrealBuildTool.ConfigCache]::ReadHierarchy(
            $type,$project,[UnrealBuildTool.UnrealTargetPlatform]::Win64,'',[string[]]@(),$null,$null)
        if ($kind -eq 'Editor') {
            $section = '/Script/UnrealEd.BlueprintEditorProjectSettings'
            $expected = [ordered]@{ bValidateUnloadedSoftActorReferences='True' }
        } else {
            $section = '/Script/UnrealEd.EditorLoadingSavingSettings'
            $expected = [ordered]@{ bAutoSaveEnable='True'; bAutoSaveMaps='True'; bAutoSaveContent='True'; AutoSaveTimeMinutes='10' }
        }
        foreach ($key in $expected.Keys) {
            $value = ''
            if (-not $hierarchy.GetString($section,$key,[ref]$value) -or $value -ine $expected[$key]) {
                throw "$kind/$key 默认值不符：$value；请核查层级覆盖，不自动改写用户设置。"
            }
            $results += @{ Hierarchy=$kind; ProjectFile=$expectedFile; Key=$key; Value=$value }
        }
    }
    Write-FoundationResult $context 0 '两个实际UBT配置层、5项设置读取通过；不代表编辑器运行时加载或自动备份已经触发。' @{ Checks=$results }
    exit 0
} catch { Write-FoundationResult $context 1 $_.Exception.Message; exit 1 }
