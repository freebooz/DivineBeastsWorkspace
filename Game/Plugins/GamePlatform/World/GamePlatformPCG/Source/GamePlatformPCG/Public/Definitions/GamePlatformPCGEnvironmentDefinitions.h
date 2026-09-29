#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Engine/StaticMesh.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"
#include "GamePlatformPCGEnvironmentDefinitions.generated.h"

/** 单个网格目录条目；具体网格仍由Definition资产持有，不通过PCG属性传任意SoftPath。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGMeshSetEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(AssetBundles="PCGGeneration"))
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float Weight = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    bool bStaticCollision = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    bool bCastShadow = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float CullDistanceCm = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    bool bNanitePreferred = true;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGPriorityEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    FName LayerId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    int32 Priority = 0;
};

/** 按跨度选择网格目录项的规则；用于围栏、桥栏和码头栏片。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGSpanMeshRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    FName MeshSetId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float MinLengthCm = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float MaxLengthCm = 100.0f;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGConnectorCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    EGamePlatformPCGConnectorType Type = EGamePlatformPCGConnectorType::Gate;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    FName ItemId = NAME_None;

    /** 指向平台Definition体系中的组合件/目录定义；1.0不新建第二套Assembly资产根。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG")
    FPrimaryAssetId ContentDefinitionId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float MinSpanCm = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="0.0"))
    float MaxSpanCm = 100000.0f;
};

/** 执行预设：声明分区、预算、烘焙和端侧策略，不替代引擎PCG调度器。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGExecPresetDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution", meta=(ClampMin="1"))
    int32 RequiredPCGSchemaMajor = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution", meta=(ClampMin="100.0"))
    float GridSizeCm = 12800.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    int32 GridBand = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    int32 LodBand = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    bool bPartitioned = false;

    /** M0/M1保持false；M2通过专项验证后才允许打开。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    bool bHierarchicalGeneration = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    bool bRuntimeGeneration = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution", meta=(ClampMin="1"))
    int32 MaxComponentsPerBatch = 64;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution", meta=(ClampMin="1"))
    int32 MaxPointsPerExecution = 65536;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution", meta=(ClampMin="0.1"))
    float FrameBudgetMs = 4.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    FName DataLayerId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    FName HLODLayerId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Execution")
    EGamePlatformPCGNetPolicy NetPolicy = EGamePlatformPCGNetPolicy::ClientGen;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGMeshSetDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|MeshSet")
    TArray<FGamePlatformPCGMeshSetEntry> Entries;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGSpawnPolicyDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Density = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn")
    FVector2D UniformScaleRange = FVector2D(1.0, 1.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn")
    FVector2D SlopeDegrees = FVector2D(0.0, 90.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn")
    FVector2D HeightCm = FVector2D(-1000000.0, 1000000.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn")
    bool bKeepVertical = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Spawn", meta=(ClampMin="0.0"))
    float SelfPruneDistanceCm = 0.0f;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGLayerDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Layer")
    FName LayerName = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Layer")
    int32 LayerIndex = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Layer")
    FPrimaryAssetId MeshSetId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Layer")
    FPrimaryAssetId SpawnPolicyId;

    /** 子层通过平台Definition依赖加载；完整递归环由GamePlatformData租约图负责拒绝。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Layer")
    TArray<FPrimaryAssetId> Children;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGBiomePresetDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Biome")
    FName BiomeId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Biome")
    int32 Seed = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Biome")
    TArray<FPrimaryAssetId> LayerIds;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGExclusionPresetDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion")
    FName SourceId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion")
    EGamePlatformPCGExclusionMode Mode = EGamePlatformPCGExclusionMode::Hard;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Strength = 1.0f;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGPriorityTableDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UGamePlatformPCGPriorityTableDefinition();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Priority")
    TArray<FGamePlatformPCGPriorityEntry> Entries;

    bool ResolvePriority(FName LayerId, int32& OutPriority) const;
    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGRoadProfileDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road", meta=(ClampMin="1.0"))
    float WidthCm = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road", meta=(ClampMin="0.0"))
    float ShoulderCm = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road", meta=(ClampMin="0.0"))
    float DitchCm = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road", meta=(ClampMin="0.0", ClampMax="90.0"))
    float MaxSlopeDegrees = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road", meta=(ClampMin="0.0"))
    float MaxCurvature = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Road")
    FName PriorityLayerId = TEXT("MinorRoad");

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGEnclosureProfileDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    EGamePlatformPCGEnclosureKind Kind = EGamePlatformPCGEnclosureKind::Fence;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure", meta=(ClampMin="1.0"))
    float HeightCm = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure", meta=(ClampMin="1.0"))
    float PostSpacingCm = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    FPrimaryAssetId PostMeshSetId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    FPrimaryAssetId SpanMeshSetId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    FPrimaryAssetId GateCatalogId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    bool bFollowTerrain = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure", meta=(ClampMin="0.0"))
    float MaxStepCm = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure", meta=(ClampMin="0.0", ClampMax="90.0"))
    float SlopeLimitDegrees = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Enclosure")
    TArray<FGamePlatformPCGSpanMeshRule> SpanMeshRules;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGParcelPresetDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Parcel")
    EGamePlatformPCGParcelUse Use = EGamePlatformPCGParcelUse::Field;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Parcel", meta=(ClampMin="0.0"))
    float InsetCm = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Parcel")
    FPrimaryAssetId EdgeEnclosureProfileId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Parcel")
    FPrimaryAssetId InnerSystemDefinitionId;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGCropProfileDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Crop", meta=(ClampMin="1.0"))
    float RowSpacingCm = 60.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Crop", meta=(ClampMin="1.0"))
    float PlantSpacingCm = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Crop")
    float RowYawDegrees = 0.0f;

    /** 春夏秋冬四套MeshSet；允许为空表示该季节沿用项目层回退。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Crop")
    TArray<FPrimaryAssetId> SeasonalMeshSetIds;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGConnectorCatalogDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Connector")
    TArray<FGamePlatformPCGConnectorCatalogEntry> Entries;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
