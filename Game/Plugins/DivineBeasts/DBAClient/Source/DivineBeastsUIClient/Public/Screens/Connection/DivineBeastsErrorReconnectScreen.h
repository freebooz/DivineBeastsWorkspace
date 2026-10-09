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
    /** 返回稳定的项目页面身份；此时只实现C++父类，未暗示已存在对应蓝图。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Connection")
    FName GetErrorReconnectScreenId() const
    {
        return TEXT("UI.Screen.ErrorReconnect");
    }

    /** 获取只读错误文本；不向玩家暴露服务端堆栈或内部错误编码。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Connection")
    FText GetDisplayErrorText() const;

protected:
    /** 仅在页面激活期间订阅重试按钮及当前ViewModel事件。 */
    virtual void NativeOnActivated() override;
    /** 页面失活时取消待处理命令并精确解绑，旧回调不得重新启用页面。 */
    virtual void NativeOnDeactivated() override;

private:
    UDivineBeastsUIViewModel* GetProjectViewModel() const;
    void RefreshErrorPresentation();

    /** 按钮仅提交现有Retry命令，网络重试策略由流程层负责。 */
    UFUNCTION()
    void HandleRetryClicked();
    UFUNCTION()
    void HandleViewStateChanged(int32 Revision, int32 PageGeneration);
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
    bool bRetryEventsBound = false;
    bool bSubmittingRetry = false;
    bool bRetryCompletedDuringSubmit = false;
};
