#requires -Version 7.0
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $root 'Build/Game/PluginDemoEvidence.psm1') -Force
$test=Join-Path $root ('Saved/Validation/PluginManualUI20261027/ContentSnapshotSafety/'+[guid]::NewGuid().ToString('N'))
$workspace=Join-Path $test 'workspace'; $engine=Join-Path $test 'engine'
foreach($path in @('Game/Content/plain.txt','Game/Plugins/Fixture/Content/plain.txt','Engine/Content/plain.txt','Engine/Plugins/Fixture/Content/plain.txt')){
    $base=if($path.StartsWith('Game/')){$workspace}else{$engine}
    $file=Join-Path $base $path
    $null=New-Item -ItemType Directory -Force -Path (Split-Path $file)
    [IO.File]::WriteAllText($file,'fixture',[Text.UTF8Encoding]::new($false))
}
foreach($file in @((Join-Path $workspace 'Game/Plugins/Fixture/Fixture.uplugin'),(Join-Path $engine 'Engine/Plugins/Fixture/Fixture.uplugin'))){[IO.File]::WriteAllText($file,'{}')}
$first=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
$again=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
if($first.FileCount -ne 4 -or $again.SHA256 -cne $first.SHA256){throw '四个内容根或未修改内容的哈希不稳定'}
[IO.File]::WriteAllText((Join-Path $engine 'Engine/Plugins/Fixture/Content/plain.txt'),'changed')
$changed=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
if($changed.SHA256 -ceq $first.SHA256){throw '引擎插件内容变化没有改变摘要'}
$link=Join-Path $workspace 'Game/Content/link'
$target=Join-Path $workspace 'Game/Plugins/Fixture/Content'
$null=New-Item -ItemType Junction -Path $link -Target $target
$rejected=$false; try{$null=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine}catch{$rejected=$true}
if(-not $rejected){throw '内容树中的junction必须失败关闭'}
Write-Output 'PluginDemoContentSnapshot: 4 roots, stable hash, changed content and junction rejection passed'
