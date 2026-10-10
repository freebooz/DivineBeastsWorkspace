#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"
#include "GamePlatformCombatFeedbackWorldSubsystem.generated.h"

/**
 * 客户端已确认战斗表现事实观察者。
 * World作用域使多个PIE世界、切图和多LocalPlayer不会共享进程级战斗状态。
 * 普通本地委托只在当前客户端分发；网络入口由权威CombatComponent实现。
 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformConfirmedCombatFeedback,
    const FGamePlatformCombatEvent&);

/**
 * UGamePlatformCombatFeedbackWorldSubsystem（战斗反馈世界总线）。
 * 所有可选表现子系统仅观察事件，不在这里结算生命、技能、控制或击退。
 */
UCLASS()
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatFeedbackWorldSubsystem
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 单一客户端世界的权威事实观察事件；订阅方必须按世界退出注销。 */
    FGamePlatformConfirmedCombatFeedback& OnConfirmedFeedback()
    {
        return ConfirmedFeedback;
    }

    /** 仅提供当前世界内的经过基本验证的已确认表现事实。 */
    void DispatchConfirmedFeedback(const FGamePlatformCombatEvent& Event);

    virtual void Deinitialize() override;

private:
    /** 只记有限个Guid和类型位，不存Actor或可复制Gameplay状态。 */
    TMap<FGuid, uint8> SeenFeedbackTypes;
    TArray<FGuid> FeedbackRing;
    int32 NextFeedbackSlot = 0;
    static constexpr int32 MaxRememberedFeedback = 2048;

    FGamePlatformConfirmedCombatFeedback ConfirmedFeedback;
};
