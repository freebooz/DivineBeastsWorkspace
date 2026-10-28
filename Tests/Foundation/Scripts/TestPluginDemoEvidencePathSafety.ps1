#requires -Version 7.0
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $workspace 'Build/Game/PluginDemoEvidence.psm1') -Force
$root=Join-Path $workspace ('Saved/Validation/PluginManualUI20261027/PathSafety/'+[guid]::NewGuid().ToString('N'))
$target=Join-Path $root 'target'
$null=New-Item -ItemType Directory -Path $target -Force
$file=Join-Path $target 'data.txt'
[IO.File]::WriteAllText($file,'abc',[Text.UTF8Encoding]::new($false))
$expected='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
if((Get-PluginDemoEvidenceFile -Root $root -Path 'target/data.txt').SHA256 -cne $expected){throw '实际文件哈希不正确'}
$missing=Resolve-PluginDemoEvidencePath -Root $root -Path 'target/new.txt'
if(-not $missing.EndsWith('new.txt')){throw '合法未创建目标不能解析'}
foreach($bad in @('../outside.txt','target/../data.txt','target//data.txt')){
    $rejected=$false; try{$null=Resolve-PluginDemoEvidencePath -Root $root -Path $bad}catch{$rejected=$true}
    if(-not $rejected){throw "越界或非规范路径未拒绝: $bad"}
}
$link=Join-Path $root 'link'
$null=New-Item -ItemType Junction -Path $link -Target $target
$rejected=$false; try{$null=Get-PluginDemoEvidenceFile -Root $root -Path 'link/data.txt'}catch{$rejected=$true}
if(-not $rejected){throw '路径包含 junction 时必须拒绝'}
$locked=[IO.File]::Open($file,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try{
    $rejected=$false; try{$null=Get-PluginDemoEvidenceFile -Root $root -Path 'target/data.txt'}catch{$rejected=$true}
    if(-not $rejected){throw '写入占用中的证据不得读取或认证'}
}finally{$locked.Dispose()}
Write-Output 'PluginDemoEvidencePathSafety: 7 passed; project-local fixtures preserved'
