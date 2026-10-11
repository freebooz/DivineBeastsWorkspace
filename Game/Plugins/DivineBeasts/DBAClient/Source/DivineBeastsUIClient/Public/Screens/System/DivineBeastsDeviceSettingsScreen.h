// 第三层系统设置显示/输入适配，所有设备命令归ViewModel；不在Widget里保存、轮询或构造视觉树。
#pragma once
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsDeviceSettingsScreen.generated.h"
class UButton;
class UComboBoxString;
class UTextBlock;
class UDivineBeastsSystemSettingsViewModel;

/** 实际显示设置页；分辨率/窗口模式/整体画质使用现有平台预览合同，15秒未确认自动回滚。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsDeviceSettingsScreen : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    /** 只指定System业务域；具体控件和所有视觉均由Monolith资产拥有。 */
    UDivineBeastsDeviceSettingsScreen();
protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
    virtual bool NativeOnHandleBackAction() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    /** Monolith绑定的稳定控件名，候选值仅在按应用时作为一次命令读取。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> ResolutionCombo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> WindowModeCombo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> QualityCombo;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ApplyButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ConfirmButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RevertButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> BackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
private:
    UDivineBeastsSystemSettingsViewModel* SettingsVM() const;
    void RefreshPresentation();
    /** 快照变化才重建表单；保留未提交的用户下拉选择，不随业务状态刷新覆盖。 */
    int64 PresentedDeviceRevision = -1;
    UFUNCTION() void HandleApply();
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleRevert();
    UFUNCTION() void HandleBack();
};
