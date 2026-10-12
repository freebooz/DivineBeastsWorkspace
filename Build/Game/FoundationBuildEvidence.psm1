#requires -Version 7.0
<#
职责：把本工程完整Editor目标的真实进程结果、构建输入端点快照和receipt产物绑定为机器证据。
本模块只同步读写证据，不启动工具、不持有引擎锁、不管理进程；调用者负责构建/测试生命周期。
范围：Game/Source、Plugins、Config、Build及uproject；排除生成目录和Content。引擎源码、SDK、
外部插件与资产不属于此源码证明；另记录引擎版本/UBT/宿主/Editor命令程序身份。证据不是签名。
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1')
$script:InputScope = 'FoundationProjectCompileInputs-v2'
$script:ExcludedPluginRootDirectories = @('Binaries','Intermediate','Saved','DerivedDataCache','Content','SourceArt','.git','.vs','.idea')

function Get-FBEPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw '证据路径不能为空。' }
    [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
}
function Test-FBEUnder([string]$Path,[string]$Root) {
    $Path.Equals($Root,[StringComparison]::OrdinalIgnoreCase) -or $Path.StartsWith($Root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)
}
function Resolve-FBEPath([string]$Root,[string]$Path) {
    $rootPath=Get-FBEPath $Root
    $full=Get-FBEPath $(if([IO.Path]::IsPathFullyQualified($Path)){$Path}else{Join-Path $rootPath $Path})
    if (-not (Test-FBEUnder $full $rootPath)) { throw "证据路径超出当前根目录：$Path" }
    $cursor=$full
    while (Test-FBEUnder $cursor $rootPath) {
        if (Test-Path -LiteralPath $cursor) {
            if (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "证据范围不接受重解析点：$cursor" }
        }
        if ($cursor.Equals($rootPath,[StringComparison]::OrdinalIgnoreCase)) { break }
        $cursor=Split-Path $cursor -Parent
    }
    $full
}
function Get-FBEFile([string]$Root,[string]$Path,[switch]$Nonempty) {
    $full=Resolve-FBEPath $Root $Path
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw [InvalidOperationException]::new("证据文件缺失：$full") }
    $info=Get-Item -LiteralPath $full
    $length=$info.Length; $ticks=$info.LastWriteTimeUtc.Ticks
    if ($Nonempty -and $length -eq 0) { throw "证据文件为空：$full" }
    $stream=[IO.File]::Open($full,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $hasher=[Security.Cryptography.SHA256]::Create()
    try { $hash=[BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','') } finally { $hasher.Dispose(); $stream.Dispose() }
    $info.Refresh()
    if ($info.Length -ne $length -or $info.LastWriteTimeUtc.Ticks -ne $ticks) { throw "读取期间文件变化：$full" }
    [pscustomobject]@{Path=[IO.Path]::GetRelativePath((Get-FBEPath $Root),$full).Replace('\','/');Length=$length;SHA256=$hash;LastWriteTimeUtcTicks=$ticks}
}
function Get-FBEHash($Entries,[string]$Scope) {
    $byPath=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($entry in @($Entries)) {
        if ([string]$entry.Path -match '[\r\n\t]' -or [string]::IsNullOrWhiteSpace($entry.Path) -or $entry.SHA256 -cnotmatch '^[A-F0-9]{64}$' -or [long]$entry.Length -lt 0) { throw '非法文件指纹。' }
        if (-not $byPath.TryAdd([string]$entry.Path,$entry)) { throw "重复文件路径：$($entry.Path)" }
    }
    [string[]]$paths=@($byPath.Keys); [Array]::Sort($paths,[StringComparer]::Ordinal)
    $text=[Text.StringBuilder]::new(); [void]$text.Append($Scope).Append([char]10)
    foreach($path in $paths) { $e=$byPath[$path]; [void]$text.Append($path).Append([char]9).Append([long]$e.Length).Append([char]9).Append($e.SHA256).Append([char]10) }
    $hasher=[Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($text.ToString()))).Replace('-','') } finally { $hasher.Dispose() }
}
function Read-FBEJson([string]$Path) {
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -Depth 100
}
function Assert-FBEInventory($Inventory,[string]$Workspace) {
    if ($Inventory.SchemaVersion -ne 1 -or $Inventory.Scope -cne $script:InputScope -or (Get-FBEPath $Inventory.Workspace) -ine (Get-FBEPath $Workspace) -or $Inventory.FileCount -ne @($Inventory.Entries).Count -or $Inventory.FileCount -lt 1) { throw '源码清单身份、范围或数量不匹配。' }
    if ((Get-FBEHash $Inventory.Entries $script:InputScope) -cne $Inventory.SHA256) { throw '源码清单聚合哈希不匹配。' }
    foreach($entry in $Inventory.Entries) { $null=Resolve-FBEPath $Workspace $entry.Path }
}
function Assert-FBEStableInputs($Before,$After,[string]$Workspace) {
    Assert-FBEInventory $Before $Workspace; Assert-FBEInventory $After $Workspace
    if ($Before.SHA256 -cne $After.SHA256) { throw '完整Editor构建期间源码输入发生变化，拒绝产生合格证据。' }
    $times=@{}; foreach($entry in $Before.Entries){$times[$entry.Path]=[long]$entry.LastWriteTimeUtcTicks}
    foreach($entry in $After.Entries) {
        if ($times[$entry.Path] -ne [long]$entry.LastWriteTimeUtcTicks) { throw "完整Editor构建期间输入时间戳变化：$($entry.Path)" }
    }
}

function Get-FoundationCompileInputInventory {
    <# 同步端点清单；每个文件读取时拒绝写入，构建前后比较内容和时间戳。不会监控或锁住整个源码树。 #>
    param([Parameter(Mandatory)][string]$Workspace)
    $workspacePath=Get-FBEPath $Workspace
    $project=Resolve-FBEPath $workspacePath 'Game/DivineBeastsArena.uproject'
    $descriptor=Read-FBEJson $project
    foreach($externalKey in @('AdditionalPluginDirectories','AdditionalRootDirectories')) {
        if (@($descriptor[$externalKey] | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }).Count) { throw "构建证据尚不覆盖$externalKey；需扩展范围后重建，不能忽略外部输入。" }
    }
    $entries=[Collections.Generic.List[object]]::new()
    $entries.Add((Get-FBEFile $workspacePath $project -Nonempty))
    $pending=[Collections.Generic.Stack[object]]::new()
    foreach($relative in @('Game/Source','Game/Plugins','Game/Config','Game/Build')) {
        $directory=Resolve-FBEPath $workspacePath $relative
        if (Test-Path -LiteralPath $directory -PathType Container) {
            $pending.Push([pscustomobject]@{Path=$directory;PluginTree=($relative -eq 'Game/Plugins');SourceTree=($relative -eq 'Game/Source')})
        }
    }
    while($pending.Count) {
        $node=$pending.Pop()
        $children=@(Get-ChildItem -LiteralPath $node.Path -Force)
        # 只在真实插件根剪除资产/生成目录。进入Source后永久完整递归，测试用.uplugin也不能重新触发剪除。
        $pluginRoot=$node.PluginTree -and -not $node.SourceTree -and @($children | Where-Object { -not $_.PSIsContainer -and $_.Extension -ieq '.uplugin' }).Count -gt 0
        foreach($item in $children) {
            if ($item.PSIsContainer -and $pluginRoot -and $item.Name -in $script:ExcludedPluginRootDirectories) { continue }
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "源码范围不接受重解析点：$($item.FullName)" }
            if ($item.PSIsContainer) {
                $pending.Push([pscustomobject]@{Path=$item.FullName;PluginTree=$node.PluginTree;SourceTree=($node.SourceTree -or ($pluginRoot -and $item.Name -ieq 'Source'))})
            } else { $entries.Add((Get-FBEFile $workspacePath $item.FullName)) }
        }
    }
    $array=@($entries.ToArray() | Sort-Object Path -CaseSensitive)
    [pscustomobject]@{SchemaVersion=1;Scope=$script:InputScope;Workspace=$workspacePath;CapturedAtUtc=[DateTime]::UtcNow.ToString('O');FileCount=$array.Count;SHA256=(Get-FBEHash $array $script:InputScope);Entries=$array;Roots=@('Game/DivineBeastsArena.uproject','Game/Source','Game/Plugins','Game/Config','Game/Build');ExcludedPluginRootDirectoryNames=$script:ExcludedPluginRootDirectories;ExclusionBoundary='Plugin roots only; Source subtrees, project Config and Build are fully traversed.';Limitations='Endpoint project-source inventory; excludes project/plugin asset and generated output roots, engine source, SDK and external input roots.'}
}
function Save-FoundationCompileInputInventory {
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)]$Inventory,[Parameter(Mandatory)][string]$Path)
    Assert-FBEInventory $Inventory $Workspace
    $full=Resolve-FBEPath $Workspace $Path
    $null=New-Item -ItemType Directory -Force -Path (Split-Path $full)
    $stream=[IO.File]::Open($full,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try {
        $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Inventory | ConvertTo-Json -Depth 8))
        $stream.Write($bytes,0,$bytes.Length)
    } finally { $stream.Dispose() }
    Get-FBEFile $Workspace $full -Nonempty
}
function Read-FBEBoundInventory($Reference,[string]$Workspace,[string]$ExpectedPath) {
    $expected=Resolve-FBEPath $Workspace $ExpectedPath
    if ((Resolve-FBEPath $Workspace $Reference.Path) -ine $expected) { throw '源码快照必须绑定本次Editor证据目录。' }
    $actual=Get-FBEFile $Workspace $expected -Nonempty
    if ($actual.SHA256 -cne $Reference.SHA256 -or $actual.Length -ne $Reference.Length) { throw '源码快照文件哈希不匹配。' }
    $inventory=Read-FBEJson $expected
    Assert-FBEInventory $inventory $Workspace
    $inventory
}
function Get-FBEEngineIdentity($Engine) {
    $tool=Get-FoundationManagedTool $Engine UnrealBuildTool
    $files=@(
        (Get-FBEFile $Engine.Root 'Engine/Build/Build.version' -Nonempty),
        (Get-FBEFile $Engine.Root $tool.Dll -Nonempty),
        (Get-FBEFile $Engine.Root ([IO.Path]::ChangeExtension($tool.Dll,'runtimeconfig.json')) -Nonempty),
        (Get-FBEFile $Engine.Root $tool.Executable -Nonempty),
        (Get-FBEFile $Engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -Nonempty))
    [pscustomobject]@{Root=(Get-FBEPath $Engine.Root);Version=$Engine.Version;Files=$files;SHA256=(Get-FBEHash $files 'FoundationEngineIdentity-v1')}
}
function Get-FBEEditorProducts([string]$Workspace,$Engine) {
    $projectDirectory=Resolve-FBEPath $Workspace 'Game'
    $engineDirectory=Resolve-FBEPath $Engine.Root 'Engine'
    $receiptPath=Resolve-FBEPath $Workspace 'Game/Binaries/Win64/DivineBeastsArenaEditor.target'
    $receiptRecord=Get-FBEFile $Workspace $receiptPath -Nonempty
    $receipt=Read-FBEJson $receiptPath
    if ($receipt.TargetName -cne 'DivineBeastsArenaEditor' -or $receipt.TargetType -cne 'Editor' -or $receipt.Platform -cne 'Win64' -or $receipt.Configuration -cne 'Development') { throw 'receipt不是本工程Development/Win64完整Editor目标。' }
    if ([string]::IsNullOrWhiteSpace($receipt.Project)) { throw 'receipt缺少Project。' }
    $receiptProject=Get-FBEPath $(if([IO.Path]::IsPathFullyQualified($receipt.Project)){$receipt.Project}else{Join-Path (Split-Path $receiptPath) $receipt.Project})
    if ($receiptProject -ine (Resolve-FBEPath $Workspace 'Game/DivineBeastsArena.uproject')) { throw 'receipt指向其他工程。' }
    $buildId=[string]$receipt.Version.BuildId
    if ([string]::IsNullOrWhiteSpace($buildId)) { throw 'receipt缺少BuildId。' }
    $files=[Collections.Generic.List[object]]::new(); $files.Add($receiptRecord)
    $dlls=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $manifests=[Collections.Generic.List[string]]::new()
    foreach($product in $receipt.BuildProducts) {
        $path=[string]$product.Path
        if ($path.StartsWith('$(ProjectDir)',[StringComparison]::Ordinal)) { $path=$projectDirectory+$path.Substring(13) }
        elseif ($path.StartsWith('$(EngineDir)',[StringComparison]::Ordinal)) { $path=$engineDirectory+$path.Substring(12) }
        elseif (-not [IO.Path]::IsPathFullyQualified($path)) { throw "receipt包含不支持的产物路径：$path" }
        $path=Get-FBEPath $path
        if (Test-FBEUnder $path $engineDirectory) { continue }
        if (-not (Test-FBEUnder $path $projectDirectory)) { throw "receipt产物超出项目和引擎：$path" }
        if ($product.Type -ceq 'DynamicLibrary') {
            if ([IO.Path]::GetExtension($path) -ine '.dll') { throw "Win64项目动态库不是DLL：$path" }
            $record=Get-FBEFile $Workspace $path -Nonempty
            if (-not $dlls.TryAdd($path,$record)) { throw "receipt项目DLL重复：$path" }
            $files.Add($record)
        } elseif ([IO.Path]::GetExtension($path) -ieq '.modules') {
            if ($product.Type -cne 'RequiredResource') { throw 'receipt模块清单类型不匹配。' }
            $manifests.Add($path); $files.Add((Get-FBEFile $Workspace $path -Nonempty))
        }
    }
    if ($dlls.Count -eq 0 -or $manifests.Count -eq 0) { throw 'receipt没有项目DLL或.modules。' }
    $modules=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    $mapped=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($path in $manifests) {
        $manifest=Read-FBEJson $path
        if ($manifest.BuildId -cne $buildId -or $manifest.Modules -isnot [Collections.IDictionary]) { throw "模块清单BuildId/Modules不匹配：$path" }
        foreach($name in $manifest.Modules.Keys) {
            $dll=[string]$manifest.Modules[$name]
            if ($name -cnotmatch '^[A-Za-z][A-Za-z0-9_]*$' -or $dll -cne [IO.Path]::GetFileName($dll)) { throw '模块名称或DLL映射非法。' }
            $full=Resolve-FBEPath $Workspace (Join-Path (Split-Path $path) $dll)
            if (-not $dlls.ContainsKey($full) -or -not $modules.TryAdd($name,$dlls[$full].Path)) { throw "模块映射重复或DLL未出现在receipt：$name" }
            $null=$mapped.Add($full)
        }
    }
    if ($mapped.Count -ne $dlls.Count -or -not $modules.ContainsKey('DivineBeastsArena')) { throw 'receipt项目DLL未全部映射或缺少主工程模块。' }
    [pscustomobject]@{BuildId=$buildId;Receipt=$receiptRecord;Files=$files.ToArray();SHA256=(Get-FBEHash $files.ToArray() 'FoundationEditorProducts-v1');Modules=$modules}
}
function Assert-FBEFullEditorProcess($ProcessResult,[string]$ProcessPath,[string]$Workspace,$Engine) {
    if ($ProcessResult.ExitCode -ne 0 -or $ProcessResult.ProcessId -le 0 -or $ProcessResult.StartTimeTicks -le 0 -or $ProcessResult.TimedOut -ne $false -or $ProcessResult.Cleaned -ne $true) { throw '需要实际成功、未超时且已退出的完整Editor构建进程。' }
    $record=Get-FBEFile $Workspace $ProcessPath -Nonempty
    $process=Read-FBEJson (Resolve-FBEPath $Workspace $record.Path)
    $tool=Get-FoundationManagedTool $Engine UnrealBuildTool
    $arguments=@($process.Arguments)
    if ($process.ProcessId -ne $ProcessResult.ProcessId -or (([DateTime]$process.StartTimeUtc).ToUniversalTime().Ticks) -ne $ProcessResult.StartTimeTicks -or (Get-FBEPath $process.FilePath) -ine (Get-FBEPath $tool.Executable)) { throw '构建进程身份与实际process.json不匹配。' }
    if ($arguments.Count -lt 5 -or (Get-FBEPath $arguments[0]) -ine (Get-FBEPath $tool.Dll) -or $arguments[1] -cne 'DivineBeastsArenaEditor' -or $arguments[2] -cne 'Win64' -or $arguments[3] -cne 'Development') { throw 'process.json不是完整Editor目标参数。' }
    if (@($arguments | Where-Object { $_ -match '^-Module(?:=|$)|^-SingleFile(?:=|$)|^-SpecificFiles(?:=|$)' }).Count) { throw '局部模块或单文件构建不能生成完整Editor证据。' }
    $projectArguments=@($arguments | Where-Object { $_ -match '^-Project=' })
    if ($projectArguments.Count -ne 1 -or (Get-FBEPath $projectArguments[0].Substring(9)) -ine (Resolve-FBEPath $Workspace 'Game/DivineBeastsArena.uproject')) { throw '构建参数工程路径不匹配。' }
    $record
}
function New-FoundationEditorBuildEvidence {
    <# 只在调用者完成真实整目标后调用；OnlyModules或期间变化拒绝资格，不把旧receipt当成构建成功。 #>
    param([Parameter(Mandatory)]$Context,[Parameter(Mandatory)]$Engine,[Parameter(Mandatory)]$ProcessResult,
        [Parameter(Mandatory)]$Before,[Parameter(Mandatory)]$After,[Parameter(Mandatory)]$BeforeFile,[Parameter(Mandatory)]$AfterFile,[string[]]$OnlyModules=@())
    if ($OnlyModules.Count) { throw 'OnlyModules不能生成完整Editor构建证据。' }
    $directory=Resolve-FBEPath $Context.Workspace "Saved/Validation/FoundationM0/$($Context.RunId)/Build/Editor"
    if ((Get-FBEPath $Context.Directory) -ine (Split-Path $directory)) { throw '构建上下文目录不匹配。' }
    Assert-FBEStableInputs $Before $After $Context.Workspace
    $savedBefore=Read-FBEBoundInventory $BeforeFile $Context.Workspace (Join-Path $directory 'CompileInputs.before.json')
    $savedAfter=Read-FBEBoundInventory $AfterFile $Context.Workspace (Join-Path $directory 'CompileInputs.after.json')
    Assert-FBEStableInputs $savedBefore $savedAfter $Context.Workspace
    if ($Before.SHA256 -cne $savedBefore.SHA256 -or $After.SHA256 -cne $savedAfter.SHA256) { throw '内存与落盘输入快照不匹配。' }
    $process=Assert-FBEFullEditorProcess $ProcessResult (Join-Path $directory 'process.json') $Context.Workspace $Engine
    if (([DateTime]$savedBefore.CapturedAtUtc).ToUniversalTime().Ticks -gt $ProcessResult.StartTimeTicks -or ([DateTime]$savedAfter.CapturedAtUtc).ToUniversalTime().Ticks -lt $ProcessResult.StartTimeTicks) { throw '输入快照没有包围实际构建进程。' }
    $products=Get-FBEEditorProducts $Context.Workspace $Engine
    [pscustomobject]@{SchemaVersion=1;Kind='SuccessfulFullEditorTarget';RunId=$Context.RunId;Workspace=(Get-FBEPath $Context.Workspace);Project=(Resolve-FBEPath $Context.Workspace 'Game/DivineBeastsArena.uproject');EngineRoot=(Get-FBEPath $Engine.Root);Target='DivineBeastsArenaEditor';Platform='Win64';Configuration='Development';Before=$BeforeFile;After=$AfterFile;InputSHA256=$After.SHA256;InputFileCount=$After.FileCount;Process=$process;Products=$products;EngineIdentity=(Get-FBEEngineIdentity $Engine);Scope=$script:InputScope;Limitations='Unsigned local build evidence; successful full incremental target is eligible. Source endpoint snapshots exclude assets, generated output, engine source, SDK and external roots; no claim that every DLL was relinked.'}
}
function Assert-FoundationEditorBuildResult {
    <# 入口门禁：缺少参数/结果文件为NotExecuted；已提供但错误、陈旧或不合格的证据为Failed。
       在启动UE前同步验证；成功返回可写入测试result的关联摘要，不启动或停止任何进程。 #>
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)]$Engine,[string]$BuildResultPath,[string[]]$RequiredModules=@())
    if ([string]::IsNullOrWhiteSpace($BuildResultPath)) { throw [IO.FileNotFoundException]::new('Execute需要-BuildResultPath指向本工作区成功完整Editor构建结果。') }
    $workspacePath=Get-FBEPath $Workspace
    $resultPath=Resolve-FBEPath $workspacePath $BuildResultPath
    if (-not (Test-Path -LiteralPath $resultPath -PathType Leaf)) { throw [IO.FileNotFoundException]::new("尚无完整Editor构建结果：$resultPath") }
    try {
        $resultRecord=Get-FBEFile $workspacePath $resultPath -Nonempty
        $build=Read-FBEJson $resultPath
        $id=[guid]::Parse($build.RunId)
        $expected=Resolve-FBEPath $workspacePath "Saved/Validation/FoundationM0/$($id.ToString('D'))/Build/result.json"
        if ($id -eq [guid]::Empty -or $resultPath -ine $expected -or $build.Operation -cne 'Build' -or $build.Status -cne 'Passed' -or $build.ExitCode -ne 0 -or $build.Configuration -cne 'Development' -or $build.Platform -cne 'Win64' -or (Get-FBEPath $build.Workspace) -ine $workspacePath -or (Get-FBEPath $build.EngineRoot) -ine (Get-FBEPath $Engine.Root)) { throw 'Build结果必须是本工作区、同一引擎的成功Development/Win64完整构建。' }
        if (-not $build.Contains('OnlyModules') -or @($build.OnlyModules).Count -ne 0 -or @($build.Targets | Where-Object { $_.Result.ExitCode -ne 0 }).Count) { throw 'OnlyModules或失败目标不能满足完整Editor构建门禁。' }
        $targets=@($build.Targets | Where-Object { $_.Target -ceq 'DivineBeastsArenaEditor' })
        if ($targets.Count -ne 1 -or -not $build.Contains('EditorBuildEvidence')) { throw '缺少唯一完整Editor目标或新构建证据；旧结果不能补证。' }
        $proof=$build.EditorBuildEvidence
        if ($proof.SchemaVersion -ne 1 -or $proof.Kind -cne 'SuccessfulFullEditorTarget' -or $proof.RunId -cne $id.ToString('D') -or $proof.Scope -cne $script:InputScope -or (Get-FBEPath $proof.Workspace) -ine $workspacePath -or (Get-FBEPath $proof.EngineRoot) -ine (Get-FBEPath $Engine.Root) -or (Get-FBEPath $proof.Project) -ine (Resolve-FBEPath $workspacePath 'Game/DivineBeastsArena.uproject') -or $proof.Target -cne 'DivineBeastsArenaEditor' -or $proof.Platform -cne 'Win64' -or $proof.Configuration -cne 'Development') { throw 'Editor构建证据身份不匹配。' }
        $directory=Join-Path (Split-Path $resultPath) 'Editor'
        if ((Get-FBEPath $targets[0].Evidence) -ine (Get-FBEPath $directory)) { throw '目标证据目录不匹配。' }
        $before=Read-FBEBoundInventory $proof.Before $workspacePath (Join-Path $directory 'CompileInputs.before.json')
        $after=Read-FBEBoundInventory $proof.After $workspacePath (Join-Path $directory 'CompileInputs.after.json')
        Assert-FBEStableInputs $before $after $workspacePath
        $process=Assert-FBEFullEditorProcess $targets[0].Result (Join-Path $directory 'process.json') $workspacePath $Engine
        if ($process.Path -cne $proof.Process.Path -or $process.SHA256 -cne $proof.Process.SHA256) { throw '构建process.json已变化。' }
        if (([DateTime]$before.CapturedAtUtc).ToUniversalTime().Ticks -gt $targets[0].Result.StartTimeTicks -or ([DateTime]$after.CapturedAtUtc).ToUniversalTime().Ticks -lt $targets[0].Result.StartTimeTicks) { throw '源码快照时间未包围构建。' }
        $current=Get-FoundationCompileInputInventory $workspacePath
        if ($current.SHA256 -cne $after.SHA256 -or $proof.InputSHA256 -cne $after.SHA256 -or $proof.InputFileCount -ne $after.FileCount) { throw '当前源码输入与完整Editor构建不一致；需重新完成完整Editor目标。' }
        $products=Get-FBEEditorProducts $workspacePath $Engine
        if ((Get-FBEHash $proof.Products.Files 'FoundationEditorProducts-v1') -cne $proof.Products.SHA256 -or $products.SHA256 -cne $proof.Products.SHA256 -or $products.BuildId -cne $proof.Products.BuildId) { throw '当前receipt、DLL或.modules与完整Editor构建不一致。' }
        foreach($name in $RequiredModules) { if (-not $products.Modules.ContainsKey($name)) { throw "本次完整Editor receipt缺少测试所属模块：$name" } }
        $identity=Get-FBEEngineIdentity $Engine
        if ((Get-FBEHash $proof.EngineIdentity.Files 'FoundationEngineIdentity-v1') -cne $proof.EngineIdentity.SHA256 -or $identity.SHA256 -cne $proof.EngineIdentity.SHA256) { throw '当前引擎工具或Editor身份与构建不一致。' }
        # 结果文件在门禁期间变化也拒绝；单个文件指纹读取拒绝写入，调用者在测试结束再复核。
        $finalRecord=Get-FBEFile $workspacePath $resultPath -Nonempty
        if ($finalRecord.SHA256 -cne $resultRecord.SHA256) { throw '验证期间Build结果变化。' }
        [pscustomobject]@{BuildRunId=$id.ToString('D');BuildResultPath=$resultPath;BuildResultSHA256=$resultRecord.SHA256;InputSHA256=$current.SHA256;InputFileCount=$current.FileCount;ArtifactSHA256=$products.SHA256;ProjectDllCount=@($products.Files | Where-Object { $_.Path.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase) }).Count;VerifiedModules=@($RequiredModules);Scope=$script:InputScope}
    } catch { throw [InvalidOperationException]::new("完整Editor构建证据拒绝：$($_.Exception.Message)",$_.Exception) }
}
Export-ModuleMember -Function Get-FoundationCompileInputInventory,Save-FoundationCompileInputInventory,New-FoundationEditorBuildEvidence,Assert-FoundationEditorBuildResult
