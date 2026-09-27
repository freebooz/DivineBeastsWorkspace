#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "UObject/Object.h"
#include "GamePlatformFeedbackService.generated.h"

class UGamePlatformFeedbackWidget;
class UGamePlatformUILayerStack;
class ULocalPlayer;

/**
 * UGamePlatformFeedbackService（游戏平台高频反馈服务）。
 *
 * 负责伤害飘字、治疗、命中、拾取等短生命周期反馈的集中限流、合并、对象池和回收。
 * 服务自身不理解游戏业务；所有业务含义由 Channel / StyleId / Text 映射表达。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformFeedbackService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ULocalPlayer* InLocalPlayer);
    void SetRootLayout(UGamePlatformUILayerStack* InRootLayout);

    /**
     * 提交高频反馈。
     * WidgetClass 相同的实例会从对象池复用；达到并发上限时失败关闭，不无限创建。
     */
    UFUNCTION(BlueprintCallable, Category="UI|Feedback")
    FGuid SubmitFeedback(
        FGamePlatformUIFeedbackRequest Request,
        TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass);

    /** 显式结束反馈并回收到池。 */
    UFUNCTION(BlueprintCallable, Category="UI|Feedback")
    bool DismissFeedback(FGuid OccurrenceId);

    /** Travel/Deinitialize 时清理活动实例和对象池。 */
    void Clear();

private:
    struct FActiveFeedback
    {
        TWeakObjectPtr<UGamePlatformFeedbackWidget> Widget;
        FName MergeKey = NAME_None;
        FTimerHandle TimerHandle;
    };

    UGamePlatformFeedbackWidget* AcquireWidget(
        TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass);

    void RecycleWidget(UGamePlatformFeedbackWidget* Widget);
    void RestartLifetime(
        FGuid OccurrenceId,
        float LifetimeSeconds);
    bool ResolveInitialScreenPosition(
        FGamePlatformUIFeedbackRequest& Request) const;
    UWorld* GetServiceWorld() const;

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    TMap<FGuid, FActiveFeedback> ActiveFeedback;
    TMap<FName, FGuid> ActiveByMergeKey;

    /**
     * 对象池使用UPROPERTY强引用保活已回收Widget，确保对象池产生真实复用收益。
     * 超过池上限的Widget不进入数组，RemoveFromParent后交给GC正常回收。
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformFeedbackWidget>> PooledWidgets;
};
