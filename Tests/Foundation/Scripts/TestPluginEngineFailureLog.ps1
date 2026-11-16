#requires -Version 7.0
<# 真实故障日志片段回归：Automation单项Success不能掩盖引擎启动Ensure；不运行UE。 #>
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$checks=0
$clean=@('[2026.10.28-11.04.50:572][599]LogAutomationController: Display: Test Started. Name={Contract}',
    '[2026.10.28-11.04.50:615][604]LogAutomationController: Display: Test Completed. Result={Success}')
if((Assert-PluginDemoNoEngineFailure -Lines $clean) -ne 2){throw '干净日志未通过。'};$checks++
$failures=@(
    '[2026.10.28-11.04.42:482][  0]LogOutputDevice: Error: Ensure condition failed: false [File:NativeGameplayTags.cpp] [Line: 41]',
    '[2026.10.28-11.04.42:482][  0]LogOutputDevice: Error: Assertion failed: Pointer [File:Example.cpp]',
    '[2026.10.28-11.04.42:482][  0]LogWindows: Error: Fatal error: Unhandled exception',
    '[2026.10.28-11.04.42:482][  0]LogWindows: Error: appError called: LowLevelFatalError [File:Example.cpp]')
foreach($failure in $failures){
    $rejected=$false;try{$null=Assert-PluginDemoNoEngineFailure -Lines (@($failure)+$clean)}catch{$rejected=$true}
    if(!$rejected){throw '测试Success掩盖引擎启动故障。'};$checks++
}
$rejected=$false;try{$null=Assert-PluginDemoNoEngineFailure -Lines @()}catch{$rejected=$true}
if(!$rejected){throw '空日志被接受。'};$checks++
Write-Output "PluginEngineFailureLogCases=$checks Failed=0 UEExecuted=False"
