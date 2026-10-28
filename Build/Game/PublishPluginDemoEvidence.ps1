#requires -Version 7.0
<#
.SYNOPSIS
显式-Publish后，把已验证的原始演示证据追加为机器版本绑定索引，不构建、不启动UE、不签人工结论。
.DESCRIPTION
必须提供当前完整Editor证据、真实Demo和Review原生结果、受控启动记录及原始终态。
先核对加载DLL、源码/内容端点、原生实际process/index和全部原始文件，再追加独立证明与新索引。
旧raw、日志、报告及决策不改写；不生成截图或资产。证据为无签名本地材料，不证明恶意外部输入可信。
退出0仅表示发布机器待审索引，1失败，2未执行；不代表完整插件覆盖、当前人工通过或部署可用。
#>
param([switch]$Publish,[string]$EngineRoot,[string]$BuildResultPath,[string]$DemoResultPath,[string]$ReviewResultPath,
    [string]$NativeSavedRoot='Game/Saved',
    [Parameter(Mandatory)][guid]$RunId,[Parameter(Mandatory)][string]$RawIndexPath)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoEvidence.psm1') -Force
$code=2
try {
    if (-not $Publish) { throw [IO.FileNotFoundException]::new('未指定Publish；不读取产物或写验证索引。') }
    if ($RunId -eq [guid]::Empty) { throw '运行身份不能为零GUID。' }
    $workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $savedRoot=Resolve-PluginDemoEvidencePath -Root $workspace -Path $NativeSavedRoot
    $root=Join-Path $savedRoot "GamePlatformValidation/$($RunId.ToString('D'))/Review"
    $engine=Get-FoundationEngine $EngineRoot
    # 当前完整Editor/Review证明须含审核模块，但-game运行观察不可强制Editor模块已加载。
    $modules=@(Get-PluginDemoRuntimeModuleNames)+@('GamePlatformDeveloperTools')
    $before=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $modules
    $rawRead=Read-PluginDemoEvidenceJson $root $RawIndexPath; $raw=$rawRead.Value
    if ($raw.run_id -cne $RunId.ToString('D')) { throw '原始索引不属于请求的运行。' }
    $launchId=[guid]::ParseExact([string]$raw.runtime_origin.launch_id,'D')
    $launchRoot=Join-Path $savedRoot "GamePlatformValidation/Launches/$($launchId.ToString('D'))"
    $launchRead=Read-PluginDemoEvidenceJson $launchRoot 'Launch.json'; $launch=$launchRead.Value
    if ($launch.SchemaVersion -ne 1 -or $launch.Kind -cne 'PluginDemoControlledLaunch' -or $launch.Target -cne 'EditorGame' -or
        $launch.EngineOrClientExe -ine (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe')) { throw '来源不是本工程受控EditorGame启动。' }
    if($savedRoot -ine (Join-Path $workspace 'Game/Saved')) {
        $expectedUserDir=Join-Path $workspace ('Saved/Validation/PluginManualUI20261027/Hosts/'+$launchId.ToString('D'))
        if($savedRoot -ine (Join-Path $expectedUserDir 'Saved') -or $launch.ManualFixture -isnot [bool] -or !$launch.ManualFixture -or
            $launch.ExpectedNativeSavedRoot -ine $savedRoot -or '-GamePlatformManualFixture' -cnotin $launch.Arguments -or ('-UserDir='+$expectedUserDir) -cnotin $launch.Arguments) {
            throw '隔离保存目录与实际受控宿主身份/参数不一致。'
        }
    }
    foreach($argument in @('-DBAPluginDemo',"-DBAPluginDemoLaunchId=$($launchId.ToString('D'))","-DBAPluginDemoContentFingerprint=$($launch.ContentSHA256)",'-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor')) {
        if ($argument -cnotin $launch.Arguments) { throw '来源启动记录缺隔离/版本参数。' }
    }
    if ([DateTime]::Parse($raw.recorded_utc).ToUniversalTime() -lt [DateTime]::Parse($launch.StartedUtc).ToUniversalTime()) { throw '原始运行早于实际启动进程。' }
    $launchContent=Read-PluginDemoEvidenceJson -Root $launchRoot -Path $launch.ContentSnapshot.Path -MaxBytes 67108864
    if ($launchContent.File.SHA256 -cne $launch.ContentSnapshot.SHA256 -or $launchContent.Value.Scope -cne 'PluginDemoContentRoots-v1' -or
        $launchContent.Value.SHA256 -cne $launch.ContentSHA256) { throw '启动内容端点文件自记录后变化。' }
    $demo=Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine.Root -ResultPath $DemoResultPath -Suite Demo
    $review=Read-PluginDemoNativeReport -Workspace $workspace -EngineRoot $engine.Root -ResultPath $ReviewResultPath -Suite Review
    foreach($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) {
        if ($review.BuildEvidence.$field -cne $before.$field) { throw '审核测试不属于同一源码/产物版本。' }
    }
    $manifest=Get-PluginDemoEvidenceFile $workspace 'Docs/Implementation/PluginDemos/PluginDemoManifest.json'
    $python=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'; Assert-FoundationFile $python
    $validation=& $python (Join-Path $workspace 'Tests/Architecture/ValidatePluginDemoManifest.py') --workspace $workspace --manifest (Join-Path $workspace $manifest.Path)
    if ($LASTEXITCODE -ne 0) { throw '当前作者源身份/关系无效。' }
    # 读取真实原始引用，摘要与实际文件逐一相等；状态必须是同一份原始转储，不信索引布尔或嵌入副本。
    $rawFiles=[Collections.Generic.List[object]]::new(); $stateCount=0
    foreach($entry in $raw.evidence) {
        $file=Get-PluginDemoEvidenceFile $root $entry.path
        if ($file.SHA256 -cne $entry.sha256) { throw '原始证据变化。' }
        $rawFiles.Add($file)
        if ($entry.type -ceq 'state_dump') {
            $stateCount++; $state=Read-PluginDemoEvidenceJson $root $entry.path
            if (($state.Value | ConvertTo-Json -Compress -Depth 30) -cne ($raw.raw_state | ConvertTo-Json -Compress -Depth 30)) { throw '嵌入状态与实际原始转储不一致。' }
        }
    }
    if ($stateCount -ne 1) { throw '原始运行必须有唯一实际状态转储。' }
    $content=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine.Root
    $after=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $modules
    foreach($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) { if ($before.$field -cne $after.$field) { throw '提升期间源码/产物变化。' } }
    $null=Assert-PluginDemoPromotionContract -Raw $raw -CurrentBuild $after -RecordedBuild $demo.BuildEvidence -Launch $launch -ContentSHA256 $content.SHA256 -ManifestSHA256 $manifest.SHA256 -Cases $demo.Cases -NativeProcess $demo.Process
    # 写前再检查全部输入文件；任何漂移均不发布索引，不能让半完成证明自动接入菜单。
    foreach($file in @($rawRead.File)+@($rawFiles.ToArray())) {
        if ((Get-PluginDemoEvidenceFile $root $file.Path).SHA256 -cne $file.SHA256) { throw '写前原始文件变化。' }
    }
    foreach($file in @($demo.Files)+@($review.Files)+@($manifest)) {
        if ((Get-PluginDemoEvidenceFile $workspace $file.Path).SHA256 -cne $file.SHA256) { throw '写前测试/作者材料变化。' }
    }
    if ((Get-PluginDemoEvidenceFile $launchRoot 'Launch.json').SHA256 -cne $launchRead.File.SHA256) { throw '写前启动记录变化。' }
    $revision=[guid]::NewGuid().ToString('N')
    $inventory=Save-PluginDemoEvidenceJson -Root $root -Relative "Content.$revision.json" -Value $content
    # 完整文件清单可能大于Editor JSON对象读取上限；人工可见asset_manifest采用有界摘要，完整清单单独绑定摘要。
    $contentSummary=Save-PluginDemoEvidenceJson -Root $root -Relative "ContentSummary.$revision.json" -Value @{SchemaVersion=1;Scope=$content.Scope;ContentSHA256=$content.SHA256;FileCount=$content.FileCount;SnapshotPath=$inventory.Path;SnapshotSHA256=$inventory.SHA256;Limitations=$content.Limitations}
    $source=Save-PluginDemoEvidenceJson -Root $root -Relative "MachineInputs.$revision.json" -Value @{Launch=$launch;DemoResult=$demo.Result;ReviewResult=$review.Result;Files=@($demo.Files)+@($review.Files);BuildEvidence=$after;Manifest=$manifest;Limitations='Unsigned local endpoint evidence, not human approval or continuous runtime attestation.'}
    $proof=@{schema_version=1;kind='PluginDemoBinaryVerification';passed=$true;run_id=$RunId.ToString('D');source_fingerprint=$manifest.SHA256;content_fingerprint=$content.SHA256;artifact_sha256=$after.ArtifactSHA256.ToLowerInvariant();raw_index_path=$rawRead.File.Path;raw_index_sha256=$rawRead.File.SHA256;tests=$demo.Cases;review_tests=$review.Cases;machine_inputs_path=$source.Path;machine_inputs_sha256=$source.SHA256;content_snapshot_path=$inventory.Path;content_snapshot_sha256=$inventory.SHA256;recorded_utc=[DateTime]::UtcNow.ToString('O')}
    $proofFile=Save-PluginDemoEvidenceJson -Root $root -Relative "BinaryVerification.$revision.json" -Value $proof
    # 原始字节不动；复制值只形成新待审修订，人工结论仍由唯一平台入口后续明确确认。
    $verified=$raw | ConvertTo-Json -Depth 40 | ConvertFrom-Json -AsHashtable -Depth 100
    $verified.content_fingerprint=$content.SHA256; $verified.build_version=$after.ArtifactSHA256.ToLowerInvariant(); $verified.binary_verified=$true; $verified.binary_verification=$proofFile.Path
    $verified.evidence+=@(@{type='asset_manifest';path=$contentSummary.Path;sha256=$contentSummary.SHA256})
    $index=Save-PluginDemoEvidenceJson -Root $root -Relative "VerifiedRunEvidence.$revision.json" -Value $verified
    Write-Output (@{Status='MachineVerifiedPendingHuman';RunId=$RunId.ToString('D');IndexPath=(Join-Path $root $index.Path);SourceFingerprint=$manifest.SHA256;ContentFingerprint=$content.SHA256;HumanDecisionWritten=$false;UEExecuted=$false;BuildExecuted=$false} | ConvertTo-Json -Depth 6)
    $code=0
} catch [IO.FileNotFoundException] { Write-Output $_.Exception.Message; $code=2 }
catch { Write-Output $_.Exception.Message; $code=1 }
exit $code
