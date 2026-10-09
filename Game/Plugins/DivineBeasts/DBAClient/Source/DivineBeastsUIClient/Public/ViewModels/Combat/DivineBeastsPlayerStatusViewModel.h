#pragma once

#include "CoreMinimal.h"
#include "Contracts/DivineBeastsPlayerStatusUIContracts.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "DivineBeastsPlayerStatusViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsPlayerStatusChangedNative,
    const FDivineBeastsPlayerStatusViewData&);

/**
 * UDivineBeastsPlayerStatusViewModel（神兽联盟玩家状态视图模型）。
 * 仅订阅生命与气势GAS属性事件，不维护盾数值；护盾以GameplayEffect状态图标显示。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPlayerStatusViewModel final
    : public UDivineBeastsViewModelBase
{
    GENERATED_BODY()

public:
    /** 绑定当前玩家 ASC；可重复调用，内部先解绑旧 ASC。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    bool BindToAbilitySystem(UAbilitySystemComponent* InAbilitySystem);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    void UnbindFromAbilitySystem();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Combat")
    FDivineBeastsPlayerStatusViewData GetStatus() const { return Status; }

    const FDivineBeastsPlayerStatusViewData& GetStatusRef() const { return Status; }
    FDivineBeastsPlayerStatusChangedNative& OnStatusChanged() { return StatusChanged; }

    virtual void BeginDestroy() override;

private:
    void RefreshSnapshot(bool bForceBroadcast);
    void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FDivineBeastsPlayerStatusViewData Status;
    FDivineBeastsPlayerStatusChangedNative StatusChanged;

    FDelegateHandle HealthHandle;
    FDelegateHandle MaxHealthHandle;
    FDelegateHandle MomentumHandle;
    FDelegateHandle MaxMomentumHandle;
};
