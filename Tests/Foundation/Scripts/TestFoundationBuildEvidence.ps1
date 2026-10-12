#requires -Version 7.0
<#
.SYNOPSIS
完整Editor构建证据的隔离合同回归；不会启动UE、UBT或Cook。
.DESCRIPTION
所有DLL/receipt/process均是系统临时目录中的明确合成夹具，仅验证脚本门禁。
通过不代表实际Editor构建或UE自动化成功；真实证据只能由BuildFoundation的完整目标产生。
#>
param([guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $root 'Build/Game/FoundationTools.psm1') -Force
$context = New-FoundationContext BuildEvidenceContractTests $RunId
$cases = [Collections.Generic.List[object]]::new()
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('FoundationBuildEvidence-' + [guid]::NewGuid().ToString('N'))
$code = 1; $message = ''
function Write-FixtureFile([string]$Path, [string]$Text) {
    $null = New-Item -ItemType Directory -Force -Path (Split-Path $Path)
    [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))
}
function Write-FixtureJson([string]$Path, $Value) {
    Write-FixtureFile $Path ($Value | ConvertTo-Json -Depth 30)
}
function Assert-Contract([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function New-Fixture {
    $base = Join-Path $fixtureRoot ([guid]::NewGuid().ToString('N'))
    $workspace = Join-Path $base 'workspace'; $engineRoot = Join-Path $base 'engine'
    $buildId = [guid]::NewGuid().ToString('D'); $id = [guid]::NewGuid().ToString('D')
    $directory = Join-Path $workspace "Saved/Validation/FoundationM0/$id/Build"
    $ctx = [pscustomobject]@{Workspace=$workspace;Project=(Join-Path $workspace 'Game/DivineBeastsArena.uproject');Directory=$directory;RunId=$id;Operation='Build'}
    Write-FixtureJson $ctx.Project @{FileVersion=3;Modules=@(@{Name='DivineBeastsArena';Type='Runtime'});Plugins=@()}
    foreach ($path in @('Game/Source/DivineBeastsArenaEditor.Target.cs','Game/Source/DivineBeastsArena/DivineBeastsArena.Build.cs','Game/Source/DivineBeastsArena/Test.cpp','Game/Source/DivineBeastsArena/Test.h','Game/Plugins/Fixture/Source/FixtureRuntime/FixtureRuntime.Build.cs','Game/Plugins/Fixture/Source/FixtureRuntime/Test.cpp','Game/Build/Windows/Resources.rc','Game/Config/DefaultEngine.ini')) {
        Write-FixtureFile (Join-Path $workspace $path) "fixture input: $path"
    }
    Write-FixtureJson (Join-Path $workspace 'Game/Plugins/Fixture/Fixture.uplugin') @{FileVersion=3;Modules=@(@{Name='FixtureRuntime';Type='Runtime'})}
    # Source中的描述文件只是测试输入，不能使递归再次按插件根剪除。
    foreach($path in @('Game/Source/DivineBeastsArena/Private/SourceFixture.uplugin','Game/Plugins/Fixture/Source/FixtureRuntime/Private/SourceFixture.uplugin')) {
        Write-FixtureJson (Join-Path $workspace $path) @{FileVersion=3;Modules=@()}
    }
    Write-FixtureJson (Join-Path $engineRoot 'Engine/Build/Build.version') @{MajorVersion=5;MinorVersion=8;PatchVersion=0}
    Write-FixtureFile (Join-Path $engineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll') 'SYNTHETIC UBT; NEVER EXECUTE'
    Write-FixtureJson (Join-Path $engineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.runtimeconfig.json') @{runtimeOptions=@{framework=@{name='Microsoft.NETCore.App';version='10.0.0'}}}
    Write-FixtureFile (Join-Path $engineRoot 'Engine/Binaries/ThirdParty/DotNet/10.0.100/win-x64/dotnet.exe') 'SYNTHETIC HOST; NEVER EXECUTE'
    Write-FixtureFile (Join-Path $engineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') 'SYNTHETIC EDITOR; NEVER EXECUTE'
    $mainDll = 'Game/Binaries/Win64/UnrealEditor-DivineBeastsArena.dll'
    $pluginDll = 'Game/Plugins/Fixture/Binaries/Win64/UnrealEditor-FixtureRuntime.dll'
    $mainModules = 'Game/Binaries/Win64/UnrealEditor.modules'
    $pluginModules = 'Game/Plugins/Fixture/Binaries/Win64/UnrealEditor.modules'
    Write-FixtureFile (Join-Path $workspace $mainDll) 'SYNTHETIC MAIN DLL; NEVER LOAD'
    Write-FixtureFile (Join-Path $workspace $pluginDll) 'SYNTHETIC PLUGIN DLL; NEVER LOAD'
    Write-FixtureJson (Join-Path $workspace $mainModules) @{BuildId=$buildId;Modules=@{DivineBeastsArena='UnrealEditor-DivineBeastsArena.dll'}}
    Write-FixtureJson (Join-Path $workspace $pluginModules) @{BuildId=$buildId;Modules=@{FixtureRuntime='UnrealEditor-FixtureRuntime.dll'}}
    $products = @(
        @{Path='$(ProjectDir)/Binaries/Win64/UnrealEditor-DivineBeastsArena.dll';Type='DynamicLibrary'},
        @{Path='$(ProjectDir)/Binaries/Win64/UnrealEditor.modules';Type='RequiredResource'},
        @{Path='$(ProjectDir)/Plugins/Fixture/Binaries/Win64/UnrealEditor-FixtureRuntime.dll';Type='DynamicLibrary'},
        @{Path='$(ProjectDir)/Plugins/Fixture/Binaries/Win64/UnrealEditor.modules';Type='RequiredResource'},
        @{Path='$(EngineDir)/Binaries/Win64/UnrealEditor-Core.dll';Type='DynamicLibrary'})
    $receipt = @{TargetName='DivineBeastsArenaEditor';TargetType='Editor';Platform='Win64';Configuration='Development';Project='../../DivineBeastsArena.uproject';Version=@{BuildId=$buildId};BuildProducts=$products}
    $receiptPath = Join-Path $workspace 'Game/Binaries/Win64/DivineBeastsArenaEditor.target'
    Write-FixtureJson $receiptPath $receipt
    $editorDirectory = Join-Path $directory 'Editor'
    $before = Get-FoundationCompileInputInventory -Workspace $workspace
    $beforeRef = Save-FoundationCompileInputInventory -Workspace $workspace -Inventory $before -Path (Join-Path $editorDirectory 'CompileInputs.before.json')
    $started = [DateTime]::UtcNow
    $processResult = [pscustomobject]@{ExitCode=0;ProcessId=123;StartTimeTicks=$started.Ticks;Ready=$false;TimedOut=$false;Cleaned=$true;DurationSeconds=0.01}
    $engine = Get-FoundationEngine $engineRoot
    $tool = Get-FoundationManagedTool $engine UnrealBuildTool
    $process = @{ProcessId=123;StartTimeUtc=$started.ToString('O');FilePath=$tool.Executable;Arguments=@($tool.Dll,'DivineBeastsArenaEditor','Win64','Development',"-Project=$($ctx.Project)",'-WaitMutex')}
    Write-FixtureJson (Join-Path $editorDirectory 'process.json') $process
    $after = Get-FoundationCompileInputInventory -Workspace $workspace
    $afterRef = Save-FoundationCompileInputInventory -Workspace $workspace -Inventory $after -Path (Join-Path $editorDirectory 'CompileInputs.after.json')
    $proofArgs = @{Context=$ctx;Engine=$engine;ProcessResult=$processResult;Before=$before;After=$after;BeforeFile=$beforeRef;AfterFile=$afterRef;OnlyModules=@()}
    $proof = New-FoundationEditorBuildEvidence @proofArgs
    $build = @{Operation='Build';RunId=$id;Workspace=$workspace;Status='Passed';ExitCode=0;EngineRoot=$engineRoot;Configuration='Development';Platform='Win64';OnlyModules=@();Targets=@(@{Target='DivineBeastsArenaEditor';Result=$processResult;Evidence=$editorDirectory});EditorBuildEvidence=$proof;FixtureOnly=$true}
    $resultPath = Join-Path $directory 'result.json'
    Write-FixtureJson $resultPath $build
    [pscustomobject]@{Workspace=$workspace;Engine=$engine;Context=$ctx;Build=$build;ResultPath=$resultPath;ProofArgs=$proofArgs;Receipt=$receipt;ReceiptPath=$receiptPath;PluginDll=(Join-Path $workspace $pluginDll);PluginModules=(Join-Path $workspace $pluginModules);Process=$process;ProcessPath=(Join-Path $editorDirectory 'process.json')}
}
function Invoke-FixtureProof($Fixture) {
    $arguments=$Fixture.ProofArgs
    New-FoundationEditorBuildEvidence @arguments
}
function Assert-Fixture($Fixture, [string[]]$Modules=@('FixtureRuntime')) {
    Assert-FoundationEditorBuildResult -Workspace $Fixture.Workspace -Engine $Fixture.Engine -BuildResultPath $Fixture.ResultPath -RequiredModules $Modules
}
function Assert-RunnerPreflight($Fixture,[string[]]$Arguments,[int]$ExpectedCode,[string]$ExpectedStatus,[switch]$RemoveBuildGate) {
    # 生产入口和模块复制进隔离工作区；只把UE调用点替换成显式哨兵，不进入系统exe加载流程。
    foreach($relative in @('Build/Game/FoundationTools.psm1','Build/Game/FoundationBuildEvidence.psm1','Build/Validation/TestPluginArchitectureUnreal.ps1','Build/Validation/FoundationAutomationReport.psm1')) {
        Write-FixtureFile (Join-Path $Fixture.Workspace $relative) ([IO.File]::ReadAllText((Join-Path $root $relative)))
    }
    $runnerPath=Join-Path $Fixture.Workspace 'Build/Validation/TestPluginArchitectureUnreal.ps1'
    $original=[IO.File]::ReadAllText($runnerPath)
    $invokePattern='(?m)^[ \t]*\$result = Invoke-FoundationProcess -FilePath \$editor[^\r\n]*'
    Assert-Contract ([regex]::Matches($original,$invokePattern).Count -eq 1) '未找到唯一的UE调用边界'
    $guarded=[regex]::Replace($original,$invokePattern,"        throw 'FoundationFixtureUEBoundaryReached'")
    if ($RemoveBuildGate) {
        # 只删除临时副本的前置门禁；保留生产Suite创建路径，验证目录断言能识别跨过门禁。
        $mutated=[regex]::Replace($guarded,'(?m)^[ \t]*\$details\.BuildEvidence = Assert-FoundationEditorBuildResult[^\r\n]*(?:\r?\n|$)','')
        Assert-Contract ($mutated -cne $guarded) '未找到可变异的构建门禁'
        $guarded=$mutated
    }
    Write-FixtureFile $runnerPath $guarded
    $id=[guid]::NewGuid().ToString('D')
    $directory=Join-Path $Fixture.Workspace "Saved/Validation/FoundationM0/$id/PluginArchitectureTests"
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=(Get-Process -Id $PID).Path
    $start.WorkingDirectory=$Fixture.Workspace; $start.UseShellExecute=$false
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    foreach($argument in @('-NoProfile','-File',(Join-Path $Fixture.Workspace 'Build/Validation/TestPluginArchitectureUnreal.ps1'),'-EngineRoot',$Fixture.Engine.Root,'-RunId',$id)+$Arguments) { $start.ArgumentList.Add($argument) }
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$start
    try {
        $null=$process.Start(); $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) { $process.Kill(); throw '隔离runner前置回归超时；只终止本次PowerShell。' }
        $text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        Assert-Contract ($process.ExitCode -eq $ExpectedCode) "runner退出码预期$ExpectedCode，实际$($process.ExitCode)：$text"
    } finally { $process.Dispose() }
    $result=Get-Content -LiteralPath (Join-Path $directory 'result.json') -Raw | ConvertFrom-Json
    if ($RemoveBuildGate) { Assert-Contract ($result.Message -ceq 'FoundationFixtureUEBoundaryReached') "变异未到达预期UE调用点：$($result.Message)" }
    Assert-Contract ($result.Status -ceq $ExpectedStatus -and @($result.Suites).Count -eq 0) "runner未在UE前返回$ExpectedStatus"
    Assert-Contract (@(Get-ChildItem -LiteralPath $directory -Directory).Count -eq 0) '门禁前不应创建Suite目录'
    Assert-Contract (@(Get-ChildItem -LiteralPath $directory -Filter process.json -Recurse -File).Count -eq 0) '门禁前不应创建UE进程'
}
function Case([string]$Name, [scriptblock]$Body, [switch]$Reject) {
    $fixture = New-Fixture
    $caught = $null
    try { & $Body $fixture } catch { $caught = $_ }
    if ($Reject -and $null -eq $caught) { throw "应拒绝却通过：$Name" }
    if (-not $Reject -and $null -ne $caught) { throw $caught }
    $cases.Add(@{Name=$Name;Status='Passed';Rejected=[bool]$Reject;Message=$(if($caught){$caught.Exception.Message}else{'contract accepted'})})
    Write-Host "PASS $Name"
}
try {
    Import-Module (Join-Path $root 'Build/Game/FoundationBuildEvidence.psm1') -Force
    Case 'FullEditorReceiptContractAccepted' { param($f) $verified=Assert-Fixture $f; Assert-Contract ($verified.BuildRunId -eq $f.Context.RunId) 'Build RunId未关联' }
    Case 'WorkspaceRelativePathIgnoresCallerDirectory' { param($f)
        $relative=[IO.Path]::GetRelativePath($f.Workspace,$f.ResultPath)
        Push-Location ([IO.Path]::GetTempPath())
        try { $null=Assert-FoundationEditorBuildResult -Workspace $f.Workspace -Engine $f.Engine -BuildResultPath $relative -RequiredModules FixtureRuntime } finally { Pop-Location }
    }
    Case 'CompileInputScopeIncludesCppHeadersRulesDescriptorsConfig' { param($f)
        $paths=@($f.ProofArgs.Before.Entries.Path)
        foreach($suffix in @('Test.cpp','Test.h','DivineBeastsArena.Build.cs','DivineBeastsArenaEditor.Target.cs','Fixture.uplugin','DivineBeastsArena.uproject','DefaultEngine.ini','Resources.rc')) {
            Assert-Contract (@($paths | Where-Object { $_.EndsWith($suffix,[StringComparison]::Ordinal) }).Count -gt 0) "清单遗漏$suffix"
        }
    }
    Case 'GeneratedAndAssetChangesExcluded' { param($f)
        foreach($path in @('Game/Content/Fixture.txt','Game/Intermediate/Generated.h','Game/Plugins/Fixture/Intermediate/Generated.h','Game/Saved/Log.txt','Game/DerivedDataCache/Cache.txt')) { Write-FixtureFile (Join-Path $f.Workspace $path) 'excluded' }
        $null=Assert-Fixture $f
    }
    Case 'OnlyModulesResultRejected' -Reject { param($f) $f.Build.OnlyModules=@('FixtureRuntime'); Write-FixtureJson $f.ResultPath $f.Build; $null=Assert-Fixture $f }
    Case 'OnlyModulesCannotCreateProof' -Reject { param($f) $f.ProofArgs.OnlyModules=@('FixtureRuntime'); $null=Invoke-FixtureProof $f }
    Case 'ModuleSwitchInActualArgumentsRejected' -Reject { param($f) $f.Process.Arguments+='-Module=FixtureRuntime'; Write-FixtureJson $f.ProcessPath $f.Process; $null=Invoke-FixtureProof $f }
    Case 'NonzeroProcessCannotCreateProof' -Reject { param($f) $f.ProofArgs.ProcessResult.ExitCode=6; $null=Invoke-FixtureProof $f }
    Case 'TimeoutCannotCreateProof' -Reject { param($f) $f.ProofArgs.ProcessResult.TimedOut=$true; $null=Invoke-FixtureProof $f }
    Case 'SourceChangesDuringBuildRejected' -Reject { param($f)
        Write-FixtureFile (Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/Test.cpp') 'changed during build'
        $f.ProofArgs.After=Get-FoundationCompileInputInventory -Workspace $f.Workspace
        $null=Invoke-FixtureProof $f
    }
    Case 'CurrentCppChangedWithSameTestNamesRejected' -Reject { param($f) Write-FixtureFile (Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/Test.cpp') 'same test names; changed implementation'; $null=Assert-Fixture $f }
    Case 'NewSourceFileRejected' -Reject { param($f) Write-FixtureFile (Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/New.cpp') 'new input'; $null=Assert-Fixture $f }
    Case 'ChangedPluginDescriptorRejected' -Reject { param($f) Write-FixtureJson (Join-Path $f.Workspace 'Game/Plugins/Fixture/Fixture.uplugin') @{FileVersion=3;EnabledByDefault=$false}; $null=Assert-Fixture $f }
    Case 'SameNameDllChangedRejected' -Reject { param($f) Write-FixtureFile $f.PluginDll 'SYNTHETIC DIFFERENT DLL'; $null=Assert-Fixture $f }
    Case 'MissingProjectDllRejected' -Reject { param($f) Remove-Item -LiteralPath $f.PluginDll; $null=Assert-Fixture $f }
    Case 'UnreceiptedModuleMappingRejected' -Reject { param($f)
        $map=Get-Content -Raw $f.PluginModules | ConvertFrom-Json -AsHashtable
        $map.Modules.FixtureRuntime='UnrealEditor-Stale.dll'; Write-FixtureJson $f.PluginModules $map
        $null=Assert-Fixture $f
    }
    Case 'ReceiptChangedRejected' -Reject { param($f) $f.Receipt.Configuration='DebugGame'; Write-FixtureJson $f.ReceiptPath $f.Receipt; $null=Assert-Fixture $f }
    Case 'RequiredSuiteModuleMissingRejected' -Reject { param($f) $null=Assert-Fixture $f @('GamePlatformData') }
    Case 'WrongWorkspaceRejected' -Reject { param($f) $null=Assert-FoundationEditorBuildResult -Workspace $root -Engine $f.Engine -BuildResultPath $f.ResultPath -RequiredModules FixtureRuntime }
    Case 'WrongEngineRejected' -Reject { param($f) $other=New-Fixture; $null=Assert-FoundationEditorBuildResult -Workspace $f.Workspace -Engine $other.Engine -BuildResultPath $f.ResultPath -RequiredModules FixtureRuntime }
    Case 'FailedBuildResultRejected' -Reject { param($f) $f.Build.Status='Failed'; Write-FixtureJson $f.ResultPath $f.Build; $null=Assert-Fixture $f }
    Case 'LegacyBuildResultRejected' -Reject { param($f) $f.Build.Remove('EditorBuildEvidence'); Write-FixtureJson $f.ResultPath $f.Build; $null=Assert-Fixture $f }
    Case 'MalformedBuildResultRejected' -Reject { param($f) Write-FixtureFile $f.ResultPath '{ broken'; $null=Assert-Fixture $f }
    Case 'EngineIdentityChangedRejected' -Reject { param($f) Write-FixtureFile (Join-Path $f.Engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') 'SYNTHETIC DIFFERENT ENGINE'; $null=Assert-Fixture $f }
    Case 'MissingEvidenceIsNotExecuted' { param($f)
        Remove-Item -LiteralPath $f.ResultPath
        $found=$false
        try { $null=Assert-Fixture $f } catch [IO.FileNotFoundException] { $found=$true }
        Assert-Contract $found '缺少证据须返回未执行类别'
    }
    Case 'CompleteBatchFailureCannotUseSuccessfulEditor' -Reject { param($f)
        $f.Build.Targets+=@{Target='DivineBeastsArenaClient';Result=@{ExitCode=6}}
        Write-FixtureJson $f.ResultPath $f.Build; $null=Assert-Fixture $f
    }
    Case 'TimestampChangeDuringBuildRejected' -Reject { param($f)
        $path=Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/Test.cpp'
        [IO.File]::SetLastWriteTimeUtc($path,([IO.File]::GetLastWriteTimeUtc($path)).AddSeconds(1))
        $f.ProofArgs.After=Get-FoundationCompileInputInventory -Workspace $f.Workspace
        $null=Invoke-FixtureProof $f
    }
    Case 'MissingModulesManifestRejected' -Reject { param($f) Remove-Item -LiteralPath $f.PluginModules; $null=Assert-Fixture $f }
    Case 'ExternalPluginInputsRejected' -Reject { param($f)
        $path=Join-Path $f.Workspace 'Game/DivineBeastsArena.uproject'
        Write-FixtureJson $path @{FileVersion=3;AdditionalPluginDirectories=@('../../ExternalPlugin')}
        $null=Get-FoundationCompileInputInventory -Workspace $f.Workspace
    }
    Case 'RunnerWithoutExecuteIsNotExecuted' { param($f) Assert-RunnerPreflight $f @() 2 'NotExecuted' }
    Case 'RunnerMissingBuildParameterIsNotExecuted' { param($f) Assert-RunnerPreflight $f @('-Execute') 2 'NotExecuted' }
    Case 'RunnerMissingBuildFileIsNotExecuted' { param($f)
        Remove-Item -LiteralPath $f.ResultPath
        Assert-RunnerPreflight $f @('-Execute','-BuildResultPath',$f.ResultPath) 2 'NotExecuted'
    }
    Case 'RunnerStaleSourceFailsBeforeUE' { param($f)
        Write-FixtureFile (Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/Test.cpp') 'stale input'
        Assert-RunnerPreflight $f @('-Execute','-BuildResultPath',$f.ResultPath) 1 'Failed'
    }
    Case 'RunnerOnlyModulesFailsBeforeUE' { param($f)
        $f.Build.OnlyModules=@('FixtureRuntime'); Write-FixtureJson $f.ResultPath $f.Build
        Assert-RunnerPreflight $f @('-Execute','-BuildResultPath',$f.ResultPath) 1 'Failed'
    }
    Case 'NestedSourceCppAddedDuringBuildRejected' -Reject { param($f)
        Write-FixtureFile (Join-Path $f.Workspace 'Game/Source/DivineBeastsArena/Private/Content/AddedDuringBuild.cpp') 'new compile input during build'
        $f.ProofArgs.After=Get-FoundationCompileInputInventory -Workspace $f.Workspace
        $null=Invoke-FixtureProof $f
    }
    Case 'AllSourceDirectoryNamesIncludeNewAndModifiedFiles' { param($f)
        foreach($sourceRoot in @('Game/Source/DivineBeastsArena/Private','Game/Plugins/Fixture/Source/FixtureRuntime/Private')) {
            foreach($name in @('Binaries','Intermediate','Saved','DerivedDataCache','Content','SourceArt','Docs','.git','.vs','.idea')) {
                $relative="$sourceRoot/$name/CompileInputProbe.cpp"
                $path=Join-Path $f.Workspace $relative
                Write-FixtureFile $path 'new source'
                $added=Get-FoundationCompileInputInventory -Workspace $f.Workspace
                Assert-Contract (@($added.Entries.Path) -ccontains $relative) "合法Source文件被目录名剪除：$relative"
                $rejected=$false
                try { $null=Assert-Fixture $f } catch [InvalidOperationException] { $rejected=$true }
                Assert-Contract $rejected "新增Source文件没有使门禁失败：$relative"
                Write-FixtureFile $path 'modified source'
                $modified=Get-FoundationCompileInputInventory -Workspace $f.Workspace
                Assert-Contract ($modified.SHA256 -cne $added.SHA256) "Source修改未进入哈希：$relative"
                Remove-Item -LiteralPath $path
            }
        }
    }
    Case 'PluginRootGeneratedAndAssetDirectoriesRemainExcluded' { param($f)
        foreach($name in @('Binaries','Intermediate','Saved','DerivedDataCache','Content','SourceArt','.git','.vs','.idea')) {
            Write-FixtureFile (Join-Path $f.Workspace "Game/Plugins/Fixture/$name/ExcludedProbe.cpp") 'plugin root asset or generated fixture'
        }
        $null=Assert-Fixture $f
    }
    Case 'RunnerHarnessDetectsRemovedBuildGate' { param($f)
        $caught=$null
        try { Assert-RunnerPreflight $f @('-Execute','-BuildResultPath',$f.ResultPath) 1 'Failed' -RemoveBuildGate } catch { $caught=$_ }
        $actual=if($null -eq $caught){'<none>'}else{$caught.Exception.Message}
        Assert-Contract ($null -ne $caught -and $actual -ceq '门禁前不应创建Suite目录') "runner回归未识别构建门禁被删除；实际异常：$actual"
    }
    $code=0; $message="$($cases.Count)个隔离合同用例通过；无真实构建或UE启动。"
} catch { $message=$_.Exception.Message; Write-Host $_.ScriptStackTrace }
finally {
    if (Test-Path -LiteralPath $fixtureRoot) { Remove-Item -LiteralPath $fixtureRoot -Recurse -Force }
}
Write-FoundationResult $context $code $message @{Cases=@($cases.ToArray());FixturesOnly=$true;ActualBuildExecuted=$false;UEExecuted=$false;PowerShellVersion=$PSVersionTable.PSVersion.ToString();RuntimeVersion=[Environment]::Version.ToString()}
exit $code
