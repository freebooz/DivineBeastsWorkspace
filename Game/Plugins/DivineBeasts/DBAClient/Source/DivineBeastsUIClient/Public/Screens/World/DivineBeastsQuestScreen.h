#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "Services/GamePlatformQuestClientSubsystem.h"
#include "DivineBeastsQuestScreen.generated.h"

/**
 * UDivineBeastsQuestScreen（神兽联盟任务页面基类）。
 *
 * 直接复用 GamePlatformQuestClient（游戏平台任务客户端）的权威快照缓存与追踪集合。
 * 项目层只负责国风布局、文本与交互组合，不复制任务状态机。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsQuestScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    /** Blueprint读取接口；数据来自平台缓存，只有跨蓝图边界时发生数组复制。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Quest")
    TArray<FGamePlatformQuestSnapshot> GetQuestSnapshots() const;

    /** C++高频读取接口，不复制排序数组。 */
    const TArray<FGamePlatformQuestSnapshot>&
    GetQuestSnapshotsView() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Quest")
    bool IsQuestTracked(FName QuestId) const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Quest")
    bool TrackQuest(FName QuestId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Quest")
    bool UntrackQuest(FName QuestId);

protected:
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;

    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Quest", meta=(DisplayName="任务视图已变化"))
    void BP_OnQuestViewChanged();

private:
    void HandleQuestChanged();
    UGamePlatformQuestClientSubsystem* ResolveQuestSubsystem() const;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformQuestClientSubsystem> QuestSubsystem = nullptr;

    FDelegateHandle QuestChangedHandle;
};
