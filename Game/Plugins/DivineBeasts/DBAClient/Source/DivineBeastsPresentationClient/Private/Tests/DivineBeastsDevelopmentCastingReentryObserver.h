// 项目客户端Editor回归专用观察者：接收UE真实Montage同步通知，不播放动画、不生成权威结果。
// 仅由Private/Tests夹具创建，Transient且无生产自动注册；一次动作先取走再执行，防止递归重复触发。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DivineBeastsDevelopmentCastingReentryObserver.generated.h"

class UAnimMontage;

#if WITH_EDITORONLY_DATA
/** 游戏线程测试观察者；夹具负责解绑，既有技能、Data、目录及资产均不由本对象修改。 */
UCLASS(Transient, NotBlueprintable)
class UDivineBeastsDevelopmentCastingReentryObserver final : public UObject
{
    GENERATED_BODY()
public:
    /** 仅本夹具安装的同步动作；参数是引擎实际启动的Montage，空动作只记录通知。 */
    TFunction<void(UAnimMontage*)> StartedAction;
    /** 实际启动通知总数，用于区分重试VFX和重复播放Montage。 */
    int32 StartedNotifications = 0;
    /** UE动态通知真实签名；取走动作后允许同步取消、后继发布和生成角色。 */
    UFUNCTION()
    void HandleMontageStarted(UAnimMontage* Montage)
    {
        ++StartedNotifications;
        auto Action = MoveTemp(StartedAction);
        if (Action) Action(Montage);
    }
};
#endif
