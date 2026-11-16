#requires -Version 7.0
<# 独立页启动参数的真实纯函数回归；不创建伪UE资产、不启动/停止任何进程。 #>
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$launcher=Join-Path $root 'Build/Game/RunPluginDemo.ps1'
$tokens=$null; $parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($launcher,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw '演示启动器语法错误'}
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Resolve-PluginDemoIndependentPage'},$true)
if(!$function){throw '缺少受控独立插件页参数函数'}
Invoke-Expression $function.Extent.Text
$manifest=Get-Content -LiteralPath (Join-Path $root 'Docs/Implementation/PluginDemos/PluginDemoManifest.json') -Raw | ConvertFrom-Json
$passed=0
$hub=Resolve-PluginDemoIndependentPage -Workspace $root -Manifest $manifest
if($hub.PluginId -or $hub.Arguments.Count -or $hub.AssetFile){throw '无选择时应沿用中心入口'}; $passed++
foreach($plugin in $manifest.plugins){
    $page=Resolve-PluginDemoIndependentPage -Workspace $root -Manifest $manifest -PluginId $plugin.plugin_id
    if($page.PluginId -cne $plugin.plugin_id -or $page.Arguments.Count -ne 1 -or $page.Arguments[0] -cne "-DBAPluginTest=$($plugin.plugin_id)") {throw '独立页参数未按实际插件身份构造'}
    if(!$page.AssetFile.StartsWith((Join-Path $root 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/Development/PluginDemos/UI/Plugins/'),[StringComparison]::OrdinalIgnoreCase)){throw '页面文件地址越过正式UI目录'}
    $passed++
}
function Reject-Page($Identity,$Document=$manifest){
    $rejected=$false
    try{$null=Resolve-PluginDemoIndependentPage -Workspace $root -Manifest $Document -PluginId $Identity}catch{$rejected=$true}
    if(!$rejected){throw '非法独立页配置被接受'}
}
foreach($invalid in @('../Other','GamePlatformCore -ExecCmds=quit','NotAnInstalledPlugin',('A'*1024))){Reject-Page $invalid; $passed++}
$bad=Get-Content -LiteralPath (Join-Path $root 'Docs/Implementation/PluginDemos/PluginDemoManifest.json') -Raw | ConvertFrom-Json
$bad.plugins[0].test_page_asset=$bad.plugins[1].test_page_asset
Reject-Page $bad.plugins[0].plugin_id $bad; $passed++
$bad.manual_page_contract_version=2
Reject-Page $bad.plugins[1].plugin_id $bad; $passed++
$fixtureFunction=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Resolve-PluginDemoManualFixture'},$true)
if(!$fixtureFunction){throw '缺少工程内隔离测试宿主目录函数'}
Invoke-Expression $fixtureFunction.Extent.Text
$fixtureId=[guid]::NewGuid()
$fixture=Resolve-PluginDemoManualFixture -Workspace $root -RunId $fixtureId
$expected=Join-Path $root ('Saved/Validation/PluginManualUI20261027/Hosts/'+$fixtureId.ToString('D'))
if($fixture.UserDir -cne $expected -or $fixture.NativeSavedRoot -cne (Join-Path $expected 'Saved')){throw '测试宿主目录必须绑定本工程与RunId'}; $passed++
if($fixture.Arguments.Count -ne 2 -or $fixture.Arguments[0] -cne '-GamePlatformManualFixture' -or $fixture.Arguments[1] -cne ('-UserDir='+$expected)){throw '隔离宿主参数错误'}; $passed++
if(Test-Path -LiteralPath $expected){throw '纯路径解析不能创建运行目录'}; $passed++
Write-Output "PluginDemoIndependentLaunchCases=$passed Failed=0 UEExecuted=False NativeAssetClaim=False"
