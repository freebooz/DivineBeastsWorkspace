#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Initialization/GamePlatformCharacterInitializer.h"
#include "Identity/DivineBeastsZodiacIdentity.h"
#include "DivineBeastsCharacterComponent.generated.h"

struct FStreamableHandle;
class UDivineBeastsHeroDefinition;

/** FDivineBeastsCharacterReadinessChangedNative（项目角色就绪变化）。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsCharacterReadinessChangedNative,
    bool);

/**
 * UDivineBeastsCharacterComponent（神兽联盟项目角色组件）。
 * 负责可信运行身份、Definition lease、移动/碰撞配置、Generation和Readiness；
 * 不拥有ASC、技能、伤害、AI Brain、装备或表现资源。
 */
UCLASS(ClassGroup=(DivineBeasts), meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSCHARACTERSRUNTIME_API UDivineBeastsCharacterComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDivineBeastsCharacterComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 仅服务器可信Spawn/Admission路径调用；客户端无RPC自报入口。 */
    bool AuthorityBindTrustedContext(
        const FGamePlatformCharacterInitializationContext& Context,
        FString& OutError);

    /** 任意身份/Definition/Generation变化后可重复调用，幂等重评估。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Character")
    void RefreshInitialization();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    FName GetHeroDefinitionId() const { return HeroDefinitionId; }

    /** 仅Owner得到持久CharacterId；远端观察者默认不复制。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    FString GetCharacterId() const { return CharacterId; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    EDivineBeastsZodiacIdentity GetZodiacIdentity() const
    {
        return ZodiacIdentity;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    int32 GetSpawnGeneration() const { return SpawnGeneration; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    int32 GetAvatarGeneration() const { return AvatarGeneration; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Character")
    bool IsCharacterReady() const
    {
        return GetOwner() && GetOwner()->HasAuthority()
            ? bServerReady
            : bServerReady && bLocalReady;
    }

    const UDivineBeastsHeroDefinition* GetLoadedDefinition() const
    {
        return LoadedDefinition;
    }

    FDivineBeastsCharacterReadinessChangedNative& OnReadinessChanged()
    {
        return ReadinessChanged;
    }

private:
    UFUNCTION()
    void OnRep_Identity();

    UFUNCTION()
    void OnRep_Generation();

    UFUNCTION()
    void OnRep_ServerReady();

    void BeginDefinitionLoad();
    void CancelDefinitionLease();
    void HandleDefinitionLoaded(
        UDivineBeastsHeroDefinition* Definition,
        int32 ExpectedRequestGeneration,
        int32 ExpectedSpawnGeneration,
        int32 ExpectedAvatarGeneration);

    bool ApplyDefinition(
        const UDivineBeastsHeroDefinition& Definition,
        FString& OutError);

    bool IsIdentityStructurallyValid() const;
    void UpdateReadiness();
    void BroadcastReadinessIfChanged(bool bPreviousReady);

    /** Owner-only持久身份，避免向所有观察者复制PlayerData档案ID。 */
    UPROPERTY(ReplicatedUsing=OnRep_Identity, Transient)
    FString CharacterId;

    UPROPERTY(ReplicatedUsing=OnRep_Identity, Transient)
    FName HeroDefinitionId = NAME_None;

    UPROPERTY(ReplicatedUsing=OnRep_Identity, Transient)
    EDivineBeastsZodiacIdentity ZodiacIdentity =
        EDivineBeastsZodiacIdentity::Rat;

    UPROPERTY(ReplicatedUsing=OnRep_Generation, Transient)
    int32 SpawnGeneration = 0;

    UPROPERTY(ReplicatedUsing=OnRep_Generation, Transient)
    int32 AvatarGeneration = 0;

    UPROPERTY(Replicated, Transient)
    bool bPersistentCharacterIdRequired = true;

    UPROPERTY(ReplicatedUsing=OnRep_ServerReady, Transient)
    bool bServerReady = false;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsHeroDefinition> LoadedDefinition = nullptr;

    bool bLocalReady = false;
    bool bConfigurationApplied = false;
    int32 DefinitionRequestGeneration = 0;
    TSharedPtr<FStreamableHandle> DefinitionLease;

    FDivineBeastsCharacterReadinessChangedNative ReadinessChanged;
};
