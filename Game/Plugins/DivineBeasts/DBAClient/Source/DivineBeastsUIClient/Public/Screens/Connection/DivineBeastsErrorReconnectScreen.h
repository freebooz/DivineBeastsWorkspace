#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsErrorReconnectScreen.generated.h"

class UButton;
class UTextBlock;
class UDivineBeastsUIViewModel;

/**
 * UDivineBeastsErrorReconnectScreen（神兽联盟错误与重连页面）。
 *
 * 仅承载展示和Retry（重试）意图：错误来自项目只读ViewModel，
 * 重试仍由ApplicationFlow（应用流程）与后端执行，不在Widget中计算网络状态。
 * Widget蓝图由DBAUIPack_Core通过Monolith创建，不允许C++动态伪造视图。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsErrorReconnectScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    /** 返回稳定的项目页面身份；蓝图交付状态由Monolith清单及实际资产验收证明。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Connection")
    FName GetErrorReconnectScreenId() const
    {
        return TEXT("UI.Screen.ErrorReconnect");
    }

    /** 获取只读错误文本；不向玩家暴露服务端堆栈或内部错误编码。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Connection")
    FText GetDisplayErrorText() const;

protected:
    /** 平台激活门闩后绑定按钮和精确VM命令通知；状态事件继续使用平台唯一订阅。 */
    virtual void BindUIEvents() override;
    /** 先失效重试作用域/精确解绑，再由父类执行业务取消，重入后不尾写后继。 */
    virtual void UnbindUIEvents() override;
    /** 激活与合法VM替换均刷新；Revision为状态序号，PageGeneration为VM页面代次。 */
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;

private:
    /** 当前平台VM的项目类型只读视图；未设置或错类型返回nullptr，不建立第二份业务状态。 */
    UDivineBeastsUIViewModel* GetProjectViewModel() const;
    /** 游戏线程只刷新激活页的安全错误文本与按钮资格，不负责后端恢复或自动重试。 */
    void RefreshErrorPresentation();
    /** 同步当前VM/Page的命令通知订阅，返回只表示本页仍处于可见作用域。 */
    bool ReconcileRetryBindings();
    /** 校验按钮/命令属于当前激活页及精确VM，所有操作限定游戏线程。 */
    bool IsRetryScopeCurrent() const;

    /** 按钮仅提交现有Retry命令，网络重试策略由流程层负责。 */
    UFUNCTION()
    void HandleRetryClicked();
    /** 保留原反射签名；当前VM页面代次匹配后刷新，不另建状态订阅。 */
    UFUNCTION()
    void HandleViewStateChanged(int32 Revision, int32 PageGeneration);
    /** 精确请求终态；同步受理栈中先记ID，异步终态只结束当前已登记重试。 */
    UFUNCTION()
    void HandleRetryCompleted(FGuid RequestId, FName ErrorCode);

    /** Monolith实际蓝图须存在同名焦点按钮，与目录的RetryButton保持一致。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UButton> RetryButton = nullptr;

    /** Monolith实际蓝图须存在同名错误文本，默认仅展示已本地化的信息。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> ErrorText = nullptr;

    /** 避免多次点击以及同步命令完成回调造成请求状态反转。 */
    FGuid ActiveRetryRequestId;
    /** 按钮和命令通知属于当前可见周期；失活先失效，再精确解绑。 */
    bool bRetryEventsBound = false;
    /** 当前Retry外部调用尚未返回；不能将返回受理误作后端已恢复。 */
    bool bSubmittingRetry = false;
    /** 命令通知精确绑定的VM，合法换VM不能从当前GetViewModel反推旧订阅。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsUIViewModel> BoundRetryViewModel = nullptr;
    /** VM自增页面代次，0表示没有绑定；无时间/帧号语义。 */
    int32 BoundRetryPageGeneration = 0;
    /** 按钮/VM作用域身份，失活与合法换VM递增，旧栈不能改后继。 */
    uint64 RetryScopeGeneration = 0;
    /** 每次提交及取消递增的操作身份，同一可见周期也不能尾写新提交。 */
    uint64 RetrySubmissionGeneration = 0;
    /** 同步完成可在Retry返回前到达；只按最终返回RequestId判断本次是否已经完成。 */
    TSet<FGuid> CompletedRequestsDuringSubmit;
};
