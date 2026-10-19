// SSLM项目客户端狼动画适配：AnimBP拥有资源，本实例只投影移动和复制攻击/死亡事实。
// 仅DivineBeastsPresentationClient编译；服务器不链接此类或动画资产，不由动画调度Combat。
#pragma once

#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "DivineBeastsVillageWolfAnimInstance.generated.h"

class ADivineBeastsVillageWolf;
class UAnimMontage;
class UGamePlatformAbilityCastComponent;
struct FGamePlatformAbilityCastFacts;

/** 由Village狼AnimBP派生；实例随Mesh初始化/卸载绑定和解绑，资源租约仍归外观世界装配。 */
UCLASS(Transient, Blueprintable)
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsVillageWolfAnimInstance : public UGamePlatformLocomotionAnimInstance
{
    GENERATED_BODY()
public:
    /** 骨骼动画求值的厘米/秒输入；只读移动速度，不查询生命/伤害/攻击业务。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="SSLM|VillageWolf")
    float WolfSpeed = 0.0f;

    /** 仅由服务器死亡复制通知写入，AnimBP据此进入非循环死亡状态并保持末帧。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="SSLM|VillageWolf")
    bool bWolfDead = false;

    /** 内容包AnimBP默认值指定同骨架DefaultSlot咬击Montage；不允许网络事实拼接资源路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|VillageWolf")
    TObjectPtr<UAnimMontage> BiteMontage;

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUninitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    TWeakObjectPtr<ADivineBeastsVillageWolf> ObservedWolf;
    TWeakObjectPtr<UGamePlatformAbilityCastComponent> ObservedCast;
    FDelegateHandle LifeSubscription;
    FDelegateHandle CastSubscription;
    /** 同一次复制、重复绑定快照或迟到取消不能重播已消费的攻击。 */
    FGuid ConsumedBite;
    FGuid PlayingBite;
    bool bClosing = true;

    /** 解绑旧Mesh作用域；只停止本实例自己的Montage，不触碰服务器资格。 */
    void ReleaseBindings();
    /** 核对事件来源和Mesh归属后应用生命快照；死亡优先于任何尚在途咬击。 */
    void HandleLifeChanged(ADivineBeastsVillageWolf* Wolf);
    /** 仅消费本实体当前复制快照；按服务器时间补偿晚到窗口并拒绝已过期攻击。 */
    void HandleCastChanged(UGamePlatformAbilityCastComponent* Component, const FGamePlatformAbilityCastFacts& Facts);
};
