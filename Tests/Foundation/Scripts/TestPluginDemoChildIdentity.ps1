#requires -Version 7.0
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw '登录启动桥语法错误'}
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Test-PluginDemoChildIdentity'},$true)
if(-not $fn){throw '启动桥缺少精确进程身份验证函数'}
. ([scriptblock]::Create($fn.Extent.Text))
$current=Get-Process -Id $PID
$actual=$current.StartTime.ToUniversalTime()
$record=@{StartedUtc=$actual.ToString('o')}|ConvertTo-Json|ConvertFrom-Json
if(-not (Test-PluginDemoChildIdentity -Process $current -RecordedStart $record.StartedUtc)){throw '真实相同PID/完整UTC ticks不应被拒绝'}
$different=@{StartedUtc=$actual.AddTicks(1).ToString('o')}|ConvertTo-Json|ConvertFrom-Json
if(Test-PluginDemoChildIdentity -Process $current -RecordedStart $different.StartedUtc){throw '不同启动时间不得认作同一进程'}
if(Test-PluginDemoChildIdentity -Process $current -RecordedStart 'not-a-date'){throw '无效时间不得认作同一进程'}
$info=[Diagnostics.ProcessStartInfo]::new()
$info.FileName=(Get-Process -Id $PID).Path; $info.UseShellExecute=$false; $info.CreateNoWindow=$true
foreach($arg in @('-NoProfile','-Command','Start-Sleep -Milliseconds 1400; exit 0')){$null=$info.ArgumentList.Add($arg)}
$owned=[Diagnostics.Process]::Start($info)
try{
    $attached=[Diagnostics.Process]::GetProcessById($owned.Id)
    try{
        if(-not (Test-PluginDemoChildIdentity -Process $attached -RecordedStart $owned.StartTime.ToUniversalTime())){throw '实时子进程身份未通过'}
        $attached.WaitForExit()
        if($null -eq $attached.ExitCode -or $attached.ExitCode -ne 0){throw '身份验证未保留可读取退出码的进程句柄'}
    }finally{$attached.Dispose()}
}finally{$owned.Dispose()}
Write-Output 'PluginDemoChildIdentity: identity, invalid time and pinned-handle checks passed; no UE or network'
