#requires -Version 7.0
<# World脚本行为测试；夹具仅检验报告与进程安全，不是UE运行、资产或Session证据。 #>
param()
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$tools = Join-Path $PSScriptRoot 'WorldValidationTools.ps1'
if (-not (Test-Path -LiteralPath $tools)) { Write-Host 'FAIL World报告与证据算法尚未实现'; exit 1 }
. $tools
$context = New-WorldValidationContext -RunId ([guid]::NewGuid()) -Operation ScriptTests
$cases = [Collections.Generic.List[object]]::new()
function Check([string]$Name,[scriptblock]$Body) {
    try { & $Body; $cases.Add(@{Name=$Name;Status='Passed'}); Write-Host "PASS $Name" }
    catch { $cases.Add(@{Name=$Name;Status='Failed';Message=$_.Exception.Message}); Write-Host "FAIL ${Name}: $_" }
}
function Equal($Actual,$Expected) { if ($Actual -cne $Expected) { throw "期望[$Expected]，实际[$Actual]" } }
function Reject([scriptblock]$Body) { $rejected=$false; try { & $Body | Out-Null } catch { $rejected=$true }; Equal $rejected $true }
function Reject-Code([scriptblock]$Body,[string]$Code) {
    $message=''; try { & $Body | Out-Null } catch {$message=$_.Exception.Message}
    if($message -notlike "$Code*"){throw "期望拒绝原因$Code，实际$message"}
}
function New-BuildEvidenceFixture {
    # 仅Saved中的源文本与报告算法夹具，不制造DLL/UE资产，不可作为真实运行凭据。
    $root=Join-Path $context.Directory ('SourceFixture-'+[guid]::NewGuid().ToString('N'))
    $null=New-Item -ItemType Directory -Path (Join-Path $root 'Game/Source/Main')
    [IO.File]::WriteAllText((Join-Path $root 'Game/Source/Main/Example.cpp'),'source-before')
    $snapshot=Get-WorldSourceSnapshot $root
    $report=[pscustomobject]@{Operation='VerifyWorld';Cases=@(@{Name='BuildEditor';Status='Passed';ExitCode=0});Details=@{BuildEvidenceVersion=2;Workspace=$root;SourceFingerprint=$snapshot.Fingerprint;EditorBinaryHashes=@{Main='00';World='00';Loading='00';Core='00';Data='00'}}}
    return @{Workspace=$root;Report=$report;Snapshot=$snapshot}
}
Check '构建后源码变化拒绝旧构建证据' {
    $fixture=New-BuildEvidenceFixture
    Equal (Get-WorldSourceSnapshot $fixture.Workspace).Fingerprint $fixture.Snapshot.Fingerprint
    [IO.File]::WriteAllText((Join-Path $fixture.Workspace 'Game/Source/Main/Example.cpp'),'source-after')
    Reject-Code {Assert-WorldBuildEvidence $fixture.Report $fixture.Workspace} 'SourceFingerprintMismatch'
}
Check '缺少主或插件DLL拒绝运行' {
    $fixture=New-BuildEvidenceFixture
    Reject-Code {Assert-WorldBuildEvidence $fixture.Report $fixture.Workspace} 'BinaryMissing:Main'
}
Check '只有主DLL哈希的旧报告拒绝运行' {
    $old=[pscustomobject]@{Operation='VerifyWorld';Cases=@(@{Name='BuildEditor';Status='Passed';ExitCode=0});Details=@{BinaryHashes=@{Editor='00'}}}
    Reject-Code {Assert-WorldBuildEvidence $old $context.Workspace} 'UnsupportedBuildEvidenceVersion'
}
Check '空矩阵不能通过' { Reject { Get-WorldVerdict @() } }
Check '只有原生通过仍未执行2' {
    $v=Get-WorldVerdict @(@{Name='Native';Status='Passed';ExitCode=0},@{Name='Runtime';Status='NotExecuted';ExitCode=2})
    Equal $v.Status 'NotExecuted'; Equal $v.ExitCode 2
}
Check '真实工具退出2保持Failed' {
    $v=Get-WorldVerdict @(@{Name='Tool';Status='Failed';ExitCode=2},@{Name='Runtime';Status='NotExecuted';ExitCode=2})
    Equal $v.Status 'Failed'; Equal $v.ExitCode 2
}
Check '非零外部码不被概括成1' { Equal (Get-WorldVerdict @(@{Name='Tool';Status='Failed';ExitCode=23})).ExitCode 23 }
Check '矛盾成功状态被拒绝' { Reject { Get-WorldVerdict @(@{Name='Tool';Status='Passed';ExitCode=2}) } }
Check '重复测试身份被拒绝' { Reject { Get-WorldVerdict @(@{Name='X';Status='Passed';ExitCode=0},@{Name='X';Status='Passed';ExitCode=0}) } }
Check '零RunId被拒绝' { Reject { New-WorldValidationContext -RunId ([guid]::Empty) -Operation Foundation } }
Check '已有RunId不能被第二入口覆盖' { Reject { New-WorldValidationContext -RunId ([guid]$context.RunId) -Operation Session } }
Check '路径逃逸地图被拒绝' { Reject { Resolve-WorldAssetPath $context '/Game/Development/World/../Secret' '.umap' } }
Check 'World启动参数绝不启用旧Coordinator' {
    $arguments=Get-WorldRuntimeArguments $context '/Game/Development/Foundation/Maps/L_FoundationSandbox' (Join-Path $context.Directory 'not-started.log') Foundation FullLifecycle
    Equal ($arguments -contains '-FoundationStandalone') $false
    Equal ($arguments -contains '-FoundationWorld') $true
    Equal ($arguments -contains '-CustomConfig=FoundationStandalone') $true
    Equal ($arguments -contains '-FoundationWorldExercise') $true
    Equal ($arguments -contains '-FoundationWorldDefinition=GamePlatformDefinition:foundation.world@1') $true
    Equal ($arguments -contains "-FoundationRunId=$($context.RunId)") $true
}
Check '仅Readiness不擅自启用跨图Exercise' {
    $arguments=Get-WorldRuntimeArguments $context '/Game/Development/Foundation/Maps/L_FoundationSandbox' (Join-Path $context.Directory 'not-started.log') Foundation Readiness
    Equal ($arguments -contains '-FoundationWorldExercise') $false
}
# 手工列出期望事件，不从生产算法生成期望值；漏掉清理或允许旧代次时必须失败。
$id=$context.RunId; $generation=[guid]::NewGuid().ToString('D'); $next=[guid]::NewGuid().ToString('D')
$events=@('Initialized','DefinitionLoaded','MapMatched','RegionsReady','WorldReady','ReturnedToBootstrap','CleanupComplete','Initialized','DefinitionLoaded','MapMatched','RegionsReady','Reentered','Completed')
$good=@(for($i=0;$i -lt $events.Count;$i++){ $g=if($i -ge 7){$next}else{$generation}; "LogDBAWorld: WorldValidation RunId=$id ProcessId=123 Scenario=Foundation Event=$($events[$i]) Generation=$g" }) -join "`n"
Check '完整同PID当前代次事件通过算法' { Equal (Test-WorldRuntimeMarkers $good $id 123 Foundation).Passed $true }
Check '错误RunId不被旧日志满足' { Equal (Test-WorldRuntimeMarkers ($good.Replace($id,[guid]::NewGuid().ToString('D'))) $id 123 Foundation).Passed $false }
Check '不同PID不能拼接证据' { Equal (Test-WorldRuntimeMarkers ($good.Replace('Event=WorldReady','Event=WorldReady').Replace('ProcessId=123 Scenario=Foundation Event=WorldReady','ProcessId=999 Scenario=Foundation Event=WorldReady')) $id 123 Foundation).Passed $false }
Check '命令行回显不算事件' { Equal (Test-WorldRuntimeMarkers ($good.Replace('LogDBAWorld:','LogInit: Command Line:')) $id 123 Foundation).Passed $false }
Check '缺少清理不通过' { Equal (Test-WorldRuntimeMarkers (($good -split "`n" | Where-Object {$_ -notmatch 'CleanupComplete'}) -join "`n") $id 123 Foundation).Passed $false }
Check '重入不能复用旧Generation' { Equal (Test-WorldRuntimeMarkers ($good.Replace($next,$generation)) $id 123 Foundation).Passed $false }
Check '新世界也必须实际初始化与区域就绪' { Equal (Test-WorldRuntimeMarkers (($good -split "`n" | Where-Object {$_ -notmatch "Event=RegionsReady Generation=$next"}) -join "`n") $id 123 Foundation).Passed $false }
Check '新世界定义回调不能携带旧Generation' { Equal (Test-WorldRuntimeMarkers ($good.Replace("Event=DefinitionLoaded Generation=$next","Event=DefinitionLoaded Generation=$generation")) $id 123 Foundation).Passed $false }
Check '同轮事件不能乱序' { $lines=$good -split "`n"; ($lines[1],$lines[2])=($lines[2],$lines[1]); Equal (Test-WorldRuntimeMarkers ($lines -join "`n") $id 123 Foundation).Passed $false }
Check '未终止半行不作为完整证据' { Equal (Test-WorldRuntimeMarkers ($good.Substring(0,$good.Length-8)) $id 123 Foundation).Passed $false }
Check '只Ready不要求宿主虚报跨图完成' {
    $ready=($good -split "`n" | Select-Object -First 5) -join "`n"
    Equal (Test-WorldRuntimeMarkers $ready $id 123 Foundation -Phase Readiness).Passed $true
    Equal (Test-WorldRuntimeMarkers $ready $id 123 Foundation -Phase FullLifecycle).Passed $false
}
Check 'CTest跳过不能通过' {
    Reject { Assert-WorldCTestReport '<testsuite tests="1" failures="0" skipped="1"><testcase name="WorldPolicyTests" status="notrun" /></testsuite>' }
}
Check 'CTest空套件不能通过' { Reject { Assert-WorldCTestReport '<testsuite tests="0" failures="0" skipped="0" />' } }
Check 'CTest真实身份必需' { Reject { Assert-WorldCTestReport '<testsuite tests="1" failures="0" skipped="0"><testcase name="Other" status="run" /></testsuite>' } }
Check 'CTest具名已执行通过可接纳' { Assert-WorldCTestReport '<testsuite tests="1" failures="0" skipped="0"><testcase name="WorldPolicyTests" status="run" /></testsuite>' }
foreach($mode in @('Ready','WrongPid','Crash')) {
    Check "日志观察与真实所属PID绑定：$mode" {
        $dir=Join-Path $context.Directory "Observed-$mode"; $null=New-Item -ItemType Directory -Path $dir
        $log=Join-Path $dir 'engine.log'
        $result=Invoke-WorldObservedProcess -Context $context -Executable (Join-Path $PSHOME 'pwsh.exe') -Arguments @('-NoProfile','-File',(Join-Path $PSScriptRoot 'WorldProcessFixture.ps1'),'-LogPath',$log,'-RunId',$id,'-Mode',$mode) -Directory $dir -Log $log -Scenario Foundation -Phase Readiness -TimeoutSeconds 3
        Equal $result.ExitCode $(if($mode -eq 'Ready'){0}elseif($mode -eq 'Crash'){17}else{1})
        Equal $result.Process.Cleaned $true
    }
}
$fixture=Join-Path $workspace 'Tests/Foundation/Scripts/FakeFoundationProcess.ps1'
foreach($code in @(0,2,23)) {
    Check "真实进程退出码${code}传播" {
        $step=Invoke-WorldTool $context "Exit-$code" (Join-Path $PSHOME 'pwsh.exe') @('-NoProfile','-File',$fixture,'-Mode','Exit','-Code',"$code") 5
        Equal $step.ExitCode $code
        Equal $step.Status $(if($code -eq 0){'Passed'}else{'Failed'})
        Equal $step.Process.Cleaned $true
    }
}
Check '超时只清理所属PID' {
    $step=Invoke-WorldTool $context 'Timeout' (Join-Path $PSHOME 'pwsh.exe') @('-NoProfile','-File',$fixture,'-Mode','Hang') 1
    Equal $step.Status 'Failed'; Equal $step.Process.TimedOut $true; Equal $step.Process.Cleaned $true
}
Check '运行日志不可复用且拒绝前不启动进程' {
    $dir=Join-Path $context.Directory 'StaleLog'; $null=New-Item -ItemType Directory $dir
    $log=Join-Path $dir 'engine.log'; [IO.File]::WriteAllText($log,'old evidence')
    Reject { Invoke-FoundationProcess -FilePath (Join-Path $PSHOME 'pwsh.exe') -WorkingDirectory $workspace -OutputDirectory $dir -ReadyLog $log -ReadyMarker WorldValidation -TimeoutSeconds 1 }
    Equal ([IO.File]::ReadAllText($log)) 'old evidence'; Equal (Test-Path (Join-Path $dir 'process.json')) $false
}
foreach($entry in @('TestWorldFoundation.ps1','TestWorldSession.ps1','TestWorldPartition.ps1')) {
    Check "入口默认不启动且返回未执行2：$entry" {
        $run=[guid]::NewGuid().ToString('D')
        $step=Invoke-WorldTool $context ([IO.Path]::GetFileNameWithoutExtension($entry)) (Join-Path $PSHOME 'pwsh.exe') @('-NoProfile','-File',(Join-Path $PSScriptRoot $entry),'-RunId',$run) 10
        Equal $step.ExitCode 2
        $report=Get-Content -Raw (Join-Path $workspace "Saved/Validation/GamePlatformWorld/$run/result.json") | ConvertFrom-Json
        Equal $report.Status 'NotExecuted'; Equal @($report.ProcessOrContainerIds).Count 0
    }
}
Check '总门禁默认包含所有未执行范围' {
    $run=[guid]::NewGuid().ToString('D')
    $step=Invoke-WorldTool $context 'VerifyDefault' (Join-Path $PSHOME 'pwsh.exe') @('-NoProfile','-File',(Join-Path $workspace 'Build/Validation/VerifyWorld.ps1'),'-RunId',$run) 10
    Equal $step.ExitCode 2
    $report=Get-Content -Raw (Join-Path $workspace "Saved/Validation/GamePlatformWorld/$run/result.json") | ConvertFrom-Json
    Equal $report.Status 'NotExecuted'
    foreach($name in @('NativeDebug','NativeRelease','BuildEditor','BuildClient','BuildServer','RuntimeFoundation','RuntimeSession','RuntimePartition','Cook','Stage','MultiPIE','ManualReview')) {
        $rows=@($report.Cases | Where-Object Name -eq $name); Equal $rows.Count 1; Equal $rows[0].Status 'NotExecuted'
    }
}
$failed=@($cases | Where-Object Status -eq Failed).Count
@{RunId=$id;Status=$(if($failed){'Failed'}else{'Passed'});ExitCode=$(if($failed){1}else{0});Passed=$cases.Count-$failed;Failed=$failed;Cases=@($cases.ToArray());Evidence=$context.Directory;Scope='脚本算法与进程安全；不证明UE运行'} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $context.Directory 'result.json') -Encoding utf8
Write-Host "Passed=$($cases.Count-$failed) Failed=$failed Evidence=$($context.Directory)"
if($failed){exit 1}; exit 0
