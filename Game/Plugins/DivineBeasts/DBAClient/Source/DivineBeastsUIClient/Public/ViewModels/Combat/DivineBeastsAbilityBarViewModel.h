#pragma once

#include "CoreMinimal.h"
#include "Components/GamePlatformSlotWidget.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "DivineBeastsAbilityBarViewModel.generated.h"

class UDivineBeastsAbilityLoadoutComponent;
class UDivineBeastsAbilityUIProfile;
struct FDivineBeastsAbilityLoadoutState;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsAbilityBarChangedNative, const TArray<FGamePlatformUISlotState>&);

/**
 * UDivineBeastsAbilityBarViewModel（生肖技能栏视图模型）。
 * 只订阅服务器 OwnerOnly（仅拥有者）技能快照，从项目客户端 UI 主资产异步补充图标；
 * 不直接 GiveAbility（授予技能），不依据客户端静态列表伪造可释放技能。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAbilityBarViewModel final
    : public UDivineBeastsViewModelBase
{
    GENERATED_BODY()

public:
    /** 切换本地玩家 Pawn/ASC 时先解绑旧作用域；无有效组件返回失败。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    bool BindToLoadout(UDivineBeastsAbilityLoadoutComponent* InLoadout);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    void UnbindFromLoadout();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FGamePlatformUISlotState> GetSlots() const { return Slots; }

    const TArray<FGamePlatformUISlotState>& GetSlotsRef() const { return Slots; }
    FDivineBeastsAbilityBarChangedNative& OnSlotsChanged() { return SlotsChanged; }

    virtual void BeginDestroy() override;

private:
    void HandleLoadoutChanged(const FDivineBeastsAbilityLoadoutState& Snapshot);
    void HandleProfileLoaded(int32 ExpectedLoadGeneration, FName ExpectedHero);
    void RefreshSlots(const FDivineBeastsAbilityLoadoutState& Snapshot);
    void ResetProfileLease();

    TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> Loadout;
    FDelegateHandle LoadoutHandle;

    /** 强引用只在本次 UI 作用域有效，回调检查代次避免跨角色污染。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityUIProfile> LoadedProfile = nullptr;

    TSharedPtr<FStreamableHandle> ProfileHandle;
    int32 LoadGeneration = 0;
    FName LoadedHeroId = NAME_None;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> Slots;

    FDivineBeastsAbilityBarChangedNative SlotsChanged;
};
