#requires -Version 7.0
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw '登录启动桥语法错误'}
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Wait-PluginDemoChildExit'},$true)
if(-not $fn){throw '启动桥缺少可靠的进程退出码等待函数'}
. ([scriptblock]::Create($fn.Extent.Text))
foreach($expected in @(0,7)){
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=(Get-Process -Id $PID).Path
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    foreach($arg in @('-NoProfile','-Command',("Start-Sleep -Milliseconds 1200; exit $expected"))){$null=$info.ArgumentList.Add($arg)}
    $owned=[Diagnostics.Process]::Start($info)
    try{
        $attached=[Diagnostics.Process]::GetProcessById($owned.Id)
        try{$actual=Wait-PluginDemoChildExit -Process $attached}finally{$attached.Dispose()}
        if($actual -ne $expected){throw "真实退出码错误：期望 $expected，实际 $actual"}
    }finally{$owned.Dispose()}
}
Write-Output 'PluginDemoChildExit: normal and failure exit codes passed; no UE or network'
