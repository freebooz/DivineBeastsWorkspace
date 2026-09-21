#requires -Version 7.0
# 行为回归：删掉退出码传播、代次匹配、超时或所有权校验均应使这些测试失败。
# 全部临时材料保留在Saved；不运行引擎、不修改游戏源文件或资产。
param()
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$testRoot = Join-Path $workspace ('Saved/Validation/FoundationM0/ScriptTests-' + [guid]::NewGuid().ToString('D'))
$null = New-Item -ItemType Directory -Path $testRoot
$failures = [Collections.Generic.List[string]]::new()
$passed = 0
function Assert-Case([string]$Name, [scriptblock]$Body) {
    try { & $Body; $script:passed++; Write-Host "PASS $Name" }
    catch { $script:failures.Add("${Name}: $_"); Write-Host "FAIL ${Name}: $_" }
}
function Assert-Equal($Actual, $Expected) { if ($Actual -cne $Expected) { throw "Expected [$Expected], actual [$Actual]" } }
Assert-Case '未授权构建必须返回未执行2' {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $workspace 'Build/Game/BuildFoundation.ps1') *> (Join-Path $testRoot 'build-default.log')
    Assert-Equal $LASTEXITCODE 2
}
Assert-Case '未授权烘焙必须返回未执行2' {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $workspace 'Build/Game/CookFoundation.ps1') *> (Join-Path $testRoot 'cook-default.log')
    Assert-Equal $LASTEXITCODE 2
}
Assert-Case '未授权启动必须返回未执行2' {
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $workspace 'Build/Game/RunFoundation.ps1') *> (Join-Path $testRoot 'run-default.log')
    Assert-Equal $LASTEXITCODE 2
}
Assert-Case '缺失引擎与未执行矩阵不能全通过' {
    $id = [guid]::NewGuid().ToString('D')
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $workspace 'Build/Validation/VerifyFoundation.ps1') -EngineRoot (Join-Path $testRoot 'MissingEngine') -RunId $id *> (Join-Path $testRoot 'verify.log')
    Assert-Equal $LASTEXITCODE 2
    $report = Get-Content -Raw (Join-Path $workspace "Saved/Validation/FoundationM0/$id/Verify/result.json") | ConvertFrom-Json
    Assert-Equal $report.Status 'Incomplete'
    if (@($report.Matrix | Where-Object Status -eq 'NotExecuted').Count -lt 8) { throw '缺少未执行矩阵项' }
}
$module = Join-Path $workspace 'Build/Game/FoundationTools.psm1'
if (Test-Path $module) {
    Import-Module $module -Force
    $fake = Join-Path $PSScriptRoot 'FakeFoundationProcess.ps1'
    foreach ($scenario in @(
        @{ Name='外部非零码原样传播'; Mode='Exit'; Code=23; Expected=23; Ready=$false },
        @{ Name='正常工具退出'; Mode='Exit'; Code=0; Expected=0; Ready=$false },
        @{ Name='完整就绪并清理所属进程'; Mode='Ready'; Code=0; Expected=0; Ready=$true },
        @{ Name='宿主标记不能代替完整就绪'; Mode='Host'; Code=0; Expected=1; Ready=$true },
        @{ Name='其他RunId不能满足本次就绪'; Mode='WrongRun'; Code=0; Expected=1; Ready=$true },
        @{ Name='超时清理'; Mode='Hang'; Code=0; Expected=1; Ready=$true },
        @{ Name='就绪后立即崩溃不能通过'; Mode='ReadyCrash'; Code=0; Expected=17; Ready=$true }
        @{ Name='真实UE时间前缀与Map字段可识别'; Mode='Fields'; Code=0; Expected=0; Ready=$true },
        @{ Name='命令行回显不能伪造就绪'; Mode='CommandLine'; Code=0; Expected=1; Ready=$true },
        @{ Name='服务器就绪不能伪造玩家就绪'; Mode='Server'; Code=0; Expected=1; Ready=$true }
    )) {
        Assert-Case $scenario.Name {
            $dir = Join-Path $testRoot $scenario.Mode
            $null = New-Item -ItemType Directory -Path $dir -Force
            $log = Join-Path $dir 'engine.log'
            $id = [guid]::NewGuid().ToString('D')
            $options = @{ FilePath=(Join-Path $PSHOME 'pwsh.exe'); Arguments=@('-NoProfile','-File',$fake,'-Mode',$scenario.Mode,'-Code',"$($scenario.Code)",'-LogPath',$log,'-RunId',$id); WorkingDirectory=$testRoot; OutputDirectory=$dir; TimeoutSeconds=2 }
            if ($scenario.Ready) { $options.ReadyLog=$log; $options.ReadyMarker="FoundationReady RunId=$id" }
            $result = Invoke-FoundationProcess @options
            Assert-Equal $result.ExitCode $scenario.Expected
            if (Get-Process -Id $result.ProcessId -ErrorAction SilentlyContinue) { throw '所属进程未清理' }
        }
    }
    Assert-Case 'ArgumentList保留中文空格引号与末尾反斜线' {
        $value = '中文 space "quoted" C:\目录\'
        $dir = Join-Path $testRoot 'Unicode Space 中文'
        $null = New-Item -ItemType Directory -Path $dir
        $result = Invoke-FoundationProcess -FilePath (Join-Path $PSHOME 'pwsh.exe') -Arguments @('-NoProfile','-File',$fake,'-Mode','Echo','-Value',$value) -WorkingDirectory $dir -OutputDirectory $dir -TimeoutSeconds 3
        Assert-Equal $result.ExitCode 0
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $dir 'stdout.log')).TrimEnd()) $value
    }
    Assert-Case '旧日志拒绝启动且不覆盖' {
        $dir = Join-Path $testRoot 'Stale'
        $null = New-Item -ItemType Directory -Path $dir
        $log = Join-Path $dir 'old.log'
        [IO.File]::WriteAllText($log,'existing evidence')
        $rejected = $false
        try { Invoke-FoundationProcess -FilePath (Join-Path $PSHOME 'pwsh.exe') -Arguments @('-NoProfile','-File',$fake,'-Mode','Ready') -WorkingDirectory $dir -OutputDirectory $dir -ReadyLog $log -ReadyMarker 'FoundationReady' -TimeoutSeconds 1 }
        catch { $rejected = $true }
        Assert-Equal $rejected $true
        Assert-Equal ([IO.File]::ReadAllText($log)) 'existing evidence'
        Assert-Equal (Test-Path (Join-Path $dir 'process.json')) $false
    }
    Assert-Case '启动时间不匹配不能终止进程' {
        $info = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
        $info.UseShellExecute = $false; $info.CreateNoWindow = $true
        foreach ($item in @('-NoProfile','-File',$fake,'-Mode','Hang')) { $info.ArgumentList.Add($item) }
        $owned = [Diagnostics.Process]::Start($info)
        $ticks = $owned.StartTime.ToUniversalTime().Ticks
        try {
            Assert-Equal (Stop-FoundationOwnedProcess $owned ($ticks + 1)) $false
            Assert-Equal $owned.HasExited $false
        } finally { $null = Stop-FoundationOwnedProcess $owned $ticks; $owned.Dispose() }
    }
    foreach ($entry in @(
        @{Script='Build/Game/BuildFoundation.ps1'; Flags=@('-Editor')},
        @{Script='Build/Game/CookFoundation.ps1'; Flags=@('-Cook')},
        @{Script='Build/Game/RunFoundation.ps1'; Flags=@('-Start','-FoundationStandalone')}
    )) {
        Assert-Case "缺引擎未执行：$($entry.Script)" {
            $arguments = @('-NoProfile','-File',(Join-Path $workspace $entry.Script),'-EngineRoot',(Join-Path $testRoot 'MissingEngine')) + $entry.Flags
            & (Join-Path $PSHOME 'pwsh.exe') @arguments *> (Join-Path $testRoot ([IO.Path]::GetFileNameWithoutExtension($entry.Script) + '-missing.log'))
            Assert-Equal $LASTEXITCODE 2
        }
    }
} else { $failures.Add('公共进程执行模块尚未实现') }
@{ Passed=$passed; Failed=$failures.Count; Failures=@($failures); Evidence=$testRoot } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $testRoot 'results.json') -Encoding utf8
Write-Host "Passed=$passed Failed=$($failures.Count) Evidence=$testRoot"
if ($failures.Count) { exit 1 }; exit 0
