#requires -Version 7.0
<# 只读取启动入口中的纯参数解析器；无UE/构建/文件写入。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$path=Join-Path $PSScriptRoot '../../../Build/Game/RunPluginDemo.ps1'
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw '启动脚本语法错误。'}
$functions=@($ast.FindAll({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Resolve-PluginDemoExecutionHost'},$true))
if($functions.Count -ne 1){throw '缺少唯一宿主解析器。'}
. ([scriptblock]::Create($functions[0].Extent.Text))
$hub='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub';$checks=0
$editor=Resolve-PluginDemoExecutionHost -Target EditorManual -ManualFixture
if($editor.StartupURL -cne $hub -or $editor.UseGameFlag -or !$editor.ManualEditorFixture -or $editor.ManualWorldHost -or
    @($editor.Arguments).Count -ne 1 -or $editor.Arguments[0] -cne '-GamePlatformManualEditorFixture'){throw 'Editor手动宿主解析错误。'};$checks++
foreach($options in @(@{Target='EditorManual'},@{Target='EditorManual';ManualFixture=$true;ManualWorldHost=$true},@{Target='EditorManual';ManualFixture=$true;ManualGameplayHost=$true},
    @{Target='EditorGame';ManualWorldHost=$true},@{Target='EditorGame';ManualGameplayHost=$true})){
    $rejected=$false;try{$null=Resolve-PluginDemoExecutionHost @options}catch{$rejected=$true};if(!$rejected){throw '不完整或混用宿主未拒绝。'};$checks++
}
$game=Resolve-PluginDemoExecutionHost -Target EditorGame
if(!$game.UseGameFlag -or $game.ManualEditorFixture -or @($game.Arguments).Count -ne 0 -or $game.StartupURL -cne $hub){throw '默认Standalone行为发生改变。'};$checks++
$gameplay=Resolve-PluginDemoExecutionHost -Target EditorGame -ManualFixture -ManualGameplayHost
if(!$gameplay.UseGameFlag -or !$gameplay.ManualWorldHost -or @($gameplay.Arguments).Count -ne 4 -or $gameplay.StartupURL -cne (Get-PluginDemoMapURL GameplayHost)){throw '现有Gameplay宿主受影响。'};$checks++
Write-Output "PluginEditorManualLaunchCases=$checks Failed=0 UEExecuted=False"
