#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformSettingRowWidget.generated.h"

/** FGamePlatformUISettingRowState（通用设置项呈现状态）。
 * 已应用与待提交值只供展示；实际设备/输入/SFX设置由各自权威客户端服务保存。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUISettingRowState
{
    GENERATED_BODY()
    /** 已注册的业务设置身份；空值非法。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    FName SettingId = NAME_None;
    /** 本地化名称。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    FText DisplayName;
    /** 本地化后经过业务服务校验的当前显示值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    FText DisplayValue;
    /** 项目业务服务是否允许当前账号修改。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    bool bEnabled = false;
    /** 尚未由业务服务确认的变更请求视觉状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    bool bPending = false;
    /** 预览失败或验证错误的展示消息。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    FText ErrorText;
    /** 当前设置源修订号，同一设置严格递增。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Settings")
    int64 Revision = -1;
};

/** UGamePlatformSettingRowWidget（通用图形、音频、输入、语言等设置项）。
 * 显示状态变更和错误；不直接读写用户.ini、注册表或内存外业务设置。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformSettingRowWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|Settings")
    bool ApplySettingRow(const FGamePlatformUISettingRowState& InState);
    UFUNCTION(BlueprintPure, Category="UI|Settings")
    FGamePlatformUISettingRowState GetSettingRow() const { return State; }
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Settings",
        meta=(DisplayName="设置显示值已变化"))
    void BP_OnSettingRowChanged(FGamePlatformUISettingRowState UpdatedState);
private:
    UPROPERTY(Transient)
    FGamePlatformUISettingRowState State;
};
