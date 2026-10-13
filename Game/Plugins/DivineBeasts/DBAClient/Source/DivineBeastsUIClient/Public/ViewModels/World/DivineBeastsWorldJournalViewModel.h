// 第三层客户端世界手册投影；只拥有本地玩家订阅，不拥有任务、技能、属性或组队权威。
#pragma once
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsWorldJournalViewModel.generated.h"

class ULocalPlayer;
class APlayerController;
class APawn;
class UDivineBeastsAbilityBarViewModel;
class UDivineBeastsPlayerStatusViewModel;
class UGamePlatformQuestClientSubsystem;
struct FGamePlatformUISlotState;
struct FDivineBeastsPlayerStatusViewData;

/** 手册是公共世界页面，不包含匹配/竞技模式或特定生肖规则。 */
UENUM(BlueprintType)
enum class EDivineBeastsWorldJournalSection : uint8
{
    Character, MapQuest, Spellbook, Party
};

/** 已获许可的显示项；Key只供本地命令精确选择，不能当成服务器授权。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsWorldJournalEntry
{
    GENERATED_BODY()
    /** 任务或已授权技能槽位身份；不展示内部指针。 */
    UPROPERTY(BlueprintReadOnly) FName Key;
    /** 已本地化名称；缺少内容文本时明确显示内容未就绪。 */
    UPROPERTY(BlueprintReadOnly) FText Label;
    /** 只读详情；数值单位由对应中文标签说明。 */
    UPROPERTY(BlueprintReadOnly) FText Details;
    /** 当前平台本地追踪偏好，不代表任务完成或可领奖。 */
    UPROPERTY(BlueprintReadOnly) bool bTracked = false;
};

/** 游戏线程事件驱动。激活绑定拥有者、属性/技能VM和任务服务；失活/销毁精确解除。
 * 地图仍由现有地图组件投影；没有公共队伍服务时明确不可用，不构造模拟成员。 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsWorldJournalViewModel final : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()
public:
    /** 仅接受该LocalPlayer当前控制器；无有效来源返回false并清旧状态。 */
    bool BindLocalPlayer(ULocalPlayer* Player);
    /** 幂等释放全部订阅及子VM资源；允许页面取消、旅行和GC前调用。 */
    void UnbindLocalPlayer();
    /** 切换本地页签；非法枚举拒绝，无网络副作用。 */
    bool SelectSection(EDivineBeastsWorldJournalSection InSection);
    EDivineBeastsWorldJournalSection GetSection() const { return Section; }
    const TArray<FDivineBeastsWorldJournalEntry>& GetEntries() const { return Entries; }
    FText GetSummary() const;
    /** 仅给现有地图组件注入已确认的当前本地Pawn，无世界搜索。 */
    APawn* GetDisplayPawn() const;
    /** 只追踪当前快照仍存在的任务；不存在、失活、非任务页返回false。 */
    bool SetQuestTracked(FName QuestId, bool bTracked);
    virtual void BeginDestroy() override;
private:
    UFUNCTION() void HandlePawnChanged(APawn* Previous, APawn* Current);
    void HandleSlotsChanged(const TArray<FGamePlatformUISlotState>& Slots);
    void HandleStatusChanged(const FDivineBeastsPlayerStatusViewData& Status);
    /** 来源事件统一重新发布有界只读列表；不启用Widget Tick或后台HTTP。 */
    void RefreshEntries();
    TWeakObjectPtr<APlayerController> BoundController;
    TWeakObjectPtr<UGamePlatformQuestClientSubsystem> QuestSource;
    FDelegateHandle QuestHandle;
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsAbilityBarViewModel> Abilities;
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsPlayerStatusViewModel> PlayerStatus;
    UPROPERTY(Transient) TArray<FDivineBeastsWorldJournalEntry> Entries;
    EDivineBeastsWorldJournalSection Section = EDivineBeastsWorldJournalSection::Character;
    /** 失活先置false，子VM解绑引起的同步通知不能复活页面。 */
    bool bSourceBound = false;
    bool bStatusAvailable = false;
};
