#pragma once

#include "Definitions/GamePlatformUIDefinitionBase.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "GamePlatformFeedbackDefinition.generated.h"

/** UGamePlatformFeedbackDefinition（游戏平台高频反馈定义）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformFeedbackDefinition
    : public UGamePlatformUIDefinitionBase
{
    GENERATED_BODY()

public:
    /** 反馈通道，例如FloatingText、Hit、Pickup；平台不解释业务含义。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Feedback")
    FName Channel = NAME_None;

    /** 反馈Widget软类；通常使用FloatingText等平台基础类的Blueprint派生。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Feedback")
    TSoftClassPtr<UGamePlatformFeedbackWidget> WidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Feedback", meta=(ClampMin="0.05"))
    float DefaultLifetimeSeconds = 1.0f;

    /** 同MergeKey的数值型反馈是否默认允许合并。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Feedback")
    bool bAllowMerge = false;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
