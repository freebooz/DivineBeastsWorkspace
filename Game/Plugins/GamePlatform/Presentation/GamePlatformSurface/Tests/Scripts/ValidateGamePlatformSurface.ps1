param(
    [Parameter(Mandatory = $false)]
    [string]$WorkspaceRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 只做源码／描述文件静态门禁；不冒充UE编译、Material Graph验证、Cook或服务器产物审计。
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "../../../../../../..")).Path
} else {
    $WorkspaceRoot = (Resolve-Path $WorkspaceRoot).Path
}

$pluginRoot = Join-Path $WorkspaceRoot "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface"
$descriptorPath = Join-Path $pluginRoot "GamePlatformSurface.uplugin"
if (-not (Test-Path -LiteralPath $descriptorPath)) { throw "缺少GamePlatformSurface.uplugin：$descriptorPath" }

$descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($descriptor.EnabledByDefault -ne $false) { throw "GamePlatformSurface必须保持EnabledByDefault=false，由Client/Editor Target显式装配。" }
if ($descriptor.CanContainContent -ne $true) { throw "GamePlatformSurface必须允许承载平台通用材质资产。" }

$moduleByName = @{}
foreach ($module in $descriptor.Modules) { $moduleByName[$module.Name] = $module }
foreach ($required in @("GamePlatformSurfaceClient", "GamePlatformSurfaceEditor")) {
    if (-not $moduleByName.ContainsKey($required)) { throw "缺少模块声明：$required" }
}
if ($moduleByName["GamePlatformSurfaceClient"].Type -ne "ClientOnly") { throw "GamePlatformSurfaceClient必须为ClientOnly。" }
if ($moduleByName["GamePlatformSurfaceEditor"].Type -ne "Editor") { throw "GamePlatformSurfaceEditor必须为Editor。" }

$serverTarget = Get-Content -LiteralPath (Join-Path $WorkspaceRoot "Game/Source/DivineBeastsArenaServer.Target.cs") -Raw -Encoding UTF8
if ($serverTarget -match "GamePlatformSurface") { throw "Dedicated Server Target不得启用GamePlatformSurface。" }
$clientTarget = Get-Content -LiteralPath (Join-Path $WorkspaceRoot "Game/Source/DivineBeastsArenaClient.Target.cs") -Raw -Encoding UTF8
$editorTarget = Get-Content -LiteralPath (Join-Path $WorkspaceRoot "Game/Source/DivineBeastsArenaEditor.Target.cs") -Raw -Encoding UTF8
if ($clientTarget -notmatch 'EnablePlugins\.Add\("GamePlatformSurface"\)') { throw "Client Target尚未显式启用GamePlatformSurface。" }
if ($editorTarget -notmatch 'EnablePlugins\.Add\("GamePlatformSurface"\)') { throw "Editor Target尚未显式启用GamePlatformSurface。" }

$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $pluginRoot "Source") -Recurse -File | Where-Object { $_.Extension -in @(".h", ".cpp", ".cs") }
$sourceText = ($sourceFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"
foreach ($forbidden in @("DivineBeasts", "DBAWorldPack_", "GamePlatformArena", "MobaPresentation")) {
    if ($sourceText -match [regex]::Escape($forbidden)) { throw "平台Surface源码出现禁止的上层项目／MOBA身份：$forbidden" }
}
if ($sourceText -match 'Tick\s*\(' -or $sourceText -match 'FTSTicker') { throw "GamePlatformSurface一期必须保持事件驱动，不允许引入Tick/Ticker。" }

Write-Output "GamePlatformSurface静态架构检查通过：双模块端侧、Target隔离、上层身份污染和Tick门禁均符合当前规范。"
