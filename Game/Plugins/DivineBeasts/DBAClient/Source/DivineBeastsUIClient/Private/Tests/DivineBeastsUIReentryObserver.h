// 项目UI Editor Automation专用同步重入观察者；仅监听真实VM事件，不实现Provider/后端或固定成功。
// 与已实际编译的DivineBeastsCharacterConfigurationTestFixture采用相同UHT条件：EditorOnlyData反射类型。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DivineBeastsUIReentryObserver.generated.h"

#if WITH_EDITORONLY_DATA
/** Transient且不可作为蓝图父类；开发测试显式创建/解绑，没有生产自动注册或实例。 */
UCLASS(Transient, NotBlueprintable)
class UDivineBeastsUIReentryObserver final : public UObject
{
    GENERATED_BODY()
public:
    /** GT测试安装的一次状态动作；回调先取走再调用，避免重入销毁当前函数对象。 */
    TFunction<void()> StateAction;
    /** GT测试安装的一次命令终态动作；只消费真实RequestId，不生成命令结果。 */
    TFunction<void(FGuid)> CommandAction;
    /** 实际收到通知的次数，无帧/时间单位；用于证实真实调用发生。 */
    int32 StateNotifications = 0;
    int32 CommandNotifications = 0;
    /** 最近一次实际命令完成身份，默认无效；不是生产请求账本。 */
    FGuid LastCompletedRequestId;
    /** 动态状态委托真实签名；Revision/页面代次由生产VM维护，观察者仅驱动测试动作。 */
    UFUNCTION()
    void HandleStateChanged(int32 Revision, int32 PageGeneration);
    /** 动态命令委托真实签名；ErrorCode为生产同步失败码，本观察者不修改它。 */
    UFUNCTION()
    void HandleCommandCompleted(FGuid RequestId, FName ErrorCode);
};
#endif
