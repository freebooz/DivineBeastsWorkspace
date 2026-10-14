#requires -Version 7.0
<# SSLM证据提升合同回归：纯内存运行/测试夹具及临时普通文本文件，不启动UE、不制造资产或人工结论。
   故障目标：旧DLL、旧内容、错运行/启动PID、缺原生测试、非终态及路径逃逸不能取得机器版本证明。 #>
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$module=Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoEvidence.psm1'
if (-not (Test-Path -LiteralPath $module)) { throw 'RED：证据提升模块尚未实现。' }
Import-Module $module -Force
$cases=0
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message }; $script:cases++ }
$build=@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64;VerifiedModuleFiles=@{DivineBeastsDemoClient=@{Path='Game/Plugins/DivineBeasts/DBAClient/Binaries/Win64/Demo.dll';SHA256='d'*64}}}
$launch=@{RunId='20261013-1234-4000-8000-000000000001';PID=456;ManifestSHA256='e'*64;ContentSHA256='f'*64;BuildEvidence=$build}
$raw=@{schema_version=1;run_id='20261013-1234-4000-8000-000000000002';scenario_id='Sample.Normal';feature_id='Sample.Feature';source_fingerprint='e'*64;content_fingerprint='unverified';build_version='unverified';binary_verified=$false;runtime_completed=$true;process_id=456;
    runtime_origin=@{launch_id=$launch.RunId;content_fingerprint='f'*64;no_live_coding=$true;live_coding_state_verified=$true;no_hot_reload=$true;hot_reload_state_verified=$true;loaded_modules=@(@{name='DivineBeastsDemoClient';path=$build.VerifiedModuleFiles.DivineBeastsDemoClient.Path;sha256='d'*64})};
    raw_state=@{run_id='20261013-1234-4000-8000-000000000002';scenario_id='Sample.Normal';feature_id='Sample.Feature';run_state=4;completed_operations=1;cleanup_result=@{status=1;code='None'};terminal_result=@{status=1}}}
# 隔离合同夹具也须完整列四个运行模块；不靠一模块的简化夹具放宽生产资格。
foreach($name in @('DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient')) {
    $build.VerifiedModuleFiles[$name]=@{Path="Game/Plugins/Fixture/Binaries/Win64/$name.dll";SHA256='d'*64}
    $raw.runtime_origin.loaded_modules+=@{name=$name;path=$build.VerifiedModuleFiles[$name].Path;sha256='d'*64}
}
$tests=@('Definition','Catalog','Lifecycle','Menu','BirdScenario','Samples','Evidence','LaunchPolicy') | ForEach-Object { @{fullTestPath="DivineBeasts.PluginDemos.$_";state='Success'} }
$process=@{ExitCode=0;TimedOut=$false;Cleaned=$true;ProcessId=123}
function Invoke-Contract($Raw=$raw,$Build=$build,$Launch=$launch,$Content=('f'*64),$Tests=$tests,$Process=$process) {
    Assert-PluginDemoPromotionContract -Raw $Raw -CurrentBuild $Build -RecordedBuild $build -Launch $Launch -ContentSHA256 $Content -ManifestSHA256 ('e'*64) -Cases $Tests -NativeProcess $Process
}
function Reject([scriptblock]$Action) { $caught=$false; try { & $Action } catch { $caught=$true }; Require $caught '非法合同被接受。' }
Require ((Invoke-Contract) -eq 8) '合法独立合同不能绑定8个必需测试。'
# EditorGame的-game不加载Type=Editor审核模块；完整Editor收据含它不意味着运行也必须加载它。
# 保留完整构建摘要和后续Review组核对，只把运行所需模块与Editor测试模块的职责分开。
$build.VerifiedModuleFiles.GamePlatformDeveloperTools=@{Path='Game/Plugins/GamePlatform/Diagnostics/GamePlatformDeveloperTools/Binaries/Win64/Review.dll';SHA256='a'*64}
Require ((Invoke-Contract) -eq 8) '合法-game运行因未加载Editor审核模块而不能提升。'
$changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed.runtime_origin.live_coding_state_verified=$false
Reject { Invoke-Contract -Raw $changed }
$changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed.runtime_origin.hot_reload_state_verified=$false
Reject { Invoke-Contract -Raw $changed }
foreach($name in @('DivineBeastsDemoClient','DivineBeastsApplicationFlowClient','GamePlatformData','GamePlatformUIClient')) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    $changed.runtime_origin.loaded_modules=@($changed.runtime_origin.loaded_modules | Where-Object {$_.name -cne $name})
    Reject { Invoke-Contract -Raw $changed }
}
# JSON类型也是合同：数字/字符串不可冒充bool，字符串数字不可冒充真实PID或枚举。
# 删除生产类型校验应让这些拒绝断言失败；不伪造进程/原生日志，仅测试纯值验证入口。
foreach($change in @(@{binary_verified=0},@{runtime_completed=1},@{schema_version='1'},@{process_id='456'})) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    foreach($key in $change.Keys){$changed[$key]=$change[$key]}; Reject { Invoke-Contract -Raw $changed }
}
foreach($field in @('no_live_coding','no_hot_reload')) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed.runtime_origin[$field]=1
    Reject { Invoke-Contract -Raw $changed }
}
foreach($change in @(@{run_state='4'},@{completed_operations='1'},@{cleanup_result=@{status='1';code='None'}},@{terminal_result=@{status=$true}})) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    foreach($key in $change.Keys){$changed.raw_state[$key]=$change[$key]}; Reject { Invoke-Contract -Raw $changed }
}
Reject { Invoke-Contract -Process @{ExitCode=0;TimedOut=0;Cleaned=1;ProcessId=123} }
foreach ($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) {
    $changed=$build | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed[$field]='0'*64
    Reject { Invoke-Contract -Build $changed }
}
Reject { Invoke-Contract -Content ('0'*64) }
foreach ($field in @('process_id','source_fingerprint','runtime_completed','binary_verified')) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    $changed[$field]=switch($field){process_id{999};source_fingerprint{'0'*64};runtime_completed{$false};binary_verified{$true}}
    Reject { Invoke-Contract -Raw $changed }
}
foreach ($field in @('launch_id','content_fingerprint','no_live_coding','no_hot_reload','loaded_modules')) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    $changed.runtime_origin[$field]=switch($field){launch_id{'20261013-1234-4000-8000-000000000099'};content_fingerprint{'0'*64};loaded_modules{@()};default{$false}}
    Reject { Invoke-Contract -Raw $changed }
}
$changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed.runtime_origin.loaded_modules[0].sha256='0'*64
Reject { Invoke-Contract -Raw $changed }
foreach ($change in @(@{run_state=2},@{run_state=4.5},@{completed_operations=0},@{completed_operations=$true},@{completed_operations=1.5},@{run_id='20261013-1234-4000-8000-000000000099'},@{cleanup_result=@{status=2;code='Failure'}})) {
    $changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable
    foreach($key in $change.Keys){$changed.raw_state[$key]=$change[$key]}; Reject { Invoke-Contract -Raw $changed }
}
Reject { Invoke-Contract -Tests @() }; Reject { Invoke-Contract -Tests @($tests | Select-Object -Skip 1) }
$changedTests=$tests | ConvertTo-Json -Depth 6 | ConvertFrom-Json -AsHashtable; $changedTests[0].state='Fail'; Reject { Invoke-Contract -Tests $changedTests }
Reject { Invoke-Contract -Process @{ExitCode=0;TimedOut=$true;Cleaned=$true;ProcessId=123} }
# 预期失败是可核验事实，不强改原始失败为成功；但仍须实际完成命令且清理成功。
$changed=$raw | ConvertTo-Json -Depth 12 | ConvertFrom-Json -AsHashtable; $changed.raw_state.run_state=5; $changed.raw_state.terminal_result.status=2
Require ((Invoke-Contract -Raw $changed) -eq 8) '预期失败被误改成不可审事实。'
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('PluginDemoEvidenceContract-'+[guid]::NewGuid().ToString('N'))
try {
    $null=New-Item -ItemType Directory -Path $temporary
    $file=Join-Path $temporary 'plain.txt'; [IO.File]::WriteAllText($file,'abc',[Text.UTF8Encoding]::new($false))
    $record=Get-PluginDemoEvidenceFile -Root $temporary -Path 'plain.txt'
    Require ($record.SHA256 -ceq 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') '实际SHA256不符合已知abc向量。'
    foreach($path in @('../escape.txt','sub/../plain.txt','sub//plain.txt')) { Reject { Get-PluginDemoEvidenceFile -Root $temporary -Path $path } }
    $saved=Save-PluginDemoEvidenceJson -Root $temporary -Relative 'report.json' -Value @{kind='contract_fixture_only'}
    Require (Test-Path -LiteralPath (Join-Path $temporary $saved.Path)) '追加文件未真实创建。'
    Reject { Save-PluginDemoEvidenceJson -Root $temporary -Relative 'report.json' -Value @{kind='replacement'} }
    [IO.File]::WriteAllText((Join-Path $temporary 'duplicate.json'),'{"passed":false,"passed":true}',[Text.UTF8Encoding]::new($false))
    Reject { Read-PluginDemoEvidenceJson -Root $temporary -Path 'duplicate.json' }
    # 内容快照必须计入项目/引擎及两侧插件的Content；使用普通txt夹具，不创建.uasset/.umap。
    $workspace=Join-Path $temporary 'workspace'; $engine=Join-Path $temporary 'engine'
    foreach ($path in @('Game/Content/plain.txt','Game/Plugins/Fixture/Content/plain.txt','Engine/Content/plain.txt','Engine/Plugins/Fixture/Content/plain.txt')) {
        $base=if($path.StartsWith('Game/')){$workspace}else{$engine}; $target=Join-Path $base $path
        $null=New-Item -ItemType Directory -Force -Path (Split-Path $target)
        [IO.File]::WriteAllText($target,'content fixture',[Text.UTF8Encoding]::new($false))
    }
    foreach($base in @($workspace,$engine)) {
        $plugin=if($base -eq $workspace){'Game/Plugins/Fixture/Fixture.uplugin'}else{'Engine/Plugins/Fixture/Fixture.uplugin'}
        [IO.File]::WriteAllText((Join-Path $base $plugin),'{"FileVersion":3,"Modules":[]}',[Text.UTF8Encoding]::new($false))
    }
    $beforeContent=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
    Require ($beforeContent.FileCount -eq 4) '内容快照漏掉项目/引擎/插件文件。'
    $stableContent=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
    Require ($beforeContent.SHA256 -ceq $stableContent.SHA256) '未变化的文件产生不稳定内容身份。'
    [IO.File]::WriteAllText((Join-Path $engine 'Engine/Plugins/Fixture/Content/plain.txt'),'changed',[Text.UTF8Encoding]::new($false))
    $afterContent=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine
    Require ($beforeContent.SHA256 -cne $afterContent.SHA256) '引擎插件内容变化未撤销资格。'
} finally {
    # 只清理本次固定前缀GUID临时目录，绝不删除工作空间或使用未核对的计算路径。
    $resolved=[IO.Path]::GetFullPath($temporary)
    if ($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved) -match '^PluginDemoEvidenceContract-[a-f0-9]{32}$') { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
Write-Output "PluginDemoEvidenceContractCases=$cases Failed=0 UEExecuted=False BuildExecuted=False HumanDecisionWritten=False"
