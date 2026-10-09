#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/GamePlatformGameplayEligibilityProvider.h"
#include "GamePlatformGameplayEligibilityComponent.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformGameplayEligibilitySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly)
    int32 AvatarGeneration = 1;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformGameplayEligibilityChangedNative,
    const FGamePlatformGameplayEligibilitySnapshot&);

/**
 * 最小可组合Gameplay Active状态组件。
 * 仅提供Active与AvatarGeneration事实，不拥有登录、出生、死亡或重生流程。
 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMGAMEPLAY_API UGamePlatformGameplayEligibilityComponent final
    : public UActorComponent
    , public IGamePlatformGameplayEligibilityProvider
{
    GENERATED_BODY()

public:
    UGamePlatformGameplayEligibilityComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual bool IsServerPlayerActiveForGameplay() const override
    {
        return bServerPlayerActive;
    }

    virtual int32 GetGameplayAvatarGeneration() const override
    {
        return AvatarGeneration;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gameplay")
    void SetServerPlayerActive(bool bActive);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gameplay")
    void AdvanceAvatarGeneration();

    /** 游戏线程权威出生适配器绑定正整数Avatar代次；必须先Inactive，旧代次/越界拒绝，不改变Active。 */
    bool BindServerAvatarGeneration(int32 NewAvatarGeneration);

    FGamePlatformGameplayEligibilitySnapshot GetSnapshot() const
    {
        FGamePlatformGameplayEligibilitySnapshot Result;
        Result.bActive = bServerPlayerActive;
        Result.AvatarGeneration = AvatarGeneration;
        return Result;
    }

    FGamePlatformGameplayEligibilityChangedNative& OnEligibilityChanged()
    {
        return EligibilityChanged;
    }

private:
    UFUNCTION()
    void OnRep_ServerPlayerActive();

    UFUNCTION()
    void OnRep_AvatarGeneration();

    void BroadcastSnapshot();

    UPROPERTY(ReplicatedUsing=OnRep_ServerPlayerActive)
    bool bServerPlayerActive = false;

    UPROPERTY(ReplicatedUsing=OnRep_AvatarGeneration)
    int32 AvatarGeneration = 1;

    FGamePlatformGameplayEligibilityChangedNative EligibilityChanged;
};
