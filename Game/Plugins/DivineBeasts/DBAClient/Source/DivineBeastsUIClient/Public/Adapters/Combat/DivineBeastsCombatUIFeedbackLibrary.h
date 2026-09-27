#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "Types/GamePlatformCombatEvent.h"
#include "DivineBeastsCombatUIFeedbackLibrary.generated.h"

/**
 * UDivineBeastsCombatUIFeedbackLibrary（神兽联盟战斗UI反馈映射库）。
 *
 * 边界：
 * - 输入必须是“已经到达当前客户端”的战斗事实。
 * - GamePlatformCombatComponent::OnCombatEvent 普通委托本身不跨网络，本类不会把它误当复制机制。
 * - 服务器事实应通过既有复制状态、GAS GameplayCue（玩法表现通知）或明确客户端适配器到达后再调用本映射。
 *
 * 本类只把项目战斗语义映射为平台中立 FeedbackRequest（反馈请求），
 * 具体字体、颜色、动画由 DBA UI Definition / Widget Blueprint 决定。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsCombatUIFeedbackLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 把客户端可见战斗事实转换为伤害/治疗/死亡等浮动反馈请求。
     * ControlApplied/ControlRemoved等不适合飘字的事件返回false。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    static bool BuildFloatingTextRequest(
        const FGamePlatformCombatEvent& Event,
        FGamePlatformUIFeedbackRequest& OutRequest);
};
