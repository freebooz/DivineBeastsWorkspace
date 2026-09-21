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
    $verifyCode = $LASTEXITCODE
    if ($verifyCode -notin @(1,2)) { throw "未执行验证不能返回$verifyCode" }
    $report = Get-Content -Raw (Join-Path $workspace "Saved/Validation/FoundationM0/$id/Verify/result.json") | ConvertFrom-Json
    Assert-Equal $report.ExitCode $verifyCode
    if ($report.Status -notin @('Incomplete','Failed')) { throw '未执行矩阵不应通过' }
    if (@($report.Matrix | Where-Object Status -eq 'NotExecuted').Count -lt 8) { throw '缺少未执行矩阵项' }
    $core = @($report.Matrix | Where-Object Name -eq 'NativeCore')
    Assert-Equal $core.Count 1
    Assert-Equal $core[0].Status 'NotExecuted'
    Assert-Equal $core[0].ExitCode 2
    # 文件包头与本地冒烟均不能替代完整验收；没有证据接入口的必需项必须逐项保留。
    foreach ($name in @('UEAutomation','AssetGeneration','AssetRegenerationProtection','AssetNegativeValidation','MultiPIE','GraphicalValidation','CancellationRecovery','ReleaseContentStripping')) {
        $required = @($report.Matrix | Where-Object Name -eq $name)
        Assert-Equal $required.Count 1
        Assert-Equal $required[0].Status 'NotExecuted'
        Assert-Equal $required[0].ExitCode 2
    }
}
Assert-Case 'Core原生配置失败必须计入失败矩阵' {
    # 故意使用不存在的CMake生成器：真实工具拒绝配置，不改源码、不构建UE或其他套件。
    $id = [guid]::NewGuid().ToString('D')
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $workspace 'Build/Validation/VerifyFoundation.ps1') -NativeTests -NativeGenerator FoundationIntentionalInvalidGenerator -RunId $id *> (Join-Path $testRoot 'verify-native-failure.log')
    Assert-Equal $LASTEXITCODE 1
    $report = Get-Content -Raw (Join-Path $workspace "Saved/Validation/FoundationM0/$id/Verify/result.json") | ConvertFrom-Json
    Assert-Equal $report.Status 'Failed'
    $core = @($report.Matrix | Where-Object Name -eq 'NativeCore')
    Assert-Equal $core.Count 1
    Assert-Equal $core[0].Status 'Failed'
    if ($core[0].ExitCode -eq 0) { throw 'Core真实工具失败码丢失' }
    if (Test-Path (Join-Path $core[0].Evidence 'Build/process.json')) { throw '配置失败后不应继续构建' }
}
Assert-Case '没有真实失败但验收未执行必须汇总Incomplete2' {
    # 仅复制待测脚本到Saved隔离夹具；不创建uproject、插件实现或假资产，不构成新游戏宿主。
    # 隔离现有坏描述文件，验证Incomplete分支不会被工作区已有Failed项掩盖。
    $fixture = Join-Path $testRoot 'IncompleteFixture'
    $validationDir = Join-Path $fixture 'Build/Validation'
    $gameBuildDir = Join-Path $fixture 'Build/Game'
    $null = New-Item -ItemType Directory -Path $validationDir,$gameBuildDir,(Join-Path $fixture 'Game/Plugins') -Force
    Copy-Item -LiteralPath (Join-Path $workspace 'Build/Validation/VerifyFoundation.ps1') -Destination $validationDir
    Copy-Item -LiteralPath (Join-Path $workspace 'Build/Game/FoundationTools.psm1') -Destination $gameBuildDir
    $id = [guid]::NewGuid().ToString('D')
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $validationDir 'VerifyFoundation.ps1') -EngineRoot (Join-Path $fixture 'MissingEngine') -RunId $id *> (Join-Path $testRoot 'verify-incomplete.log')
    Assert-Equal $LASTEXITCODE 2
    $report = Get-Content -Raw (Join-Path $fixture "Saved/Validation/FoundationM0/$id/Verify/result.json") | ConvertFrom-Json
    Assert-Equal $report.Status 'Incomplete'
    Assert-Equal $report.ExitCode 2
    foreach ($name in @('UEAutomation','AssetGeneration','AssetRegenerationProtection','AssetNegativeValidation','MultiPIE','GraphicalValidation','CancellationRecovery','ReleaseContentStripping')) {
        $row = @($report.Matrix | Where-Object Name -eq $name)
        Assert-Equal $row.Count 1
        Assert-Equal $row[0].Status 'NotExecuted'
    }
}
$module = Join-Path $workspace 'Build/Game/FoundationTools.psm1'
if (Test-Path $module) {
    Import-Module $module -Force
    $fake = Join-Path $PSScriptRoot 'FakeFoundationProcess.ps1'
    foreach ($scenario in @(
        @{ Name='外部非零码原样传播'; Mode='Exit'; Code=23; Expected=23; Ready=$false },
        @{ Name='外部退出2也必须保留'; Mode='Exit'; Code=2; Expected=2; Ready=$false },
        @{ Name='正常工具退出'; Mode='Exit'; Code=0; Expected=0; Ready=$false },
        @{ Name='完整就绪并清理所属进程'; Mode='Ready'; Code=0; Expected=0; Ready=$true },
        @{ Name='宿主标记不能代替完整就绪'; Mode='Host'; Code=0; Expected=1; Ready=$true },
        @{ Name='其他RunId不能满足本次就绪'; Mode='WrongRun'; Code=0; Expected=1; Ready=$true },
        @{ Name='超时清理'; Mode='Hang'; Code=0; Expected=1; Ready=$true },
        @{ Name='就绪后立即崩溃不能通过'; Mode='ReadyCrash'; Code=0; Expected=17; Ready=$true }
        @{ Name='真实UE时间前缀与Map字段可识别'; Mode='Fields'; Code=0; Expected=0; Ready=$true },
        @{ Name='日志被引擎持续写入时仍能核验'; Mode='HeldLog'; Code=0; Expected=0; Ready=$true },
        @{ Name='命令行回显不能伪造就绪'; Mode='CommandLine'; Code=0; Expected=1; Ready=$true },
        @{ Name='服务器就绪不能伪造玩家就绪'; Mode='Server'; Code=0; Expected=1; Ready=$true }
    )) {
        Assert-Case $scenario.Name {
            $dir = Join-Path $testRoot "$($scenario.Mode)-$($scenario.Code)"
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
    foreach ($entry in @(@{Mode='Host'; Marker='FoundationHostReady'},@{Mode='Server'; Marker='FoundationServerReady'})) {
        Assert-Case "显式标记可通过：$($entry.Marker)" {
            $dir = Join-Path $testRoot $entry.Marker
            $null = New-Item -ItemType Directory -Path $dir
            $log = Join-Path $dir 'engine.log'; $id = [guid]::NewGuid().ToString('D')
            $result = Invoke-FoundationProcess -FilePath (Join-Path $PSHOME 'pwsh.exe') -Arguments @('-NoProfile','-File',$fake,'-Mode',$entry.Mode,'-LogPath',$log,'-RunId',$id) -WorkingDirectory $dir -OutputDirectory $dir -TimeoutSeconds 3 -ReadyLog $log -ReadyMarker "$($entry.Marker) RunId=$id"
            Assert-Equal $result.ExitCode 0
            Assert-Equal $result.Ready $true
        }
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
    foreach ($layout in @('framework','frameworks')) {
        Assert-Case "UE工具运行时配置解析：$layout" {
            # 仅测试工具路径解析；这些Saved标记文件绝不会被启动，也不构成引擎或游戏宿主。
            $root = Join-Path $testRoot "RuntimeConfig-$layout"
            $toolDir = Join-Path $root 'Engine/Binaries/DotNET/AutomationTool'
            $sdkDir = Join-Path $root 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64'
            $null = New-Item -ItemType Directory -Path $toolDir,$sdkDir -Force
            [IO.File]::WriteAllText((Join-Path $toolDir 'AutomationTool.dll'),'test-only-not-executable')
            [IO.File]::WriteAllText((Join-Path $sdkDir 'dotnet.exe'),'test-only-not-executable')
            $framework = @{ name='Microsoft.NETCore.App'; version='10.0.0' }
            $options = @{}; $options[$layout] = if ($layout -eq 'framework') { $framework } else { @($framework,@{name='Microsoft.WindowsDesktop.App';version='10.0.0'}) }
            @{runtimeOptions=$options} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $toolDir 'AutomationTool.runtimeconfig.json') -Encoding utf8
            $tool = Get-FoundationManagedTool ([pscustomobject]@{Root=$root}) AutomationTool
            Assert-Equal $tool.Executable (Join-Path $sdkDir 'dotnet.exe')
        }
    }
} else { $failures.Add('公共进程执行模块尚未实现') }
@{ Passed=$passed; Failed=$failures.Count; Failures=@($failures); Evidence=$testRoot } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $testRoot 'results.json') -Encoding utf8
Write-Host "Passed=$passed Failed=$($failures.Count) Evidence=$testRoot"
if ($failures.Count) { exit 1 }; exit 0
