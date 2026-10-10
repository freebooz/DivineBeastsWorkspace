#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"
#include "Services/GamePlatformPCGAdvancedSpatialRules.h"
#include "GamePlatformPCGAdvancedDefinitions.generated.h"

/**
 * P4生态/湖岸/岩组与P5聚落共用的领域配置。
 * 只描述领域、所属阶段和平台Definition依赖；不重复Water、Weather、Surface及VFX系统。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGWorldFeatureDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 只能使用FGamePlatformPCGDomainIds登记的稳定领域；不允许项目名称硬编码到底层类。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature")
    FName DomainId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature")
    EGamePlatformPCGWorldStage Stage = EGamePlatformPCGWorldStage::InterfaceBands;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature")
    FPrimaryAssetId MeshSetDefinitionId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature")
    FPrimaryAssetId SpawnPolicyDefinitionId;

    /** 静态岸线/林缘/过渡带最大偏移半宽，厘米；0表示不额外扩张。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature", meta=(ClampMin="0.0"))
    float BandWidthCm = 0.0f;

    /** 玩法碰撞仅由通过Cook与权威审查的静态结果提供；M2不会进行运行时地形写入。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Feature")
    bool bEditorStaticOnly = true;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

/** P5组合件的一个确定性局部插槽；所有资源仍按Definition租约加载，不把网格硬引用存入平台协议。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGAssemblySlot
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    FName SlotId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    FTransform RelativeTransform = FTransform::Identity;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    FPrimaryAssetId MeshSetDefinitionId;
};

/** P5院落、屋组、岩组通用组合契约；最多64个稳定插槽，逻辑归平台，具体美术/地图归项目内容包。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGAssemblyDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    FName DomainId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    TArray<FGamePlatformPCGAssemblySlot> Slots;

    /** 建筑物必须人工验证地块、通行/退让，PCG不自行确定玩法出生与导航规则。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Assembly")
    bool bRequiresNavigationReview = true;

    virtual FGamePlatformResult ValidateDefinition() const override;
};


/**
 * P5组合件位置解析：将审核过的局部Slot变换投影到指定的静态放置器根变换。
 * 返回的只是可审计候选位置，不创建Actor/网格，也不覆盖其它世界生成所有权。
 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGAssemblyRules
{
    static bool ResolveSlotTransforms(const UGamePlatformPCGAssemblyDefinition& Definition,
        const FTransform& RootTransform, TArray<FTransform>& OutWorldTransforms);
};

/** P7体腔/洞穴当前仅能描述排除体积；不可逆地形写入必须由单独审查的Landscape工具完成。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGCavityDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Cavity")
    FGamePlatformPCGCavityExclusion Exclusion;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Cavity")
    bool bRequestTerrainWrite = false;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

/** P8室内/地牢只发布可验证的拓扑关系；可视模型仍由审核过的项目Assembly与地图生产。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGSpatialGraphDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|SpatialGraph")
    TArray<FGamePlatformPCGSpaceNode> Nodes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|SpatialGraph")
    TArray<FGamePlatformPCGSpaceEdge> Edges;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|SpatialGraph")
    FName EntryNodeId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|SpatialGraph")
    FName ExitNodeId = NAME_None;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

/** P5玩法锚点生成的通用策略；位置仅为候选，准入由服务器Gameplay/Interaction审批。 */
UCLASS(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGAnchorPolicyDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Anchors")
    FName DomainId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Anchors", meta=(ClampMin="1.0"))
    float MinSeparationCm = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Anchors", meta=(ClampMin="1", ClampMax="4096"))
    int32 MaxCandidates = 128;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|PCG|Anchors")
    bool bRequireServerApproval = true;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
