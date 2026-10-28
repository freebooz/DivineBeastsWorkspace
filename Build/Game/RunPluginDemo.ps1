#requires -Version 7.0
<#
.SYNOPSIS
仅在人工显式同意后启动插件演示开发客户端，不进入生产登录。
.DESCRIPTION
固定总览地图与两项开发模式参数；不会写入玩家账号、连接参数或密钥。
只创建本次RunId独占证据目录，不停止其它UE进程。若未制作真实地图立即拒绝启动。
不代表演示UI或正式地图Cook已完成；通过独立测试再核验。
#>
param(
    [switch]$Start,
    [ValidateSet('EditorGame','EditorManual','Client')][string]$Target='EditorGame',
    [string]$EngineRoot,
    [string]$BuildResultPath,
    [string]$ClientExe,
    # 可选真实插件身份；省略打开中心。独立页仍必须使用同一显式开发模式及构建证明。
    [string]$PluginId='',
    # 存储/偏好人工测试使用RunId自有UserDir，保留真实FPaths/Save实现且不碰既有玩家数据。
    [switch]$ManualFixture,
    # 显式选择真实Hub世界服务，不连接生产后端；完整离线夹具门禁仍强制。
    [switch]$ManualWorldHost,
    # 使用唯一Gameplay开发GameMode，要求隔离夹具并显式初始化真实离线Experience。
    [switch]$ManualGameplayHost,
    [ValidateRange(960,3840)][int]$WindowWidth=1920,
    [ValidateRange(540,2160)][int]$WindowHeight=1080,
    [guid]$RunId=[guid]::NewGuid()
)
function Resolve-PluginDemoIndependentPage {
    <# 纯参数合同：不启动进程、不加载UE、不把路径声明当原生资产已存在。 #>
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)]$Manifest,[AllowEmptyString()][string]$PluginId='')
    if([string]::IsNullOrEmpty($PluginId)) {
        return [pscustomobject]@{PluginId='';AssetFile='';Arguments=[string[]]@()}
    }
    if($PluginId.Length -ge 1024 -or $PluginId -cnotmatch '^[A-Za-z][A-Za-z0-9_]*$' -or $Manifest.manual_page_contract_version -ne 1) {
        throw '独立页要求有效插件身份及已支持的页面合同。'
    }
    $pageMatches=@($Manifest.plugins | Where-Object { $_.plugin_id -eq $PluginId })
    if($pageMatches.Count -ne 1){throw '独立页插件未登记或身份重复。'}
    $canonical=[string]$pageMatches[0].plugin_id
    $expected='/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_'+$canonical
    if($pageMatches[0].test_page_asset -cne $expected){throw '独立页资产与插件身份不匹配。'}
    $asset=Join-Path ([IO.Path]::GetFullPath($Workspace)) ('Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_'+$canonical+'.uasset')
    return [pscustomobject]@{PluginId=$canonical;AssetFile=$asset;Arguments=[string[]]@('-DBAPluginTest='+$canonical)}
}
function Resolve-PluginDemoManualFixture {
    <# 纯路径/参数解析；实际NativeSavedRoot必须在宿主日志中再核对。 #>
    param([Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)][guid]$RunId)
    if($RunId -eq [guid]::Empty){throw '隔离宿主需要非空运行身份。'}
    $fixtureRoot=[IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetFullPath($Workspace)) ('Saved/Validation/PluginManualUI20261027/Hosts/'+$RunId.ToString('D'))))
    return [pscustomobject]@{UserDir=$fixtureRoot;NativeSavedRoot=(Join-Path $fixtureRoot 'Saved');Arguments=[string[]]@('-GamePlatformManualFixture',('-UserDir='+$fixtureRoot))}
}
function Resolve-PluginDemoExecutionHost {
    <# 纯宿主参数合同。EditorManual进入正常编辑器，由真实单人PIE运行页面；不伪装Standalone。
       World/Gameplay宿主沿唯一既有参数映射，互斥模式不混用；无进程、资源或业务副作用。 #>
    param([ValidateSet('EditorGame','EditorManual','Client')][string]$Target='EditorGame',
        [switch]$ManualFixture,[switch]$ManualWorldHost,[switch]$ManualGameplayHost)
    $worldRequested=[bool]($ManualWorldHost -or $ManualGameplayHost)
    $editorRequested=$Target -eq 'EditorManual'
    if(($worldRequested -or $editorRequested) -and -not $ManualFixture){throw '显式开发世界/Editor宿主要求ManualFixture隔离UserDir。'}
    if($editorRequested -and $worldRequested){throw 'Editor/PIE不能与Standalone World/Gameplay宿主混用。'}
    $url=Get-PluginDemoMapURL Demo;$flags=@()
    if($editorRequested){$flags=@(Get-PluginDemoNativeFlag EditorManual)}
    elseif($ManualGameplayHost){$url=Get-PluginDemoMapURL GameplayHost;$flags=@(Get-PluginDemoHostFlags GameplayHost)}
    elseif($worldRequested){$flags=@(Get-PluginDemoHostFlags WorldHostLive)}
    [pscustomobject]@{StartupURL=$url;Arguments=[string[]]$flags;UseGameFlag=($Target -eq 'EditorGame');
        ManualWorldHost=$worldRequested;ManualGameplayHost=[bool]$ManualGameplayHost;ManualEditorFixture=$editorRequested}
}
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoEvidence.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PluginDemoValidation.psm1') -Force
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $workspace 'Game/DivineBeastsArena.uproject'
$map='/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'
$executionSelection=Resolve-PluginDemoExecutionHost -Target $Target -ManualFixture:$ManualFixture -ManualWorldHost:$ManualWorldHost -ManualGameplayHost:$ManualGameplayHost
$ManualWorldHost=$executionSelection.ManualWorldHost
$startupURL=$executionSelection.StartupURL
$mapFile=Join-Path $workspace 'Game/Content/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.umap'
# 演示门户必须同时拥有真实UE地图与Monolith制作的Widget蓝图，禁止启动无交互空白关卡。
$uiRoot=Join-Path $workspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/Development/PluginDemos'
$hubWidgetFile=Join-Path $uiRoot 'UI/WBP_DBA_PluginDemoHub.uasset'
if (-not $Start) { throw '必须显式传入-Start以允许新建插件演示进程。' }
if (-not (Test-Path -LiteralPath $mapFile -PathType Leaf)) {
    throw "开发总览地图尚未通过UE制作，拒绝启动虚假演示：$mapFile"
}
if (-not (Test-Path -LiteralPath $hubWidgetFile -PathType Leaf)) {
    throw "开发总览界面尚未通过Monolith制作，拒绝启动无交互演示：$hubWidgetFile"
}
foreach ($relative in @('UI/WBP_DBA_PluginDemoRoot.uasset','UI/WBP_DBA_PluginDemoMenuRow.uasset','UI/WBP_DBA_PluginDemoParameterRow.uasset','UI/WBP_DBA_PluginPortal.uasset','UI/WBP_DBA_PluginPortalCard.uasset','Definitions/DA_DBA_PluginDemoCatalog.uasset','Definitions/DA_DBA_PluginDemoHubScreen.uasset')) {
    Assert-FoundationFile (Join-Path $uiRoot $relative)
}
$authorDocument=Get-Content -LiteralPath (Join-Path $workspace 'Docs/Implementation/PluginDemos/PluginDemoManifest.json') -Raw | ConvertFrom-Json
$independentPage=Resolve-PluginDemoIndependentPage -Workspace $workspace -Manifest $authorDocument -PluginId $PluginId
if($independentPage.AssetFile -and !(Test-Path -LiteralPath $independentPage.AssetFile -PathType Leaf)) {
    throw '指定插件的原生独立页尚未制作；拒绝启动空页面。'
}
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw '找不到正式主工程。' }
if ($Target -in @('EditorGame','EditorManual')) {
    $engine=Get-FoundationEngine $EngineRoot
    # -game不加载Editor审核模块，运行只观察四个机制模块；独立Review测试仍要求完整工具模块证明。
    $requiredModules=@(Get-PluginDemoRuntimeModuleNames)
    if($executionSelection.ManualEditorFixture){$requiredModules+=@('GamePlatformDeveloperTools','BlueprintAutoLayout','GlooPrint','GamePlatformPCGEditor','GamePlatformSurfaceEditor')}
    $buildProof=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $requiredModules
    $exe=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
    $args=@($project,$startupURL)
    if($executionSelection.UseGameFlag){$args+='-game'}
} else {
    # Editor证据不能证明另一个Client可执行文件；在有Client产物绑定门禁前明确拒绝，禁止偷用旧exe。
    throw 'Client发布产物绑定证据尚未实现；当前仅允许匹配完整Editor证据的EditorGame，不自动构建。'
    if (-not $ClientExe) { throw 'Client模式需要显式提供已验证的开发客户端exe。' }
    $exe=(Resolve-Path -LiteralPath $ClientExe -ErrorAction Stop).Path
    if ([IO.Path]::GetFileName($exe) -ne 'DivineBeastsArenaClient.exe') { throw '拒绝使用非本项目Client Target。' }
    $args=@($map,'-CustomConfig=PluginDemoClient')
}
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "可执行文件不存在：$exe" }
# 来源端点在实际进程前冻结；内容较大时成本需实测，不以默认摘要或旧文件替代。
$manifest=Get-PluginDemoEvidenceFile -Root $workspace -Path 'Docs/Implementation/PluginDemos/PluginDemoManifest.json'
$content=Get-PluginDemoContentSnapshot -Workspace $workspace -EngineRoot $engine.Root
$afterContentBuild=Assert-FoundationEditorBuildResult -Workspace $workspace -Engine $engine -BuildResultPath $BuildResultPath -RequiredModules $requiredModules
foreach($field in @('BuildResultSHA256','InputSHA256','ArtifactSHA256')) { if ($buildProof.$field -cne $afterContentBuild.$field) { throw '启动快照期间源码/产物变化；未启动进程。' } }
$nativeSavedRoot=Join-Path $workspace 'Game/Saved'
$fixture=$null
if($ManualFixture){
    $fixture=Resolve-PluginDemoManualFixture -Workspace $workspace -RunId $RunId
    if(Test-Path -LiteralPath $fixture.UserDir){throw '拒绝复用已有人工测试宿主目录。'}
    $nativeSavedRoot=$fixture.NativeSavedRoot
}
$originRoot=Join-Path $nativeSavedRoot "GamePlatformValidation/Launches/$($RunId.ToString('D'))"
if (Test-Path -LiteralPath $originRoot) { throw '拒绝复用开发启动来源身份。' }
$contentFile=Save-PluginDemoEvidenceJson -Root $originRoot -Relative 'Content.before.json' -Value $content
$dir=Join-Path $workspace "Saved/Validation/PluginDemos/Launch/$RunId"
if (Test-Path -LiteralPath $dir) { throw "拒绝覆盖已有运行证据：$dir" }
$null=New-Item -ItemType Directory -Path $dir -Force
$log=Join-Path $dir 'PluginDemo.log'
$args+=@('-DBAPluginDemo',"-DBAPluginDemoMap=$map","-DBAPluginDemoLaunchId=$($RunId.ToString('D'))","-DBAPluginDemoContentFingerprint=$($content.SHA256)",'-windowed',"-ResX=$WindowWidth", "-ResY=$WindowHeight",'-nosplash','-NoLiveCoding','-LiveCoding=false','-NoHotReload','-NoCompile','-NoCompileEditor',"-abslog=$log")
$args+=@($independentPage.Arguments)
if($fixture){$args+=@($fixture.Arguments)}
$args+=@($executionSelection.Arguments)
$info=[Diagnostics.ProcessStartInfo]::new()
$info.FileName=$exe
$info.WorkingDirectory=Split-Path $exe
$info.UseShellExecute=$false
$info.CreateNoWindow=$false
foreach($arg in $args) { $info.ArgumentList.Add([string]$arg) }
# 演示模式不传递本地开发密码、令牌及生产网关地址。
foreach($key in @('DBA_DEV_LOGIN_SECRET','DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON','DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON',
    'DIVINEBEASTS_DEV_LOGIN_USER','DIVINEBEASTS_DEV_LOGIN_PASSWORD','DIVINEBEASTS_GATEWAY_BASE_URL')) { $null=$info.Environment.Remove($key) }
$process=[Diagnostics.Process]::Start($info)
if (-not $process) { throw '启动插件演示子进程失败。' }
$started=$process.StartTime.ToUniversalTime()
$record=[ordered]@{SchemaVersion=1;Kind='PluginDemoControlledLaunch';RunId=$RunId.ToString('D');Mode='DBAPluginDemo';Target=$Target;PluginId=$independentPage.PluginId;ManualFixture=[bool]$ManualFixture;ManualWorldHost=[bool]$ManualWorldHost;ManualGameplayHost=[bool]$ManualGameplayHost;ManualEditorFixture=[bool]$executionSelection.ManualEditorFixture;StartupURL=$startupURL;ExpectedNativeSavedRoot=$nativeSavedRoot;PID=$process.Id;Map=$map;EngineOrClientExe=$exe;Log=$log;StartedUtc=$started.ToString('o');Arguments=$args;BuildEvidence=$buildProof;ManifestSHA256=$manifest.SHA256;ContentSHA256=$content.SHA256;ContentSnapshot=$contentFile;HumanReview='Deferred'}
if($executionSelection.ManualEditorFixture){$record['EditorInstruction']='在本开发Hub使用单人Standalone网络模式启动PIE；Editor功能必须处于真实PIE，不能用-game替代。'}
try {
    $null=Save-PluginDemoEvidenceJson -Root $originRoot -Relative 'Launch.json' -Value $record
    $null=Save-PluginDemoEvidenceJson -Root $dir -Relative 'Launch.json' -Value $record
} catch {
    # 只收回本次PID/启动时间仍相同的进程，不能让缺来源记录的窗口假装可核验。
    $null=Stop-FoundationOwnedProcess -Process $process -StartTimeTicks $started.Ticks
    throw
}
$record|ConvertTo-Json -Depth 12
