#include "Definitions/GamePlatformPCGAdvancedDefinitions.h"

#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformPCGDomainIds.h"
#include "Services/GamePlatformPCGAdvancedSpatialRules.h"

namespace
{
FGamePlatformResult CheckReferencedDefinition(const UGamePlatformDefinitionBase& Owner,
    const FPrimaryAssetId& Id, bool bMandatory)
{
    if (!Id.IsValid())
    {
        return bMandatory
            ? FGamePlatformResult::Failure(TEXT("PCGAdvancedDefinitionMissing"), TEXT("配置缺少真实MeshSet等必需定义。"))
            : FGamePlatformResult::Success();
    }
    if (Id.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
        !Owner.RequiredDefinitions.Contains(Id) || Id == Owner.GetPrimaryAssetId())
    {
        return FGamePlatformResult::Failure(TEXT("PCGAdvancedDependencyUndeclared"),
            TEXT("高级PCG字段引用须登记到GamePlatformData.RequiredDefinitions且不能自引用。"));
    }
    return FGamePlatformResult::Success();
}

bool IsFeatureDomain(FName Domain)
{
    return Domain == FGamePlatformPCGDomainIds::WaterRiver ||
        Domain == FGamePlatformPCGDomainIds::WaterLake ||
        Domain == FGamePlatformPCGDomainIds::WaterBank ||
        Domain == FGamePlatformPCGDomainIds::ForestEdge ||
        Domain == FGamePlatformPCGDomainIds::AgriOrchard ||
        Domain == FGamePlatformPCGDomainIds::AgriFallow ||
        Domain == FGamePlatformPCGDomainIds::RockFormation ||
        Domain == FGamePlatformPCGDomainIds::EnclosureRoadGuard ||
        Domain == FGamePlatformPCGDomainIds::EnclosureCliffRail ||
        Domain == FGamePlatformPCGDomainIds::EnclosureDockRail ||
        Domain == FGamePlatformPCGDomainIds::SettlementYard;
}

bool IsAssemblyDomain(FName Domain)
{
    return Domain == FGamePlatformPCGDomainIds::SettlementBuilding ||
        Domain == FGamePlatformPCGDomainIds::SettlementYard ||
        Domain == FGamePlatformPCGDomainIds::RockFormation;
}

bool IsAnchorDomain(FName Domain)
{
    return Domain == FGamePlatformPCGDomainIds::PlayResource ||
        Domain == FGamePlatformPCGDomainIds::PlayCover ||
        Domain == FGamePlatformPCGDomainIds::PlayClimb ||
        Domain == FGamePlatformPCGDomainIds::PlaySpawn;
}
}

FGamePlatformResult UGamePlatformPCGWorldFeatureDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!IsFeatureDomain(DomainId) || !FGamePlatformPCGDomainIds::IsKnown(DomainId) ||
        Stage == EGamePlatformPCGWorldStage::TerrainWrite ||
        Stage == EGamePlatformPCGWorldStage::CutFillRequest ||
        Stage == EGamePlatformPCGWorldStage::Interiors ||
        Stage == EGamePlatformPCGWorldStage::ApplyState ||
        Stage == EGamePlatformPCGWorldStage::RuntimeDetail ||
        !bEditorStaticOnly || !FMath::IsFinite(BandWidthCm) ||
        BandWidthCm < 0.0f || BandWidthCm > 10000.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidWorldFeature"),
            TEXT("领域、生成阶段、带宽或编辑器静态交付约束无效；不能直接写地形或变更服务器状态。"));
    }
    const FGamePlatformResult Mesh = CheckReferencedDefinition(*this, MeshSetDefinitionId, true);
    return Mesh.IsSuccess()
        ? CheckReferencedDefinition(*this, SpawnPolicyDefinitionId, false) : Mesh;
}

FGamePlatformResult UGamePlatformPCGAssemblyDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!IsAssemblyDomain(DomainId) || Slots.IsEmpty() || Slots.Num() > 64)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidAssembly"), TEXT("组合件领域非法或插槽数量超过64。"));
    }

    TSet<FName> Seen;
    for (const FGamePlatformPCGAssemblySlot& Slot : Slots)
    {
        if (Slot.SlotId.IsNone() || Seen.Contains(Slot.SlotId) ||
            Slot.RelativeTransform.ContainsNaN())
        {
            return FGamePlatformResult::Failure(TEXT("PCGAssemblySlotInvalid"),
                TEXT("组合件插槽身份重复/为空或空间变换含无效数字。"));
        }
        Seen.Add(Slot.SlotId);
        const FGamePlatformResult Dependency = CheckReferencedDefinition(*this, Slot.MeshSetDefinitionId, true);
        if (!Dependency.IsSuccess()) { return Dependency; }
    }
    return FGamePlatformResult::Success();
}


bool FGamePlatformPCGAssemblyRules::ResolveSlotTransforms(
    const UGamePlatformPCGAssemblyDefinition& Definition,
    const FTransform& RootTransform, TArray<FTransform>& OutWorldTransforms)
{
    OutWorldTransforms.Reset();
    if (!Definition.ValidateDefinition().IsSuccess() || RootTransform.ContainsNaN())
    {
        return false;
    }
    OutWorldTransforms.Reserve(Definition.Slots.Num());
    for (const FGamePlatformPCGAssemblySlot& Slot : Definition.Slots)
    {
        const FTransform World = Slot.RelativeTransform * RootTransform;
        if (World.ContainsNaN())
        {
            OutWorldTransforms.Reset();
            return false;
        }
        OutWorldTransforms.Add(World);
    }
    return true;
}

FGamePlatformResult UGamePlatformPCGCavityDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (bRequestTerrainWrite)
    {
        return FGamePlatformResult::Unsupported(TEXT("PCGTerrainWriteNotApproved"),
            TEXT("P7洞穴仅提供非破坏性的三维排除体积；地形雕刻/路堑须先专项审批。"));
    }
    bool bInside = false;
    return FGamePlatformPCGAdvancedSpatialRules::IsInsideCavity(
        Exclusion, FVector(0.0, 0.0, Exclusion.MinZCm), bInside)
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(TEXT("PCGCavityInvalid"),
            TEXT("三维洞穴排除体积的闭合轮廓或高程上下界非法。"));
}

FGamePlatformResult UGamePlatformPCGSpatialGraphDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    return FGamePlatformPCGAdvancedSpatialRules::ValidateSpatialGraph(
        Nodes, Edges, EntryNodeId, ExitNodeId)
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(TEXT("PCGSpatialGraphInvalid"),
            TEXT("室内/地牢空间图包含重复节点/边、不合法标识或无法从入口到达所有空间。"));
}
FGamePlatformResult UGamePlatformPCGAnchorPolicyDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!IsAnchorDomain(DomainId) || !FMath::IsFinite(MinSeparationCm) ||
        MinSeparationCm < 1.0f || MinSeparationCm > 100000.0f ||
        MaxCandidates <= 0 || MaxCandidates > 4096 || !bRequireServerApproval)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidAnchorPolicy"),
            TEXT("玩法锚点策略非法；候选不能自行绕过服务器审批。"));
    }
    return FGamePlatformResult::Success();
}
