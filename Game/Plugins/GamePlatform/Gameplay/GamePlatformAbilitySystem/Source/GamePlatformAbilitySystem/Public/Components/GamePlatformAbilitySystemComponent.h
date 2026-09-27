#pragma once

#include "AbilitySystemComponent.h"
#include "GamePlatformAbilitySystemComponent.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityAvatarBindingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 AvatarGeneration = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bBound = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAbilityAvatarBindingChangedNative,
    const FGamePlatformAbilityAvatarBindingSnapshot&);

class AActor;

/** 平台级 ASC 基类；不包含具体游戏技能、伤害公式或输入绑定。 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UGamePlatformAbilitySystemComponent();

    UFUNCTION(BlueprintCallable, Category="GamePlatform|AbilitySystem")
    bool BindAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

    UFUNCTION(BlueprintCallable, Category="GamePlatform|AbilitySystem")
    void ClearAbilityAvatar();

    FGamePlatformAbilityAvatarBindingSnapshot GetAvatarBindingSnapshot() const
    {
        FGamePlatformAbilityAvatarBindingSnapshot Result;
        Result.AvatarGeneration = AvatarGeneration;
        Result.bBound = AbilityActorInfo.IsValid() && AbilityActorInfo->AvatarActor.IsValid();
        return Result;
    }

    FGamePlatformAbilityAvatarBindingChangedNative& OnAvatarBindingChanged()
    {
        return AvatarBindingChanged;
    }

private:
    void BroadcastAvatarBinding();

    int32 AvatarGeneration = 0;
    FGamePlatformAbilityAvatarBindingChangedNative AvatarBindingChanged;
};
