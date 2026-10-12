#pragma once

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsCharacterCreateViewModel.generated.h"

struct FGamePlatformDataLease;
struct FGamePlatformResult;
struct FDivineBeastsHeroOverviewLeaseState;

/** 单项创建介绍只读文本；AbilityId仅标识作者内容，不表示服务器授予或允许激活。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsHeroOverviewAbility
{
    GENERATED_BODY()
    /** 作者技能稳定编号；空值表示没有条目，不作GAS激活资格或服务器授权。 */
    UPROPERTY(BlueprintReadOnly) FName AbilityId = NAME_None;
    /** 从创建介绍DataAsset复制的本地化名称，不按数组索引猜测英雄技能身份。 */
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    /** 作者机制说明与开发限制；缺失/纯空白由Profile校验拒绝，不由Widget补造。 */
    UPROPERTY(BlueprintReadOnly) FText Description;
};

/** 当前本地浏览英雄的显示快照，与ApplicationFlow已选角色和DefaultAbilitySetId相互独立。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsHeroOverview
{
    GENERATED_BODY()
    /** 当前浏览英雄的稳定身份；清空/失活时None，不替代业务已选角色。 */
    UPROPERTY(BlueprintReadOnly) FName HeroDefinitionId = NAME_None;
    /** HeroDefinition的中文介绍副本；加载/失败/取消时为空，成功前不混入旧英雄正文。 */
    UPROPERTY(BlueprintReadOnly) FText Introduction;
    /** 当前Profile的五项只读名称和说明；清理时整个集合撤销，无技能授权含义。 */
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsHeroOverviewAbility> Abilities;
    /** 由VM格式化作者名称/说明，Widget只投影文本，不解析业务内容。 */
    UPROPERTY(BlueprintReadOnly) FText SkillIntroductionText;
    /** 介绍域的加载、错误或未批准提示，不包含账号密码或网络票据。 */
    UPROPERTY(BlueprintReadOnly) FText StatusText;
    /** 仅表示已载入介绍是否为开发候选；默认false并不表示空内容已经批准。 */
    UPROPERTY(BlueprintReadOnly) bool bDescriptionsAreDraft = false;
    /** 介绍加载自身的Empty/Loading/Ready/Error状态，独立于权威创建流程状态。 */
    UPROPERTY(BlueprintReadOnly) EDivineBeastsUIPageState PageState = EDivineBeastsUIPageState::Empty;
    /** 可诊断的介绍域错误编号；正常或清理为None，用户可读说明由StatusText承载。 */
    UPROPERTY(BlueprintReadOnly) FName ErrorCode = NAME_None;
    /** 页面代次与独立介绍请求代次，供同步重入观察者拒绝旧快照，不是服务器版本。 */
    UPROPERTY(BlueprintReadOnly) int32 PageGeneration = 0;
    UPROPERTY(BlueprintReadOnly) int64 RequestGeneration = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsHeroOverviewChangedNative, const FDivineBeastsHeroOverview&);

/**
 * UDivineBeastsCharacterCreateViewModel（持久角色创建视图模型）。
 * 英雄候选来自 ApplicationFlow（应用流程）公开投影；创建命令仍走统一 UI Command。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateViewModel
    : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()

public:
    UDivineBeastsCharacterCreateViewModel();
    virtual ~UDivineBeastsCharacterCreateViewModel() override;

    /** 游戏线程提交本地浏览意图；仅活动页/有效英雄可受理。失败有明确状态，不提交创建或技能授权。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterCreate")
    bool SelectHeroOverview(FName HeroDefinitionId);
    /** 撤销本VM自己的加载/成功租约，清空快照并发事件；同实例其他用途的租约不受影响。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterCreate")
    void ClearHeroOverview();
    /** 返回当前介绍的值副本，不暴露作者UObject或租约；调用方不得把它写回业务状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    FDivineBeastsHeroOverview GetHeroOverview() const { return HeroOverview; }
    /** 独立于业务ViewState的游戏线程介绍事件；订阅者须核对快照双代次，生命周期结束精确解绑。 */
    FDivineBeastsHeroOverviewChangedNative& OnHeroOverviewChanged() { return HeroOverviewChanged; }
    virtual void BeginDestroy() override;
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    TArray<FDivineBeastsUICreateHeroItem> GetCreateHeroOptions() const
    {
        return GetStateRef().CreateHeroOptions;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    bool CanSubmitCreation() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return IsPageActive() &&
            !ViewState.bBusy &&
            ViewState.AllowedCommands.Contains(TEXT("CreateCharacter")) &&
            !ViewState.CreateHeroOptions.IsEmpty();
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    bool IsCreationSubmitting() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return ViewState.CurrentStep == TEXT("DBA.Flow.CreateCharacter") ||
            ViewState.PageState == EDivineBeastsUIPageState::Submitting;
    }

protected:
    /** 平台页面钩子拥有介绍用途；不隐藏非虚的OnScreenActivated/OnScreenDeactivated业务订阅入口。 */
    virtual void OnPageBegan() override;
    virtual void OnPageEnded() override;

private:
    /** 代次先失效，随后释放旧需求；bNotify=false仅用于对象销毁时避免调用外部观察者。 */
    void ResetHeroOverview(bool bNotify);
    bool IsOverviewRequestCurrent(int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero) const;
    void HandleOverviewResourcesLoaded(int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero,
        const FSoftObjectPath& HeroPath, const FSoftObjectPath& ProfilePath,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    /** 失败原子清除正文/释放本租约，终态仅发布一次；不改变业务创建资格。 */
    void PublishOverviewFailure(int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero,
        FName ErrorCode, const FText& Message);
    void ReleaseOverviewLease();
    void BroadcastHeroOverview();

    UPROPERTY(Transient) FDivineBeastsHeroOverview HeroOverview;
    FDivineBeastsHeroOverviewChangedNative HeroOverviewChanged;
    /** 不透明私有租约上下文；共享指针兼容UHT不完整类型销毁，只有VM持有，回调不捕获它。 */
    TSharedPtr<FDivineBeastsHeroOverviewLeaseState> OverviewLeaseState;
    int64 OverviewRequestGeneration = 0;
    bool bOverviewCompletionPublished = false;
};
