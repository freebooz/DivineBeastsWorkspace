#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Nodes/GamePlatformPCGNodeMetadata.h"
#include "Data/PCGPointArrayData.h"
#include "Metadata/PCGMetadata.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Schema/GamePlatformPCGSchema.h"
#include "Services/GamePlatformPCGLinearRules.h"
#include "Services/GamePlatformPCGPriorityRules.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "Services/GamePlatformPCGTemplateContract.h"
#include "Types/GamePlatformPCGDomainIds.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGSchemaContractTest,
    "GamePlatform.PCG.Schema.V1Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGSchemaContractTest::RunTest(const FString&)
{
    const FGamePlatformPCGSchemaVersion Version = FGamePlatformPCGSchema::CurrentVersion();
    TestEqual(TEXT("Schema主版本固定为1"), Version.Major, 1);
    TestTrue(TEXT("核心字段已注册"), FGamePlatformPCGSchema::IsKnownAttribute(FGamePlatformPCGAttr::LayerName));
    TestTrue(TEXT("排除字段已注册"), FGamePlatformPCGSchema::IsKnownAttribute(FGamePlatformPCGAttr::ExcludeMask));
    TestTrue(TEXT("未登记Pcg字段必须拒绝"),
        !FGamePlatformPCGSchema::ValidateAttributeName(TEXT("Pcg.Unknown.Illegal")).IsSuccess());
    TestTrue(TEXT("非Pcg前缀必须拒绝"),
        !FGamePlatformPCGSchema::ValidateAttributeName(TEXT("Other.Layer")).IsSuccess());

    const UEnum* PrimitiveEnum = StaticEnum<EGamePlatformPCGPrimitive>();
    TestNotNull(TEXT("Primitive枚举存在"), PrimitiveEnum);
    if (PrimitiveEnum)
    {
        // UENUM末尾包含引擎生成的_MAX；真实原语必须严格为P0-P9十项。
        TestEqual(TEXT("P0-P9共十个真实原语"), PrimitiveEnum->NumEnums() - 1, 10);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGDomainAndPriorityTest,
    "GamePlatform.PCG.Domain.PriorityContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGDomainAndPriorityTest::RunTest(const FString&)
{
    TestTrue(TEXT("Forest.Canopy属于稳定领域ID"), FGamePlatformPCGDomainIds::IsKnown(TEXT("Forest.Canopy")));
    TestTrue(TEXT("Agri.Crop属于稳定领域ID"), FGamePlatformPCGDomainIds::IsKnown(TEXT("Agri.Crop")));
    TestFalse(TEXT("未知领域ID拒绝"), FGamePlatformPCGDomainIds::IsKnown(TEXT("DivineBeasts.Village.Peach")));

    TestTrue(TEXT("高优先级且达到Mask阈值时挖洞"), FGamePlatformPCGPriorityRules::ShouldCarve(30, 70, 1.0f, 0.5f));
    TestFalse(TEXT("低优先级不能反切高优先级"), FGamePlatformPCGPriorityRules::ShouldCarve(70, 30, 1.0f, 0.5f));
    TestFalse(TEXT("同优先级不互切"), FGamePlatformPCGPriorityRules::ShouldCarve(50, 50, 1.0f, 0.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGLinearRulesTest,
    "GamePlatform.PCG.Linear.SpanRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGLinearRulesTest::RunTest(const FString&)
{
    TArray<FGamePlatformPCGSpanMeshRule> Rules;
    const auto AddRule = [&Rules](FName MeshSetId, float MinLengthCm, float MaxLengthCm)
    {
        FGamePlatformPCGSpanMeshRule Rule;
        Rule.MeshSetId = MeshSetId;
        Rule.MinLengthCm = MinLengthCm;
        Rule.MaxLengthCm = MaxLengthCm;
        Rules.Add(Rule);
    };
    AddRule(TEXT("Span.200"), 150.0f, 250.0f);
    AddRule(TEXT("Span.100"), 90.0f, 110.0f);
    AddRule(TEXT("Span.Flexible"), 50.0f, 300.0f);

    FName Selected;
    TestTrue(TEXT("100cm跨度存在匹配"), FGamePlatformPCGLinearRules::SelectSpanMeshByLength(100.0f, Rules, Selected));
    TestEqual(TEXT("优先选择最窄覆盖区间"), Selected, FName(TEXT("Span.100")));
    TestTrue(TEXT("达到柱距时保留柱"), FGamePlatformPCGLinearRules::ShouldKeepPost(200.0f, 200.0f));
    TestFalse(TEXT("未达到柱距时过滤柱"), FGamePlatformPCGLinearRules::ShouldKeepPost(120.0f, 200.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGDefinitionDependencyTest,
    "GamePlatform.PCG.Definition.RequiredDependencies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGDefinitionDependencyTest::RunTest(const FString&)
{
    auto MakeCatalog = []()
    {
        UGamePlatformPCGConnectorCatalogDefinition* Catalog = NewObject<UGamePlatformPCGConnectorCatalogDefinition>();
        FGamePlatformId::TryParse(TEXT("test.pcg.connector_catalog@1"), Catalog->LogicalId);
        return Catalog;
    };

    {
        UGamePlatformPCGConnectorCatalogDefinition* Catalog = MakeCatalog();
        FGamePlatformPCGConnectorCatalogEntry Entry;
        Entry.ItemId = TEXT("Gate.Empty");
        Entry.MinSpanCm = 50.0f;
        Entry.MaxSpanCm = 300.0f;
        Catalog->Entries.Add(Entry);
        TestFalse(TEXT("连接件目录实际条目缺少ContentDefinitionId必须失败"), Catalog->ValidateDefinition().IsSuccess());
    }

    const FPrimaryAssetId ContentId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(),
        FName(TEXT("test.pcg.connector_content@1")));

    {
        UGamePlatformPCGConnectorCatalogDefinition* Catalog = MakeCatalog();
        FGamePlatformPCGConnectorCatalogEntry Entry;
        Entry.ItemId = TEXT("Gate.Undeclared");
        Entry.ContentDefinitionId = ContentId;
        Entry.MinSpanCm = 50.0f;
        Entry.MaxSpanCm = 300.0f;
        Catalog->Entries.Add(Entry);
        TestFalse(TEXT("字段引用未登记RequiredDefinitions必须失败"), Catalog->ValidateDefinition().IsSuccess());
    }

    {
        UGamePlatformPCGConnectorCatalogDefinition* Catalog = MakeCatalog();
        FGamePlatformPCGConnectorCatalogEntry Entry;
        Entry.ItemId = TEXT("Gate.Valid");
        Entry.ContentDefinitionId = ContentId;
        Entry.MinSpanCm = 50.0f;
        Entry.MaxSpanCm = 300.0f;
        Catalog->Entries.Add(Entry);
        Catalog->RequiredDefinitions.Add(ContentId);
        TestTrue(TEXT("有效引用登记RequiredDefinitions后通过字段级校验"), Catalog->ValidateDefinition().IsSuccess());
    }

    return true;
}

// 校验SchemaWriter先建NAME_None属性后的覆盖语义；这与空属性仅修改默认值的旧实现不同。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGAssignMeshSetMetadataTest,
    "GamePlatform.PCG.Metadata.AssignMeshSetToPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGAssignMeshSetMetadataTest::RunTest(const FString&)
{
    UPCGPointArrayData* Points = NewObject<UPCGPointArrayData>();
    if (!TestNotNull(TEXT("创建UE5.8真实PCG点数据"), Points))
    {
        return false;
    }
    Points->SetNumPoints(2);
    Points->AllocateProperties(EPCGPointNativeProperties::MetadataEntry);
    UPCGMetadata* Metadata = Points->MutableMetadata();
    if (!TestNotNull(TEXT("PCG点数据拥有可写元数据"), Metadata))
    {
        return false;
    }

    FPCGMetadataAttribute<FName>* Existing = Metadata->FindOrCreateAttribute<FName>(
        FGamePlatformPCGAttr::SpawnMeshSetId, NAME_None, false, true, true);
    if (!TestNotNull(TEXT("模拟WriteSchemaDefaults先创建空网格集合属性"), Existing))
    {
        return false;
    }

    const FName FirstMeshSet(TEXT("Test.Canopy"));
    TestTrue(TEXT("逐点写入合法MeshSetId"), GamePlatformPCGNodeMetadata::AssignMeshSetId(*Points, FirstMeshSet));
    const TConstPCGValueRange<PCGMetadataEntryKey> Keys = Points->GetConstMetadataEntryValueRange();
    TestEqual(TEXT("点数量保持不变"), Keys.Num(), 2);
    for (const PCGMetadataEntryKey Key : Keys)
    {
        TestTrue(TEXT("点元数据键已初始化"), Key != PCGInvalidEntryKey);
        TestEqual(TEXT("默认空ID被实际值覆盖"), Existing->GetValueFromItemKey(Key), FirstMeshSet);
    }

    const FName NextMeshSet(TEXT("Test.Understory"));
    TestTrue(TEXT("受控覆盖已有ID"), GamePlatformPCGNodeMetadata::AssignMeshSetId(*Points, NextMeshSet));
    for (const PCGMetadataEntryKey Key : Keys)
    {
        TestEqual(TEXT("全部点已更新ID"), Existing->GetValueFromItemKey(Key), NextMeshSet);
    }

    TestFalse(TEXT("空ID安全拒绝"), GamePlatformPCGNodeMetadata::AssignMeshSetId(*Points, NAME_None));
    for (const PCGMetadataEntryKey Key : Keys)
    {
        TestEqual(TEXT("非法配置不污染现有ID"), Existing->GetValueFromItemKey(Key), NextMeshSet);
    }
    return true;
}

// ValidateSchema不允许显式字段清单绕过核心字段，也不能仅凭名字接受类型错误或未知Pcg.*。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGMetadataTypeValidationTest,
    "GamePlatform.PCG.Schema.MetadataTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGMetadataTypeValidationTest::RunTest(const FString&)
{
    UPCGPointArrayData* Data = NewObject<UPCGPointArrayData>();
    UPCGMetadata* Metadata = Data->MutableMetadata();
    if (!TestNotNull(TEXT("真实PCG数据支持元数据"), Metadata))
    {
        return false;
    }
    Metadata->FindOrCreateAttribute<FName>(FGamePlatformPCGAttr::LayerName, FName(TEXT("Test")), false, true, true);
    Metadata->FindOrCreateAttribute<float>(FGamePlatformPCGAttr::ExcludeMask, 0.0f, false, true, true);
    Metadata->FindOrCreateAttribute<int32>(FGamePlatformPCGAttr::ExecSeed, 1, false, true, true);

    TestTrue(TEXT("核心字段类型正确时通过"), GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Metadata, {}));
    TestFalse(TEXT("显式字段清单不能要求不存在的字段"),
        GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Metadata, {FGamePlatformPCGAttr::SpawnMeshSetId}));
    Metadata->FindOrCreateAttribute<FName>(FGamePlatformPCGAttr::SpawnMeshSetId, FName(TEXT("Test.MeshSet")), false, true, true);
    TestTrue(TEXT("声明的已存在字段通过"),
        GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Metadata, {FGamePlatformPCGAttr::SpawnMeshSetId}));

    UPCGPointArrayData* InvalidType = NewObject<UPCGPointArrayData>();
    UPCGMetadata* Typed = InvalidType->MutableMetadata();
    Typed->FindOrCreateAttribute<int32>(FGamePlatformPCGAttr::LayerName, 1, false, true, true);
    Typed->FindOrCreateAttribute<float>(FGamePlatformPCGAttr::ExcludeMask, 0.0f, false, true, true);
    Typed->FindOrCreateAttribute<int32>(FGamePlatformPCGAttr::ExecSeed, 1, false, true, true);
    TestFalse(TEXT("错误的Layer.Name存储类型必须拒绝"),
        GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Typed, {}));

    UPCGPointArrayData* MissingCore = NewObject<UPCGPointArrayData>();
    UPCGMetadata* Partial = MissingCore->MutableMetadata();
    Partial->FindOrCreateAttribute<FName>(FGamePlatformPCGAttr::SpawnMeshSetId, FName(TEXT("Test.MeshSet")), false, true, true);
    TestFalse(TEXT("显式字段不能绕过缺失的核心Schema字段"),
        GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Partial, {FGamePlatformPCGAttr::SpawnMeshSetId}));

    Metadata->FindOrCreateAttribute<FName>(TEXT("Pcg.Unknown.Unregistered"), FName(TEXT("Bad")), false, true, true);
    TestFalse(TEXT("未知Pcg.*字段被拒绝"),
        GamePlatformPCGNodeMetadata::HasAllRequiredAttributes(*Metadata, {}));
    return true;
}

// P2纯空间几何规则：优先级严格、边界缓冲、来源顺序稳定及非法数据拒绝。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGSpatialMasksTest,
    "GamePlatform.PCG.Spatial.CarveGeometryAndPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGSpatialMasksTest::RunTest(const FString&)
{
    FGamePlatformPCGSpatialMask Road;
    Road.SourceId = FGuid(1, 2, 3, 4);
    Road.Priority = 70;
    Road.Strength = 1.0f;
    Road.HalfWidthCm = 150.0f;
    Road.Vertices = {FVector2D(-200.0, 0.0), FVector2D(200.0, 0.0)};

    FGamePlatformPCGSpatialMask Parcel;
    Parcel.SourceId = FGuid(5, 6, 7, 8);
    Parcel.Priority = 50;
    Parcel.Strength = 1.0f;
    Parcel.bClosed = true;
    Parcel.Vertices = {FVector2D(-100.0, -100.0), FVector2D(100.0, -100.0),
        FVector2D(100.0, 100.0), FVector2D(-100.0, 100.0)};

    TestTrue(TEXT("有效道路样条空间掩码"), FGamePlatformPCGSpatialRules::ValidateMask(Road));
    TestTrue(TEXT("有效闭合地块空间掩码"), FGamePlatformPCGSpatialRules::ValidateMask(Parcel));
    TestTrue(TEXT("道路缓冲区命中"), FGamePlatformPCGSpatialRules::ContainsPoint(Road, FVector2D(0.0, 149.0)));
    TestFalse(TEXT("道路缓冲区外不命中"), FGamePlatformPCGSpatialRules::ContainsPoint(Road, FVector2D(0.0, 151.0)));
    TestTrue(TEXT("闭合农田内命中"), FGamePlatformPCGSpatialRules::ContainsPoint(Parcel, FVector2D(20.0, 20.0)));
    // 同一个闭合四边形，田地Mask应填充内部，栏杆Mask应只清除边缘。
    FGamePlatformPCGSpatialMask Fence = Parcel;
    Fence.SourceId = FGuid(4, 3, 2, 1);
    Fence.bFillInterior = false;
    Fence.HalfWidthCm = 12.0f;
    TestFalse(TEXT("闭合围栏不能排空院内全部作物"), FGamePlatformPCGSpatialRules::ContainsPoint(Fence, FVector2D(0.0, 0.0)));
    TestTrue(TEXT("闭合围栏沿边界缓冲应正确排除"), FGamePlatformPCGSpatialRules::ContainsPoint(Fence, FVector2D(98.0, 0.0)));


    float Mask = 0.0f;
    FGuid Source;
    TArray<FGamePlatformPCGSpatialMask> Shapes = {Parcel, Road};
    TestTrue(TEXT("计算可复现空间规则"), FGamePlatformPCGSpatialRules::Evaluate(
        FVector2D(0, 0), 30, 0.5f, Shapes, Mask, Source));
    TestEqual(TEXT("主路70优先于地块50"), Source, Road.SourceId);
    TestEqual(TEXT("主路完全排除乔木"), Mask, 1.0f);
    TestTrue(TEXT("主路优先级低于地块，不得切除地块"), FGamePlatformPCGSpatialRules::Evaluate(
        FVector2D(0, 0), 80, 0.5f, Shapes, Mask, Source));
    TestFalse(TEXT("较高优先级不应被低层切除"), Source.IsValid());

    // 等级相同的两个外部Carver必须与数组遍历次序无关；优先最小稳定来源标识。
    FGamePlatformPCGSpatialMask OtherRoad = Road;
    OtherRoad.SourceId = FGuid(9, 0, 0, 0);
    Shapes = {OtherRoad, Road};
    TestTrue(TEXT("路源顺序A"), FGamePlatformPCGSpatialRules::Evaluate(
        FVector2D(0, 0), 30, 0.5f, Shapes, Mask, Source));
    TestEqual(TEXT("同级来源采用稳定GUID顺序"), Source, Road.SourceId);
    Shapes = {Road, OtherRoad};
    TestTrue(TEXT("路源顺序B"), FGamePlatformPCGSpatialRules::Evaluate(
        FVector2D(0, 0), 30, 0.5f, Shapes, Mask, Source));
    TestEqual(TEXT("颠倒登记顺序不改变来源"), Source, Road.SourceId);

    FGamePlatformPCGSpatialMask Invalid = Road;
    Invalid.HalfWidthCm = -1.0f;
    Shapes = {Road, Invalid};
    TestFalse(TEXT("任意非法来源必须整体失败关闭"), FGamePlatformPCGSpatialRules::Evaluate(
        FVector2D(0, 0), 30, 0.5f, Shapes, Mask, Source));
    TestFalse(TEXT("失败不得残留上一次已选来源"), Source.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformPCGTemplateContractTest,
    "GamePlatform.PCG.Template.ContractIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGTemplateContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("曲面散布模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_ScatterSurface")));
    TestTrue(TEXT("闭合围合模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_EnclosureClosed")));
    TestTrue(TEXT("栏杆附着模板ID已登记"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_RailingAttached")));
    TestFalse(TEXT("城市模板尚未进入1.0批准合同"), FGamePlatformPCGTemplateIds::IsKnown(TEXT("TPL_UrbanBlock")));
    return true;
}

#endif
