// 平台双端中立施法事实；角色组件复制，客户端只读，不包含项目资源、伤害或动画控制对象。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformAbilityCastFacts.generated.h"

/** 一次服务器确认的有限表现窗口；时间单位秒，晚到观察者仅重建剩余窗口。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityCastFacts
{
    GENERATED_BODY()
    /** 每次技能激活的唯一身份；零值表示从未发布，不作为客户端授权凭据。 */
    UPROPERTY(BlueprintReadOnly) FGuid ActivationId;
    /** 中立技能逻辑身份及表现风格身份；不是资源路径，平台不解释项目语义。 */
    UPROPERTY(BlueprintReadOnly) FName AbilityId;
    UPROPERTY(BlueprintReadOnly) FName PresentationStyleId;
    /** 权威角色化身代次，正整数；旧代事实不能用于重生后的角色。 */
    UPROPERTY(BlueprintReadOnly) int32 AvatarGeneration = 0;
    /** GameState同步的服务器世界时间；无GameState的隔离/单机世界使用World时间。 */
    UPROPERTY(BlueprintReadOnly) double StartedServerSeconds = 0;
    /** 0..10秒的有效窗口；只约束表现事实，绝不调度伤害。 */
    UPROPERTY(BlueprintReadOnly) float DurationSeconds = 0;
    /** 活跃/取消或自然过期；终态保留原身份供观察者撤销其自有表现。 */
    UPROPERTY(BlueprintReadOnly) bool bActive = false;
};
