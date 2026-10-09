#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformQuestTrackerWidget.generated.h"

/** FGamePlatformUITrackedObjective（任务目标只读展示）。
 * 由GamePlatformQuest等领域插件预先汇总，平台UI不持有Quest状态机。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUITrackedObjective
{
    GENERATED_BODY()
    /** 公开目标语义标识。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    FName ObjectiveId = NAME_None;
    /** 已本地化显示文本，不暴露服务器内部字段。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    FText Label;
    /** 0～1归一化进度；由适配器计算，进度未知可关闭显示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    float Progress = 0.0f;
    /** 是否显示目标进度值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    bool bShowProgress = false;
    /** 权威任务系统确认后的完成显示态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    bool bCompleted = false;
};

/** FGamePlatformUIQuestTrackerState（任务追踪UI快照）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIQuestTrackerState
{
    GENERATED_BODY()
    /** 同一追踪分组身份下只接受递增修订号。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    FName TrackerId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    int64 Revision = -1;
    /** 当前画面最多显示16个目标，超量由任务领域先行分页/裁剪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|QuestTracker")
    TArray<FGamePlatformUITrackedObjective> Objectives;
};

/** UGamePlatformQuestTrackerWidget（通用任务追踪面板）。
 * 任务领取、奖励发放和目标达成都不是UI职责。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformQuestTrackerWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|QuestTracker")
    bool ApplyTrackedObjectives(const FGamePlatformUIQuestTrackerState& InState);
    UFUNCTION(BlueprintPure, Category="UI|QuestTracker")
    FGamePlatformUIQuestTrackerState GetTrackedObjectives() const { return State; }
    const FGamePlatformUIQuestTrackerState& GetTrackedObjectivesView() const { return State; }
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|QuestTracker",
        meta=(DisplayName="任务追踪列表已更新"))
    void BP_OnObjectivesChanged(FGamePlatformUIQuestTrackerState UpdatedState);
private:
    UPROPERTY(Transient)
    FGamePlatformUIQuestTrackerState State;
};
