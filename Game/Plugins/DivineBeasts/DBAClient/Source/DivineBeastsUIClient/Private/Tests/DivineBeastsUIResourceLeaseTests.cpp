// 项目客户端资源生命周期回归：直接绑定真实VM/Loadout并调用真实本地玩家子系统入口。
// Data替身只记录签发/释放并控制延后终态；根布局最终视口边界注入一个瞬态根实例。
// 这些测试验证接线和作用域，不加载生产地图、不访问后端、不授予技能、不声明真实Widget/网络/Cook通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/DivineBeastsUIResourceLeaseFixture.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
/** 仅测试合同替身：无AssetManager/原生句柄；每条记录独立，允许显式投递已释放请求以检验消费方代次。 */
class FRecordingUIResourceService final : public IGamePlatformDataService
{
public:
    struct FRequest
    {
        FGamePlatformDataLease Lease;
        FGamePlatformDataCompletion Completion;
        TWeakObjectPtr<UObject> Caller;
        EGamePlatformDataLifetime Lifetime = EGamePlatformDataLifetime::Instance;
        EGamePlatformDataRequestState State = EGamePlatformDataRequestState::Loading;
        int32 Releases = 0;
    };
    TArray<TSharedPtr<FRequest>> Requests;
    FGuid Scope = FGuid::NewGuid();
    int32 DefinitionRequests = 0;
    int32 InvalidReleases = 0;

    FGamePlatformDataLease AcquireResources(const TArray<FSoftObjectPath>& Paths,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> Caller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) override
    {
        auto Request = MakeShared<FRequest>();
        Request->Lease.ScopeId = Scope;
        Request->Lease.LeaseId = FGuid::NewGuid();
        Request->Lease.Generation = Requests.Num() + 1;
        Request->Lease.IssuerProof = FGuid::NewGuid();
        Request->Lease.ResourcePaths = Paths;
        Request->Lease.RequestState = EGamePlatformDataRequestState::Loading;
        Request->Completion = MoveTemp(Completion);
        Request->Caller = Caller;
        Request->Lifetime = Lifetime;
        Requests.Add(Request);
        OutResult = FGamePlatformResult::Success();
        return Request->Lease; // 不同步完成，保持生产Data的延后合同。
    }
    FGamePlatformResult ReleaseResources(const FGamePlatformDataLease& Lease) override
    {
        for (const auto& Request : Requests)
        {
            if (Request->Lease.ScopeId == Lease.ScopeId && Request->Lease.LeaseId == Lease.LeaseId &&
                Request->Lease.Generation == Lease.Generation && Request->Lease.IssuerProof == Lease.IssuerProof &&
                Request->Lease.ResourcePaths == Lease.ResourcePaths)
            {
                ++Request->Releases;
                Request->State = EGamePlatformDataRequestState::Released;
                return FGamePlatformResult::Success();
            }
        }
        ++InvalidReleases;
        return FGamePlatformResult::Failure(TEXT("UnknownTestLease"), TEXT("测试消费方释放了其他调用者的租约。"));
    }
    EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const override
    {
        for (const auto& Request : Requests)
            if (Request->Lease.ScopeId == Lease.ScopeId && Request->Lease.LeaseId == Lease.LeaseId)
                return Request->State;
        return EGamePlatformDataRequestState::Invalid;
    }
    /** 只由测试在受理返回后驱动；迟到成功保留Released实时状态，不伪造仍可读取的租约。 */
    void Complete(int32 Index, bool bSuccess)
    {
        const auto Request = Requests[Index];
        if (Request->State != EGamePlatformDataRequestState::Released)
            Request->State = bSuccess ? EGamePlatformDataRequestState::Succeeded : EGamePlatformDataRequestState::Failed;
        FGamePlatformDataLease Completed = Request->Lease;
        Completed.RequestState = bSuccess ? EGamePlatformDataRequestState::Succeeded : EGamePlatformDataRequestState::Failed;
        Request->Completion(Completed, bSuccess ? FGamePlatformResult::Success() :
            FGamePlatformResult::Failure(TEXT("MissingTestResource"), TEXT("测试缺失资源。")));
    }
    FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId&,
        TSubclassOf<UGamePlatformDefinitionBase>, const TArray<FName>&,
        EGamePlatformDataLifetime, TWeakObjectPtr<UObject>, FGamePlatformDataCompletion,
        FGamePlatformResult& OutResult) override
    {
        ++DefinitionRequests;
        OutResult = FGamePlatformResult::Failure(TEXT("WrongTestAPI"), TEXT("UI普通资源不能按平台Definition加载。"));
        return {};
    }
    const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease&) const override { return nullptr; }
    FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease&) override
    {
        ++InvalidReleases;
        return FGamePlatformResult::Failure(TEXT("WrongTestAPI"), TEXT("UI普通资源不能按Definition释放。"));
    }
    FGamePlatformDataDiagnostics GetDiagnostics() const override
    {
        FGamePlatformDataDiagnostics Result;
        Result.ScopeId = Scope;
        for (const auto& Request : Requests)
        {
            if (Request->State == EGamePlatformDataRequestState::Loading) ++Result.PendingRequests;
            if (Request->State == EGamePlatformDataRequestState::Succeeded) ++Result.ActiveLeases;
        }
        return Result;
    }
};
}

/** 私有测试访问仅装配边界/读取结果；请求、回调、换英雄和清理均走实际生产入口。 */
struct FDivineBeastsUIResourceLeaseTestAccess
{
    static void Configure(UDivineBeastsAbilityBarViewModel& VM, IGamePlatformDataService& Service,
        TMap<FName, FSoftObjectPath> Paths)
    {
        VM.DataServiceForTests = &Service;
        VM.ProfilePathForTests = [Paths = MoveTemp(Paths)](FName Hero) { return Paths.FindRef(Hero); };
    }
    static void Clear(UDivineBeastsAbilityBarViewModel& VM)
    {
        VM.UnbindFromLoadout();
        VM.DataServiceForTests = nullptr;
        VM.ProfilePathForTests = nullptr;
    }
    static UDivineBeastsAbilityUIProfile* Profile(const UDivineBeastsAbilityBarViewModel& VM) { return VM.LoadedProfile; }
    static bool IconsReady(const UDivineBeastsAbilityBarViewModel& VM) { return VM.bIconsReady; }
    static bool HasProfileLease(const UDivineBeastsAbilityBarViewModel& VM) { return VM.ProfileLease.IsValid(); }
    static bool HasIconLease(const UDivineBeastsAbilityBarViewModel& VM) { return VM.IconLease.IsValid(); }
    static bool SetSnapshot(UDivineBeastsAbilityLoadoutComponent& Source, FName Hero)
    {
        // 仅给真实复制字段输入测试快照，然后广播组件真实事件；不调用Gameplay权威入口。
        auto* Property = FindFProperty<FStructProperty>(Source.GetClass(), TEXT("LoadoutState"));
        if (!Property) return false;
        auto* Snapshot = Property->ContainerPtrToValuePtr<FDivineBeastsAbilityLoadoutState>(&Source);
        Snapshot->HeroDefinitionId = Hero;
        Source.OnLoadoutChanged().Broadcast(*Snapshot);
        return true;
    }
    static void Configure(UDivineBeastsUIClientSubsystem& UI, UGamePlatformUIManagerSubsystem& Platform,
        IGamePlatformDataService& Service)
    {
        UI.PlatformUI = &Platform;
        UI.RootDataServiceForTests = &Service;
        UI.RootLayoutPathForTests = FSoftObjectPath(UDivineBeastsUIResourceLeaseRootFixture::StaticClass());
        UI.bDefaultThemeRequested = true; // 本夹具未初始化主题服务，不触发无关主题加载。
    }
    static void SetRootPath(UDivineBeastsUIClientSubsystem& UI, const FSoftObjectPath& Path) { UI.RootLayoutPathForTests = Path; }
    static bool Ensure(UDivineBeastsUIClientSubsystem& UI) { return UI.EnsureDefaultRootLayout(); }
    static bool Pending(const UDivineBeastsUIClientSubsystem& UI)
        { return UI.RootLayoutLoadState == UDivineBeastsUIClientSubsystem::ERootLayoutLoadState::Pending; }
    static bool Failed(const UDivineBeastsUIClientSubsystem& UI)
        { return UI.RootLayoutLoadState == UDivineBeastsUIClientSubsystem::ERootLayoutLoadState::Failed; }
    static bool Ready(const UDivineBeastsUIClientSubsystem& UI)
        { return UI.RootLayoutLoadState == UDivineBeastsUIClientSubsystem::ERootLayoutLoadState::Ready; }
    static bool Installing(const UDivineBeastsUIClientSubsystem& UI) { return UI.bInstallingRootLayout; }
    static void SetInstaller(UDivineBeastsUIClientSubsystem& UI,
        TFunction<bool(TSubclassOf<UGamePlatformUILayerStack>)> Installer) { UI.RootInstallerForTests = MoveTemp(Installer); }
    static void Clear(UDivineBeastsUIClientSubsystem& UI)
    {
        UI.Deinitialize();
        UI.RootDataServiceForTests = nullptr;
        UI.RootInstallerForTests = nullptr;
    }
};
using FLeaseAccess = FDivineBeastsUIResourceLeaseTestAccess;

namespace
{
/** 独立瞬态世界/GI只提供真实资源作用域；不InitializeStandalone，不启动流程/后端或BeginPlay。 */
struct FAbilityLeaseFixture
{
    FRecordingUIResourceService Data;
    TStrongObjectPtr<UGameInstance> Instance { NewObject<UGameInstance>(GEngine) };
    TStrongObjectPtr<UWorld> World { UWorld::CreateWorld(EWorldType::Game, false) };
    TStrongObjectPtr<AActor> Owner { World->SpawnActor<AActor>() };
    TStrongObjectPtr<UDivineBeastsAbilityLoadoutComponent> Source { NewObject<UDivineBeastsAbilityLoadoutComponent>(Owner.Get()) };
    TStrongObjectPtr<UDivineBeastsAbilityBarViewModel> VM { NewObject<UDivineBeastsAbilityBarViewModel>(Owner.Get()) };
    FAbilityLeaseFixture()
    {
        World->SetGameInstance(Instance.Get());
        Owner->AddInstanceComponent(Source.Get());
    }
    ~FAbilityLeaseFixture()
    {
        FLeaseAccess::Clear(*VM.Get());
        World->SetGameInstance(nullptr);
        World->DestroyWorld(false);
    }
};
/** 创建真实Transient Profile/Texture对象，只有租约完成后生产ResolveObject才会消费；不保存假资产。 */
UDivineBeastsAbilityUIProfile* MakeProfile(FName Hero, UTexture2D* Texture)
{
    auto* Profile = NewObject<UDivineBeastsAbilityUIProfile>();
    Profile->HeroDefinitionId = Hero;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FDivineBeastsAbilityUIEntry Entry;
        Entry.AbilityId = FName(*FString::Printf(TEXT("dba.ability.lease_test_%d@1"), Index));
        Entry.DisplayName = FText::FromString(TEXT("测试技能"));
        Entry.Description = FText::FromString(TEXT("测试文本，不表示正式玩法授权。"));
        Entry.Icon = Texture;
        Profile->Entries.Add(Entry); // 两条目共用同一纹理，期望一次去重图标请求。
    }
    return Profile;
}

/** 无窗口的真实LocalPlayer/Controller/PlatformUI夹具；最终挂载替身的边界在成功测试内显式说明。 */
struct FRootLeaseFixture
{
    FRecordingUIResourceService Data;
    TStrongObjectPtr<UGameInstance> Instance { NewObject<UGameInstance>(GEngine) };
    TStrongObjectPtr<UWorld> World { UWorld::CreateWorld(EWorldType::Game, false) };
    TStrongObjectPtr<UGameViewportClient> Viewport { NewObject<UGameViewportClient>(GEngine) };
    TStrongObjectPtr<ULocalPlayer> Player { NewObject<ULocalPlayer>(GEngine) };
    TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Platform { NewObject<UGamePlatformUIManagerSubsystem>(Player.Get()) };
    TStrongObjectPtr<UDivineBeastsUIClientSubsystem> UI { NewObject<UDivineBeastsUIClientSubsystem>(Player.Get()) };
    TStrongObjectPtr<UDivineBeastsUIResourceLeaseRootFixture> Root { NewObject<UDivineBeastsUIResourceLeaseRootFixture>() };
    TWeakObjectPtr<APlayerController> Controller;
    bool bValid = false;
    FRootLeaseFixture()
    {
        World->SetGameInstance(Instance.Get());
        auto* ViewportWorld = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("World"));
        if (!ViewportWorld) return;
        ViewportWorld->SetObjectPropertyValue_InContainer(Viewport.Get(), World.Get());
        Player->ViewportClient = Viewport.Get();
        ReplaceController();
        FLeaseAccess::Configure(*UI.Get(), *Platform.Get(), Data);
        bValid = Controller.IsValid() && UI->GetWorld() == World.Get();
    }
    APlayerController* ReplaceController()
    {
        auto* NewController = World->SpawnActor<APlayerController>();
        if (NewController)
        {
            NewController->SetPlayer(Player.Get());
            Player->PlayerController = NewController;
            Controller = NewController;
        }
        return NewController;
    }
    bool PublishTransientRoot()
    {
        auto* RootProperty = FindFProperty<FObjectPropertyBase>(Platform->GetClass(), TEXT("RootLayout"));
        if (!RootProperty) return false;
        RootProperty->SetObjectPropertyValue_InContainer(Platform.Get(), Root.Get());
        return true;
    }
    ~FRootLeaseFixture()
    {
        FLeaseAccess::Clear(*UI.Get());
        Player->PlayerController = nullptr;
        Player->ViewportClient = nullptr;
        World->SetGameInstance(nullptr);
        World->DestroyWorld(false);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityResourceHeroSwitchTest,
    "DivineBeasts.UI.Resources.AbilityProfileAndIcons.HeroSwitchReleasesBoth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilityResourceHeroSwitchTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FAbilityLeaseFixture F;
    const TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> A(MakeProfile(TEXT("Hero.Zodiac.Rat"), Texture.Get()));
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> B(MakeProfile(TEXT("Hero.Zodiac.Ox"), Texture.Get()));
    TMap<FName, FSoftObjectPath> Paths;
    Paths.Add(A->HeroDefinitionId, FSoftObjectPath(A.Get()));
    Paths.Add(B->HeroDefinitionId, FSoftObjectPath(B.Get()));
    FLeaseAccess::Configure(*F.VM.Get(), F.Data, Paths);
    if (!TestTrue(TEXT("真实Loadout字段可写入测试输入"), FLeaseAccess::SetSnapshot(*F.Source.Get(), A->HeroDefinitionId))) return false;
    TestTrue(TEXT("生产绑定受理"), F.VM->BindToLoadout(F.Source.Get()));
    const TStrongObjectPtr<UDivineBeastsAbilityLoadoutComponent> OtherSource(NewObject<UDivineBeastsAbilityLoadoutComponent>(F.Owner.Get()));
    const TStrongObjectPtr<UDivineBeastsAbilityBarViewModel> OtherVM(NewObject<UDivineBeastsAbilityBarViewModel>(F.Owner.Get()));
    FLeaseAccess::Configure(*OtherVM.Get(), F.Data, Paths);
    FLeaseAccess::SetSnapshot(*OtherSource.Get(), A->HeroDefinitionId);
    OtherVM->BindToLoadout(OtherSource.Get());
    if (!TestEqual(TEXT("两个消费者分别持有Profile请求"), F.Data.Requests.Num(), 2)) { FLeaseAccess::Clear(*OtherVM.Get()); return false; }
    F.Data.Complete(0, true);
    F.Data.Complete(1, true);
    if (!TestEqual(TEXT("真实回调发起各自图标批次"), F.Data.Requests.Num(), 4)) { FLeaseAccess::Clear(*OtherVM.Get()); return false; }
    TestEqual(TEXT("同纹理条目去重"), F.Data.Requests[2]->Lease.ResourcePaths.Num(), 1);
    TestEqual(TEXT("Profile使用Loadout世界期限"), F.Data.Requests[0]->Lifetime, EGamePlatformDataLifetime::World);
    TestEqual(TEXT("Data弱调用者为真实来源"), F.Data.Requests[0]->Caller.Get(), static_cast<UObject*>(F.Source.Get()));
    F.Data.Complete(2, true); F.Data.Complete(3, true);
    TestTrue(TEXT("图标完成后可用"), FLeaseAccess::IconsReady(*F.VM.Get()));
    FLeaseAccess::SetSnapshot(*F.Source.Get(), B->HeroDefinitionId);
    TestEqual(TEXT("换英雄释放旧Profile"), F.Data.Requests[0]->Releases, 1);
    TestEqual(TEXT("换英雄释放旧图标"), F.Data.Requests[2]->Releases, 1);
    TestEqual(TEXT("另一个消费者Profile不被释放"), F.Data.Requests[1]->Releases, 0);
    TestEqual(TEXT("另一个消费者图标不被释放"), F.Data.Requests[3]->Releases, 0);
    TestTrue(TEXT("另一个消费者仍可读Profile"), FLeaseAccess::Profile(*OtherVM.Get()) == A.Get());
    if (!TestEqual(TEXT("换英雄仅新建一份Profile请求"), F.Data.Requests.Num(), 5)) { FLeaseAccess::Clear(*OtherVM.Get()); return false; }
    F.Data.Complete(4, true);
    if (!TestEqual(TEXT("新英雄按实际回调接线到图标"), F.Data.Requests.Num(), 6)) { FLeaseAccess::Clear(*OtherVM.Get()); return false; }
    F.Data.Complete(5, true);
    TestTrue(TEXT("只发布新英雄Profile"), FLeaseAccess::Profile(*F.VM.Get()) == B.Get());
    F.VM->UnbindFromLoadout();
    TestEqual(TEXT("解绑新Profile恰好一次"), F.Data.Requests[4]->Releases, 1);
    TestEqual(TEXT("解绑新图标恰好一次"), F.Data.Requests[5]->Releases, 1);
    FLeaseAccess::Clear(*OtherVM.Get());
    TestEqual(TEXT("无错误Definition加载"), F.Data.DefinitionRequests, 0);
    TestEqual(TEXT("只释放已签发的自身普通租约"), F.Data.InvalidReleases, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityResourceStaleDestroyTest,
    "DivineBeasts.UI.Resources.AbilityProfileAndIcons.StaleCompletionAndDestroy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilityResourceStaleDestroyTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FAbilityLeaseFixture F;
    const TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> A(MakeProfile(TEXT("Hero.Zodiac.Rat"), Texture.Get()));
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> B(MakeProfile(TEXT("Hero.Zodiac.Ox"), Texture.Get()));
    TMap<FName, FSoftObjectPath> Paths;
    Paths.Add(A->HeroDefinitionId, FSoftObjectPath(A.Get())); Paths.Add(B->HeroDefinitionId, FSoftObjectPath(B.Get()));
    FLeaseAccess::Configure(*F.VM.Get(), F.Data, Paths);
    FLeaseAccess::SetSnapshot(*F.Source.Get(), A->HeroDefinitionId); F.VM->BindToLoadout(F.Source.Get());
    FLeaseAccess::SetSnapshot(*F.Source.Get(), B->HeroDefinitionId);
    if (!TestEqual(TEXT("切换前后两份Profile请求"), F.Data.Requests.Num(), 2)) return false;
    F.Data.Complete(0, true);
    TestNull(TEXT("过期Profile不能污染当前英雄"), FLeaseAccess::Profile(*F.VM.Get()));
    TestEqual(TEXT("过期完成不能发起图标"), F.Data.Requests.Num(), 2);
    F.Data.Complete(1, true);
    if (!TestEqual(TEXT("当前Profile才发起图标"), F.Data.Requests.Num(), 3)) return false;
    F.VM->UnbindFromLoadout(); F.Data.Complete(2, true);
    TestNull(TEXT("解绑后迟到图标不能复活Profile"), FLeaseAccess::Profile(*F.VM.Get()));
    TestFalse(TEXT("解绑后图标不可用"), FLeaseAccess::IconsReady(*F.VM.Get()));
    TestEqual(TEXT("迟到通知不重复释放Profile"), F.Data.Requests[1]->Releases, 1);
    TestEqual(TEXT("迟到通知不重复释放图标"), F.Data.Requests[2]->Releases, 1);
    F.VM->BindToLoadout(F.Source.Get());
    if (!TestEqual(TEXT("重新绑定有独立Profile请求"), F.Data.Requests.Num(), 4)) return false;
    F.Data.Complete(3, true);
    if (!TestEqual(TEXT("重新绑定有独立图标请求"), F.Data.Requests.Num(), 5)) return false;
    F.VM->ConditionalBeginDestroy();
    F.Data.Complete(4, true);
    TestEqual(TEXT("BeginDestroy真实入口释放Profile"), F.Data.Requests[3]->Releases, 1);
    TestEqual(TEXT("BeginDestroy真实入口释放在途图标"), F.Data.Requests[4]->Releases, 1);
    TestEqual(TEXT("销毁不会重新生成授权槽位"), F.VM->GetSlotsRef().Num(), 0);
    TestFalse(TEXT("销毁撤销真实Loadout委托"), F.Source->OnLoadoutChanged().IsBoundToObject(F.VM.Get()));
    TestEqual(TEXT("没有跨租约释放"), F.Data.InvalidReleases, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityResourceFallbackTest,
    "DivineBeasts.UI.Resources.AbilityProfileAndIcons.MissingResourceFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilityResourceFallbackTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FAbilityLeaseFixture F;
    const TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> B(MakeProfile(TEXT("Hero.Zodiac.Ox"), Texture.Get()));
    TMap<FName, FSoftObjectPath> Paths;
    Paths.Add(TEXT("Hero.Zodiac.Rat"), FSoftObjectPath(TEXT("/Game/Development/Tests/MissingProfile.MissingProfile")));
    Paths.Add(B->HeroDefinitionId, FSoftObjectPath(B.Get()));
    FLeaseAccess::Configure(*F.VM.Get(), F.Data, Paths);
    FLeaseAccess::SetSnapshot(*F.Source.Get(), TEXT("Hero.Zodiac.Rat")); F.VM->BindToLoadout(F.Source.Get());
    if (!TestEqual(TEXT("真实消费方发起缺失Profile请求"), F.Data.Requests.Num(), 1)) return false;
    F.Data.Complete(0, false);
    TestNull(TEXT("缺失Profile保持安全缺省"), FLeaseAccess::Profile(*F.VM.Get()));
    TestFalse(TEXT("失败Profile终态记录立即释放"), FLeaseAccess::HasProfileLease(*F.VM.Get()));
    FLeaseAccess::SetSnapshot(*F.Source.Get(), TEXT("Hero.Zodiac.Rat"));
    TestEqual(TEXT("同英雄状态事件不造成失败循环"), F.Data.Requests.Num(), 1);
    FLeaseAccess::SetSnapshot(*F.Source.Get(), B->HeroDefinitionId); F.Data.Complete(1, true);
    if (!TestEqual(TEXT("合法Profile继续加载图标"), F.Data.Requests.Num(), 3)) return false;
    F.Data.Complete(2, false);
    TestTrue(TEXT("图标失败仍保留已验证文本Profile"), FLeaseAccess::Profile(*F.VM.Get()) == B.Get());
    TestTrue(TEXT("文本仍有自己的成功租约"), FLeaseAccess::HasProfileLease(*F.VM.Get()));
    TestFalse(TEXT("已驻留纹理也不能掩盖本次图标租约失败"), FLeaseAccess::IconsReady(*F.VM.Get()));
    TestFalse(TEXT("图标失败终态立即释放"), FLeaseAccess::HasIconLease(*F.VM.Get()));
    TestEqual(TEXT("无真实角色就绪时仍不伪造授权槽位"), F.VM->GetSlotsRef().Num(), 0);
    FLeaseAccess::SetSnapshot(*F.Source.Get(), TEXT("Hero.Unregistered"));
    TestNull(TEXT("主资产未登记回退为空Profile"), FLeaseAccess::Profile(*F.VM.Get()));
    TestEqual(TEXT("无主资产路径不伪造资源请求"), F.Data.Requests.Num(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRootResourceLifecycleTest,
    "DivineBeasts.UI.Resources.RootLayout.PendingFailureAndControllerChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRootResourceLifecycleTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FRootLeaseFixture F;
    if (!TestTrue(TEXT("本地玩家/控制器/世界夹具关系有效"), F.bValid)) return false;
    F.UI->PlayerControllerChanged(F.Controller.Get());
    TestTrue(TEXT("控制器事件真实接线Pending"), FLeaseAccess::Pending(*F.UI.Get()));
    FLeaseAccess::Ensure(*F.UI.Get()); FLeaseAccess::Ensure(*F.UI.Get());
    if (!TestEqual(TEXT("在途请求合并"), F.Data.Requests.Num(), 1)) return false;
    TestEqual(TEXT("根布局弱调用者为所属LocalPlayer"), F.Data.Requests[0]->Caller.Get(), static_cast<UObject*>(F.Player.Get()));
    TestEqual(TEXT("根布局跨Travel使用Instance期限"), F.Data.Requests[0]->Lifetime, EGamePlatformDataLifetime::Instance);
    F.Data.Complete(0, false);
    TestTrue(TEXT("缺失根资源有显式Failed"), FLeaseAccess::Failed(*F.UI.Get()));
    FLeaseAccess::Ensure(*F.UI.Get());
    TestEqual(TEXT("同控制器失败不无限重试"), F.Data.Requests.Num(), 1);
    F.UI->PlayerControllerChanged(F.ReplaceController());
    if (!TestEqual(TEXT("真实新控制器开启新代次"), F.Data.Requests.Num(), 2)) return false;
    F.UI->PlayerControllerChanged(F.ReplaceController());
    if (!TestEqual(TEXT("再次切换仅保留后继请求"), F.Data.Requests.Num(), 3)) return false;
    F.Data.Complete(1, true);
    TestEqual(TEXT("旧控制器在途租约只释放一次"), F.Data.Requests[1]->Releases, 1);
    TestNull(TEXT("过期成功不能安装布局"), F.Platform->GetRootLayout());
    TestTrue(TEXT("后继仍Pending"), FLeaseAccess::Pending(*F.UI.Get()));
    F.UI->Deinitialize(); F.Data.Complete(2, true);
    TestEqual(TEXT("Deinitialize释放后继在途租约"), F.Data.Requests[2]->Releases, 1);
    TestFalse(TEXT("退出后拒绝新请求"), FLeaseAccess::Ensure(*F.UI.Get()));
    TestNull(TEXT("退出后的迟到成功不能复活布局"), F.Platform->GetRootLayout());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRootResourceInstallTest,
    "DivineBeasts.UI.Resources.RootLayout.ReadyExternalInstallAndReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRootResourceInstallTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FRootLeaseFixture F;
    if (!TestTrue(TEXT("本地玩家夹具有效"), F.bValid)) return false;
    int32 Installs = 0;
    FLeaseAccess::SetInstaller(*F.UI.Get(), [&](TSubclassOf<UGamePlatformUILayerStack> Class)
    {
        ++Installs;
        TestEqual(TEXT("生产回调由ResolveObject得到真实软类"), Class.Get(), UDivineBeastsUIResourceLeaseRootFixture::StaticClass());
        TestTrue(TEXT("安装边界已有重入保护"), FLeaseAccess::Installing(*F.UI.Get()));
        FLeaseAccess::Ensure(*F.UI.Get());
        TestFalse(TEXT("安装期间外部安装不能递归"), F.UI->InstallRootLayoutClass(Class));
        return F.PublishTransientRoot(); // 唯一替身边界：无窗口挂载，不冒充真实Slate成功。
    });
    F.UI->PlayerControllerChanged(F.Controller.Get());
    if (!TestEqual(TEXT("先异步申请资源"), F.Data.Requests.Num(), 1)) return false;
    F.Data.Complete(0, true);
    TestEqual(TEXT("成功完成只安装一次"), Installs, 1);
    TestTrue(TEXT("安装后显式Ready"), FLeaseAccess::Ready(*F.UI.Get()));
    TestEqual(TEXT("安装重入不重复申请"), F.Data.Requests.Num(), 1);
    TestEqual(TEXT("Ready保持类资源租约"), F.Data.Requests[0]->Releases, 0);
    F.UI->PlayerControllerChanged(F.ReplaceController());
    TestTrue(TEXT("同LocalPlayer切图保留已安装根"), FLeaseAccess::Ensure(*F.UI.Get()));
    TestEqual(TEXT("同玩家切换不重载根类"), F.Data.Requests.Num(), 1);
    TestEqual(TEXT("同玩家切换保留成功租约"), F.Data.Requests[0]->Releases, 0);
    TestTrue(TEXT("公开外部安装入口成功"), F.UI->InstallRootLayoutClass(UDivineBeastsUIResourceLeaseRootFixture::StaticClass()));
    TestEqual(TEXT("外部安装替换后只释放旧自有租约"), F.Data.Requests[0]->Releases, 1);
    TestTrue(TEXT("外部根保持Ready"), FLeaseAccess::Ready(*F.UI.Get()));
    F.UI->Deinitialize();
    TestEqual(TEXT("退出不重复释放外部已替换租约"), F.Data.Requests[0]->Releases, 1);

    FRootLeaseFixture Pending;
    if (!TestTrue(TEXT("外部安装抢占夹具有效"), Pending.bValid)) return false;
    int32 ExternalInstalls = 0;
    FLeaseAccess::SetInstaller(*Pending.UI.Get(), [&](TSubclassOf<UGamePlatformUILayerStack>)
        { ++ExternalInstalls; return Pending.PublishTransientRoot(); });
    Pending.UI->PlayerControllerChanged(Pending.Controller.Get());
    if (!TestEqual(TEXT("外部安装前有在途请求"), Pending.Data.Requests.Num(), 1)) return false;
    TestTrue(TEXT("外部安装可接管Pending"), Pending.UI->InstallRootLayoutClass(UDivineBeastsUIResourceLeaseRootFixture::StaticClass()));
    Pending.Data.Complete(0, true);
    TestEqual(TEXT("抢占释放在途租约一次"), Pending.Data.Requests[0]->Releases, 1);
    TestEqual(TEXT("迟到默认根不能覆盖外部安装"), ExternalInstalls, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRootResourcePlayerIsolationTest,
    "DivineBeasts.UI.Resources.RootLayout.LocalPlayerIsolationAndInvalidClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRootResourcePlayerIsolationTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine"), GEngine)) return false;
    FRootLeaseFixture A; FRootLeaseFixture B;
    if (!TestTrue(TEXT("两个独立本地玩家夹具有效"), A.bValid && B.bValid)) return false;
    A.UI->PlayerControllerChanged(A.Controller.Get()); B.UI->PlayerControllerChanged(B.Controller.Get());
    if (!TestEqual(TEXT("玩家A独立请求"), A.Data.Requests.Num(), 1) ||
        !TestEqual(TEXT("玩家B独立请求"), B.Data.Requests.Num(), 1)) return false;
    A.UI->PlayerControllerChanged(nullptr);
    TestEqual(TEXT("撤控制器释放A在途资源"), A.Data.Requests[0]->Releases, 1);
    TestEqual(TEXT("B租约不受A影响"), B.Data.Requests[0]->Releases, 0);
    TestTrue(TEXT("B仍处于Pending"), FLeaseAccess::Pending(*B.UI.Get()));
    A.UI->PlayerControllerChanged(B.Controller.Get());
    TestEqual(TEXT("其他玩家控制器不能为A恢复请求"), A.Data.Requests.Num(), 1);
    TestEqual(TEXT("误投控制器事件不释放B"), B.Data.Requests[0]->Releases, 0);
    A.Data.Complete(0, true);
    TestNull(TEXT("无控制器时迟到完成不能装根"), A.Platform->GetRootLayout());
    B.UI->Deinitialize();
    TestEqual(TEXT("B仅清自己的在途需求"), B.Data.Requests[0]->Releases, 1);
    TestEqual(TEXT("不存在跨玩家释放"), A.Data.InvalidReleases + B.Data.InvalidReleases, 0);
    // 正式完成检查必须拒绝非根布局UClass；已驻留类也不能绕过类型边界。
    FRootLeaseFixture Invalid;
    if (!TestTrue(TEXT("非法类夹具有效"), Invalid.bValid)) return false;
    FLeaseAccess::SetRootPath(*Invalid.UI.Get(), FSoftObjectPath(UObject::StaticClass()));
    Invalid.UI->PlayerControllerChanged(Invalid.Controller.Get());
    if (!TestEqual(TEXT("非法类场景有在途请求"), Invalid.Data.Requests.Num(), 1)) return false;
    Invalid.Data.Complete(0, true);
    TestNull(TEXT("普通UObject类不能安装为根布局"), Invalid.Platform->GetRootLayout());
    TestEqual(TEXT("非法类成功资源租约仍须释放"), Invalid.Data.Requests[0]->Releases, 1);
    TestTrue(TEXT("失败状态保留"), FLeaseAccess::Failed(*Invalid.UI.Get()));
    TestFalse(TEXT("外部空类拒绝且不改变失败状态"), Invalid.UI->InstallRootLayoutClass(nullptr));
    TestTrue(TEXT("拒绝外部空类不触发自动重试"), FLeaseAccess::Failed(*Invalid.UI.Get()));
    return true;
}
#endif
