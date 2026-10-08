#pragma once

#include "CoreMinimal.h"
#include "Screens/GamePlatformUIScreen.h"
#include "DivineBeastsUIScreen.generated.h"

/**
 * UDivineBeastsUIScreen（神兽联盟项目页面基类）。
 *
 * 职责：
 * - 作为神兽联盟所有普通业务 Screen（页面）的统一项目层父类。
 * - 视觉 Blueprint 必须继承本类或更具体的 Menu / Modal / Loading 项目基类。
 * - 实际页面栈、输入焦点和自适应事件仍由 GamePlatformUI 统一管理。
 *
 * 性能约束：
 * - 不启用业务 Tick。
 * - 页面激活/失活时才绑定/解绑 ViewModel 与项目事件。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()

public:
    /** 页面资源失败时的可见反馈，仅为客户端显示状态，不改变应用流程或认证状态。 */
    void ShowPageLoadError(const FText& Message);
    const FText& GetPageLoadError() const { return PageLoadError; }

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
private:
    FText PageLoadError;
};
