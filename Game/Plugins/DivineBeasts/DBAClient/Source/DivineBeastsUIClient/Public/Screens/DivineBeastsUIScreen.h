#pragma once

#include "CoreMinimal.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Contracts/DivineBeastsUIDomainTypes.h"
#include "DivineBeastsUIScreen.generated.h"

class UDivineBeastsUIViewModel;

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
    /** 十大业务域标识仅用于视觉导航、审计与内容归属，不是权限依据。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Domain")
    EDivineBeastsUIDomain GetBusinessDomain() const { return UIDomain; }

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|UI|Domain")
    EDivineBeastsUIDomain UIDomain = EDivineBeastsUIDomain::Core;

public:
    /** 页面资源失败时的可见反馈，仅为客户端显示状态，不改变应用流程或认证状态。 */
    void ShowPageLoadError(const FText& Message);
    const FText& GetPageLoadError() const { return PageLoadError; }

protected:
    /** 平台已验证激活后绑定项目VM；失活在平台撤资格后精确解绑原VM，游戏线程。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    /** 合法换VM/初始刷新经平台钩子同步项目订阅，不能借用当前VM去解绑旧绑定。 */
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
private:
    /** 协调唯一项目VM订阅；旧VM取消命令可同步重入，返回仅继续本次仍有效的选择。 */
    bool ReconcileProjectViewModelBinding();
    /** 精确拥有OnScreenActivated建立的项目状态订阅；Transient不改变资产或保存合同。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsUIViewModel> BoundProjectViewModel = nullptr;
    /** 捕获VM自增页面代次；0仅表示尚无绑定，不是帧号或时间单位。 */
    int32 BoundProjectPageGeneration = 0;
    /** 本页项目订阅操作身份；换VM/解绑递增，外部取消返回只认原值。 */
    uint64 ProjectBindingGeneration = 0;
    /** 平台已授权的激活订阅周期；失活先置false，防回调复活旧绑定。 */
    bool bProjectEventsBound = false;
    FText PageLoadError;
};
