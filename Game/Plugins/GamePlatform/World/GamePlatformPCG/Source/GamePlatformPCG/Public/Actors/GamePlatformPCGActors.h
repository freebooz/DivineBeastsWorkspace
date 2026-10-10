#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "Services/GamePlatformPCGAnchorContracts.h"
#include "GamePlatformPCGActors.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraph;
class USceneComponent;
class USplineComponent;

/**
 * AGamePlatformPCGActorBase（PCG放置器基类）。
 * 只保存通用生成身份、图/Profile/ExecPreset和阶段信息；项目内容通过Child BP/Data Asset扩展。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGActorBase : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformPCGActorBase();

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<UPCGComponent> PCGComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    EGamePlatformPCGPrimitive Primitive = EGamePlatformPCGPrimitive::P1_Scatter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    EGamePlatformPCGWorldStage WorldStage = EGamePlatformPCGWorldStage::Scatter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TSoftObjectPtr<UPCGGraph> Graph;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    FPrimaryAssetId ProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    FPrimaryAssetId ExecPresetId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG", meta=(ClampMin="1"))
    int32 RequiredSchemaMajor = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    int32 Seed = 1;

    /** 正式模板默认锁定；项目层只能使用Graph Instance或受控参数，不复制逻辑图。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    bool bLockedGraph = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    EGamePlatformPCGBakeState BakeState = EGamePlatformPCGBakeState::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    FGamePlatformId OwnerRegionId;

    /** 编辑器实例稳定来源ID；不作为网络授权或玩家身份。 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, NonPIEDuplicateTransient, Category="GamePlatform|PCG")
    FGuid SourceId;
    /** 选填领域ID，玩法候选锚点必须使用Play.Resource/Cover/Climb/Spawn之一。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Domain")
    FName DomainId = NAME_None;
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGVolumeActor : public AGamePlatformPCGActorBase
{
    GENERATED_BODY()
public:
    AGamePlatformPCGVolumeActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<UBoxComponent> Bounds;
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGSplineActor : public AGamePlatformPCGActorBase
{
    GENERATED_BODY()
public:
    AGamePlatformPCGSplineActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<USplineComponent> Spline;
    /** 道路、林径等线性系统写入确定性二维排除缓冲区；只供低优先级生成层使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial")
    bool bExportsSpatialMask = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0.0"))
    float CarveHalfWidthCm = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0", ClampMax="100"))
    int32 CarvePriority = 70;
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGPolygonActor : public AGamePlatformPCGActorBase
{
    GENERATED_BODY()
public:
    AGamePlatformPCGPolygonActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<USplineComponent> Boundary;
    /** 地块/院落内部排除低优先级乔木；允许项目关闭以便生成叠加地被。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial")
    bool bExportsSpatialMask = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0", ClampMax="100"))
    int32 CarvePriority = 50;
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGConnectorActor : public AGamePlatformPCGActorBase
{
    GENERATED_BODY()
public:
    AGamePlatformPCGConnectorActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Connector")
    EGamePlatformPCGConnectorType ConnectorType = EGamePlatformPCGConnectorType::Gate;

    /** GateOverride（强制门）等需求统一通过覆盖模式表达，不新建项目专属C++ Actor。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Connector")
    EGamePlatformPCGOverrideMode OverrideMode = EGamePlatformPCGOverrideMode::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Connector")
    FPrimaryAssetId ConnectorCatalogId;
    /** 门、手摆桥的保留区域半径，防止自动树木/作物与连接件碰撞。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0.0"))
    float CarveRadiusCm = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0", ClampMax="100"))
    int32 CarvePriority = 80;
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGExclusionActor : public AGamePlatformPCGVolumeActor
{
    GENERATED_BODY()
public:
    AGamePlatformPCGExclusionActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion")
    FName ExclusionSource = TEXT("ManualLock");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Strength = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Exclusion", meta=(ClampMin="0", ClampMax="100"))
    int32 CarvePriority = 100;
};

/**
 * AGamePlatformPCGWorldDirector（PCG世界编排器）。
 * 只持有显式参与者、阶段顺序和校验入口；不扫描全世界Actor，也不复制WorldSubsystem请求生命周期。
 */
UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGWorldDirector : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformPCGWorldDirector();

    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Director")
    bool RegisterParticipant(AGamePlatformPCGActorBase* Participant);

    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Director")
    bool UnregisterParticipant(AGamePlatformPCGActorBase* Participant);

    UFUNCTION(BlueprintPure, Category="GamePlatform|PCG|Director")
    TArray<AGamePlatformPCGActorBase*> GetParticipantsForStage(EGamePlatformPCGWorldStage Stage) const;

    UFUNCTION(BlueprintPure, Category="GamePlatform|PCG|Director")
    bool ValidateParticipantSet(FString& OutError) const;
    /**
     * 只读取已注册参与者的世界空间数值快照，不扫描地图其它Actor、不启动官方PCG任务。
     * 输出优先级排序确定、空间几何有界；非法/缺失样条会失败关闭，不产生半套掩码。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Director")
    bool CollectSpatialMasks(TArray<FGamePlatformPCGSpatialMask>& OutMasks, FString& OutError) const;

    /**
     * 生成固定阶段的确定性参与者计划；M0/M1不执行TerrainWrite、挖填、室内及动态状态。
     * 真正的图生成仍由UE PCGComponent和编辑器工具承担，Director不取代官方调度器。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Director")
    bool BuildStaticExecutionPlan(TArray<AGamePlatformPCGActorBase*>& OutPlan, FString& OutError) const;
    /**
     * P5从显式注册的GameplayAnchors放置器产生稳定空间候选。
     * 结果必须再交服务器Gameplay/Navigation/Interaction审批；不直接执行玩家出生或资源发放。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Director")
    bool CollectGameplayAnchorCandidates(const FGamePlatformId& WorldId,
        const FGamePlatformId& RegionId, int32 ContentRevision,
        TArray<FGamePlatformPCGAnchorCandidate>& OutCandidates, FString& OutError) const;

private:
    UPROPERTY(EditInstanceOnly, Category="GamePlatform|PCG|Director")
    TArray<TObjectPtr<AGamePlatformPCGActorBase>> Participants;
};
