#pragma once

#include "Screens/Account/DivineBeastsAccountScreenBase.h"
#include "DivineBeastsLoginScreen.generated.h"

class UDivineBeastsLoginViewModel;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UWidget;

/**
 * UDivineBeastsLoginScreen（神兽联盟登录页面 C++ 基类）。
 *
 * 职责：
 * - 作为 WBP_DBA_UI_Login 等登录视觉蓝图的唯一项目父类。
 * - 复用平台 Screen 的焦点、输入模式、PC/Mobile 自适应和事件驱动 ViewModel 生命周期。
 * - 不直接调用认证服务；登录意图必须经 UDivineBeastsLoginViewModel 提交。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoginScreen
    : public UDivineBeastsAccountScreenBase
{
    GENERATED_BODY()

public:
    /** 跨项目语义化样式默认绑定；具体外观由DBAUIPack_Core资源主题统一决定。 */
    UDivineBeastsLoginScreen();
    /** 返回稳定页面标识，与项目 UI Catalog 保持一致。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Login")
    FName GetLoginScreenId() const
    {
        return TEXT("UI.Screen.Login");
    }

    /** 返回类型安全的登录 ViewModel；类型不匹配时返回 nullptr。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Login")
    UDivineBeastsLoginViewModel* GetLoginViewModel() const;

protected:
    /** 页面激活时绑定一次控件与ViewModel事件，并立即刷新当前只读快照。 */
    virtual void NativeOnActivated() override;

    /**
     * Development（开发）运行时从命令行注入测试账号默认值。
     * Shipping 编译中为空操作；不写资产、不写配置、不进入日志/遥测。
     */
    void ApplyDevelopmentCredentialDefaults();

    /** 页面失活时解绑全部事件、淘汰当前请求身份并清空密码。 */
    virtual void NativeOnDeactivated() override;

private:
    /** 绑定命名控件和当前登录ViewModel；重复调用不会重复订阅。 */
    void BindLoginEvents();

    /** 解绑本页面建立的事件；不影响ViewModel或其他页面订阅者。 */
    void UnbindLoginEvents();

    /** 根据最新事件快照更新按钮、忙碌、维护和错误显示。 */
    void RefreshLoginPresentation();

    /** 清空只允许瞬时存在的密码输入；账号名可以保留在当前页面控件中。 */
    void ClearSensitiveInput();

    /** 登录按钮事件：读取一次输入、提交命令并立即清空密码。 */
    UFUNCTION()
    void HandleLoginClicked();

    /** 输入文本变化只更新按钮可用状态，不提交网络请求。 */
    UFUNCTION()
    void HandleCredentialTextChanged(const FText& NewText);

    /** ViewModel状态事件；修订号和页面代次由ViewModel保证单调与隔离。 */
    UFUNCTION()
    void HandleViewStateChanged(int32 Revision, int32 PageGeneration);

    /** 当前页面命令完成事件；只清理本页面发出的登录请求身份。 */
    UFUNCTION()
    void HandleCommandCompleted(FGuid RequestId, FName ErrorCode);

    /** 账号输入框；名称同时是Screen Definition的默认焦点目标。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UEditableTextBox> AccountInput = nullptr;

    /** 密码输入框；Widget资产必须启用密码遮蔽，内容不得持久化。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UEditableTextBox> PasswordInput = nullptr;

    /** 登录提交按钮；可用性由输入有效性和ViewModel状态共同决定。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UButton> LoginButton = nullptr;

    /** 登录错误文本；只显示已本地化的只读错误，不显示技术堆栈。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> ErrorText = nullptr;

    /** 忙碌提示控件；具体视觉类型由Monolith生成的Widget资产决定。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UWidget> BusyIndicator = nullptr;

    /** 维护提示文本；是否显示由认证状态事件决定。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> MaintenanceText = nullptr;

    /** 本页面当前登录请求；仅用于抑制状态事件返回前的重复点击。 */
    FGuid ActiveLoginRequestId;

    /** 防止激活事件重复绑定动态委托。 */
    bool bLoginEventsBound = false;

    /** 标记Login调用栈尚未返回，用于识别同步拒绝回调。 */
    bool bSubmittingLogin = false;

    /** Login调用返回前若已收到完成事件，返回后不得重新标记为待处理。 */
    bool bLoginCompletedDuringSubmit = false;
    /** 仅开发回归的一次性提交意图；Shipping不会读取启动参数或环境密码。 */
    bool bDevelopmentAutoLoginRequested = false;
};
