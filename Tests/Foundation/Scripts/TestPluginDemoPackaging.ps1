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
$origin=Get-Content -LiteralPath (Join-Path $root 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/Evidence/DivineBeastsDemoRuntimeOrigin.cpp') -Raw
# 仅辅助源代码门禁：UE5.8在!IS_MONOLITHIC下才声明GetModuleFilename；移除隔离必须失败。
# 此断言不能替代Client编译，单体来源资格仍需以后独立实现，不能静默沿用Editor的DLL证明。
Require ($origin -match '(?s)#if\s+!IS_MONOLITHIC.*?Modules\.GetModuleFilename.*?#else.*?Demo\.UnsupportedMonolithicOrigin.*?#endif') '动态模块来源接口缺单体链接隔离/明确拒绝诊断。'
Require ($origin.Contains('FParse::Bool') -and $origin.Contains('-LiveCoding=') -and $origin.Contains('HasStarted()') -and
    $launch.Contains('-LiveCoding=false')) 'Live Coding只有不生效的NoLiveCoding参数，没有真实启动参数/实际会话观察。'
Require ($cook.Contains('StagedDevelopmentConfigFiles') -and $cook.Contains('RequiredDevelopmentRuntimeArguments')) '开发Stage未核对配置及真实启动参数。'
foreach ($file in @('CookFoundation.ps1','RunPluginDemo.ps1','TestPluginDemo.ps1','PluginDemoValidation.psm1','PluginDemoEvidence.psm1','PublishPluginDemoEvidence.ps1')) {
    $tokens=$null; $errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root "Build/Game/$file"),[ref]$tokens,[ref]$errors)
    Require ($errors.Count -eq 0) "脚本语法错误：$file"
}
Write-Output "PluginDemoPackagingStaticCases=$cases Failed=0 CookExecuted=False UEExecuted=False"
