#requires -Version 7.0
<#
SSLM 正式UE5.8 Editor 模块状态只读检查。默认仅输出诊断JSON。
-WriteReport 明确启用时才把报告保存至 Saved/Validation；
本脚本不启动UE、不构建、不修改DLL/BuildId、资产、插件或源码。
#>
[CmdletBinding()]
param(
    [string]$EngineRoot = 'F:\UnrealEngine-5.8.0-release',
    [switch]$WriteReport
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ([IO.Path]::GetFileName($root.TrimEnd('\', '/')) -ne 'DivineBeastsWorkspace') {
    throw '只允许检查正式DivineBeastsWorkspace工作空间'
}
$project = Join-Path $root 'Game/DivineBeastsArena.uproject'
$target = Join-Path $root 'Game/Source/DivineBeastsArenaEditor.Target.cs'
if (-not (Test-Path -LiteralPath $project -PathType Leaf) -or
    -not (Test-Path -LiteralPath $target -PathType Leaf)) {
    throw '正式工程或编辑器Target不存在'
}
function Inspect-Manifest([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ Path=$Path; Exists=$false; BuildId=$null; Modules=@{}; LastWriteUtc=$null }
    }
    $source = Get-Item -LiteralPath $Path
    $json = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    $mapping = @{}
    $property = $json.PSObject.Properties['Modules']
    if ($null -ne $property -and $null -ne $property.Value) {
        foreach ($entry in $property.Value.PSObject.Properties) {
            $mapping[$entry.Name] = [string]$entry.Value
        }
    }
    $buildId = $null
    if ($null -ne $json.PSObject.Properties['BuildId']) {
        $buildId = [string]$json.BuildId
    }
    return [ordered]@{
        Path=$Path; Exists=$true; BuildId=$buildId
        Modules=$mapping; LastWriteUtc=$source.LastWriteTimeUtc.ToString('o')
    }
}
$engineBinary = Join-Path $EngineRoot 'Engine/Binaries/Win64'
$core = Inspect-Manifest (Join-Path $engineBinary 'UnrealEditor.modules')
$other = @(
    (Inspect-Manifest (Join-Path $engineBinary 'NoRedist/UnrealEditor.modules')),
    (Inspect-Manifest (Join-Path $engineBinary 'NotForLicensees/UnrealEditor.modules'))
)
$mismatches = @(
    foreach ($item in $other) {
        if ($core.Exists -and $item.Exists -and $core.BuildId -ne $item.BuildId) {
            [ordered]@{ Manifest=$item.Path; ActualBuildId=$item.BuildId; CoreBuildId=$core.BuildId }
        }
    }
)
$projectManifest = Inspect-Manifest (Join-Path $root 'Game/Binaries/Win64/UnrealEditor.modules')
$interesting = @(
    'GamePlatformSurface','GamePlatformVFX','GamePlatformUI','DBAClient',
    'DBAArena','DBAUIPack_Core','DBAPresentationPack_Core','GamePlatformDeveloperTools'
)
$plugins = @()
$pluginRoot = Join-Path $root 'Game/Plugins'
if (Test-Path -LiteralPath $pluginRoot) {
    $descriptors = @(
        Get-ChildItem -LiteralPath $pluginRoot -Recurse -File -Filter '*.uplugin' |
            Where-Object { $_.BaseName -in $interesting }
    )
    foreach ($descriptor in $descriptors) {
        $definition = Get-Content -LiteralPath $descriptor.FullName -Raw -Encoding utf8 | ConvertFrom-Json
        $pluginManifest = Inspect-Manifest (Join-Path $descriptor.DirectoryName 'Binaries/Win64/UnrealEditor.modules')
        $modules = @()
        if ($null -ne $definition.PSObject.Properties['Modules']) {
            foreach ($item in @($definition.Modules)) {
                if ($null -eq $item) { continue }
                $name = [string]$item.Name
                $dll = $null
                if ($pluginManifest.Modules.ContainsKey($name)) {
                    $dll = $pluginManifest.Modules[$name]
                }
                $modules += [ordered]@{
                    Name=$name; Type=[string]$item.Type; Registered=($null -ne $dll)
                    Dll=$dll
                    DllExists=($null -ne $dll -and (Test-Path -LiteralPath (
                        Join-Path $descriptor.DirectoryName ('Binaries/Win64/'+$dll)) -PathType Leaf))
                }
            }
        }
        $plugins += [ordered]@{
            Name=$descriptor.BaseName; Descriptor=$descriptor.FullName
            ManifestExists=$pluginManifest.Exists
            ManifestBuildId=$pluginManifest.BuildId
            BuildIdDiffersFromCore=($core.Exists -and $pluginManifest.Exists -and
                $core.BuildId -ne $pluginManifest.BuildId)
            DeclaredModules=$modules
        }
    }
}
$processes = @(
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^(UnrealEditor|UnrealBuildTool|MSBuild|cl|link|c1xx|UbaHost|UbaAgent|UbaSessionServer)(\.exe)?$' } |
        Select-Object ProcessId,ParentProcessId,Name
)
$latestLog = Join-Path $root 'Game/Saved/Logs/DivineBeastsArena.log'
$logEvidence = @()
if (Test-Path -LiteralPath $latestLog -PathType Leaf) {
    $logEvidence = @(
        Select-String -LiteralPath $latestLog -Pattern @(
            'FailedDueToEngineChange','Please build from an IDE','BuildId',
            'missing or built with a different engine version'
        ) | Select-Object -Last 25 | ForEach-Object {
            [ordered]@{ Line=$_.LineNumber; Message=([string]$_.Line).Trim() }
        }
    )
}
$report = [ordered]@{
    SchemaVersion=1
    ObservedUtc=[DateTime]::UtcNow.ToString('o')
    EvidenceType='read_only_diagnostics_not_build_or_editor_success'
    Project=$project
    EngineRoot=$EngineRoot
    UnrealEditorExeExists=(Test-Path -LiteralPath (Join-Path $engineBinary 'UnrealEditor.exe') -PathType Leaf)
    CoreEditorManifest=$core
    ExtraEngineManifests=$other
    BuildIdMismatches=$mismatches
    ProjectEditorManifest=$projectManifest
    ProjectBuildIdMismatch=($core.Exists -and $projectManifest.Exists -and
        $core.BuildId -ne $projectManifest.BuildId)
    PluginModuleCandidates=$plugins
    UnrealProcesses=$processes
    ProjectStartupLog=$latestLog
    LogEvidence=$logEvidence
    Caveat='插件Module必须与真实Editor启用闭包交叉核对；缺失候选DLL不一定阻止本次Editor装配。'
}
$json = $report | ConvertTo-Json -Depth 16
if ($WriteReport) {
    $dir = Join-Path $root 'Saved/Validation/MaterialLibrary/EditorStartupRecovery'
    $null = New-Item -ItemType Directory -Path $dir -Force
    $name = 'Inspection_'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'_'+[guid]::NewGuid().ToString('N')+'.json'
    $path = Join-Path $dir $name
    $json | Set-Content -LiteralPath $path -Encoding utf8
    Write-Host ('SSLM_EDITOR_DIAG_REPORT='+$path)
}
Write-Output $json
