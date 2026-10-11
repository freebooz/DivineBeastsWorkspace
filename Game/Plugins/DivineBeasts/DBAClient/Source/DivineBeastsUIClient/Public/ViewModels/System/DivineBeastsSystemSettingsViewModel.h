// 项目客户端设备设置视图：平台Settings拥有持久值，本地页面只拥有订阅和一次预览；全接口游戏线程。
#pragma once
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Types/GamePlatformDeviceSettingsTypes.h"
#include "Containers/Ticker.h"
#include "DivineBeastsSystemSettingsViewModel.generated.h"

class UGameInstance;
class IGamePlatformDeviceSettingsService;

/** 系统设置命令与只读快照；失活/旅行/15秒未确认均撤销本页预览，不改权威游戏状态。 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSystemSettingsViewModel : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()
public:
    /** 当前已应用/暂存快照；服务不存在时bDeviceSettingsAvailable=false，不能宣称设置成功。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="DivineBeasts|UI|Settings") FGamePlatformDeviceSettingsSnapshot DeviceSnapshot;
    /** 当前设备真实分辨率列表，附加当前窗口尺寸；空列表禁止分辨率命令。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="DivineBeasts|UI|Settings") TArray<FIntPoint> SupportedResolutions;
    UPROPERTY(Transient, BlueprintReadOnly, Category="DivineBeasts|UI|Settings") bool bDeviceSettingsAvailable = false;
    /** 最近一次命令的可显示反馈，无账号或凭据。 */
    UPROPERTY(Transient, BlueprintReadOnly, Category="DivineBeasts|UI|Settings") FText DeviceError;
    /** 校验后应用不写盘预览；失败返回false并保留原设置，15秒内必须确认。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Settings") bool PreviewDeviceSettings(FGamePlatformDeviceSettings Candidate);
    /** 只确认本页拥有的预览并经平台写盘；无预览/失败返回false。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Settings") bool ConfirmDeviceSettings();
    /** 回滚本页预览，不撤销其他调用者的设置。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Settings") bool RevertDeviceSettings();
protected:
    virtual void OnPageBegan() override;
    virtual void OnPageEnded() override;
private:
    IGamePlatformDeviceSettingsService* GetDeviceService() const;
    bool PublishResult(const FGamePlatformResult& Result);
    void CleanupDeviceSettings();
    TWeakObjectPtr<UGameInstance> SettingsGameInstance;
    FGamePlatformDeviceSettingsSubscription DeviceSubscription;
    /** 仅本页预览期间存在的CoreTicker；确认/回滚/失活精确撤销，不使用世界时间或Widget Tick。 */
    FTSTicker::FDelegateHandle PreviewTimeout;
    /** FPlatformTime单调真实秒截止点；0表示无预览，不受Pause或TimeDilation影响。 */
    double PreviewDeadlineSeconds = 0.0;
    /** 设置服务回调中禁止修改；失活先撤资格，命令返回后再回滚，避免同步重入。 */
    bool bCommandInProgress = false;
    bool bCleanupAfterCommand = false;
    bool bOwnsPreview = false;
    bool bOwnsStagedSettings = false;
};
