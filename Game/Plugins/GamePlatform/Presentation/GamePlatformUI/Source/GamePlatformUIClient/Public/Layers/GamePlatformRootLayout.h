#pragma once

#include "Layers/GamePlatformUILayerStack.h"
#include "GamePlatformRootLayout.generated.h"

class UTextBlock;

/**
 * UGamePlatformRootLayout（游戏平台根布局基类）。
 *
 * 职责：
 * - 作为每个 LocalPlayer 唯一 UI 根容器的语义扩展点。
 * - 复用 LayerStack（层栈）中的 HUD、Screen、Modal、System、Notification、Loading、Debug 层。
 * - 向 Blueprint 提供当前响应式布局等级，便于 PC / Mobile 使用同一根布局逻辑。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformRootLayout
    : public UGamePlatformUILayerStack
{
    GENERATED_BODY()

public:
    /** 返回当前响应式布局等级；未取得自适应上下文时返回 Regular（标准）。 */
    UFUNCTION(BlueprintPure, Category="UI|RootLayout")
    EGamePlatformUILayoutClass GetCurrentLayoutClass() const
    {
        return GetAdaptiveContext().LayoutClass;
    }

protected:
    /** 绑定可选FrameRateText控件；只在显式开发启动开关启用时订阅引擎帧结束通知。 */
    virtual void NativeConstruct() override;
    /** 根布局销毁前撤销通知，禁止旧根布局继续更新新世界的界面。 */
    virtual void NativeDestruct() override;

private:
    /** 统计真实渲染循环的帧结束事件，每0.25秒发布一次FPS文本，不查询任何业务状态。 */
    void HandleFrameRateEndFrame();
    /** 成对释放引擎通知及弱控件引用；重复Construct/Destruct保持幂等。 */
    void UnbindFrameRateNotifications();

    TWeakObjectPtr<UTextBlock> FrameRateText; // 视觉由第三层Monolith资产拥有，平台不创建布局。
    FDelegateHandle FrameRateEndFrameHandle; // 当前根布局拥有的帧结束通知。
    double FrameRateSampleStartSeconds = 0.0; // 当前采样窗口的单调时钟起点，单位秒。
    uint32 FrameRateSampleFrames = 0; // 本采样窗口实际收到的帧结束次数。
    int32 LastDisplayedFrameRate = INDEX_NONE; // 已发布整数FPS；相同值不重复修改文本。
};
