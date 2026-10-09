#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformInteractionPromptWidget.generated.h"

/** FGamePlatformUIInteractionPromptState（通用交互提示UI状态）。
 * 语义由GamePlatformInteraction与输入适配器提供，Widget不调用交互动作或决定资格。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIInteractionPromptState
{
    GENERATED_BODY()
    /** 当前焦点交互目标展示ID，空表示应该隐藏提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Interaction")
    FName TargetDisplayId = NAME_None;
    /** 输入动作语义；不同键鼠/手柄/触屏布局由输入适配器提供图标。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Interaction")
    FName ActionSemanticId = NAME_None;
    /** 已本地化的动作标签，例如拾取、交谈或进入。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Interaction")
    FText ActionLabel;
    /** 禁用时提示仍可显示，但不得直接提交动作。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Interaction")
    bool bEnabled = false;
    /** 数据源修订号；同一目标只能由更新的状态覆盖。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Interaction")
    int64 Revision = -1;
};

/** UGamePlatformInteractionPromptWidget（通用交互提示组件）。
 * 仅显示当前交互意图的可用性，不直接派发输入事件。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformInteractionPromptWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|Interaction")
    bool ApplyInteractionPrompt(const FGamePlatformUIInteractionPromptState& InState);
    /** 明确丢失焦点时清除提示，不依赖轮询Actor。 */
    UFUNCTION(BlueprintCallable, Category="UI|Interaction")
    void ClearInteractionPrompt();
    UFUNCTION(BlueprintPure, Category="UI|Interaction")
    FGamePlatformUIInteractionPromptState GetInteractionPrompt() const { return State; }
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Interaction",
        meta=(DisplayName="交互提示已变化"))
    void BP_OnInteractionPromptChanged(FGamePlatformUIInteractionPromptState UpdatedState);
private:
    UPROPERTY(Transient)
    FGamePlatformUIInteractionPromptState State;
};
