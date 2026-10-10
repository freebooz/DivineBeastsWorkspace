#requires -Version 5.1
<#
.SYNOPSIS
GamePlatformPCG（游戏平台程序化内容生成插件）M0/M1源码架构门禁。
.DESCRIPTION
验证双模块、三层边界、P0-P9原语、Schema、模板合同和开发组件清单。
本脚本不替代UHT/C++编译、UE Automation、真实PCG资产、Gold Level或Cook。
#>
[CmdletBinding()]
param([string]$WorkspaceRoot)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}

$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/World/GamePlatformPCG'
$descriptorPath = Join-Path $pluginRoot 'GamePlatformPCG.uplugin'
$runtimeBuild = Join-Path $pluginRoot 'Source/GamePlatformPCG/GamePlatformPCG.Build.cs'
$editorBuild = Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/GamePlatformPCGEditor.Build.cs'
$schemaHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Schema/GamePlatformPCGSchema.h'
$primitiveHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Types/GamePlatformPCGEnvironmentTypes.h'
$templateHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Services/GamePlatformPCGTemplateContract.h'
$componentDoc = Join-Path $pluginRoot 'Docs/组件清单与使用说明.md'
$editorTarget = Join-Path $WorkspaceRoot 'Game/Source/DivineBeastsArenaEditor.Target.cs'
$editorLibraryHeader = Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.h'
$foundationGraphCpp = Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp'
$foundationCommandlet = Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGFoundationTemplatesCommandlet.cpp'
$worldValidator = Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Validators/GamePlatformPCGWorldValidator.cpp'
$findings = [Collections.Generic.List[string]]::new()

function Assert-True([bool]$Condition,[string]$Message) {
    if (-not $Condition) { $script:findings.Add($Message) }
}

foreach ($path in @($descriptorPath,$runtimeBuild,$editorBuild,$schemaHeader,$primitiveHeader,$templateHeader,$componentDoc,$editorTarget,$editorLibraryHeader,$foundationGraphCpp,$foundationCommandlet,$worldValidator)) {
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) ("缺少必要文件：{0}" -f $path)
}

if ($findings.Count -eq 0) {
    $descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True (@($descriptor.Modules).Count -eq 2) '插件必须保持GamePlatformPCG Runtime + GamePlatformPCGEditor Editor双模块。'
    # 既查模块数量也查实际注册名/宿主，防止空壳或误把编辑器服务装入Dedicated Server。
    $runtimeModule = @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformPCG' -and $_.Type -eq 'Runtime' })
    $editorModule = @($descriptor.Modules | Where-Object { $_.Name -eq 'GamePlatformPCGEditor' -and $_.Type -eq 'Editor' })
    Assert-True ($runtimeModule.Count -eq 1 -and $editorModule.Count -eq 1) 'PCG必须且仅能注册共享Runtime与Editor两个准确模块。'
    Assert-True (@($editorModule[0].TargetAllowList).Count -eq 1 -and @($editorModule[0].TargetAllowList)[0] -eq 'Editor') 'PCGEditor目标白名单必须明确限制为Editor。'
    foreach ($dependency in @('GamePlatformCore','GamePlatformData','GamePlatformWorld','PCG')) {
        Assert-True (@($descriptor.Plugins | Where-Object { $_.Name -eq $dependency -and $_.Enabled }).Count -eq 1) ("PCG缺少或重复插件依赖：{0}" -f $dependency)
    }
    $editorTargetSource = Get-Content -LiteralPath $editorTarget -Raw -Encoding UTF8
    Assert-True ($editorTargetSource.Contains('EnablePlugins.Add("GamePlatformPCG")')) 'DivineBeastsArenaEditor Target必须显式装配GamePlatformPCG编辑器工具能力。'
    Assert-True (-not [bool]$descriptor.CanContainContent) '生产模板资产尚未验收前CanContainContent必须保持false。'
    $editorBuildSource = Get-Content -LiteralPath $editorBuild -Raw -Encoding UTF8
    Assert-True ($editorBuildSource.Contains('DataValidation')) 'GamePlatformPCGEditor必须显式依赖DataValidation以承载原生Editor Validator。'
    $runtimeBuildSource = Get-Content -LiteralPath $runtimeBuild -Raw -Encoding UTF8
    Assert-True ($runtimeBuildSource.Contains('"PCG"') -and $runtimeBuildSource.Contains('"GamePlatformData"') -and $runtimeBuildSource.Contains('"GamePlatformCore"')) 'PCGRuntime模块必须通过构建规则显式依赖官方PCG、Data与Core。'
    Assert-True (-not $runtimeBuildSource.Contains('"UnrealEd"') -and -not $runtimeBuildSource.Contains('"GamePlatformPCGEditor"')) '共享Runtime不允许反向依赖编辑器模块。'
    Assert-True ($editorBuildSource.Contains('"GamePlatformPCG"') -and $editorBuildSource.Contains('"UnrealEd"')) 'PCGEditor必须以私有依赖消费共享Runtime与编辑器接口。'
    $worldSubsystem = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp') -Raw -Encoding UTF8
    Assert-True ($worldSubsystem.Contains('NM_DedicatedServer') -and $worldSubsystem.Contains('NM_ListenServer') -and $worldSubsystem.Contains('ServerCosmeticForbidden')) '平台PCG共享运行端必须拒绝服务端纯装饰生成。'

    $runtimeFiles = Get-ChildItem (Join-Path $pluginRoot 'Source/GamePlatformPCG') -Recurse -File -Include *.h,*.cpp,*.cs
    $runtimeSource = ($runtimeFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"
    foreach ($token in @('ProjectPCG','DivineBeastsPCG','MobaCommon/','DivineBeasts/')) {
        Assert-True (-not $runtimeSource.Contains($token)) ("平台运行源码出现禁止的平行/上层身份：{0}" -f $token)
    }

    $primitive = Get-Content -LiteralPath $primitiveHeader -Raw -Encoding UTF8
    foreach ($id in 0..9) { Assert-True ($primitive.Contains(("P{0}_" -f $id))) ("缺少固定原语P{0}" -f $id) }
    Assert-True (-not $primitive.Contains('P10_')) '禁止新增P10原语。'
    Assert-True (-not $primitive.Contains('L11')) 'Override不得伪装为L11原语。'

    $schema = Get-Content -LiteralPath $schemaHeader -Raw -Encoding UTF8
    foreach ($name in @('BiomeId','LayerName','SpawnMeshSetId','ExcludeMask','ExecSeed','MutableId')) {
        Assert-True ($schema.Contains($name)) ("Schema缺少关键字段：{0}" -f $name)
    }

    $environmentDefinitions = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Definitions/GamePlatformPCGEnvironmentDefinitions.cpp') -Raw -Encoding UTF8
    $profileDefinitions = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Validation/PCGDefinitions.cpp') -Raw -Encoding UTF8
    Assert-True ($environmentDefinitions.Contains('RequiredDefinitions.Contains(Id)')) '环境Definition字段引用必须纳入RequiredDefinitions统一租约。'
    Assert-True ($profileDefinitions.Contains('RequiredDefinitions.Contains(Id)')) 'Profile的ExecPreset/PriorityTable引用必须纳入RequiredDefinitions统一租约。'
    $graphInspection = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Validation/PCGGraphInspection.cpp') -Raw -Encoding UTF8
    Assert-True ($graphInspection.Contains('PCGTemplateRuntimeExecutionDeferred')) 'Template Contract运行时执行在真实资产闭环前必须失败关闭。'
    Assert-True ($graphInspection.Contains('TemplateSchemaBoundaryDisconnected')) 'Template Contract必须验证SchemaWriter→SchemaValidator→Output真实可达关系。'
    Assert-True ($graphInspection.Contains('DuplicateTemplateSchemaWriter')) 'Template Contract必须拒绝重复Schema Writer。'
    Assert-True ($graphInspection.Contains('DuplicateTemplateSchemaValidator')) 'Template Contract必须拒绝重复Schema Validator。'
    $environmentHeader = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Definitions/GamePlatformPCGEnvironmentDefinitions.h') -Raw -Encoding UTF8
    Assert-True ($environmentHeader.Contains('AssetBundles="PCGGeneration"')) 'MeshSet真实网格软引用必须进入PCGGeneration Asset Bundle。'
    # 防止回归为仅给属性设置默认值而没有覆盖已有点数据的实际MeshSetId。
    # 这是源码形状检查；真正的元数据值与重复赋值语义由UE Automation运行验证。
    $meshMetadataHeader = Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Nodes/GamePlatformPCGNodeMetadata.h'
    $meshNodeCpp = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Nodes/GamePlatformPCGNodes.cpp') -Raw -Encoding UTF8
    $meshTests = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Tests/PCGEnvironmentContractTests.cpp') -Raw -Encoding UTF8
    Assert-True (Test-Path -LiteralPath $meshMetadataHeader -PathType Leaf) '缺少MeshSetId逐点写入私有契约。'
    Assert-True ($meshNodeCpp.Contains('Attribute->SetValue(Key, MeshSetId)') -and $meshNodeCpp.Contains('AssignMeshSetId(*OutputData, Settings->MeshSetId)')) 'AssignMeshSet必须实际逐点写入ID，不能只设置属性默认值。'
    Assert-True ($meshTests.Contains('GamePlatform.PCG.Metadata.AssignMeshSetToPoints')) '缺少MeshSetId默认空值与重复赋值的UE自动化回归测试源码。'
    Assert-True ($meshNodeCpp.Contains('GamePlatformPCGNodeMetadata::HasAllRequiredAttributes') -and $meshNodeCpp.Contains('ValidateRequiredAttributes(Available)') -and $meshNodeCpp.Contains('IsSchemaTypeCompatible') -and $meshNodeCpp.Contains('Pcg.')) 'ValidateSchema必须校验核心字段、实际元数据类型并拒绝未知Pcg前缀。'
    Assert-True ($meshTests.Contains('GamePlatform.PCG.Schema.MetadataTypes')) '缺少PCG元数据类型/核心字段失败关闭的UE自动化回归测试源码。'

    $template = Get-Content -LiteralPath $templateHeader -Raw -Encoding UTF8
    # 领域清单包含42个跨游戏稳定语义ID，仅作静态映射合同；资产实际存在性由Editor/AssetRegistry另验。
    $domainCatalog = Get-Content -LiteralPath (Join-Path $pluginRoot 'Docs/DomainCatalog.md') -Encoding UTF8
    $domainNames = @($domainCatalog | ForEach-Object { if ($_ -match '^\|\s*([A-Za-z]+\.[A-Za-z]+)\s*\|') { $Matches[1] } })
    Assert-True ($domainNames.Count -eq 42 -and @($domainNames | Sort-Object -Unique).Count -eq 42) 'PCG领域目录必须恰好登记42个无重复稳定语义。'
    foreach ($name in @('ScatterSurface','LinearDresser','EnclosureClosed','CropField','RailingAttached')) {
        Assert-True ($template.Contains($name)) ("Template Contract缺少模板ID：{0}" -f $name)
    }
    foreach ($name in @('ProjectOnLandscape','PriorityCarve','ApplySpawnPolicy','AssignMeshSet','FitPostsToSpline','BreakByIntersection','WriteClosedExclude')) {
        Assert-True ($template.Contains($name)) ("Template Contract缺少公共子图ID：{0}" -f $name)
    }

    $editorLibrary = Get-Content -LiteralPath $editorLibraryHeader -Raw -Encoding UTF8
    $foundationGraph = Get-Content -LiteralPath $foundationGraphCpp -Raw -Encoding UTF8
    $commandlet = Get-Content -LiteralPath $foundationCommandlet -Raw -Encoding UTF8
    Assert-True ($editorLibrary.Contains('CreateFoundationTemplateAssets')) 'Editor Library缺少Foundation模板资产创建入口。'
    Assert-True ($editorLibrary.Contains('CreateFoundationSubgraphAssets')) 'Editor Library缺少Foundation公共子图资产创建入口。'
    Assert-True ($editorLibrary.Contains('CreateFoundationAssets')) 'Editor Library缺少模板+子图统一预检创建入口。'
    Assert-True ($foundationGraph.Contains('CreateFoundationTemplateGraph')) '缺少Foundation模板真实UPCGGraph生成器。'
    Assert-True ($foundationGraph.Contains('CreateFoundationSubgraphGraph')) '缺少Foundation公共子图真实UPCGGraph生成器。'
    Assert-True ($foundationGraph.Contains('UPCGProjectionSettings')) 'SG_ProjectOnLandscape必须使用UE5.8官方Projection节点。'
    Assert-True ($foundationGraph.Contains('DefaultLandscapeLabel')) 'SG_ProjectOnLandscape必须公开Landscape输入Pin。'
    Assert-True ($foundationGraph.Contains('UGamePlatformPCGWriteExcludeSettings')) 'SG_WriteClosedExclude必须使用统一排除属性节点。'
    Assert-True ($foundationGraph.Contains('bIsTemplate = true')) 'Foundation模板生成器必须设置官方PCG模板标记。'
    Assert-True ($commandlet.Contains('GamePlatformPCGFoundationTemplatesCommandlet')) '缺少Foundation模板命令行生成入口。'
    $validator = Get-Content -LiteralPath $worldValidator -Raw -Encoding UTF8
    Assert-True ($validator.Contains('Directors.Num() != 1')) '世界校验器必须要求PCG地图恰好一个WorldDirector。'
    Assert-True ($validator.Contains('Registered.Contains(Participant)')) '世界校验器必须拒绝未注册到WorldDirector的PCG放置器。'


    # P2～P7代码交付静态门禁：这些检查只保证归属、入口与失败关闭源码存在，不能替代UE生成/编译。
    $advancedHeader = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Definitions/GamePlatformPCGAdvancedDefinitions.h') -Raw -Encoding UTF8
    $advancedAlgorithms = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Services/GamePlatformPCGAdvancedSpatialRules.h') -Raw -Encoding UTF8
    $actorHeader = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Actors/GamePlatformPCGActors.h') -Raw -Encoding UTF8
    $anchorContract = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Public/Services/GamePlatformPCGAnchorContracts.h') -Raw -Encoding UTF8
    $advancedTests = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCG/Private/Tests/PCGAdvancedContractTests.cpp') -Raw -Encoding UTF8
    $editorBlueprintSource = Get-Content -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp') -Raw -Encoding UTF8
    foreach ($keyword in @('UGamePlatformPCGWorldFeatureDefinition','UGamePlatformPCGAssemblyDefinition',
            'UGamePlatformPCGAnchorPolicyDefinition','UGamePlatformPCGCavityDefinition',
            'UGamePlatformPCGSpatialGraphDefinition')) {
        Assert-True ($advancedHeader.Contains($keyword)) ("P4～P7缺少实际通用定义源码：{0}" -f $keyword)
    }
    foreach ($keyword in @('FindBridgeCandidates','IsInsideCavity','ValidateSpatialGraph')) {
        Assert-True ($advancedAlgorithms.Contains($keyword)) ("P7缺少稳定纯空间算法入口：{0}" -f $keyword)
    }
    Assert-True ($actorHeader.Contains('CollectSpatialMasks') -and $actorHeader.Contains('CollectGameplayAnchorCandidates')) 'P2/P5世界编排器缺少空间与候选锚点入口。'
    Assert-True ($anchorContract.Contains('ValidateAuthoritativeStates')) 'P6必须保留服务器权威状态快照校验，不在PCG中复制Save。'
    Assert-True ($editorBlueprintSource.Contains('FKismetEditorUtilities::CreateBlueprint') -and
        $editorBlueprintSource.Contains('CreatePCGPlacementBlueprints')) 'Editor必须有UE原生蓝图创作入口，不能生成文本假资源。'
    Assert-True ($advancedTests.Contains('GamePlatform.PCG.Advanced.BridgeCavitySpatialGraph') -and
        $advancedTests.Contains('GamePlatform.PCG.Advanced.StableAnchorAndAuthority')) '缺少P4～P7正式UE自动化用例源码。'
    $validatorSource = Get-Content -LiteralPath $worldValidator -Raw -Encoding UTF8
    Assert-True ($validatorSource.Contains('FWorldPartitionHelpers::ForEachActorDescInstance')) '世界分区校验不能只使用已加载TActorIterator。'

    $docs = Get-Content -LiteralPath $componentDoc -Raw -Encoding UTF8
    foreach ($token in @('怎么使用','当前实现状态','M2','不允许的用法')) {
        Assert-True ($docs.Contains($token)) ("开发组件清单缺少内容：{0}" -f $token)
    }
}

if ($findings.Count -gt 0) {
    Write-Output ("GamePlatformPCG架构门禁失败：{0}项" -f $findings.Count)
    $findings | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    exit 1
}

Write-Output 'GamePlatformPCG架构门禁通过。'
Write-Output '已确认：双模块、无平行PCG插件、P0-P9固定原语、Schema关键字段、模板合同和开发组件清单。'
Write-Output '此结果不替代UE5.8编译、Automation、真实模板资产、Gold Level、Cook或性能验收。'
exit 0
