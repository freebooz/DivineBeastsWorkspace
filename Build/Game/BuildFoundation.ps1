#requires -Version 7.0
<#
.SYNOPSIS
构建Foundation正式Editor/Client/Server目标；必须显式指定目标开关。
.DESCRIPTION
EngineRoot或UE_ROOT指定UE5.8根。默认Development/Win64，最大并行动作2，禁共享PCH；
NoPCH额外禁用所有PCH。未选目标/缺少前置返回2；脚本失败1；外部非零码原样传播。
证据写入Saved/Validation/FoundationM0/<RunId>/Build，不清理任何旧产物。
持有Foundation锁后仍等待UBT全局互斥锁，兼容同一锁定引擎上的其他合法项目构建；
等待计入TimeoutSeconds，超时只清理本次拥有的进程，不中止其他构建。
UsePrecompiled显式复用锁定引擎的现有二进制依赖，项目模块仍真实编译/链接；
该模式不验证引擎源码重新构建，缺失或不兼容的预编译产物仍由UBT/链接器报错。
完整Editor成功后记录构建前后源码清单、真实receipt的项目DLL/.modules和引擎工具身份；
期间输入变化或证据不完整将失败。OnlyModules不具备UE测试入口的完整构建资格。
#>
param([string]$EngineRoot, [switch]$Editor, [switch]$Client, [switch]$Server,
    [switch]$NoPCH, [switch]$NoSharedPCH = $true,
    # 只复用已存在的引擎二进制；默认false保留原引擎源码构建工作流，不关闭项目模块。
    [switch]$UsePrecompiled,
    # UE5.8仍由UBA调度，显式关闭进程拦截以排查编译器内部崩溃；不改编译版本、优化或模块装配。
    [switch]$NoUBA,
    # 仅本次构建额外装配的已有插件，供插件完整编译验收；不改正式目标默认装配。
    [ValidatePattern("^[A-Za-z][A-Za-z0-9_]*$")][string[]]$AdditionalPlugins = @(),
    # 非空时构建所列模块及必要链接依赖；记录为模块构建，不能宣称整目标/Cook完成。
    [ValidatePattern("^[A-Za-z][A-Za-z0-9_]*$")][string[]]$OnlyModules = @(),
    [ValidateRange(1,128)][int]$MaxParallelActions = 2,
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 3600,
    [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1') -Force
$context = $null; $lock = $null; $code = 2; $message = ''; $details = @{ Targets=@(); Configuration='Development'; Platform='Win64'; AdditionalPlugins=$AdditionalPlugins; OnlyModules=$OnlyModules; UsePrecompiled=[bool]$UsePrecompiled; NoUBA=[bool]$NoUBA }
try {
    $context = New-FoundationContext Build $RunId
    $details.Project = $context.Project
    $details.EditorBuildEvidenceStatus = 'NoSuccessfulFullEditorTarget'
    $targets = @(); if ($Editor) { $targets += 'Editor' }; if ($Client) { $targets += 'Client' }; if ($Server) { $targets += 'Server' }
    if (-not $targets.Count) { throw [IO.FileNotFoundException]::new('需显式指定-Editor/-Client/-Server。') }
    $engine = Get-FoundationEngine $EngineRoot
    $details.EngineVersion = $engine.Version; $details.EngineRoot = $engine.Root
    Assert-FoundationFile $context.Project
    $null = Get-Content -Raw $context.Project | ConvertFrom-Json
    foreach ($target in $targets) { Assert-FoundationFile (Join-Path $context.Workspace "Game/Source/DivineBeastsArena$target.Target.cs") }
    $tool = Get-FoundationManagedTool $engine UnrealBuildTool
    $lock = Enter-FoundationEngineLock $engine.Root
    $code = 0
    foreach ($target in $targets) {
        $directory = Join-Path $context.Directory $target
        $null = New-Item -ItemType Directory -Path $directory
        # Foundation锁只能协调本入口；UBT锁覆盖编辑器和直接工具调用，避免抢锁导致误报失败。
        $arguments = @($tool.Dll,"DivineBeastsArena$target",'Win64','Development',"-Project=$($context.Project)","-MaxParallelActions=$MaxParallelActions",'-NoHotReloadFromIDE','-WaitMutex',"-Log=$(Join-Path $directory 'UBT.log')")
        if ($AdditionalPlugins.Count) { $arguments += ('-EnablePlugin=' + (($AdditionalPlugins | Sort-Object -Unique) -join '+')) }
        if ($OnlyModules.Count) { $arguments += ('-Module=' + (($OnlyModules | Sort-Object -Unique) -join '+')) }
        if ($NoPCH) { $arguments += '-NoPCH' }
        if ($NoSharedPCH) { $arguments += '-NoSharedPCH' }
        if ($UsePrecompiled) { $arguments += '-UsePrecompiled' }
        if ($NoUBA) { $arguments += '-NoUBA' }
        $fullEditor = $target -eq 'Editor' -and $OnlyModules.Count -eq 0
        if ($fullEditor) {
            # 快照在真实进程之前完成；失败只留下不合格的诊断清单，不允许事后给旧DLL补证。
            $before = Get-FoundationCompileInputInventory -Workspace $context.Workspace
            $beforeFile = Save-FoundationCompileInputInventory -Workspace $context.Workspace -Inventory $before -Path (Join-Path $directory 'CompileInputs.before.json')
            $details.EditorBuildEvidenceStatus = 'Incomplete'
        } elseif ($target -eq 'Editor') { $details.EditorBuildEvidenceStatus = 'NotEligibleOnlyModules' }
        $result = Invoke-FoundationProcess -FilePath $tool.Executable -Arguments $arguments -WorkingDirectory (Join-Path $engine.Root 'Engine/Source') -OutputDirectory $directory -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment
        $details.Targets += @{ Target="DivineBeastsArena$target"; Result=$result; Evidence=$directory }
        if ($result.ExitCode -ne 0) { $code = $result.ExitCode; break }
        if ($fullEditor) {
            try {
                $after = Get-FoundationCompileInputInventory -Workspace $context.Workspace
                $afterFile = Save-FoundationCompileInputInventory -Workspace $context.Workspace -Inventory $after -Path (Join-Path $directory 'CompileInputs.after.json')
                $details.EditorBuildEvidence = New-FoundationEditorBuildEvidence -Context $context -Engine $engine -ProcessResult $result -Before $before -After $after -BeforeFile $beforeFile -AfterFile $afterFile -OnlyModules $OnlyModules
                $details.EditorBuildEvidenceStatus = 'Eligible'
            } catch { throw [InvalidOperationException]::new("完整Editor目标返回0，但构建证据生成失败：$($_.Exception.Message)",$_.Exception) }
        }
    }
    $message = '所选目标已顺序执行；后续目标在首次失败时停止。'
} catch [IO.FileNotFoundException] { $code = 2; $message = $_.Exception.Message }
catch { $code = 1; $message = $_.Exception.Message }
finally { if ($null -ne $lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
$status = if (@($details.Targets | Where-Object { $_.Result.ExitCode -ne 0 }).Count) { 'Failed' } else { '' }
if ($context) { Write-FoundationResult $context $code $message $details $status } else { Write-Host $message }
exit $code
