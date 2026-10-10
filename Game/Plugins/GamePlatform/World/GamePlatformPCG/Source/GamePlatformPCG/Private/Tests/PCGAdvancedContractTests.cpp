#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformPCGAdvancedDefinitions.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Services/GamePlatformPCGAdvancedSpatialRules.h"
#include "Services/GamePlatformPCGAnchorContracts.h"
#include "Types/GamePlatformPCGDomainIds.h"

namespace
{
void AssignId(UGamePlatformDefinitionBase& Definition, const TCHAR* Id)
{
    FGamePlatformId::TryParse(Id, Definition.LogicalId);
}

FGamePlatformPCGSpatialMask MakeBoxFootprint()
{
    FGamePlatformPCGSpatialMask Mask;
    Mask.SourceId = FGuid(1, 2, 3, 4);
    Mask.Priority = 100;
    Mask.bClosed = true;
    Mask.Vertices = {FVector2D(0, 0), FVector2D(200, 0),
        FVector2D(200, 200), FVector2D(0, 200)};
    return Mask;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPCGWorldFeatureDefinitionTest,
    "GamePlatform.PCG.Advanced.FeatureAndAssemblyDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGWorldFeatureDefinitionTest::RunTest(const FString&)
{
    const FPrimaryAssetId MeshId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("test.pcg_feature_mesh@1"));

    UGamePlatformPCGWorldFeatureDefinition* Feature = NewObject<UGamePlatformPCGWorldFeatureDefinition>();
    AssignId(*Feature, TEXT("test.pcg_feature@1"));
    Feature->DomainId = FGamePlatformPCGDomainIds::WaterBank;
    Feature->MeshSetDefinitionId = MeshId;
    Feature->Stage = EGamePlatformPCGWorldStage::InterfaceBands;
    TestFalse(TEXT("缺少Definition声明时失败关闭"), Feature->ValidateDefinition().IsSuccess());
    Feature->RequiredDefinitions.Add(MeshId);
    TestTrue(TEXT("已声明水岸环境定义有效"), Feature->ValidateDefinition().IsSuccess());
    Feature->bEditorStaticOnly = false;
    TestFalse(TEXT("不准运行时生成玩法水岸"), Feature->ValidateDefinition().IsSuccess());
    Feature->bEditorStaticOnly = true;

    UGamePlatformPCGAssemblyDefinition* Assembly = NewObject<UGamePlatformPCGAssemblyDefinition>();
    AssignId(*Assembly, TEXT("test.pcg_assembly@1"));
    Assembly->DomainId = FGamePlatformPCGDomainIds::SettlementBuilding;
    FGamePlatformPCGAssemblySlot& Slot = Assembly->Slots.AddDefaulted_GetRef();
    Slot.SlotId = TEXT("Foundation");
    Slot.MeshSetDefinitionId = MeshId;
    Assembly->RequiredDefinitions.Add(MeshId);
    TestTrue(TEXT("建筑组合件使用已声明可复用MeshSet"), Assembly->ValidateDefinition().IsSuccess());
    TArray<FTransform> WorldSlots;
    const FTransform Placement(FRotator::ZeroRotator, FVector(100.0, 200.0, 300.0));
    TestTrue(TEXT("建筑Slot位置可由静态原点组合求出"),
        FGamePlatformPCGAssemblyRules::ResolveSlotTransforms(*Assembly, Placement, WorldSlots));
    TestEqual(TEXT("一项Slot获得一项位置候选"), WorldSlots.Num(), 1);
    if (WorldSlots.Num() == 1)
    {
        TestEqual(TEXT("Root变换正确传递"), WorldSlots[0].GetLocation(), FVector(100.0, 200.0, 300.0));
    }
    Assembly->Slots.Add(Slot);
    TestFalse(TEXT("组合件插槽身份不允许重复"), Assembly->ValidateDefinition().IsSuccess());

    UGamePlatformPCGAnchorPolicyDefinition* Anchor = NewObject<UGamePlatformPCGAnchorPolicyDefinition>();
    AssignId(*Anchor, TEXT("test.pcg_anchor_policy@1"));
    Anchor->DomainId = FGamePlatformPCGDomainIds::PlayResource;
    TestTrue(TEXT("通用资源锚点策略需要服务器审批"), Anchor->ValidateDefinition().IsSuccess());
    Anchor->bRequireServerApproval = false;
    TestFalse(TEXT("锚点策略不允许绕过服务器准入"), Anchor->ValidateDefinition().IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPCGAdvancedSpatialGeometryTest,
    "GamePlatform.PCG.Advanced.BridgeCavitySpatialGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGAdvancedSpatialGeometryTest::RunTest(const FString&)
{
    FGamePlatformPCGSpatialMask Road;
    Road.SourceId = FGuid(1, 1, 1, 1);
    Road.Vertices = {FVector2D(-500, 0), FVector2D(500, 0)};
    Road.HalfWidthCm = 200.0f;
    FGamePlatformPCGSpatialMask River;
    River.SourceId = FGuid(2, 2, 2, 2);
    River.Vertices = {FVector2D(0, -500), FVector2D(0, 500)};
    River.HalfWidthCm = 250.0f;
    TArray<FGamePlatformPCGBridgeCandidate> Bridges;
    TestTrue(TEXT("正确生成道路河流交叉候选"), FGamePlatformPCGAdvancedSpatialRules::FindBridgeCandidates(
        Road, River, 100.0f, Bridges));
    TestEqual(TEXT("一个明确交叉点"), Bridges.Num(), 1);
    if (!Bridges.IsEmpty())
    {
        TestTrue(TEXT("桥梁候选必须等待人工审查"), Bridges[0].bRequiresHumanApproval);
        TestEqual(TEXT("水域宽度和退界确定桥跨需求"), Bridges[0].RequiredSpanCm, 700.0f);
    }

    FGamePlatformPCGCavityExclusion Cavity;
    Cavity.Footprint = MakeBoxFootprint();
    Cavity.MinZCm = -200.0f;
    Cavity.MaxZCm = 100.0f;
    bool bInside = false;
    TestTrue(TEXT("3D体腔只做不破坏地形的空间判断"),
        FGamePlatformPCGAdvancedSpatialRules::IsInsideCavity(Cavity, FVector(50, 50, 0), bInside));
    TestTrue(TEXT("体腔内明确命中"), bInside);

    UGamePlatformPCGCavityDefinition* CavityDefinition = NewObject<UGamePlatformPCGCavityDefinition>();
    AssignId(*CavityDefinition, TEXT("test.pcg_cavity@1"));
    CavityDefinition->Exclusion = Cavity;
    TestTrue(TEXT("体腔排除定义有效"), CavityDefinition->ValidateDefinition().IsSuccess());
    CavityDefinition->bRequestTerrainWrite = true;
    TestFalse(TEXT("未经批准的地形写入安全拒绝"), CavityDefinition->ValidateDefinition().IsSuccess());

    UGamePlatformPCGSpatialGraphDefinition* Graph = NewObject<UGamePlatformPCGSpatialGraphDefinition>();
    AssignId(*Graph, TEXT("test.pcg_spatial_graph@1"));
    Graph->EntryNodeId = TEXT("Entrance");
    Graph->ExitNodeId = TEXT("Exit");
    FGamePlatformPCGSpaceNode Entrance; Entrance.NodeId = Graph->EntryNodeId;
    FGamePlatformPCGSpaceNode Exit; Exit.NodeId = Graph->ExitNodeId;
    Graph->Nodes = {Entrance, Exit};
    FGamePlatformPCGSpaceEdge Link; Link.FromNodeId = TEXT("Entrance"); Link.ToNodeId = TEXT("Exit");
    Graph->Edges = {Link};
    TestTrue(TEXT("两空间连通合法"), Graph->ValidateDefinition().IsSuccess());
    Graph->Edges.Reset();
    TestFalse(TEXT("断开的室内空间被拒绝"), Graph->ValidateDefinition().IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPCGStableAnchorStateTest,
    "GamePlatform.PCG.Advanced.StableAnchorAndAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformPCGStableAnchorStateTest::RunTest(const FString&)
{
    FGamePlatformId World;
    FGamePlatformId Region;
    FGamePlatformId::TryParse(TEXT("test.world@1"), World);
    FGamePlatformId::TryParse(TEXT("test.region@1"), Region);
    const FGuid Source(5, 6, 7, 8);
    FGuid First, Again, NextRevision;
    TestTrue(TEXT("可生成跨地图重开稳定锚点身份"), FGamePlatformPCGAnchorRules::MakeStableId(
        World, Region, Source, FGamePlatformPCGDomainIds::PlayResource, 1, 0, First));
    TestTrue(TEXT("相同输入可复现ID"), FGamePlatformPCGAnchorRules::MakeStableId(
        World, Region, Source, FGamePlatformPCGDomainIds::PlayResource, 1, 0, Again));
    TestEqual(TEXT("StableId不依赖数组迭代顺序"), First, Again);
    TestTrue(TEXT("新修订生成新版本"), FGamePlatformPCGAnchorRules::MakeStableId(
        World, Region, Source, FGamePlatformPCGDomainIds::PlayResource, 2, 0, NextRevision));
    TestNotEqual(TEXT("版本变化不可静默命中旧存档"), First, NextRevision);

    FGamePlatformPCGAnchorCandidate Candidate;
    Candidate.StableId = First;
    Candidate.SourceId = Source;
    Candidate.DomainId = FGamePlatformPCGDomainIds::PlayResource;
    Candidate.ContentRevision = 1;
    FGamePlatformPCGObjectStateSnapshot State;
    State.StableId = First;
    State.ContentRevision = 1;
    State.ServerSequence = 1;
    State.StateCode = 1;
    TArray<FGamePlatformPCGAnchorCandidate> Candidates = {Candidate};
    TArray<FGamePlatformPCGObjectStateSnapshot> States = {State};
    TestTrue(TEXT("服务器序列和内容版本一致才可映射"), FGamePlatformPCGAnchorRules::ValidateAuthoritativeStates(
        Candidates, States));
    States.Add(State);
    TestFalse(TEXT("拒绝重复同一锚点状态"), FGamePlatformPCGAnchorRules::ValidateAuthoritativeStates(
        Candidates, States));
    States.SetNum(1);
    States[0].ContentRevision = 2;
    TestFalse(TEXT("拒绝错版的服务端状态"), FGamePlatformPCGAnchorRules::ValidateAuthoritativeStates(
        Candidates, States));
    return true;
}

#endif
