#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsAbilityLoadoutComponent.generated.h"

class UAbilitySystemComponent;
class UDivineBeastsCharacterComponent;
class UGamePlatformAbilitySetDefinition;
class IGamePlatformDataService;

/**
 * FDivineBeastsGrantedAbilitySlot（服务器已授予技能的最小 UI 安全事实）。
 * 这是授予结果的只读 owner-only 副本，绝不代替 ASC 中实际的 AbilitySpec。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSABILITIESRUNTIME_API FDivineBeastsGrantedAbilitySlot
{
    GENERATED_BODY()

    /** 规范稳定技能 ID，来源于 AbilitySet 内 FGamePlatformId。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    FName AbilityId = NAME_None;

    /** 输入标签；被动技能可以为空，空输入不允许由槽位直接激活。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    FGameplayTag InputTag;

    /** 当前由服务端授权的等级，不从客户端图标或 Tooltip 读取。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    int32 AbilityLevel = 0;

    /** 与 Slot/AbilityId/InputTag 互不混淆的稳定 UI 槽位身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    FName SlotId = NAME_None;
};

/** FDivineBeastsAbilityLoadoutState（同一角色代次的原子技能栏授权快照）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSABILITIESRUNTIME_API FDivineBeastsAbilityLoadoutState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    FName HeroDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    int32 AvatarGeneration = 0;

    /** 服务器单调修订；新角色代次允许重置。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    int64 Revision = 0;

    /** false 表示技能仍未授予、尚在请求或已失败。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    bool bReady = false;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Abilities")
    TArray<FDivineBeastsGrantedAbilitySlot> Slots;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsAbilityLoadoutChangedNative,
    const FDivineBeastsAbilityLoadoutState&);

/**
 * UDivineBeastsAbilityLoadoutComponent（神兽联盟已授权技能装配组件）。
 * 只由服务器可信角色身份和已有 DataService（统一数据服务）读取真实 AbilitySet；
 * 同一代次幂等，技能类以 AbilitySet Bundle 异步加载。动态调用 GAS GiveAbility；
 * 仅向拥有者复制 UI 所需的最小授权状态，客户端无法通过本组件授予或结算技能。
 */
UCLASS(ClassGroup=(DivineBeasts), meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsAbilityLoadoutComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDivineBeastsAbilityLoadoutComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 当前本地/拥有者只读投影。未就绪状态不能视作已授权。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Abilities")
    FDivineBeastsAbilityLoadoutState GetLoadoutState() const { return LoadoutState; }

    const FDivineBeastsAbilityLoadoutState& GetLoadoutStateRef() const { return LoadoutState; }
    FDivineBeastsAbilityLoadoutChangedNative& OnLoadoutChanged() { return Changed; }

    /** 供可信服务器组合点重复调用；非服务器调用返回 false。 */
    bool AuthorityRefreshFromCharacter(FString& OutError);

private:
    UFUNCTION()
    void OnRep_LoadoutState();

    void HandleCharacterReadinessChanged(bool bReady);
    void HandleAbilitySetLoaded(const FGamePlatformDataLease& CompletedLease,
        int32 ExpectedRequestSerial, FName ExpectedHero, int32 ExpectedAvatar);
    bool ApplyAbilitySet(const UGamePlatformAbilitySetDefinition& Set, FString& OutError);
    void RevokeOwnedGrants();
    void PublishLoadout(FName HeroDefinitionId, int32 AvatarGeneration,
        bool bReady, const TArray<FDivineBeastsGrantedAbilitySlot>& Slots);
    IGamePlatformDataService* FindDataService() const;

    /** 有效的已有 ASC；此组件不创建第二套 GAS 组件。 */
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;
    FDelegateHandle ReadinessHandle;
    int32 RequestSerial = 0;
    FName PendingSetId = NAME_None;
    FGamePlatformDataLease DefinitionLease;
    TArray<FGameplayAbilitySpecHandle> OwnedAbilityHandles;
    TArray<FActiveGameplayEffectHandle> OwnedEffectHandles;

    /** 只有主人的客户端收到该快照；角色/技能标识不包含私有账号信息。 */
    UPROPERTY(ReplicatedUsing=OnRep_LoadoutState, Transient)
    FDivineBeastsAbilityLoadoutState LoadoutState;

    FDivineBeastsAbilityLoadoutChangedNative Changed;
};
