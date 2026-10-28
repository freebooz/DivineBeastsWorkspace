#requires -Version 7.0
<# SSLM开发演示机器证据：工具层，供启动入口/证据提升入口消费，不启动UE或构建、不签人工结论。
   真实源码/产物资格仍归FoundationBuildEvidence；本模块负责运行来源、内容端点及固定根内追加文件。
   所有摘要都是无签名本地证据，不证明恶意外部输入可信；失败不覆盖原始索引、测试或历史报告。 #>
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'PluginDemoValidation.psm1')

function Resolve-PluginDemoEvidencePath {
    <# 同步只读解析；允许根内绝对路径或规范相对路径，拒绝空段/点段/逃逸及所有祖先重解析点。 #>
    param([string]$Root,[string]$Path)
    if ([string]::IsNullOrWhiteSpace($Root) -or [string]::IsNullOrWhiteSpace($Path)) { throw '证据根/路径不能为空。' }
    $base=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    if (-not [IO.Path]::IsPathFullyQualified($Path)) {
        if ($Path -match '\\|:|^/' -or @($Path.Split('/') | Where-Object { $_ -in @('','.','..') }).Count) { throw '证据相对路径不是规范路径。' }
        $Path=Join-Path $base $Path
    }
    $full=[IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($base+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw '证据路径越过固定根。' }
    for ($cursor=$full; -not [string]::IsNullOrEmpty($cursor); $cursor=Split-Path $cursor -Parent) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw '证据路径包含junction/符号链接。' }
    }
    $full
}
function Get-PluginDemoEvidenceFile {
    <# 流式计算实际文件SHA256，读取期间拒绝写入，并比较长度/时间戳；不把声明摘要当实际摘要。
       MaxBytes默认64MiB仅为外部证据读取保护，内容快照可显式使用更大上限，不是性能预算。 #>
    param([string]$Root,[string]$Path,[long]$MaxBytes=67108864,[switch]$AllowEmpty)
    $full=Resolve-PluginDemoEvidencePath $Root $Path
    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "实际证据缺失：$Path" }
    $info=Get-Item -LiteralPath $full; $size=$info.Length; $ticks=$info.LastWriteTimeUtc.Ticks
    if (($size -eq 0 -and -not $AllowEmpty) -or $size -gt $MaxBytes) { throw '证据为空或超过读取保护上限。' }
    $stream=[IO.File]::Open($full,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $hasher=[Security.Cryptography.SHA256]::Create()
    try { $hash=[Convert]::ToHexString($hasher.ComputeHash($stream)).ToLowerInvariant() } finally { $hasher.Dispose(); $stream.Dispose() }
    $info.Refresh()
    if ($size -ne $info.Length -or $ticks -ne $info.LastWriteTimeUtc.Ticks) { throw '证据读取期间变化。' }
    [pscustomobject]@{Path=[IO.Path]::GetRelativePath([IO.Path]::GetFullPath($Root),$full).Replace('\','/');Length=$size;SHA256=$hash;LastWriteTimeUtcTicks=$ticks}
}
function Read-PluginDemoEvidenceJson {
    <# 有界JSON对象读取；递归拒绝重复属性，先后摘要必须相同，文件不能在解析时变化。 #>
    param([string]$Root,[string]$Path,[long]$MaxBytes=4194304)
    $before=Get-PluginDemoEvidenceFile $Root $Path $MaxBytes
    $text=Get-Content -LiteralPath (Resolve-PluginDemoEvidencePath $Root $Path) -Raw
    $options=[Text.Json.JsonDocumentOptions]::new(); $options.MaxDepth=100
    $parsed=[Text.Json.JsonDocument]::Parse([string]$text,$options)
    try { Assert-PDEUniqueJsonProperties $parsed.RootElement } finally { $parsed.Dispose() }
    $value=$text | ConvertFrom-Json -AsHashtable -Depth 100
    if ($value -isnot [Collections.IDictionary]) { throw '证据不是JSON对象。' }
    $after=Get-PluginDemoEvidenceFile $Root $Path $MaxBytes
    if ($before.SHA256 -cne $after.SHA256) { throw 'JSON读取期间变化。' }
    [pscustomobject]@{Value=$value;File=$after}
}
function Assert-PDEUniqueJsonProperties($Element) {
    # PowerShell/UE不同JSON消费者对重复键的保留行为不同；必须在转换为字典前拒绝歧义材料。
    if ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object) {
        $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach($property in $Element.EnumerateObject()) {
            if (-not $names.Add($property.Name)) { throw "JSON重复属性：$($property.Name)" }
            Assert-PDEUniqueJsonProperties $property.Value
        }
    } elseif ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array) {
        foreach($child in $Element.EnumerateArray()) { Assert-PDEUniqueJsonProperties $child }
    }
}
function Save-PluginDemoEvidenceJson {
    <# 固定根内追加新修订；CreateNew与独占写入拒绝覆盖，调用者只在全部验证通过后发布最终索引。 #>
    param([string]$Root,[string]$Relative,$Value)
    $full=Resolve-PluginDemoEvidencePath $Root $Relative
    $null=New-Item -ItemType Directory -Path (Split-Path $full) -Force
    $full=Resolve-PluginDemoEvidencePath $Root $Relative
    $stream=[IO.File]::Open($full,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 40)); $stream.Write($bytes,0,$bytes.Length) }
    finally { $stream.Dispose() }
    Get-PluginDemoEvidenceFile $Root $Relative
}
function Test-PDEIntegerNumber($Value,[double]$Minimum,[double]$Maximum) {
    # JSON数字在PowerShell可为Int64/Double等；仅接受有限且精确的整数，不做字符串/布尔隐式转换。
    # Maximum由字段定义决定，2^53-1仅用于UE JSON的计数精确边界，不是运行预算。
    if ($Value -isnot [byte] -and $Value -isnot [sbyte] -and $Value -isnot [int16] -and $Value -isnot [uint16] -and
        $Value -isnot [int32] -and $Value -isnot [uint32] -and $Value -isnot [int64] -and $Value -isnot [uint64] -and
        $Value -isnot [single] -and $Value -isnot [double] -and $Value -isnot [decimal]) { return $false }
    $number=[double]$Value
    [double]::IsFinite($number) -and $number -ge $Minimum -and $number -le $Maximum -and [math]::Floor($number) -eq $number
}
function Get-PluginDemoRuntimeModuleNames {
    <# EditorGame通过-game运行，只要求四个真实运行模块；Type=Editor审核模块由独立Review测试证明。
       此分离不降低完整Editor收据/源码门禁，也不让Client依赖或强载Editor模块。 #>
    @('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient')
}
function Assert-PluginDemoPromotionContract {
    <# 纯值合同；输入必须由调用者从实际不可变材料读取。机器验证通过只允许待审，不得写审核人。
       运行PID/启动身份、实际加载模块、内容端点和完整原生测试分别约束，旧raw不能靠新Git/DLL补证。 #>
    param($Raw,$CurrentBuild,$RecordedBuild,$Launch,[string]$ContentSHA256,[string]$ManifestSHA256,$Cases,$NativeProcess)
    foreach ($hash in @($ContentSHA256,$ManifestSHA256)) { if ($hash -cnotmatch '^[a-f0-9]{64}$') { throw '源/内容摘要缺失或不规范。' } }
    foreach ($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) {
        if ([string]$CurrentBuild.$field -cnotmatch '^[A-Fa-f0-9]{64}$' -or
            [string]$CurrentBuild.$field -cne [string]$RecordedBuild.$field -or
            [string]$CurrentBuild.$field -cne [string]$Launch.BuildEvidence.$field) { throw "运行/测试/当前构建不一致：$field" }
    }
    $run=[guid]::Empty; $launchId=[guid]::Empty
    if (-not [guid]::TryParseExact([string]$Raw.run_id,'D',[ref]$run) -or $run -eq [guid]::Empty -or
        -not [guid]::TryParseExact([string]$Launch.RunId,'D',[ref]$launchId) -or $launchId -eq [guid]::Empty -or
        -not (Test-PDEIntegerNumber $Raw.schema_version 1 1) -or $Raw.binary_verified -isnot [bool] -or $Raw.binary_verified -ne $false -or $Raw.source_fingerprint -cne $ManifestSHA256 -or
        $Launch.ManifestSHA256 -cne $ManifestSHA256 -or $Launch.ContentSHA256 -cne $ContentSHA256 -or
        -not (Test-PDEIntegerNumber $Raw.process_id 1 4294967295) -or -not (Test-PDEIntegerNumber $Launch.PID 1 4294967295) -or
        $Raw.process_id -ne $Launch.PID) { throw '原始运行/启动/作者身份不匹配。' }
    $origin=$Raw.runtime_origin
    if ($origin.launch_id -cne $Launch.RunId -or $origin.content_fingerprint -cne $ContentSHA256 -or
        $origin.no_live_coding -isnot [bool] -or $origin.no_live_coding -ne $true -or
        $origin.live_coding_state_verified -isnot [bool] -or $origin.live_coding_state_verified -ne $true -or
        $origin.hot_reload_state_verified -isnot [bool] -or $origin.hot_reload_state_verified -ne $true -or
        $origin.no_hot_reload -isnot [bool] -or $origin.no_hot_reload -ne $true) { throw '运行缺受控启动来源、内容端点或热重载隔离。' }
    $observations=@{}; foreach($module in $origin.loaded_modules) {
        if ($observations.ContainsKey($module.name)) { throw '重复加载模块观察。' }; $observations[$module.name]=$module
    }
    if (-not $CurrentBuild.VerifiedModuleFiles.Count) { throw '完整构建未给出必需模块实际文件。' }
    foreach($name in Get-PluginDemoRuntimeModuleNames) {
        if (-not $CurrentBuild.VerifiedModuleFiles.Contains($name)) { throw "完整构建缺运行必需模块实际文件：$name" }
        $expected=$CurrentBuild.VerifiedModuleFiles[$name]
        if (-not $observations.ContainsKey($name) -or $observations[$name].path -cne $expected.Path -or
            $observations[$name].sha256 -cne $expected.SHA256.ToLowerInvariant()) { throw "运行实际模块与测试二进制不匹配：$name" }
    }
    $state=$Raw.raw_state
    $phase=$state.run_state; $commands=$state.completed_operations
    if ($Raw.runtime_completed -isnot [bool] -or $Raw.runtime_completed -ne $true -or $state.run_id -cne $Raw.run_id -or $state.scenario_id -cne $Raw.scenario_id -or
        $state.feature_id -cne $Raw.feature_id -or -not (Test-PDEIntegerNumber $phase 4 6) -or
        -not (Test-PDEIntegerNumber $commands 1 9007199254740991) -or
        -not (Test-PDEIntegerNumber $state.cleanup_result.status 1 1) -or $state.cleanup_result.code -cne 'None' -or
        -not (Test-PDEIntegerNumber $state.terminal_result.status 1 4) -or
        ($phase -eq 4 -and $state.terminal_result.status -ne 1) -or
        ($phase -eq 5 -and $state.terminal_result.status -notin @(2,4)) -or
        ($phase -eq 6 -and $state.terminal_result.status -ne 3)) { throw '原始运行没有可验证的完成操作/终态/成功清理事实。' }
    if ($NativeProcess.TimedOut -isnot [bool] -or $NativeProcess.Cleaned -isnot [bool] -or
        -not (Test-PDEIntegerNumber $NativeProcess.ExitCode 0 0) -or -not (Test-PDEIntegerNumber $NativeProcess.ProcessId 1 4294967295)) {
        throw '原生进程事实的字段类型不合法。'
    }
    $count=Assert-PluginDemoRunResult -ProcessResult $NativeProcess -Cases @($Cases) -Before $RecordedBuild -After $CurrentBuild -Suite Demo
    $count
}
function Get-PluginDemoContentSnapshot {
    <# 同步流式端点快照，保守覆盖项目Content、全部项目插件Content、引擎Content及引擎插件Content。
       不扫描SourceArt/缓存、不查询资源业务语义；包含外置Actor/对象的所有实际文件。
       此范围较宽，会使无关内容变动也撤销资格；成本须实测，不能声称它已优化或证明运行时动态资源。
       不写资产；链接、读取期间变化或缺必需根失败。前后快照不是整个内容树的持续锁。 #>
    param([string]$Workspace,[string]$EngineRoot)
    $roots=[Collections.Generic.List[object]]::new()
    foreach($group in @(@{Root=$Workspace;Prefix='project';Content='Game/Content';Plugins='Game/Plugins'},@{Root=$EngineRoot;Prefix='engine';Content='Engine/Content';Plugins='Engine/Plugins'})) {
        $content=Resolve-PluginDemoEvidencePath $group.Root $group.Content
        $plugins=Resolve-PluginDemoEvidencePath $group.Root $group.Plugins
        if (-not (Test-Path -LiteralPath $content -PathType Container) -or -not (Test-Path -LiteralPath $plugins -PathType Container)) { throw '内容快照必需根缺失。' }
        $roots.Add(@{Root=$group.Root;Prefix=$group.Prefix;Directory=$content})
        foreach($descriptor in Get-ChildItem -LiteralPath $plugins -Recurse -File -Filter '*.uplugin') {
            $null=Get-PluginDemoEvidenceFile $group.Root $descriptor.FullName
            $directory=Join-Path $descriptor.DirectoryName 'Content'
            if (Test-Path -LiteralPath $directory -PathType Container) {
                $null=Resolve-PluginDemoEvidencePath $group.Root $directory
                $roots.Add(@{Root=$group.Root;Prefix=$group.Prefix;Directory=$directory})
            }
        }
    }
    $entries=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($root in $roots) {
        foreach($item in Get-ChildItem -LiteralPath $root.Directory -Recurse -Force) {
            $null=Resolve-PluginDemoEvidencePath $root.Root $item.FullName
            if ($item.PSIsContainer) { continue }
            $file=Get-PluginDemoEvidenceFile -Root $root.Root -Path $item.FullName -MaxBytes ([long]::MaxValue) -AllowEmpty
            $key=$root.Prefix+'/'+$file.Path
            if (-not $entries.TryAdd($key,@{Path=$key;Length=$file.Length;SHA256=$file.SHA256;LastWriteTimeUtcTicks=$file.LastWriteTimeUtcTicks})) { throw '内容快照重复路径/嵌套插件根。' }
        }
    }
    if ($entries.Count -eq 0) { throw '没有实际内容文件，不能生成版本证明。' }
    [string[]]$paths=@($entries.Keys); [Array]::Sort($paths,[StringComparer]::Ordinal)
    $canonical=[Text.StringBuilder]::new(); [void]$canonical.Append("PluginDemoContentRoots-v1`n")
    $ordered=@(foreach($path in $paths) {
        if ($path -match '[\r\n\t]') { throw '内容文件身份含控制字符。' }
        $entry=$entries[$path]; [void]$canonical.Append($path).Append([char]9).Append([long]$entry.Length).Append([char]9).Append($entry.SHA256).Append([char]10); $entry
    })
    $digest=[Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($canonical.ToString()))
    [pscustomobject]@{SchemaVersion=1;Scope='PluginDemoContentRoots-v1';CapturedUtc=[DateTime]::UtcNow.ToString('O');SHA256=[Convert]::ToHexString($digest).ToLowerInvariant();FileCount=$ordered.Count;Entries=$ordered;Limitations='Unsigned endpoint snapshot of all project/engine and plugin Content roots; excludes external non-content resources and does not hold a tree-wide lock.'}
}
function Read-PluginDemoNativeReport {
    <# 只读本次独占测试目录；校验实际process/index/结果身份和前后证明。无UE进程创建能力。
       返回实际用例与文件引用，供证据提升消费；不接受VerifyOnly、外部目录或只有Passed文字的摘要。 #>
    param([string]$Workspace,[string]$EngineRoot,[string]$ResultPath,[ValidateSet('Demo','Review','Presentation','PresentationTheme','PresentationLive','WorldLive','WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive')][string]$Suite)
    $read=Read-PluginDemoEvidenceJson $Workspace $ResultPath
    $result=$read.Value; $id=[guid]::ParseExact([string]$result.RunId,'D')
    $directory="Saved/Validation/FoundationM0/$($id.ToString('D'))/PluginDemoTests"
    if ($id -eq [guid]::Empty -or $read.File.Path -cne "$directory/result.json" -or
        $result.Operation -cne 'PluginDemoTests' -or $result.Suite -cne $Suite -or $result.Status -cne 'Passed' -or
        -not (Test-PDEIntegerNumber $result.ExitCode 0 0) -or $result.UEExecuted -isnot [bool] -or $result.UEExecuted -ne $true -or
        $result.BuildExecuted -isnot [bool] -or $result.BuildExecuted -ne $false -or
        -not (Test-PDEIntegerNumber $result.ValidatedTests 1 9007199254740991) -or @($result.Suites).Count -ne 1) { throw '原生报告不是本次已实跑成功组。' }
    $entry=$result.Suites[0]; $policy=Get-PluginDemoSuitePolicy $Suite
    if ($entry.Name -cne $Suite -or [IO.Path]::GetFullPath($entry.Evidence) -ine (Join-Path $Workspace "$directory/$Suite")) { throw '原生测试目录/组不匹配。' }
    if (-not (Test-PDEIntegerNumber $entry.Result.ProcessId 1 4294967295) -or -not (Test-PDEIntegerNumber $entry.Result.ExitCode 0 0) -or
        $entry.Result.TimedOut -isnot [bool] -or $entry.Result.Cleaned -isnot [bool]) { throw '测试进程结果字段不是合法数字/布尔事实。' }
    $process=Read-PluginDemoEvidenceJson $Workspace "$directory/$Suite/process.json"
    $arguments=@($process.Value.Arguments)
    # 报告目录必须由此进程唯一指定；仅固定目录内有JSON或摘要相等不足以关联本次process。
    $exports=@($arguments | Where-Object { $_ -clike '-ReportExportPath=*' })
    if ($exports.Count -ne 1 -or [IO.Path]::GetFullPath($exports[0].Substring('-ReportExportPath='.Length)) -ine
        [IO.Path]::GetFullPath((Join-Path $Workspace "$directory/$Suite/Automation"))) { throw '原生进程缺唯一实际报告输出目录，或输出指向其他运行。' }
    if (-not (Test-PDEIntegerNumber $process.Value.ProcessId 1 4294967295) -or $process.Value.ProcessId -ne $entry.Result.ProcessId -or
        ([DateTime]$process.Value.StartTimeUtc).ToUniversalTime().Ticks -ne $entry.Result.StartTimeTicks -or
        [IO.Path]::GetFullPath($process.Value.FilePath) -ine (Join-Path $EngineRoot $(if($Suite -in @('PresentationLive','WorldLive','WorldHostLive','WorldAuthoringLive','EvidenceHashLive')){'Engine/Binaries/Win64/UnrealEditor.exe'}else{'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'})) -or
        $arguments[0] -ine (Join-Path $Workspace 'Game/DivineBeastsArena.uproject') -or
        @($arguments | Where-Object { $_ -clike '-ExecCmds=*' }).Count -ne 1 -or
        ("-ExecCmds=Automation RunTests "+(Get-PluginDemoAutomationFilter $Suite)) -cnotin $arguments) { throw '实际原生测试进程身份/过滤器不匹配。' }
    if($Suite -in @('PresentationLive','WorldLive','WorldHostLive','WorldAuthoringLive','EvidenceHashLive')) {
        $nativeFlag=Get-PluginDemoNativeFlag $Suite
        foreach($flag in @('-game','-DBAPluginDemo','-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub','-GamePlatformManualFixture',$nativeFlag,'-RenderOffscreen')) {
            if($flag -cnotin $arguments){throw 'Live组缺真正Standalone/显式离线夹具参数。'}
        }
        if($arguments[1] -cne '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub' -or '-nullrhi' -cin $arguments -or $result.ExecutionHost -cne 'EditorGameStandaloneClientContext'){throw 'Live组宿主/地图不匹配。'}
        $userDirs=@($arguments | Where-Object { $_ -clike '-UserDir=*' })
        $expectedFixture=Join-Path $Workspace "Saved/Validation/PluginManualUI20261027/Hosts/$($id.ToString('D'))"
        if($userDirs.Count -ne 1 -or
            [IO.Path]::GetFullPath($userDirs[0].Substring('-UserDir='.Length)) -ine $expectedFixture -or
            [IO.Path]::GetFullPath([string]$result.ManualFixtureRoot) -ine $expectedFixture){throw 'Live组缺少本Run工程内独占UserDir身份。'}
    }
    if($Suite -eq 'WorldHostLive'){
        foreach($flag in @('-FoundationWorld','-DBAManualWorldHost')){if($flag -cnotin $arguments){throw '专用Host组缺完整显式初始化选择。'}}
    } elseif($Suite -in @('WorldLive','WorldAuthoringLive')) {
        if('-FoundationWorld' -cin $arguments -or '-DBAManualWorldHost' -cin $arguments){throw '无宿主回归不得意外初始化World。'}
    }
    foreach($flag in @('-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',"-FoundationRunId=$($id.ToString('D'))")) {
        if ($flag -cnotin $arguments) { throw '原生测试缺受控启动/禁止编译参数。' }
    }
    $automation=Read-PluginDemoEvidenceJson $Workspace "$directory/$Suite/Automation/index.json"
    if ($automation.File.SHA256 -cne $result.AutomationReportSHA256.ToLowerInvariant()) { throw 'Automation报告自测试结束后变化。' }
    $count=Assert-PluginDemoRunResult -ProcessResult $entry.Result -Cases @($automation.Value.tests) -Before $result.BuildEvidence -After $result.BuildEvidenceAfter -Suite $Suite
    if ($count -ne $result.ValidatedTests) { throw '实际测试数量与结果摘要不匹配。' }
    # Automation可能带有其他组的项目；只把本次过滤器消费的身份写入独立证明，不能污染C++审核合同。
    $selected=if($Suite -in @('WorldHostEditor','WorldHostLive','WorldAuthoringLive','EvidenceHash','EvidenceHashLive')){@($automation.Value.tests|Where-Object{$_.fullTestPath -cin $policy.Tests})}else{@($automation.Value.tests | Where-Object { ([string]$_.fullTestPath).StartsWith($policy.Prefix,[StringComparison]::Ordinal) })}
    [pscustomobject]@{Result=$result;Cases=$selected;Files=@($read.File,$process.File,$automation.File);Process=$entry.Result;BuildEvidence=$result.BuildEvidence}
}
Export-ModuleMember -Function Resolve-PluginDemoEvidencePath,Get-PluginDemoEvidenceFile,Read-PluginDemoEvidenceJson,Save-PluginDemoEvidenceJson,Assert-PluginDemoPromotionContract,Get-PluginDemoContentSnapshot,Read-PluginDemoNativeReport,Get-PluginDemoRuntimeModuleNames
