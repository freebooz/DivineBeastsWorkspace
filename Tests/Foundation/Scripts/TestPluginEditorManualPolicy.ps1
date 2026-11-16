#requires -Version 7.0
<# Editor/PIE精确宿主合同；合成记录不证明UE、键鼠或画面。 #>
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../../../Build/Game/PluginDemoValidation.psm1') -Force
$expected=@('ContractClosed','PIE.CentralRunnerRetainedCleanup','PIE.HostEligibilityAndLease','PIE.NineVendorLifecycle',
    'PIE.StopReentryAndWorldExit','PIE.TwoEngineGameInstancesExclusive','Vendor.OwnedGraphAndConflict')|ForEach-Object{'DivineBeasts.PluginDemos.Editor.'+$_}
$policy=Get-PluginDemoSuitePolicy EditorManual
if($policy.Host -cne 'EditorPIE' -or (Compare-Object $policy.Tests $expected)){throw 'Editor原生必需集合或真实宿主不匹配。'}
if((Get-PluginDemoAutomationFilter EditorManual) -cne ($policy.Tests -join '+')){throw 'Editor过滤器未采用完整精确集合。'}
if((Get-PluginDemoNativeFlag EditorManual) -cne '-GamePlatformManualEditorFixture' -or @(Get-PluginDemoHostFlags EditorManual).Count -ne 0){throw 'Editor宿主误用Standalone/World初始化旗标。'}
if((Get-PluginDemoMapURL EditorManual) -cne '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'){throw 'Editor宿主必须使用真实Hub。'}
$build=[pscustomobject]@{BuildResultSHA256='a'*64;InputSHA256='b'*64;ArtifactSHA256='c'*64}
$process=[pscustomobject]@{ExitCode=0;ProcessId=123;Cleaned=$true;TimedOut=$false}
$valid=@($expected|ForEach-Object{[pscustomobject]@{fullTestPath=$_;state='Success'}});$checks=4
if((Assert-PluginDemoRunResult -ProcessResult $process -Cases $valid -Before $build -After $build -Suite EditorManual) -ne 7){throw '完整成功集合未接受。'};$checks++
$variants=@(@($valid|Select-Object -Skip 1),@($valid+$valid[0]),@($valid+[pscustomobject]@{fullTestPath='Other.Extra';state='Success'}))
foreach($variant in $variants){$rejected=$false;try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases $variant -Before $build -After $build -Suite EditorManual}catch{$rejected=$true};if(!$rejected){throw '缺失/重复/额外Editor测试仍接受。'};$checks++}
foreach($state in @('Fail','NotRun','Skipped')){$bad=@($valid|ForEach-Object{[pscustomobject]@{fullTestPath=$_.fullTestPath;state=$_.state}});$bad[0].state=$state;$rejected=$false;try{$null=Assert-PluginDemoRunResult -ProcessResult $process -Cases $bad -Before $build -After $build -Suite EditorManual}catch{$rejected=$true};if(!$rejected){throw '非成功Editor记录仍接受。'};$checks++}
Write-Output "PluginEditorManualPolicyCases=$checks Failed=0 UEExecuted=False"
