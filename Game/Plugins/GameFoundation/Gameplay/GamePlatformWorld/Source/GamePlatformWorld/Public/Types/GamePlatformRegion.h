#pragma once
#include "Types/GamePlatformWorldContext.h"
#include "UObject/PrimaryAssetId.h"

/** 区域/订阅/贡献者均使用世界代次加随机登记ID，撤销不接受跨世界句柄。 */
struct FGamePlatformWorldRegistration
{
    FGuid ContextGeneration; // 所属世界代次。
    FGuid RegistrationId; // 此次登记身份，不复用整数槽。
    bool IsValid() const { return ContextGeneration.IsValid() && RegistrationId.IsValid(); }
};
/** 一个逻辑区域只允许一个Provider；边界厘米闭区间，重复注册明确失败。 */
struct FGamePlatformRegionProvider
{
    FGamePlatformId RegionId; // 必须与已加载RegionDefinition的LogicalId一致。
    FGuid ContextGeneration; // 调用者读取的当前代次，过期拒绝。
    TWeakObjectPtr<UObject> Provider; // 必须属于当前世界；销毁自动撤销。
    FBox Bounds = FBox(ForceInit); // 非退化有限轴对齐盒，单位厘米。
    int32 Priority = 0; // 重叠先高优先级、再较小体积，同级同体积报告歧义。
    FPrimaryAssetId DefinitionId; // 必须是WorldDefinition声明的区域定义。
};
/** 每个事件绑定显式观察者；仅本地通知，不是网络复制或权威玩法结算。 */
struct FGamePlatformRegionEvent
{
    FGuid ContextGeneration; // 事件所属代次。
    TWeakObjectPtr<UObject> Observer; // 不延长观察者生命周期。
    FGamePlatformId RegionId; // 进入/离开的逻辑身份。
    bool bEntered = false; // false表示离开；切换先离开后进入。
};
