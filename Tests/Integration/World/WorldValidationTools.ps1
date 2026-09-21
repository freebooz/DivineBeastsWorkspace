#requires -Version 7.0
<# World验证共用函数。复用Foundation进程所有权、引擎版本与构建锁；不复制进程实现。
   证据只写Saved/Validation/GamePlatformWorld，严禁用脚本夹具冒充UE/Session运行。
   退出码2可以是外部工具真实失败；必须同时检查Status，而不能只按数字归类。 #>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:WorldWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $script:WorldWorkspace 'Build/Game/FoundationTools.psm1') -Force

function New-WorldValidationContext {
    <# 每次入口使用新GUID；不同操作也不可复用同一目录。保留全部证据，不删除旧运行。 #>
    param([guid]$RunId,[ValidateSet('VerifyWorld','Foundation','Session','Partition','ScriptTests')][string]$Operation)
    if($RunId -eq [guid]::Empty){throw 'RunId不能为零GUID。'}
    $directory=Join-Path $script:WorldWorkspace "Saved/Validation/GamePlatformWorld/$($RunId.ToString('D'))"
    if(Test-Path -LiteralPath $directory){throw "拒绝覆盖已有World证据：$directory"}
    $null=New-Item -ItemType Directory -Path $directory -ErrorAction Stop
    [pscustomobject]@{Workspace=$script:WorldWorkspace;Project=(Join-Path $script:WorldWorkspace 'Game/DivineBeastsArena.uproject');Directory=$directory;RunId=$RunId.ToString('D');Operation=$Operation}
}
function New-WorldCase {
    <# 未执行是显式状态，不能当成成功；Message说明缺什么或实际失败的步骤。 #>
    param([string]$Name,[ValidateSet('Passed','Failed','NotExecuted')][string]$Status='NotExecuted',[int]$ExitCode=2,[string]$Message='本轮未选择执行。',[object[]]$Evidence=@())
    [pscustomobject]@{Name=$Name;Status=$Status;ExitCode=$ExitCode;Message=$Message;Evidence=$Evidence}
}
function Get-WorldVerdict {
    <# 拒绝空矩阵、重复身份及矛盾状态；失败优先，再未执行，绝不以已选项代替完整验收。 #>
    param([object[]]$Cases)
    if(-not $Cases -or $Cases.Count -eq 0){throw '空验收矩阵不能通过。'}
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($case in $Cases){
        if(-not $case.Name -or -not $names.Add($case.Name)){throw '验收项名称为空或重复。'}
        if($case.Status -notin @('Passed','Failed','NotExecuted')){throw '未知验收状态。'}
        if(($case.Status -eq 'Passed' -and $case.ExitCode -ne 0) -or ($case.Status -eq 'Failed' -and $case.ExitCode -eq 0) -or ($case.Status -eq 'NotExecuted' -and $case.ExitCode -ne 2)){throw '状态与退出码矛盾。'}
    }
    $failed=@($Cases | Where-Object Status -eq Failed)
    if($failed.Count){return [pscustomobject]@{Status='Failed';ExitCode=[int]$failed[0].ExitCode}}
    if(@($Cases | Where-Object Status -eq NotExecuted).Count){return [pscustomobject]@{Status='NotExecuted';ExitCode=2}}
    [pscustomobject]@{Status='Passed';ExitCode=0}
}
function Write-WorldReport {
    <# 统一机器可读结果；包含本轮实际进程身份及证据，不收集环境变量或历史日志。 #>
    param($Context,[object[]]$Cases,[object[]]$Steps=@(),[hashtable]$Details=@{})
    $verdict=Get-WorldVerdict $Cases
    $identities=@($Steps | Where-Object {$null -ne $_.Process} | ForEach-Object {[pscustomobject]@{ProcessId=$_.Process.ProcessId;StartTimeTicks=$_.Process.StartTimeTicks;Cleaned=$_.Process.Cleaned;Evidence=$_.Evidence}})
    $report=[ordered]@{Operation=$Context.Operation;RunId=$Context.RunId;Status=$verdict.Status;ExitCode=$verdict.ExitCode;TimestampUtc=[DateTime]::UtcNow.ToString('O');Cases=$Cases;Steps=$Steps;ProcessOrContainerIds=$identities;Evidence=$Context.Directory;Details=$Details}
    $report | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $Context.Directory 'result.json') -Encoding utf8
    Write-Host "$($Context.Operation): $($verdict.Status) exit=$($verdict.ExitCode) Evidence=$($Context.Directory)"
    return $verdict
}
function Invoke-WorldTool {
    <# 外部工具只通过共用进程工具调用；目录不可覆盖、参数逐项传递、真实非零码原样记录。 #>
    param($Context,[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Name,[string]$Executable,[string[]]$Arguments,[int]$TimeoutSeconds=180,[hashtable]$Environment=@{},[string]$WorkingDirectory)
    $directory=Join-Path $Context.Directory $Name
    if(Test-Path -LiteralPath $directory){throw '步骤目录已经存在，拒绝覆盖。'}
    $null=New-Item -ItemType Directory -Path $directory
    if(-not $WorkingDirectory){$WorkingDirectory=$Context.Workspace}
    $process=$null; $code=1; $message=''
    try {
        $process=Invoke-FoundationProcess -FilePath $Executable -Arguments $Arguments -WorkingDirectory $WorkingDirectory -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds -Environment $Environment
        $code=$process.ExitCode
    } catch {$message=$_.Exception.Message}
    $step=[pscustomobject]@{Name=$Name;Status=$(if($code -eq 0){'Passed'}else{'Failed'});ExitCode=$code;Process=$process;Evidence=$directory;Message=$message}
    $step | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $directory 'result.json') -Encoding utf8
    return $step
}
function Resolve-WorldAssetPath {
    <# 仅允许开发资产长包名，不接受旅行URL、对象后缀、路径逃逸；真实包合法性由UE负责。 #>
    param($Context,[string]$Package,[ValidateSet('.umap','.uasset')][string]$Extension)
    if($Package -cnotmatch '^/Game/Development/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+$'){throw '必须指定/Game/Development内无旅行参数的资产长包名。'}
    return Join-Path $Context.Workspace ('Game/Content/'+$Package.Substring(6)+$Extension)
}
function Test-WorldRuntimeMarkers {
    <# 待主工程实现的日志消费协议：每事件一整行，RunId/PID/Scenario/Generation均绑定当前运行。
       这仅是日志判定算法，不证明宿主已实现这些事件。不得将FoundationReady替代World完成。
       同轮初始化至清理代次不变；Foundation重入必须新代次；事件缺失、拼接、回显均失败。 #>
    param([string]$Content,[string]$RunId,[int]$ProcessId,[ValidateSet('Foundation','Partition')][string]$Scenario,[ValidateSet('Readiness','FullLifecycle')][string]$Phase='FullLifecycle')
    $required=if($Scenario -eq 'Foundation'){@('Initialized','DefinitionLoaded','MapMatched','RegionsReady','WorldReady','ReturnedToBootstrap','CleanupComplete','Reentered','Completed')}else{@('Initialized','DefinitionLoaded','MapMatched','PartitionConfirmed','SourceRegistered','StreamingReady','SourceRevoked','CleanupComplete','Completed')}
    if($Scenario -eq 'Foundation' -and $Phase -eq 'Readiness'){$required=@('Initialized','DefinitionLoaded','MapMatched','RegionsReady','WorldReady')}
    $pattern='^(?:\[[^\r\n]*\])*(?:Log[A-Za-z0-9_]+:[ \t]*(?:(?:Display|Verbose):[ \t]*)?)?WorldValidation RunId=([0-9a-f-]{36}) ProcessId=([1-9][0-9]*) Scenario=(Foundation|Partition) Event=([A-Za-z]+) Generation=([0-9a-f-]{36})[ \t]*$'
    $records=[Collections.Generic.List[object]]::new()
    foreach($line in ($Content -split '\r?\n')){
        if($line -cmatch $pattern){
            if($Matches[1] -cne $RunId -or [int]$Matches[2] -ne $ProcessId -or $Matches[3] -cne $Scenario){return [pscustomobject]@{Passed=$false;Message='发现其他RunId/PID/场景的World事件。'}}
            $generation=[guid]::Empty
            if(-not [guid]::TryParseExact($Matches[5],'D',[ref]$generation) -or $generation -eq [guid]::Empty){return [pscustomobject]@{Passed=$false;Message='代次不是非零GUID。'}}
            $records.Add(@{Event=$Matches[4];Generation=$generation.ToString('D')})
        }
    }
    if($records.Count -ne $required.Count){return [pscustomobject]@{Passed=$false;Message='阶段事件缺失或重复。'}}
    $first=$records[0].Generation
    for($i=0;$i -lt $required.Count;$i++){
        if($records[$i].Event -cne $required[$i]){return [pscustomobject]@{Passed=$false;Message='阶段事件顺序错误。'}}
        $reentered=$Scenario -eq 'Foundation' -and $i -ge 7
        if((-not $reentered -and $records[$i].Generation -cne $first) -or ($reentered -and ($records[$i].Generation -ceq $first -or $records[$i].Generation -cne $records[7].Generation))){return [pscustomobject]@{Passed=$false;Message='世界清理/重入代次错误。'}}
    }
    [pscustomobject]@{Passed=$true;Message='本次PID完整阶段与代次证据匹配；仅覆盖显式事件协议。'}
}
function Assert-WorldCTestReport {
    <# CTest退出0也必须核验具名测试确实执行；skip/空套件或其他测试不能冒充World生产算法。 #>
    param([string]$Content)
    [xml]$xml=$Content
    $suite=$xml.SelectSingleNode('/testsuite'); $tests=@($xml.SelectNodes('/testsuite/testcase'))
    if(-not $suite -or $suite.GetAttribute('tests') -notmatch '^[1-9][0-9]*$' -or $suite.GetAttribute('failures') -ne '0' -or $suite.GetAttribute('skipped') -ne '0' -or $tests.Count -ne [int]$suite.GetAttribute('tests')){throw 'CTest报告为空、失败、跳过或数量不一致。'}
    foreach($test in $tests){if($test.GetAttribute('status') -ne 'run' -or $test.SelectSingleNode('failure|error|skipped')){throw 'CTest包含未执行或失败项。'}}
    if(@($tests | Where-Object {$_.GetAttribute('name') -ceq 'WorldPolicyTests'}).Count -ne 1){throw '缺少唯一WorldPolicyTests。'}
}
function Invoke-WorldObservedProcess {
    <# Foundation进程工具仅支持静态前缀，无法预知尚未创建的UE PID。
       本线程观察者读取该工具创建的process.json，逐行校验当前日志；全部证据满足后才写独占就绪文件。
       不复制进程启动/终止逻辑；finally仅停止自己的观察线程，UE清理由Foundation的PID+启动时间保护完成。 #>
    param($Context,[string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Log,[ValidateSet('Foundation','Partition')][string]$Scenario,[ValidateSet('Readiness','FullLifecycle')][string]$Phase,[int]$TimeoutSeconds)
    $beacon=Join-Path $Directory 'observed.log'
    if((Test-Path -LiteralPath $Log) -or (Test-Path -LiteralPath $beacon) -or (Test-Path -LiteralPath (Join-Path $Directory 'process.json'))){throw '拒绝复用运行日志或进程清单。'}
    $observer=Start-ThreadJob -ArgumentList @((Join-Path $PSScriptRoot 'WorldValidationTools.ps1'),$Directory,$Log,$beacon,$Context.RunId,$Scenario,$Phase,$TimeoutSeconds) -ScriptBlock {
        param($toolsPath,$directory,$log,$beacon,$runId,$scenario,$phase,$timeout)
        . $toolsPath
        $watch=[Diagnostics.Stopwatch]::StartNew()
        while($watch.Elapsed.TotalSeconds -lt $timeout){
            try {
                $manifest=Join-Path $directory 'process.json'
                if((Test-Path -LiteralPath $manifest) -and (Test-Path -LiteralPath $log)){
                    $identity=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
                    $stream=[IO.File]::Open($log,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
                    $reader=[IO.StreamReader]::new($stream)
                    try {$content=$reader.ReadToEnd()} finally {$reader.Dispose()}
                    $check=Test-WorldRuntimeMarkers $content $runId $identity.ProcessId $scenario -Phase $phase
                    if($check.Passed){[IO.File]::WriteAllText($beacon,"WorldValidationObserved RunId=$runId`n"); return}
                }
            } catch [IO.IOException] { } catch [ArgumentException] { } # 文件正在创建/写入，下一轮重读；不吞掉工具或验证错误。
              catch [System.Management.Automation.RuntimeException] { if($_.Exception.Message -notmatch 'JSON|Json'){throw} }
            Start-Sleep -Milliseconds 50
        }
    }
    try {
        $process=Invoke-FoundationProcess -FilePath $Executable -Arguments $Arguments -WorkingDirectory $Context.Workspace -OutputDirectory $Directory -TimeoutSeconds $TimeoutSeconds -ReadyLog $beacon -ReadyMarker "WorldValidationObserved RunId=$($Context.RunId)"
        $code=$process.ExitCode; $message='进程失败、超时或缺少当前PID的阶段证据。'
        if($code -eq 0){
            $check=Test-WorldRuntimeMarkers ([IO.File]::ReadAllText($Log)) $Context.RunId $process.ProcessId $Scenario -Phase $Phase
            $message=$check.Message; if(-not $check.Passed){$code=1}
        }
        $step=[pscustomobject]@{Name='UE';Status=$(if($code -eq 0){'Passed'}else{'Failed'});ExitCode=$code;Process=$process;Evidence=$Directory;Message=$message}
        $step | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $Directory 'result.json') -Encoding utf8
        return $step
    } finally {Stop-Job -Job $observer -ErrorAction SilentlyContinue; Remove-Job -Job $observer -Force -ErrorAction SilentlyContinue}
}
function Invoke-WorldRuntimeGate {
    <# 三入口共用fail-closed门禁。Session尚无公共快照及双进程适配，绝不伪造联网成功。
       Foundation/Partition需要显式Start、真实资产、UE5.8以及成功Editor构建的二进制哈希证据。
       运行参数/事件是待主工程接入的合同；未实现时缺标记超时失败，不退化为启动成功。
       当前尚无真实地图，预计前置阶段NotExecuted 2；不自动生成资产或构建引擎。 #>
    param([ValidateSet('Foundation','Session','Partition')][string]$Scenario,[guid]$RunId,[switch]$Start,[string]$EngineRoot,[string]$Map,[string]$DefinitionPackage,[string]$BuildResult,[int]$TimeoutSeconds=120,[ValidateSet('Readiness','FullLifecycle')][string]$Phase='FullLifecycle')
    $context=New-WorldValidationContext $RunId $Scenario
    $steps=@(); $case=New-WorldCase "Runtime$Scenario"; $extraCases=@(); $details=@{Scope='本入口不证明Cook、Stage、MultiPIE或完整World验收。';Phase=$Phase;HostProtocol='Foundation：-FoundationWorld -FoundationWorldRunId=<D GUID>。只消费WorldValidation具名事件，不要求额外完成标记。Partition宿主适配尚待接入。'}
    try {
        if($Scenario -eq 'Session'){
            throw [IO.FileNotFoundException]::new('Session公开服务/真实快照及客户端+专用服务器验证适配尚未接入；匹配与错误WorldId均未执行。不能用私有State、同名地图或开发上下文代替。')
        }
        if(-not $Start){throw [IO.FileNotFoundException]::new('未指定-Start，不启动UE进程。')}
        $engine=Get-FoundationEngine $EngineRoot; $details.EngineVersion=$engine.Version
        Assert-FoundationFile $context.Project
        foreach($package in @($Map,$DefinitionPackage)){if(-not $package){throw [IO.FileNotFoundException]::new('必须指定真实开发Map与DefinitionPackage；本脚本不制造资产。')}}
        Assert-FoundationFile (Resolve-WorldAssetPath $context $Map '.umap')
        Assert-FoundationFile (Resolve-WorldAssetPath $context $DefinitionPackage '.uasset')
        if($Scenario -eq 'Foundation'){Assert-FoundationFile (Resolve-WorldAssetPath $context '/Game/Development/Foundation/Maps/L_FoundationBootstrap' '.umap')}
        if(-not $BuildResult){throw [IO.FileNotFoundException]::new('缺少-BuildResult：需要VerifyWorld实际Editor构建成功结果，不以现存exe代替构建证据。')}
        Assert-FoundationFile $BuildResult
        $build=Get-Content -Raw -LiteralPath $BuildResult | ConvertFrom-Json
        $rows=@($build.Cases | Where-Object {$_.Name -eq 'BuildEditor' -and $_.Status -eq 'Passed' -and $_.ExitCode -eq 0})
        if($build.Operation -cne 'VerifyWorld' -or $rows.Count -ne 1 -or -not $build.Details.BinaryHashes.Editor){throw [IO.FileNotFoundException]::new('构建报告未证明World正式Editor目标成功。')}
        $binary=Join-Path $context.Workspace 'Game/Binaries/Win64/UnrealEditor-DivineBeastsArena.dll'
        Assert-FoundationFile $binary
        if((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -cne $build.Details.BinaryHashes.Editor){throw [IO.FileNotFoundException]::new('当前Editor模块与构建证据不一致，需要重新构建。')}
        $editor=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'; Assert-FoundationFile $editor
        $directory=Join-Path $context.Directory 'UE'; $null=New-Item -ItemType Directory -Path $directory
        $log=Join-Path $directory 'engine.log'
        $arguments=@($context.Project,$Map,'-game','-FoundationStandalone','-FoundationWorld',"-FoundationWorldRunId=$($context.RunId)",'-CustomConfig=FoundationStandalone','-unattended','-nosplash','-nosound','-stdout','-FullStdOutLogOutput','-UTF8Output',"-abslog=$log",'-NoLiveCoding','-NoHotReload','-UDPMESSAGING_TRANSPORT_ENABLE=0','-MULTIHOME=127.0.0.1')
        if($Scenario -eq 'Partition'){$arguments+='-FoundationWorldScenario=Partition'}
        $step=Invoke-WorldObservedProcess -Context $context -Executable $editor -Arguments $arguments -Directory $directory -Log $log -Scenario $Scenario -Phase $Phase -TimeoutSeconds $TimeoutSeconds
        $steps=@($step)
        $case=New-WorldCase "Runtime$Scenario" $step.Status $step.ExitCode $step.Message @($directory)
        if($Scenario -eq 'Foundation' -and $Phase -eq 'Readiness' -and $step.ExitCode -eq 0){
            $extraCases=@(New-WorldCase FoundationReadiness Passed 0 '真实当前PID到达WorldReady；未验证跨图。' @($directory))
            $case=New-WorldCase RuntimeFoundation NotExecuted 2 'Bootstrap不自动travel；返回、清理和新代次重入尚未执行，不能虚报完整生命周期通过。' @($directory)
        }
    } catch [IO.FileNotFoundException] {$case=New-WorldCase "Runtime$Scenario" NotExecuted 2 $_.Exception.Message}
      catch {$case=New-WorldCase "Runtime$Scenario" Failed 1 $_.Exception.Message}
    return (Write-WorldReport $context (@($case)+$extraCases) $steps $details).ExitCode
}
