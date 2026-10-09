#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformCombatAlertWidget.generated.h"

class UTextBlock;

/** EGamePlatformUICombatAlertSeverity（紧急战斗提示视觉严重程度）。 */
UENUM(BlueprintType)
enum class EGamePlatformUICombatAlertSeverity : uint8
{
    Information UMETA(DisplayName="一般说明"),
    Warning UMETA(DisplayName="重要警告"),
    Critical UMETA(DisplayName="关键危险")
};

/**
 * FGamePlatformUICombatAlertState（首领/战斗机制固定警告只读快照）。
 * 区别于短Toast（弹出提示）：关键机制必须保留在可见固定区，不能被普通通知挤掉。
 * 数据由已授权战斗客户端适配提供，平台不预测未公开机制或倒计时。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUICombatAlertState
{
    GENERATED_BODY()

    /** 观察者/世界/对局作用域。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    FGuid SourceScopeId;

    /** 警告实例唯一显示身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    FName AlertInstanceId = NAME_None;

    /** 已授权显示的施法者/危险来源，未知时保持为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    FName SourceDisplayId = NAME_None;

    /** 本地化危险标题。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    FText AlertTitle;

    /** 本地化可执行动作建议，仅由已确认内容决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    FText ActionHint;

    /** 显示严重程度，不改变战斗规则优先级。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    EGamePlatformUICombatAlertSeverity Severity =
        EGamePlatformUICombatAlertSeverity::Information;

    /** 剩余秒数，-1表示来源未知；由领域按真实事件触发显示更新。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    float RemainingSeconds = -1.0f;

    /** 当前是否应显示，false仅从界面移除提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    bool bActive = false;

    /** 来源是否已确认本机制允许打断。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    bool bInterruptible = false;

    /** 严格递增的来源修订号。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|CombatAlert")
    int64 Revision = -1;
};

/** FGamePlatformUICombatAlertPresentation（基础警告输入校验器）。 */
struct GAMEPLATFORMUICLIENT_API FGamePlatformUICombatAlertPresentation
{
    /** 输入合法后才允许覆盖可见警告。 */
    static bool IsValidSnapshot(const FGamePlatformUICombatAlertState& InState);
};

/**
 * UGamePlatformCombatAlertWidget（通用关键战斗预警展示控件）。
 * 世界首领、竞技目标与训练教程共享，不在平台实现具体Boss行为。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformCombatAlertWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 绑定可见性已筛选的观察者数据源。 */
    UFUNCTION(BlueprintCallable, Category="UI|CombatAlert")
    bool BindAlertSource(FGuid InSourceScopeId);

    /** 仅接收合法、未过期的本数据源事件。 */
    UFUNCTION(BlueprintCallable, Category="UI|CombatAlert")
    bool ApplyAlertSnapshot(const FGamePlatformUICombatAlertState& InState);

    /** 清除当前警告，不取消服务器正在发生的机制。 */
    UFUNCTION(BlueprintCallable, Category="UI|CombatAlert")
    void ClearAlertSource();

    UFUNCTION(BlueprintPure, Category="UI|CombatAlert")
    FGamePlatformUICombatAlertState GetAlertSnapshot() const { return State; }

protected:
    /** 仅在蓝图存在相同命名TextBlock时绑定标题。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|CombatAlert")
    TObjectPtr<UTextBlock> AlertTitleText = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="UI|CombatAlert",
        meta=(DisplayName="关键战斗警告状态已变化"))
    void BP_OnCombatAlertChanged(FGamePlatformUICombatAlertState UpdatedState);

private:
    UPROPERTY(Transient)
    FGuid BoundScopeId;

    UPROPERTY(Transient)
    FGamePlatformUICombatAlertState State;
};
