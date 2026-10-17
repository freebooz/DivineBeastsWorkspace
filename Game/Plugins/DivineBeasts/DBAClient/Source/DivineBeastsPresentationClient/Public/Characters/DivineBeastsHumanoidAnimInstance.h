// SSLM人型角色动画实例：复用平台移动输入，仅用Combat通知驱动死亡分支。
#pragma once

#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "DivineBeastsHumanoidAnimInstance.generated.h"

class UGamePlatformCombatComponent;

/** Kachujin与Arissa的动画基类；战斗生死是只读事实，客户端动画无权改写生命。 */
UCLASS(Transient, Blueprintable)
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsHumanoidAnimInstance
    : public UGamePlatformLocomotionAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient, BlueprintReadOnly, Category="SSLM|Animation")
    bool bCombatDead = false;

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUninitializeAnimation() override;

private:
    TWeakObjectPtr<UGamePlatformCombatComponent> ObservedCombat;
    FDelegateHandle CombatSubscription;
    void HandleCombatChanged();
    void ReleaseCombatSubscription();
};
