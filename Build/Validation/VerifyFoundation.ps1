#requires -Version 7.0
<#
.SYNOPSIS
输出Foundation逐项证据矩阵；默认只做前置核查，绝不把未执行项标为通过。
.DESCRIPTION
显式-BuildEditor/-BuildClient/-BuildServer、-CookClient/-CookServer、-RunEditor/-RunClient/-RunServer授权昂贵操作。
运行还必须-FoundationStandalone。默认FullM0；HostOnly总结果最多Incomplete。
NativeTests显式配置/编译/运行Host/Core/Data/Flow四个CMake套件；仅验证原生逻辑，不替代UE反射和适配层。
客户端/服务端优先使用本轮Cook的Stage，或显式ClientStageDirectory/ServerStageDirectory。
0=全部必需验收项通过，1=任一真实失败，2=缺少前置/验收未执行/仅HostOnly。
尚无真实证据接入口的完整验收项固定NotExecuted，因此当前即使构建/Cook/运行全通过也返回2。
外部工具退出码保存在每项结果；不使用旧日志或包头检查替代完整验收。
#>
param([string]$EngineRoot,
    [switch]$BuildEditor, [switch]$BuildClient, [switch]$BuildServer,
    [switch]$CookClient, [switch]$CookServer,
    [switch]$RunEditor, [switch]$RunClient, [switch]$RunServer, [switch]$FoundationStandalone,
    [ValidateSet('FullM0','HostOnly')][string]$Phase = 'FullM0',
    [string]$ClientStageDirectory, [string]$ServerStageDirectory, [switch]$NullRHI,
    [switch]$NativeTests, [string]$NativeGenerator = 'Visual Studio 17 2022',
    [ValidateRange(1,3600)][int]$NativeTimeoutSeconds = 120,
    [switch]$NoPCH, [switch]$NoSharedPCH = $true, [ValidateRange(1,128)][int]$MaxParallelActions = 2,
    [ValidateRange(1,86400)][int]$BuildTimeoutSeconds = 3600,
    [ValidateRange(1,86400)][int]$CookTimeoutSeconds = 1800,
    [ValidateRange(1,86400)][int]$RunTimeoutSeconds = 120,
    [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Game/FoundationTools.psm1') -Force
$context = $null
$matrix = [Collections.Generic.List[object]]::new()
function Add-Check([string]$Name,[string]$Status,[int]$Code,[string]$Evidence) {
    $matrix.Add([pscustomobject]@{ Name=$Name; Status=$Status; ExitCode=$Code; Evidence=$Evidence })
}
function Invoke-Check([string]$Name,[string]$Script,[string[]]$Arguments,[string]$Operation) {
    # 以子PowerShell调用，子脚本exit不能终止矩阵生成；逐项串行，不启动并行UE构建。
    $output = Join-Path $context.Directory "$Name.log"
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $Script @Arguments *> $output
    $toolCode = $LASTEXITCODE
    $evidence = Join-Path (Split-Path $context.Directory) "$Operation/result.json"
    if (Test-Path -LiteralPath $evidence) {
        $report = Get-Content -Raw -LiteralPath $evidence | ConvertFrom-Json
        if ($report.RunId -ne $context.RunId -or $report.ExitCode -ne $toolCode) { Add-Check $Name Failed 1 $output }
        else { Add-Check $Name $report.Status $toolCode $evidence }
    } else { Add-Check $Name Failed $toolCode $output }
}
try {
    $context = New-FoundationContext Verify $RunId
    try { $engine = Get-FoundationEngine $EngineRoot; $EngineRoot = $engine.Root; Add-Check Engine Passed 0 "$($engine.Root) UE$($engine.Version)" }
    catch [IO.FileNotFoundException] { Add-Check Engine NotExecuted 2 $_.Exception.Message }
    catch { Add-Check Engine Failed 1 $_.Exception.Message }
    $required = @('Game/DivineBeastsArena.uproject','Game/Source/DivineBeastsArenaEditor.Target.cs','Game/Source/DivineBeastsArenaClient.Target.cs','Game/Source/DivineBeastsArenaServer.Target.cs')
    foreach ($file in $required) {
        try { Assert-FoundationFile (Join-Path $context.Workspace $file); Add-Check $file Passed 0 '文件非空；不代表编译成功' }
        catch { Add-Check $file NotExecuted 2 $_.Exception.Message }
    }
    # UBT即使未启用插件也会扫描描述文件；原位报告坏JSON，绝不移动/删除或创建替代宿主。
    $descriptors = @($context.Project) + @(Get-ChildItem -LiteralPath (Join-Path $context.Workspace 'Game/Plugins') -Filter '*.uplugin' -Recurse -File | ForEach-Object FullName)
    foreach ($descriptor in $descriptors) {
        try {
            if (-not (Test-Path -LiteralPath $descriptor)) { throw [IO.FileNotFoundException]::new("缺失描述文件：$descriptor") }
            $content = Get-Content -Raw -LiteralPath $descriptor
            if ([string]::IsNullOrWhiteSpace($content)) { throw "描述文件为空：$descriptor" }
            $parsed = $content | ConvertFrom-Json
            if ($null -eq $parsed -or -not $parsed.FileVersion) { throw "描述文件缺少FileVersion：$descriptor" }
            Add-Check "Descriptor/$([IO.Path]::GetFileName($descriptor))" Passed 0 $descriptor
        } catch [IO.FileNotFoundException] { Add-Check "Descriptor/$([IO.Path]::GetFileName($descriptor))" NotExecuted 2 $_.Exception.Message }
        catch { Add-Check "Descriptor/$([IO.Path]::GetFileName($descriptor))" Failed 1 $_.Exception.Message }
    }
    # 仅检查UE包头，文本占位不得通过；Cook及FullM0运行承担真实加载证据。
    $assets = @('Maps/L_FoundationBootstrap.umap','Maps/L_FoundationSandbox.umap','Definitions/DA_FoundationProbe.uasset','Definitions/DA_FoundationFlow.uasset')
    foreach ($asset in $assets) {
        $path = Join-Path $context.Workspace "Game/Content/Development/Foundation/$asset"
        try {
            Assert-FoundationFile $path
            $stream = [IO.File]::OpenRead($path)
            try { $reader = [IO.BinaryReader]::new($stream); $magic = $reader.ReadUInt32() } finally { $stream.Dispose() }
            if ($magic -ne 0x9E2A83C1) { throw '不是合法UE包头；不得使用文本占位。' }
            Add-Check "Asset/$asset" Passed 0 '仅包头前置核查，加载证据由Cook/Run提供'
        } catch [IO.FileNotFoundException] { Add-Check "Asset/$asset" NotExecuted 2 $_.Exception.Message }
        catch { Add-Check "Asset/$asset" Failed 1 $_.Exception.Message }
    }
    foreach ($suite in @(
        @{ Name='Host'; Source='Tests/Foundation/Host' },
        @{ Name='Core'; Source='Game/Plugins/GameFoundation/Core/GamePlatformCore/Tests' },
        @{ Name='Data'; Source='Game/Plugins/GameFoundation/Core/GamePlatformData/Tests' },
        @{ Name='Flow'; Source='Game/Plugins/GameFoundation/Application/GamePlatformApplicationFlow/Tests' }
    )) {
        if (-not $NativeTests) { Add-Check "Native$($suite.Name)" NotExecuted 2 '未指定-NativeTests；不替代UE测试'; continue }
        try {
            $source = Join-Path $context.Workspace $suite.Source
            Assert-FoundationFile (Join-Path $source 'CMakeLists.txt')
            $cmake = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue
            $ctest = Get-Command ctest -CommandType Application -ErrorAction SilentlyContinue
            if (-not $cmake -or -not $ctest) { throw [IO.FileNotFoundException]::new('需要CMake与CTest。') }
            $directory = Join-Path $context.Directory "Native-$($suite.Name)"
            $buildDirectory = Join-Path $directory 'Build'
            $steps = @(
                @{ Name='Configure'; Tool=$cmake.Source; Args=@('-S',$source,'-B',$buildDirectory,'-G',$NativeGenerator) },
                @{ Name='Build'; Tool=$cmake.Source; Args=@('--build',$buildDirectory,'--config','Debug','--parallel',"$MaxParallelActions") },
                @{ Name='Test'; Tool=$ctest.Source; Args=@('--test-dir',$buildDirectory,'-C','Debug','--output-on-failure','--no-tests=error','--timeout',"$NativeTimeoutSeconds") }
            )
            $suiteCode = 0
            foreach ($step in $steps) {
                $stepDirectory = Join-Path $directory $step.Name
                $null = New-Item -ItemType Directory -Path $stepDirectory -Force
                $result = Invoke-FoundationProcess -FilePath $step.Tool -Arguments $step.Args -WorkingDirectory $source -OutputDirectory $stepDirectory -TimeoutSeconds $NativeTimeoutSeconds
                if ($result.ExitCode -ne 0) { $suiteCode = $result.ExitCode; break }
            }
            $suiteStatus = if ($suiteCode -eq 0) { 'Passed' } else { 'Failed' }
            Add-Check "Native$($suite.Name)" $suiteStatus $suiteCode $directory
        } catch [IO.FileNotFoundException] { Add-Check "Native$($suite.Name)" NotExecuted 2 $_.Exception.Message }
        catch { Add-Check "Native$($suite.Name)" Failed 1 $_.Exception.Message }
    }
    $common = @('-EngineRoot',$EngineRoot,'-RunId',$context.RunId)
    $buildTargets = @(); if ($BuildEditor) { $buildTargets += 'Editor' }; if ($BuildClient) { $buildTargets += 'Client' }; if ($BuildServer) { $buildTargets += 'Server' }
    if ($buildTargets.Count) {
        $arguments = $common + @('-MaxParallelActions',"$MaxParallelActions",'-TimeoutSeconds',"$BuildTimeoutSeconds")
        foreach ($target in $buildTargets) { $arguments += "-$target" }
        if ($NoPCH) { $arguments += '-NoPCH' }
        if (-not $NoSharedPCH) { $arguments += '-NoSharedPCH:$false' }
        Invoke-Check Build (Join-Path $PSScriptRoot '../Game/BuildFoundation.ps1') $arguments Build
        $buildReportPath = Join-Path (Split-Path $context.Directory) 'Build/result.json'
        $buildReport = if (Test-Path $buildReportPath) { Get-Content -Raw $buildReportPath | ConvertFrom-Json } else { $null }
    }
    foreach ($target in @('Editor','Client','Server')) {
        $entry = if ($buildTargets.Count -and $buildReport) { @($buildReport.Targets | Where-Object Target -eq "DivineBeastsArena$target") } else { @() }
        if ($entry.Count) {
            $status = if ($entry[0].Result.ExitCode -eq 0) { 'Passed' } else { 'Failed' }
            Add-Check "Build$target" $status $entry[0].Result.ExitCode $entry[0].Evidence
        } else { Add-Check "Build$target" NotExecuted 2 '未授权、前置缺失或前一目标失败' }
    }
    foreach ($target in @('Client','Server')) {
        $enabled = if ($target -eq 'Client') { $CookClient } else { $CookServer }
        if ($enabled) {
            Invoke-Check "Cook$target" (Join-Path $PSScriptRoot '../Game/CookFoundation.ps1') ($common + @('-Cook','-Target',$target,'-TimeoutSeconds',"$CookTimeoutSeconds")) "Cook-$target"
            $last = $matrix[$matrix.Count-1]
            if ($last.Status -eq 'Passed') {
                $cookReport = Get-Content -Raw $last.Evidence | ConvertFrom-Json
                if ($target -eq 'Client') { $ClientStageDirectory = $cookReport.StageDirectory } else { $ServerStageDirectory = $cookReport.StageDirectory }
            }
        } else { Add-Check "Cook$target" NotExecuted 2 '未指定显式Cook开关' }
    }
    foreach ($target in @('Editor','Client','Server')) {
        $enabled = switch ($target) { Editor { $RunEditor }; Client { $RunClient }; Server { $RunServer } }
        if ($enabled) {
            $arguments = $common + @('-Start','-Target',$target,'-Phase',$Phase,'-TimeoutSeconds',"$RunTimeoutSeconds")
            if ($FoundationStandalone) { $arguments += '-FoundationStandalone' }
            if ($NullRHI) { $arguments += '-NullRHI' }
            $stage = if ($target -eq 'Client') { $ClientStageDirectory } elseif ($target -eq 'Server') { $ServerStageDirectory } else { $null }
            if ($stage) { $arguments += @('-StageDirectory',$stage) }
            Invoke-Check "Run$target" (Join-Path $PSScriptRoot '../Game/RunFoundation.ps1') $arguments "Run-$target"
        } else { Add-Check "Run$target" NotExecuted 2 '未指定显式Run开关' }
    }
    # 完整验收必须逐项有本次运行的执行器及结构化证据校验，不能仅凭旧日志字符串或文件存在通过。
    # 当前未实现这些接入口，保持明确未执行；后续需实现并验证对应接入口才能替换各项状态。
    foreach ($acceptance in @(
        @{ Name='UEAutomation'; Reason='未接入本次UE自动化执行、测试计数及失败结果校验' },
        @{ Name='AssetGeneration'; Reason='未接入真实引擎资产创建与加载核验；Asset包头项仅为前置检查' },
        @{ Name='AssetRegenerationProtection'; Reason='未接入资产创建重跑及已有资产保护行为验证' },
        @{ Name='AssetNegativeValidation'; Reason='未接入缺失、冲突等资产负例的真实引擎验证' },
        @{ Name='MultiPIE'; Reason='未接入多PIE实例隔离与生命周期验证' },
        @{ Name='GraphicalValidation'; Reason='未接入真实图形与交互验证；Ready日志和NullRHI不构成此证据' },
        @{ Name='CancellationRecovery'; Reason='未接入流程取消、恢复及清理的UE集成验证' },
        @{ Name='ReleaseContentStripping'; Reason='未接入正式发行Cook/Stage及产物剥离审计；开发Cook不能替代' }
    )) { Add-Check $acceptance.Name NotExecuted 2 $acceptance.Reason }
    if ($Phase -eq 'HostOnly') { Add-Check FullM0 NotExecuted 2 'HostOnly不能证明完整M0就绪' }
    $code = if (@($matrix | Where-Object Status -eq 'Failed').Count) { 1 } elseif (@($matrix | Where-Object Status -ne 'Passed').Count) { 2 } else { 0 }
    $status = if ($code -eq 0) { 'Passed' } elseif ($code -eq 1) { 'Failed' } else { 'Incomplete' }
    $matrix | Format-Table -AutoSize | Out-Host
    Write-FoundationResult $context $code '包含完整必需验收项；无本次真实证据接入口的项目保持未执行，不能汇总全部通过。' @{ Matrix=@($matrix); Phase=$Phase } $status
    exit $code
} catch {
    if ($context) { Write-FoundationResult $context 1 $_.Exception.Message @{ Matrix=@($matrix) } Failed } else { Write-Host $_ }
    exit 1
}
