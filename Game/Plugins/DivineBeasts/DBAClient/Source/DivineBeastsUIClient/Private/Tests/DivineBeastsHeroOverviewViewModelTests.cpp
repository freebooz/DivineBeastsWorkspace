// 项目客户端创建介绍回归；调用方为UE Automation，使用真实Data服务或静态Profile校验。
// 只读既有资源，不授予技能、不发HTTP、不保存资产。异步夹具关闭时先结束VM租约再关闭实例。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsCharacterCreationProfile.h"
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

// 不加载图标；正向字段夹具用于隔离草案标识和五条介绍的校验，不冒充服务器技能授权。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCreationProfileIdentityTest,
    "DivineBeasts.UI.Characters.HeroOverview.CreationProfileIdentityAndDraftValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCreationProfileIdentityTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsCharacterCreationProfile> Profile(NewObject<UDivineBeastsCharacterCreationProfile>());
    Profile->HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    const TCHAR* Ids[] = {TEXT("dba.ability.rat_dev_primary@1"), TEXT("dba.ability.rat_dev_passive@1"),
        TEXT("dba.ability.rat_dev_active01@1"), TEXT("dba.ability.rat_dev_active02@1"), TEXT("dba.ability.rat_dev_ultimate@1")};
    for (const TCHAR* Id : Ids)
    {
        FDivineBeastsAbilityUIEntry Entry;
        Entry.AbilityId = Id;
        Entry.DisplayName = FText::FromString(TEXT("测试作者名称"));
        Entry.Description = FText::FromString(TEXT("开发候选，未作为正式技能批准。仅验证创建介绍数据，不表示技能授权。"));
        Entry.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/DBAHeroPack_Rat/UI/Abilities/T_Test.T_Test")));
        Profile->Entries.Add(Entry);
    }
    FString Error;
    TestEqual(TEXT("创建介绍与战斗UIProfile使用不同主资产类型"), Profile->GetPrimaryAssetId(),
        FPrimaryAssetId(TEXT("DivineBeastsCharacterCreationProfile"), TEXT("Hero.Zodiac.Rat")));
    TestTrue(TEXT("明确标记的五项候选介绍有效"), Profile->ValidateCreationProfile(Error));
    Profile->bDescriptionsAreDraft = false;
    TestFalse(TEXT("开发技能身份不能伪装正式介绍"), Profile->ValidateCreationProfile(Error));
    Profile->bDescriptionsAreDraft = true;
    Profile->Entries[0].Description = FText::FromString(TEXT("遗漏未批准标识的候选说明"));
    TestFalse(TEXT("每条候选说明都必须声明未正式批准"), Profile->ValidateCreationProfile(Error));
    Profile->Entries[0].Description = FText::FromString(TEXT("开发候选，未作为正式技能批准。测试说明。"));
    Profile->Entries.RemoveAt(4);
    TestFalse(TEXT("首期五条介绍不可缺少或暗补条目"), Profile->ValidateCreationProfile(Error));
    return true;
}

namespace
{
/** 真实异步资源夹具：切英雄后旧取消回调不得覆盖新投影，失活后不得重新申请或发布文本。 */
class FHeroOverviewLeaseLifecycleCommand final : public IAutomationLatentCommand
{
public:
    explicit FHeroOverviewLeaseLifecycleCommand(FAutomationTestBase* InTest)
        : Test(InTest), StartedSeconds(FPlatformTime::Seconds()) {}
    ~FHeroOverviewLeaseLifecycleCommand() override
    {
        if (VM.IsValid())
        {
            VM->OnHeroOverviewChanged().Remove(OverviewHandle);
            if (VM->IsPageActive()) VM->EndPage();
            else VM->ClearHeroOverview();
        }
        if (Instance.IsValid())
        {
            UWorld* World = Instance->GetWorld();
            if (World) World->DestroyWorld(false);
            Instance->Shutdown();
            if (World && GEngine) GEngine->DestroyWorldContext(World);
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test->AddError(TEXT("创建介绍真实租约测试超时；不得把缺少新资产记为通过。")); return true; }
        if (Phase == 0)
        {
            if (!GEngine) { Test->AddError(TEXT("真实引擎上下文缺失。")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            auto* Data = IGamePlatformDataService::Get(*Instance);
            if (!Data) { Test->AddError(TEXT("真实GamePlatformData实例服务缺失。")); return true; }
            Baseline = Data->GetDiagnostics();
            VM.Reset(NewObject<UDivineBeastsCharacterCreateViewModel>(Instance.Get()));
            OverviewHandle = VM->OnHeroOverviewChanged().AddLambda([this](const FDivineBeastsHeroOverview&)
            { ++OverviewNotifications; });
            VM->BeginPage();
            if (!Test->TestTrue(TEXT("首个真实英雄介绍请求受理"), VM->SelectHeroOverview(TEXT("Hero.Zodiac.Rat")))) return true;
            const int64 PreviousGeneration = VM->GetHeroOverview().RequestGeneration;
            if (!Test->TestTrue(TEXT("切换到当前英雄请求受理"), VM->SelectHeroOverview(TEXT("Hero.Zodiac.Ox")))) return true;
            const auto Overview = VM->GetHeroOverview();
            Test->TestTrue(TEXT("切英雄递增独立请求代次"), Overview.RequestGeneration > PreviousGeneration);
            Test->TestEqual(TEXT("加载期只记录当前英雄"), Overview.HeroDefinitionId, FName(TEXT("Hero.Zodiac.Ox")));
            Test->TestTrue(TEXT("切换即时清除旧正文"), Overview.Introduction.IsEmpty() && Overview.SkillIntroductionText.IsEmpty());
            Test->TestEqual(TEXT("提前切换只保留一个待完成租约"), Data->GetDiagnostics().PendingRequests, Baseline.PendingRequests + 1);
            const int64 Accepted = Data->GetDiagnostics().TotalAcceptedRequests;
            VM->SelectHeroOverview(TEXT("Hero.Zodiac.Ox"));
            Test->TestEqual(TEXT("同英雄重复刷新不重复加载"), Data->GetDiagnostics().TotalAcceptedRequests, Accepted);
            Phase = 1;
            return false;
        }
        if (Phase == 1)
        {
            const auto Overview = VM->GetHeroOverview();
            if (Overview.PageState == EDivineBeastsUIPageState::Loading) return false;
            if (!Test->TestEqual(TEXT("当前英雄真实资源最终成功"), Overview.PageState, EDivineBeastsUIPageState::Ready)) return true;
            Test->TestEqual(TEXT("旧英雄迟到取消不能覆盖当前英雄"), Overview.HeroDefinitionId, FName(TEXT("Hero.Zodiac.Ox")));
            Test->TestFalse(TEXT("真实HeroDefinition角色介绍已投影"), Overview.Introduction.IsEmpty());
            Test->TestEqual(TEXT("真实CreationProfile五项介绍已投影"), Overview.Abilities.Num(), 5);
            Test->TestTrue(TEXT("草案状态必须可见"), Overview.bDescriptionsAreDraft && Overview.StatusText.ToString().Contains(TEXT("开发候选，未作为正式技能批准")));
            auto* Data = IGamePlatformDataService::Get(*Instance);
            Test->TestEqual(TEXT("完成后保留自己的成功租约"), Data->GetDiagnostics().ActiveLeases, Baseline.ActiveLeases + 1);
            VM->ClearHeroOverview();
            Test->TestEqual(TEXT("显式清除立即释放成功租约"), Data->GetDiagnostics().ActiveLeases, Baseline.ActiveLeases);
            if (!Test->TestTrue(TEXT("再次申请真实资源"), VM->SelectHeroOverview(TEXT("Hero.Zodiac.Rat")))) return true;
            VM->EndPage();
            NotificationsAfterEnd = OverviewNotifications;
            Test->TestEqual(TEXT("失活立即撤销待加载租约"), Data->GetDiagnostics().PendingRequests, Baseline.PendingRequests);
            Test->TestTrue(TEXT("失活立即清空介绍投影"), VM->GetHeroOverview().HeroDefinitionId.IsNone() && VM->GetHeroOverview().Abilities.IsEmpty());
            Phase = 2;
            return false;
        }
        // 让中央Data的真实延后取消通知跨过两个调度轮；未手工调用或伪造VM完成回调。
        if (++DrainedFrames < 2) return false;
        Test->TestEqual(TEXT("失活后迟到取消不再发布介绍事件"), OverviewNotifications, NotificationsAfterEnd);
        Test->TestTrue(TEXT("迟到取消不能复活旧正文"), VM->GetHeroOverview().Introduction.IsEmpty() && VM->GetHeroOverview().SkillIntroductionText.IsEmpty());
        Test->TestFalse(TEXT("失活页不能重新申请介绍"), VM->SelectHeroOverview(TEXT("Hero.Zodiac.Rat")));
        return true;
    }
private:
    FAutomationTestBase* Test;
    double StartedSeconds;
    int32 Phase = 0, DrainedFrames = 0, OverviewNotifications = 0, NotificationsAfterEnd = 0;
    FGamePlatformDataDiagnostics Baseline;
    FDelegateHandle OverviewHandle;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> VM;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroOverviewLeaseLifecycleTest,
    "DivineBeasts.UI.Characters.HeroOverview.SwitchAndDeactivateReleaseLeases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHeroOverviewLeaseLifecycleTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FHeroOverviewLeaseLifecycleCommand(this));
    return true;
}

// 未初始化实例不允许退回直接AssetManager加载；失败显示应保持正文为空，用户仍能看到真实原因。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroOverviewUninitializedTest,
    "DivineBeasts.UI.Characters.HeroOverview.UninitializedDataRejects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHeroOverviewUninitializedTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    const TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> VM(NewObject<UDivineBeastsCharacterCreateViewModel>(Instance.Get()));
    VM->BeginPage();
    TestFalse(TEXT("无真实Data服务必须拒绝"), VM->SelectHeroOverview(TEXT("Hero.Zodiac.Rat")));
    TestEqual(TEXT("资源失败有明确状态"), VM->GetHeroOverview().PageState, EDivineBeastsUIPageState::Error);
    TestTrue(TEXT("失败不能假造正文"), VM->GetHeroOverview().Introduction.IsEmpty() && VM->GetHeroOverview().Abilities.IsEmpty());
    VM->EndPage();
    TestTrue(TEXT("失活清除失败英雄身份"), VM->GetHeroOverview().HeroDefinitionId.IsNone());
    return true;
}
// 真实CommonUI创建页同步Loading换VM回归：状态夹具仅给本地候选，不提供Owner/GI/授权。
// 以无Data的明确Error验证新VM确实处理了介绍意图；不依赖12份作者资产，不伪造加载成功。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroOverviewScreenSwapDuringLoadingTest,
    "DivineBeasts.UI.Characters.HeroOverview.ScreenSwapDuringLoadingRefreshesIncoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHeroOverviewScreenSwapDuringLoadingTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsCharacterCreateScreen> Screen(NewObject<UDivineBeastsCharacterCreateScreen>());
    const TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> Previous(NewObject<UDivineBeastsCharacterCreateViewModel>());
    const TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> Incoming(NewObject<UDivineBeastsCharacterCreateViewModel>());
    ON_SCOPE_EXIT { Screen->DeactivateWidget(); Screen->InitializeActivatableViewModel(nullptr); };
    const auto BindControl = [this, Page = Screen.Get()](FName Field, UObject* Control)
    {
        auto* Property = FindFProperty<FObjectPropertyBase>(Page->GetClass(), Field);
        if (!TestNotNull(*Field.ToString(), Property)) return false;
        Property->SetObjectPropertyValue_InContainer(Page, Control);
        return true;
    };
    auto* HeroOptions = NewObject<UComboBoxString>(Screen.Get());
    auto* OverviewStatus = NewObject<UTextBlock>(Screen.Get());
    auto* NameInput = NewObject<UEditableTextBox>(Screen.Get());
    if (!BindControl(TEXT("HeroOptions"), HeroOptions) ||
        !BindControl(TEXT("CharacterNameInput"), NameInput) ||
        !BindControl(TEXT("CreateButton"), NewObject<UButton>(Screen.Get())) ||
        !BindControl(TEXT("LogoutButton"), NewObject<UButton>(Screen.Get())) ||
        !BindControl(TEXT("StatusText"), NewObject<UTextBlock>(Screen.Get())) ||
        !BindControl(TEXT("ErrorText"), NewObject<UTextBlock>(Screen.Get())) ||
        !BindControl(TEXT("HeroOverviewStatusText"), OverviewStatus) ||
        !BindControl(TEXT("HeroIntroductionText"), NewObject<UTextBlock>(Screen.Get())) ||
        !BindControl(TEXT("SkillIntroductionText"), NewObject<UTextBlock>(Screen.Get()))) return false;
    auto* StateProperty = FindFProperty<FStructProperty>(Previous->GetClass(), TEXT("State"));
    if (!TestNotNull(TEXT("真实VM快照字段"), StateProperty)) return false;
    auto* PreviousState = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(Previous.Get());
    auto* IncomingState = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(Incoming.Get());
    FDivineBeastsUICreateHeroItem IncomingHero;
    IncomingHero.HeroDefinitionId = TEXT("Hero.Zodiac.Ox");
    IncomingState->CreateHeroOptions.Add(IncomingHero);
    // 激活时旧候选为空，防派生NativeOnActivated结尾的额外刷新掩盖嵌套初刷丢失。
    Screen->InitializeActivatableViewModel(Previous.Get());
    Screen->ActivateWidget();
    int32 Swaps = 0;
    const FDelegateHandle SwapHandle = Previous->OnHeroOverviewChanged().AddLambda(
        [Page = Screen.Get(), Next = Incoming.Get(), &Swaps](const FDivineBeastsHeroOverview& Overview)
        {
            if (Overview.PageState == EDivineBeastsUIPageState::Loading && Swaps++ == 0)
                Page->InitializeActivatableViewModel(Next);
        });
    ON_SCOPE_EXIT { Previous->OnHeroOverviewChanged().Remove(SwapHandle); };
    FDivineBeastsUICreateHeroItem PreviousHero;
    PreviousHero.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    PreviousState->CreateHeroOptions.Add(PreviousHero);
    // 使用真实单一输入监听，不广播旧VM的双重状态监听，避免第二个监听在旧栈结束后偶然补刷。
    // 仅派发一次控件输入事件，不先SetText，完全排除Slate或FieldNotification带来第二次刷新。
    // 不手工调用VM完成或页面刷新函数；输入文本不提交业务创建，字段可继续为空。
    NameInput->OnTextChanged.Broadcast(FText::FromString(TEXT("仅供回归的本地名称")));
    TestEqual(TEXT("Loading观察者实际同步换VM一次"), Swaps, 1);
    TestEqual(TEXT("当前Screen必须保留后继VM"), Screen->GetCharacterCreateViewModel(), Incoming.Get());
    TestFalse(TEXT("旧VM页面已结束"), Previous->IsPageActive());
    const auto Overview = Incoming->GetHeroOverview();
    TestEqual(TEXT("补刷新必须提交新英雄介绍意图"), Overview.HeroDefinitionId, FName(TEXT("Hero.Zodiac.Ox")));
    TestEqual(TEXT("补刷新使用新VM当前页面代次"), Overview.PageGeneration, Incoming->GetPageGeneration());
    TestEqual(TEXT("无Data应显示真实失败而非未刷新Empty"), Overview.PageState, EDivineBeastsUIPageState::Error);
    TestEqual(TEXT("三个动态文本中的状态由后继VM投影"), OverviewStatus->GetText().ToString(), Overview.StatusText.ToString());
    TestEqual(TEXT("资格控件同样刷新到后继英雄"), HeroOptions->GetOptionCount(), 1);
    return true;
}
#endif
