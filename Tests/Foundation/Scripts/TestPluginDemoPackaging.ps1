#requires -Version 7.0
<# SSLM开发演示发行策略静态回归；只读取源码/配置，不启动UE、UAT、Cook或构建。
   检查默认禁入、显式Client分支、真实目录扫描和启动二进制门禁；不证明实际产物已剥离。 #>
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$cook=Get-Content -LiteralPath (Join-Path $root 'Build/Game/CookFoundation.ps1') -Raw
$launch=Get-Content -LiteralPath (Join-Path $root 'Build/Game/RunPluginDemo.ps1') -Raw
$default=Get-Content -LiteralPath (Join-Path $root 'Game/Config/DefaultGame.ini') -Raw
$demo=Get-Content -LiteralPath (Join-Path $root 'Game/Config/Custom/PluginDemoClient/DefaultGame.ini') -Raw
$cases=0
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message }; $script:cases++ }
foreach ($path in @('/Game/Development/PluginDemos','/DBAUIPack_Core/Development/PluginDemos')) {
    Require ($default.Contains('+DirectoriesToNeverCook=(Path="'+$path+'")')) "默认发行缺开发目录禁入：$path"
    Require ($demo.Contains('-DirectoriesToNeverCook=(Path="'+$path+'")')) "开发Client未显式撤销：$path"
}
Require ($cook.Contains("`$CustomConfig -eq 'PluginDemoClient' -and `$Target -ne 'Client'")) '缺演示配置Server拒绝门禁。'
Require ($cook.Contains('--print-cook-maps') -and $cook.Contains('-cnotin $allowed')) '地图集合未由有效作者源限制。'
Require ($launch.Contains('DBAUIPack_Core/Content/Development/PluginDemos')) '启动仍使用旧Hub路径。'
Require ($launch.Contains('Assert-FoundationEditorBuildResult') -and $launch.Contains('-NoCompileEditor')) '启动缺当前二进制门禁/禁止自动编译。'
Require (-not ($launch -match 'BuildFoundation|live_compile|trigger_build')) '启动存在自动构建旁路。'
Require ($default.Contains('/DBAUIPack_Core/Development/PluginDemos/Definitions')) '开发目录未进入唯一Data扫描。'
foreach ($file in @('CookFoundation.ps1','RunPluginDemo.ps1','TestPluginDemo.ps1','PluginDemoValidation.psm1')) {
    $tokens=$null; $errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root "Build/Game/$file"),[ref]$tokens,[ref]$errors)
    Require ($errors.Count -eq 0) "脚本语法错误：$file"
}
Write-Output "PluginDemoPackagingStaticCases=$cases Failed=0 CookExecuted=False UEExecuted=False"
