#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"

#include "Schema/GamePlatformPCGSchema.h"

namespace
{
bool IsFiniteRange(const FVector2D& Range)
{
    return FMath::IsFinite(Range.X) && FMath::IsFinite(Range.Y) && Range.X <= Range.Y;
}

FGamePlatformResult ValidateAssetIdIfSet(const FPrimaryAssetId& Id)
{
    return !Id.IsValid()
        ? FGamePlatformResult::Success()
        : (Id.PrimaryAssetType == UGamePlatformPrimaryDataAsset::DefinitionAssetType()
            ? FGamePlatformResult::Success()
            : FGamePlatformResult::Failure(TEXT("PCGInvalidDefinitionReference"), TEXT("PCG配置引用必须指向GamePlatformDefinition主资产。")));
}

/**
 * PCG Definition（定义）字段中的主资产引用必须同时登记到基类 RequiredDefinitions（必需定义）。
 * GamePlatformData（平台数据）只对 RequiredDefinitions 建立递归租约；这里只校验，不另建加载器。
 */
FGamePlatformResult ValidateDeclaredDependency(
    const UGamePlatformDefinitionBase& Owner,
    const FPrimaryAssetId& Id)
{
    const FGamePlatformResult Ref = ValidateAssetIdIfSet(Id);
    if (!Ref.IsSuccess() || !Id.IsValid())
    {
        return Ref;
    }

    if (!Owner.RequiredDefinitions.Contains(Id))
    {
        return FGamePlatformResult::Failure(
            TEXT("PCGUndeclaredDefinitionDependency"),
            TEXT("PCG字段引用必须同步登记到RequiredDefinitions，才能由GamePlatformData建立统一租约。"));
    }

    return FGamePlatformResult::Success();
}
}

FGamePlatformResult UGamePlatformPCGExecPresetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }

    if (RequiredPCGSchemaMajor != FGamePlatformPCGSchema::CurrentVersion().Major)
    {
        return FGamePlatformResult::Failure(TEXT("PCGSchemaVersionMismatch"), TEXT("ExecPreset要求的PCG Schema主版本与运行时代码不一致。"));
    }

    if (!FMath::IsFinite(GridSizeCm) || GridSizeCm < 100.0f || MaxComponentsPerBatch <= 0 || MaxPointsPerExecution <= 0 ||
        !FMath::IsFinite(FrameBudgetMs) || FrameBudgetMs <= 0.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidExecutionBudget"), TEXT("PCG执行预设存在非法Grid或预算参数。"));
    }

    if (bHierarchicalGeneration)
    {
        return FGamePlatformResult::Unsupported(TEXT("PCGHiGenDeferred"), TEXT("HiGen属于M2能力，M0/M1不得提前启用。"));
    }

    if (NetPolicy != EGamePlatformPCGNetPolicy::ClientGen)
    {
        return FGamePlatformResult::Unsupported(TEXT("PCGNetPolicyDeferred"), TEXT("1.0当前只允许ClientGen纯表现策略；服务器权威与复制状态仍为后续阶段。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGMeshSetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (Entries.IsEmpty())
    {
        return FGamePlatformResult::Failure(TEXT("PCGEmptyMeshSet"), TEXT("MeshSet至少需要一个真实网格条目。"));
    }

    float TotalWeight = 0.0f;
    for (const FGamePlatformPCGMeshSetEntry& Entry : Entries)
    {
        if (Entry.Mesh.IsNull() || !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0.0f ||
            !FMath::IsFinite(Entry.CullDistanceCm) || Entry.CullDistanceCm < 0.0f)
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidMeshSetEntry"), TEXT("MeshSet包含空网格、非法权重或非法剔除距离。"));
        }
        TotalWeight += Entry.Weight;
    }

    return FMath::IsFinite(TotalWeight) && TotalWeight > 0.0f
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(TEXT("PCGInvalidMeshSetWeight"), TEXT("MeshSet总权重必须为有限正数。"));
}

FGamePlatformResult UGamePlatformPCGSpawnPolicyDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!FMath::IsFinite(Density) || Density < 0.0f || Density > 1.0f ||
        !IsFiniteRange(UniformScaleRange) || UniformScaleRange.X <= 0.0 ||
        !IsFiniteRange(SlopeDegrees) || SlopeDegrees.X < 0.0 || SlopeDegrees.Y > 90.0 ||
        !IsFiniteRange(HeightCm) || !FMath::IsFinite(SelfPruneDistanceCm) || SelfPruneDistanceCm < 0.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidSpawnPolicy"), TEXT("SpawnPolicy密度、缩放、坡度、高度或自修剪参数非法。"));
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGLayerDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (LayerName.IsNone())
    {
        return FGamePlatformResult::Failure(TEXT("PCGLayerNameMissing"), TEXT("PCG Layer必须具有稳定层名称。"));
    }

    for (const FPrimaryAssetId& Id : {MeshSetId, SpawnPolicyId})
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Id);
        if (!Ref.IsSuccess()) { return Ref; }
    }

    TSet<FPrimaryAssetId> UniqueChildren;
    for (const FPrimaryAssetId& Child : Children)
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Child);
        if (!Ref.IsSuccess()) { return Ref; }
        if (!Child.IsValid() || Child == GetPrimaryAssetId() || UniqueChildren.Contains(Child))
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidLayerChild"), TEXT("Layer子层不能无效、自引用或重复；递归环由GamePlatformData依赖图继续校验。"));
        }
        UniqueChildren.Add(Child);
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGBiomePresetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (BiomeId.IsNone() || LayerIds.IsEmpty())
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidBiome"), TEXT("BiomePreset必须声明BiomeId并至少引用一个Layer。"));
    }

    TSet<FPrimaryAssetId> Unique;
    for (const FPrimaryAssetId& LayerId : LayerIds)
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, LayerId);
        if (!Ref.IsSuccess() || !LayerId.IsValid() || Unique.Contains(LayerId))
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidBiomeLayer"), TEXT("BiomePreset包含无效或重复的Layer定义。"));
        }
        Unique.Add(LayerId);
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGExclusionPresetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (SourceId.IsNone() || !FMath::IsFinite(Strength) || Strength < 0.0f || Strength > 1.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidExclusionPreset"), TEXT("ExclusionPreset必须具有来源ID，强度必须位于0..1。"));
    }
    return FGamePlatformResult::Success();
}

UGamePlatformPCGPriorityTableDefinition::UGamePlatformPCGPriorityTableDefinition()
{
    const auto MakeEntry = [](FName LayerId, int32 Priority)
    {
        FGamePlatformPCGPriorityEntry Entry;
        Entry.LayerId = LayerId;
        Entry.Priority = Priority;
        return Entry;
    };

    Entries =
    {
        MakeEntry(TEXT("ManualLock"), 100),
        MakeEntry(TEXT("GameplayExclusion"), 90),
        MakeEntry(TEXT("MajorGate"), 82),
        MakeEntry(TEXT("Connector"), 80),
        MakeEntry(TEXT("MajorRoad"), 70),
        MakeEntry(TEXT("RoadGuard"), 68),
        MakeEntry(TEXT("WaterBody"), 60),
        MakeEntry(TEXT("YardWall"), 55),
        MakeEntry(TEXT("Parcel"), 50),
        MakeEntry(TEXT("FieldFence"), 48),
        MakeEntry(TEXT("MinorRoad"), 40),
        MakeEntry(TEXT("Canopy"), 30),
        MakeEntry(TEXT("Crop"), 20),
        MakeEntry(TEXT("DecorFence"), 18),
        MakeEntry(TEXT("RockProp"), 15),
        MakeEntry(TEXT("InterfaceBand"), 10),
        MakeEntry(TEXT("GroundCover"), 5)
    };
}

bool UGamePlatformPCGPriorityTableDefinition::ResolvePriority(FName LayerId, int32& OutPriority) const
{
    if (const FGamePlatformPCGPriorityEntry* Entry = Entries.FindByPredicate(
        [LayerId](const FGamePlatformPCGPriorityEntry& Candidate) { return Candidate.LayerId == LayerId; }))
    {
        OutPriority = Entry->Priority;
        return true;
    }
    return false;
}

FGamePlatformResult UGamePlatformPCGPriorityTableDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (Entries.IsEmpty())
    {
        return FGamePlatformResult::Failure(TEXT("PCGEmptyPriorityTable"), TEXT("PriorityTable不能为空。"));
    }

    TSet<FName> Names;
    TSet<int32> Values;
    for (const FGamePlatformPCGPriorityEntry& Entry : Entries)
    {
        if (Entry.LayerId.IsNone() || Entry.Priority < 0 || Entry.Priority > 100 || Names.Contains(Entry.LayerId) || Values.Contains(Entry.Priority))
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidPriorityTable"), TEXT("PriorityTable层名和优先级必须唯一，优先级范围为0..100。"));
        }
        Names.Add(Entry.LayerId);
        Values.Add(Entry.Priority);
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGRoadProfileDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!FMath::IsFinite(WidthCm) || WidthCm <= 0.0f || !FMath::IsFinite(ShoulderCm) || ShoulderCm < 0.0f ||
        !FMath::IsFinite(DitchCm) || DitchCm < 0.0f || !FMath::IsFinite(MaxSlopeDegrees) || MaxSlopeDegrees < 0.0f ||
        MaxSlopeDegrees > 90.0f || !FMath::IsFinite(MaxCurvature) || MaxCurvature < 0.0f || PriorityLayerId.IsNone())
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidRoadProfile"), TEXT("RoadProfile宽度、路肩、边沟、坡度、曲率或优先级层非法。"));
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGEnclosureProfileDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!FMath::IsFinite(HeightCm) || HeightCm <= 0.0f || !FMath::IsFinite(PostSpacingCm) || PostSpacingCm <= 0.0f ||
        !FMath::IsFinite(MaxStepCm) || MaxStepCm < 0.0f || !FMath::IsFinite(SlopeLimitDegrees) ||
        SlopeLimitDegrees < 0.0f || SlopeLimitDegrees > 90.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidEnclosureProfile"), TEXT("EnclosureProfile高度、柱距、步进或坡度限制非法。"));
    }

    for (const FGamePlatformPCGSpanMeshRule& Rule : SpanMeshRules)
    {
        if (Rule.MeshSetId.IsNone() || !FMath::IsFinite(Rule.MinLengthCm) || !FMath::IsFinite(Rule.MaxLengthCm) ||
            Rule.MinLengthCm < 0.0f || Rule.MaxLengthCm < Rule.MinLengthCm)
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidSpanMeshRule"), TEXT("围合跨度网格规则存在空ID或非法长度范围。"));
        }
    }

    for (const FPrimaryAssetId& Id : {PostMeshSetId, SpanMeshSetId, GateCatalogId})
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Id);
        if (!Ref.IsSuccess()) { return Ref; }
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGParcelPresetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!FMath::IsFinite(InsetCm) || InsetCm < 0.0f)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidParcelInset"), TEXT("ParcelPreset内缩距离必须为有限非负数。"));
    }
    for (const FPrimaryAssetId& Id : {EdgeEnclosureProfileId, InnerSystemDefinitionId})
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Id);
        if (!Ref.IsSuccess()) { return Ref; }
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGCropProfileDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }
    if (!FMath::IsFinite(RowSpacingCm) || RowSpacingCm <= 0.0f || !FMath::IsFinite(PlantSpacingCm) || PlantSpacingCm <= 0.0f ||
        !FMath::IsFinite(RowYawDegrees) || SeasonalMeshSetIds.Num() > 4)
    {
        return FGamePlatformResult::Failure(TEXT("PCGInvalidCropProfile"), TEXT("CropProfile垄距、株距、朝向或季节网格数量非法。"));
    }
    for (const FPrimaryAssetId& Id : SeasonalMeshSetIds)
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Id);
        if (!Ref.IsSuccess()) { return Ref; }
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPCGConnectorCatalogDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) { return Base; }

    TSet<FName> Items;
    for (const FGamePlatformPCGConnectorCatalogEntry& Entry : Entries)
    {
        const FGamePlatformResult Ref = ValidateDeclaredDependency(*this, Entry.ContentDefinitionId);
        if (Entry.ItemId.IsNone() || Items.Contains(Entry.ItemId) || !Ref.IsSuccess() ||
            !FMath::IsFinite(Entry.MinSpanCm) || !FMath::IsFinite(Entry.MaxSpanCm) ||
            Entry.MinSpanCm < 0.0f || Entry.MaxSpanCm < Entry.MinSpanCm)
        {
            return FGamePlatformResult::Failure(TEXT("PCGInvalidConnectorCatalog"), TEXT("ConnectorCatalog存在重复ID、非法定义引用或非法跨度范围。"));
        }
        Items.Add(Entry.ItemId);
    }
    return FGamePlatformResult::Success();
}
