#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformHeroDefinition.generated.h"

/** FGamePlatformCharacterSpawnEnvelope（平台角色出生包络）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCHARACTER_API FGamePlatformCharacterSpawnEnvelope
{
    GENERATED_BODY()

    /** 胶囊半径，单位cm。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Spawn", meta=(ClampMin="1.0"))
    float CapsuleRadius = 42.0f;

    /** 胶囊半高，单位cm。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Spawn", meta=(ClampMin="1.0"))
    float CapsuleHalfHeight = 96.0f;

    /** 碰撞Profile（碰撞配置）逻辑名。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Spawn")
    FName CollisionProfileName = TEXT("Pawn");

    bool IsValid(FString& OutError) const;
};

/** FGamePlatformCharacterMovementDefinition（平台角色移动定义）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCHARACTER_API FGamePlatformCharacterMovementDefinition
{
    GENERATED_BODY()

    /** 最大步行速度，单位cm/s。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Movement", meta=(ClampMin="1.0"))
    float MaxWalkSpeed = 600.0f;

    /** 最大加速度，单位cm/s²。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Movement", meta=(ClampMin="1.0"))
    float MaxAcceleration = 2048.0f;

    /** 跳跃初速度，单位cm/s。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Movement", meta=(ClampMin="0.0"))
    float JumpZVelocity = 420.0f;

    /** Yaw旋转速度，单位deg/s。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Movement", meta=(ClampMin="0.0"))
    float RotationRateYaw = 360.0f;

    /** 是否允许基础Crouch（蹲伏）；不代表项目实现冲刺/攀爬等自定义移动。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Movement")
    bool bCanCrouch = true;

    bool IsValid(FString& OutError) const;
};

/**
 * UGamePlatformHeroDefinition（平台通用英雄定义）。
 * Server-safe：只允许稳定逻辑ID、出生包络和基础移动参数，不硬绑定Mesh/VFX/SFX/UI/Ability类。
 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMCHARACTER_API UGamePlatformHeroDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 跨系统稳定Definition ID（定义编号）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero")
    FName DefinitionId = NAME_None;

    /** Schema Version（结构版本）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ClampMin="1"))
    int32 Version = 1;

    /** Content Revision（内容修订），用于客户端/服务器内容对账。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero")
    FString ContentRevision = TEXT("1");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero|Spawn")
    FGamePlatformCharacterSpawnEnvelope SpawnEnvelope;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero|Movement")
    FGamePlatformCharacterMovementDefinition Movement;

    /** 逻辑外观Profile ID；具体视觉资源由表现/Content Pack解析。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero|Presentation")
    FName AppearanceProfileId = NAME_None;

    /** 逻辑表现Profile ID；禁止在本Definition硬引用Niagara/Sound/UI资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero|Presentation")
    FName PresentationProfileId = NAME_None;

    /** 骨架兼容逻辑ID；不是SkeletalMesh硬引用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero|Presentation")
    FName SkeletonCompatibilityId = NAME_None;

    /** 其它Definition稳定ID依赖，供DeveloperTools聚合检查。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero")
    TArray<FName> RequiredDefinitions;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    bool IsDefinitionValid(FString& OutError) const;
};
