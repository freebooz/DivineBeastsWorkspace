#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformPCGEnvironmentTypes.h"
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
};

UCLASS(Blueprintable)
class GAMEPLATFORMPCG_API AGamePlatformPCGPolygonActor : public AGamePlatformPCGActorBase
{
    GENERATED_BODY()
public:
    AGamePlatformPCGPolygonActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG")
    TObjectPtr<USplineComponent> Boundary;
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

private:
    UPROPERTY(EditInstanceOnly, Category="GamePlatform|PCG|Director")
    TArray<TObjectPtr<AGamePlatformPCGActorBase>> Participants;
};
