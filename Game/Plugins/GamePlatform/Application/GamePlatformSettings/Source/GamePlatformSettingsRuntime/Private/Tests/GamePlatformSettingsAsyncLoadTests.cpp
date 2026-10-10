// 平台设置读取回归：测试Port手动控制终态，不访问磁盘或网络；实际Runtime负责代次和原子发布。
// 验证受理不等于已加载、读取失败保留旧快照、重复终态、工厂卸载和销毁后迟到回调。
#include "Subsystems/GamePlatformSettingsSubsystem.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Features/IModularFeatures.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FSettingsLoadTestSignals
{
    TArray<FGamePlatformSettingsLoadCompletion> Completions;
    TArray<FGamePlatformSettingsSaveCompletion> SaveCompletions;
    bool bSyncSave = false;
    bool bRejectSave = false;
};
class FSettingsAsyncFixtureProvider final : public IGamePlatformSettingsProvider
{
public:
    FName GetProviderId() const override { return TEXT("Fixture.AsyncSettings"); }
    void GetSettingDescriptors(TArray<FGamePlatformSettingDescriptor>& Descriptors) const override
    {
        FGamePlatformSettingDescriptor Descriptor;
        Descriptor.SettingId = TEXT("Fixture.Value"); Descriptor.Category = TEXT("Fixture");
        Descriptor.ValueType = EGamePlatformSettingValueType::Number;
        Descriptor.DefaultValue = FGamePlatformSettingValue::MakeNumber(1.0);
        Descriptor.DefaultLayer = EGamePlatformSettingLayer::ProviderDefault;
        Descriptor.PersistenceScope = EGamePlatformSettingScope::User;
        Descriptor.RuntimeScope = EGamePlatformSettingRuntimeScope::Client;
        Descriptors.Add(Descriptor);
    }
};
class FSettingsAsyncFixturePersistence final : public IGamePlatformSettingsPersistenceProvider
{
public:
    TSharedRef<FSettingsLoadTestSignals> Signals;
    explicit FSettingsAsyncFixturePersistence(TSharedRef<FSettingsLoadTestSignals> InSignals) : Signals(InSignals) {}
    TUniquePtr<IGamePlatformSettingsPersistenceProvider> CreateScopedProvider() const override
    { return MakeUnique<FSettingsAsyncFixturePersistence>(Signals); }
    FName GetPersistenceId() const override { return TEXT("Fixture.AsyncPersistence"); }
    bool SupportsRuntime(EGamePlatformSettingRuntimeScope Scope) const override { return Scope == EGamePlatformSettingRuntimeScope::Client; }
    FGamePlatformResult SetUserContext(const FString&) override { return FGamePlatformResult::Success(); }
    FGamePlatformResult Load(const TMap<FName, FGamePlatformSettingDescriptor>&, FGamePlatformSettingsPersistencePayload&) override
    { return FGamePlatformResult::Unsupported(TEXT("FixtureMustUseAsync"), TEXT("本测试要求消费异步读取入口。")); }
    FGamePlatformResult BeginLoad(const TMap<FName, FGamePlatformSettingDescriptor>&, FGamePlatformSettingsLoadCompletion Completion) override
    { Signals->Completions.Add(MoveTemp(Completion)); return FGamePlatformResult::Success(); }
    FGamePlatformResult BeginSave(const TMap<FName, FGamePlatformSettingValue>&, int32, FGamePlatformSettingsSaveCompletion Completion) override
    { Signals->SaveCompletions.Add(Completion); if (Signals->bSyncSave) { Completion(FGamePlatformResult::Success()); } return Signals->bRejectSave ? FGamePlatformResult::Failure(TEXT("FixtureStartFailed"), TEXT("测试受理失败。")) : FGamePlatformResult::Success(); }
};
// 测试隔离真实模块工厂，作用域结束按原指针恢复；既有GI会收到正常拓扑事件，禁止并行执行此回归。
struct FScopedSettingsPersistenceFixture
{
    TArray<IGamePlatformSettingsPersistenceProvider*> Original;
    FSettingsAsyncFixturePersistence& Fixture;
    explicit FScopedSettingsPersistenceFixture(FSettingsAsyncFixturePersistence& InFixture) : Fixture(InFixture)
    {
        auto& Features = IModularFeatures::Get();
        Original = Features.GetModularFeatureImplementations<IGamePlatformSettingsPersistenceProvider>(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName());
        for (auto* Provider : Original) { Features.UnregisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), Provider); }
        Features.RegisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &Fixture);
    }
    ~FScopedSettingsPersistenceFixture()
    {
        auto& Features = IModularFeatures::Get();
        Features.UnregisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &Fixture);
        for (auto* Provider : Original) { Features.RegisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), Provider); }
    }
};
FGamePlatformSettingsPersistencePayload SettingsPayload(double Value)
{
    FGamePlatformSettingsPersistencePayload Payload;
    Payload.Layers.FindOrAdd(EGamePlatformSettingLayer::User).Add(TEXT("Fixture.Value"), FGamePlatformSettingValue::MakeNumber(Value));
    return Payload;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsAsyncLoadGenerationTest, "GamePlatform.Settings.Runtime.AsyncLoadGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSettingsAsyncLoadGenerationTest::RunTest(const FString&)
{
    auto Signals = MakeShared<FSettingsLoadTestSignals>();
    FSettingsAsyncFixturePersistence Persistence(Signals); FScopedSettingsPersistenceFixture Scope(Persistence);
    FSettingsAsyncFixtureProvider Provider;
    auto& Features = IModularFeatures::Get();
    Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    auto* Instance = NewObject<UGameInstance>();
    auto* Service = NewObject<UGamePlatformSettingsSubsystem>(Instance);
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Service->Initialize(Collection);
    TestTrue(TEXT("初始化受理但快照仍标记加载中"), Service->GetSnapshot().bLoading);
    if (Signals->Completions.Num() != 1) { AddError(TEXT("异步读取没有唯一受理")); Service->Deinitialize(); Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider); return false; }
    auto First = Signals->Completions[0]; First(SettingsPayload(3.0), FGamePlatformResult::Success());
    FGamePlatformSettingValue Value;
    TestTrue(TEXT("成功终态发布已加载值"), Service->GetValue(TEXT("Fixture.Value"), Value)); TestEqual(TEXT("发布值正确"), Value.NumberValue, 3.0);
    const auto Revision = Service->GetSnapshot().Revision;
    First(SettingsPayload(9.0), FGamePlatformResult::Success());
    TestEqual(TEXT("重复终态不增加发布版本"), Service->GetSnapshot().Revision, Revision);
    TestTrue(TEXT("重载仅返回受理"), Service->Reload().IsSuccess());
    TestTrue(TEXT("重载在飞标记"), Service->GetSnapshot().bLoading);
    Signals->Completions[1]({}, FGamePlatformResult::Failure(TEXT("FixtureReadFailed"), TEXT("测试读取失败。")));
    Service->GetValue(TEXT("Fixture.Value"), Value); TestEqual(TEXT("失败保留旧只读快照"), Value.NumberValue, 3.0);
    Service->Reload(); auto PreviousAccount = Signals->Completions.Last();
    TestFalse(TEXT("非法用户键不得受理"), Service->SwitchUserContext(FString::ChrN(129, TEXT('x'))).IsSuccess());
    TestTrue(TEXT("非法换绑不取消在飞读取"), Service->GetSnapshot().bLoading);
    TestTrue(TEXT("新账号可换绑并异步读取"), Service->SwitchUserContext(TEXT("Fixture-B")).IsSuccess());
    const auto AfterSwitch = Service->GetSnapshot().Revision;
    PreviousAccount(SettingsPayload(9.0), FGamePlatformResult::Success());
    TestEqual(TEXT("旧账号在飞回调不发布"), Service->GetSnapshot().Revision, AfterSwitch);
    Signals->Completions.Last()(SettingsPayload(7.0), FGamePlatformResult::Success());
    Service->GetValue(TEXT("Fixture.Value"), Value); TestEqual(TEXT("新账号终态发布自己的值"), Value.NumberValue, 7.0);
    Service->Reload(); auto BeforeInvalidTopology = Signals->Completions.Last();
    FSettingsAsyncFixtureProvider ConflictingProvider;
    AddExpectedError(TEXT("Settings provider topology reload failed"), EAutomationExpectedErrorFlags::Contains, 1);
    Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &ConflictingProvider);
    const auto BeforeLateRevision = Service->GetSnapshot().Revision;
    BeforeInvalidTopology(SettingsPayload(99.0), FGamePlatformResult::Success());
    TestEqual(TEXT("重建失败也必须失效旧加载终态"), Service->GetSnapshot().Revision, BeforeLateRevision);
    Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &ConflictingProvider);
    Signals->Completions.Last()(SettingsPayload(7.0), FGamePlatformResult::Success());
    Service->Reload(); auto Late = Signals->Completions.Last();
    Features.UnregisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &Persistence);
    TestFalse(TEXT("工厂卸载后默认候选必须结束加载"), Service->GetSnapshot().bLoading);
    TestTrue(TEXT("无可选持久层仍有有效默认视图"), Service->GetValue(TEXT("Fixture.Value"), Value));
    TestEqual(TEXT("默认层候选不能丢弃自己的完成代次"), Value.NumberValue, 1.0);
    const auto AfterTopology = Service->GetSnapshot().Revision;
    Late(SettingsPayload(9.0), FGamePlatformResult::Success());
    TestEqual(TEXT("工厂卸载使旧完成失效"), Service->GetSnapshot().Revision, AfterTopology);
    Features.RegisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &Persistence);
    auto AfterClose = Signals->Completions.Last(); Service->Deinitialize();
    AfterClose(SettingsPayload(9.0), FGamePlatformResult::Success());
    TestEqual(TEXT("退出后完成不能发布"), Service->GetSnapshot().Revision, int64(0));
    Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    return true;
}

// 保存回归：使用实际Runtime与同步/重复Completion，确保独立请求门闩先于外部Provider调用建立。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsSaveTerminalGateTest, "GamePlatform.Settings.Runtime.SaveTerminalGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSettingsSaveTerminalGateTest::RunTest(const FString&)
{
    auto Signals = MakeShared<FSettingsLoadTestSignals>();
    FSettingsAsyncFixturePersistence Persistence(Signals); FScopedSettingsPersistenceFixture Scope(Persistence);
    FSettingsAsyncFixtureProvider Provider; auto& Features = IModularFeatures::Get();
    Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    auto* Instance = NewObject<UGameInstance>(); auto* Service = NewObject<UGamePlatformSettingsSubsystem>(Instance);
    FSubsystemCollection<UGameInstanceSubsystem> Collection; Service->Initialize(Collection);
    Signals->Completions.Last()(SettingsPayload(1), FGamePlatformResult::Success());
    Service->SetValue(TEXT("Fixture.Value"), EGamePlatformSettingLayer::User, FGamePlatformSettingValue::MakeNumber(2));
    Service->Apply(EGamePlatformSettingsChangeReason::Apply); Signals->bSyncSave = true;
    TestTrue(TEXT("同步成功被受理"), Service->Save().IsSuccess());
    TestFalse(TEXT("同步终态不得被返回栈重标在飞"), Service->GetSnapshot().bSaveInFlight);
    auto Old = Signals->SaveCompletions.Last(); Signals->bSyncSave = false;
    Service->SetValue(TEXT("Fixture.Value"), EGamePlatformSettingLayer::User, FGamePlatformSettingValue::MakeNumber(3));
    Service->Apply(EGamePlatformSettingsChangeReason::Apply); Service->Save();
    Old(FGamePlatformResult::Success());
    TestTrue(TEXT("旧保存重复结果不得消费新请求"), Service->GetSnapshot().bSaveInFlight);
    Signals->SaveCompletions.Last()(FGamePlatformResult::Success());
    Service->SetValue(TEXT("Fixture.Value"), EGamePlatformSettingLayer::User, FGamePlatformSettingValue::MakeNumber(4));
    Service->Apply(EGamePlatformSettingsChangeReason::Apply); Signals->bRejectSave = true;
    TestFalse(TEXT("启动失败必须以相同终态路径拒绝"), Service->Save().IsSuccess());
    TestFalse(TEXT("启动失败不遗留在飞标记"), Service->GetSnapshot().bSaveInFlight);
    auto Rejected = Signals->SaveCompletions.Last(); Rejected(FGamePlatformResult::Success());
    TestTrue(TEXT("启动失败后的迟到成功不得清除未保存Dirty"), Service->GetSnapshot().bDirty);
    Signals->bRejectSave = false; Service->Save(); Signals->SaveCompletions.Last()(FGamePlatformResult::Success());
    bool bClose = false; FGamePlatformResult SubscriptionResult;
    Service->Subscribe(Instance, [Service, &bClose](auto&&...) { if (bClose) { Service->Deinitialize(); } }, SubscriptionResult);
    bClose = true; const int32 LoadCount = Signals->Completions.Num();
    TestFalse(TEXT("账号投影广播内关闭使换绑失败"), Service->SwitchUserContext(TEXT("Fixture-NewUser")).IsSuccess());
    TestEqual(TEXT("关闭后不得重建读取"), Signals->Completions.Num(), LoadCount);
    Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    return true;
}


// 订阅者而非轮询Snapshot观察准备失败；直接Reload与拓扑调用方均应仅发布一次非Loading失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsPreparationFailureEventTest, "GamePlatform.Settings.Runtime.PreparationFailureEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSettingsPreparationFailureEventTest::RunTest(const FString&)
{
    auto Signals = MakeShared<FSettingsLoadTestSignals>();
    FSettingsAsyncFixturePersistence Persistence(Signals); FScopedSettingsPersistenceFixture Scope(Persistence);
    FSettingsAsyncFixtureProvider Provider; FSettingsAsyncFixtureProvider ConflictingProvider;
    auto& Features = IModularFeatures::Get(); Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    auto* Instance = NewObject<UGameInstance>(); auto* Service = NewObject<UGamePlatformSettingsSubsystem>(Instance);
    FSubsystemCollection<UGameInstanceSubsystem> Collection; Service->Initialize(Collection); Signals->Completions.Last()(SettingsPayload(3), FGamePlatformResult::Success());
    int32 Events = 0; bool bObservedLoading = true; bool bObservedSuccess = true; FGamePlatformResult SubscriptionResult;
    Service->Subscribe(Instance, [&Events, &bObservedLoading, &bObservedSuccess](const auto&, const auto& Published)
    { ++Events; bObservedLoading = Published.bLoading; bObservedSuccess = Published.LastResult.IsSuccess(); }, SubscriptionResult);
    Events = 0; AddExpectedError(TEXT("Settings provider topology reload failed"), EAutomationExpectedErrorFlags::Contains, 1);
    Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &ConflictingProvider);
    TestEqual(TEXT("拓扑准备失败只通知一次"), Events, 1); TestFalse(TEXT("订阅者收到非Loading终态"), bObservedLoading); TestFalse(TEXT("订阅者收到真实失败"), bObservedSuccess);
    Events = 0; TestFalse(TEXT("直接Reload拒绝冲突注册表"), Service->Reload().IsSuccess());
    TestEqual(TEXT("直接Reload准备失败也通知一次"), Events, 1); TestFalse(TEXT("直接失败不遗留加载投影"), bObservedLoading); TestFalse(TEXT("直接失败状态不伪成功"), bObservedSuccess);
    Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &ConflictingProvider); Signals->Completions.Last()(SettingsPayload(3), FGamePlatformResult::Success());
    FSettingsAsyncFixturePersistence SecondPersistence(Signals);
    Events = 0; AddExpectedError(TEXT("Settings provider topology reload failed"), EAutomationExpectedErrorFlags::Contains, 1);
    Features.RegisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &SecondPersistence);
    TestEqual(TEXT("持久化Provider解析失败只通知一次"), Events, 1); TestFalse(TEXT("Provider失败终态不Loading"), bObservedLoading); TestFalse(TEXT("Provider失败终态可见"), bObservedSuccess);
    Events = 0; TestFalse(TEXT("直接Reload拒绝歧义持久化Provider"), Service->Reload().IsSuccess()); TestEqual(TEXT("直接Provider失败也通知一次"), Events, 1);
    Service->Deinitialize(); Features.UnregisterModularFeature(IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(), &SecondPersistence); Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider);
    return true;
}

namespace
{
// 仅测试调度真实GT延后拓扑唤醒；夹具自身持有订阅所有者与服务直到终态，不访问磁盘。
class FSettingsDeferredPreparationCommand final : public IAutomationLatentCommand
{
public:
    explicit FSettingsDeferredPreparationCommand(FAutomationTestBase* InTest) : Test(InTest), Signals(MakeShared<FSettingsLoadTestSignals>()), Persistence(Signals), Scope(Persistence) {}
    ~FSettingsDeferredPreparationCommand() override
    {
        if (Service.IsValid()) { Service->Deinitialize(); }
        auto& Features = IModularFeatures::Get();
        if (bConflictRegistered) { Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Conflict); }
        if (bProviderRegistered) { Features.UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider); }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 10) { Test->AddError(TEXT("延后拓扑失败终态事件超时")); return true; }
        if (!bStarted)
        {
            auto& Features = IModularFeatures::Get(); Features.RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Provider); bProviderRegistered = true;
            Instance.Reset(NewObject<UGameInstance>()); Service.Reset(NewObject<UGamePlatformSettingsSubsystem>(Instance.Get())); Service->Initialize(Collection); Signals->Completions.Last()(SettingsPayload(3), FGamePlatformResult::Success());
            FGamePlatformResult SubscribeResult;
            Service->Subscribe(Instance.Get(), [this](const auto&, const auto& Published)
            {
                if (Published.bLoading && !bConflictRegistered)
                {
                    bConflictRegistered = true;
                    IModularFeatures::Get().RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(), &Conflict);
                }
                if (!Published.bLoading && !Published.LastResult.IsSuccess()) { ++Failures; }
            }, SubscribeResult);
            bStarted = true; Service->Reload(); return false;
        }
        if (Failures == 0) { return false; }
        Test->TestEqual(TEXT("延后Topology唤醒准备失败订阅者收到一次终态"), Failures, 1);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    TSharedRef<FSettingsLoadTestSignals> Signals;
    FSettingsAsyncFixturePersistence Persistence;
    FScopedSettingsPersistenceFixture Scope;
    FSettingsAsyncFixtureProvider Provider, Conflict;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UGamePlatformSettingsSubsystem> Service;
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    bool bStarted = false, bProviderRegistered = false, bConflictRegistered = false;
    int32 Failures = 0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsDeferredPreparationEventTest, "GamePlatform.Settings.Runtime.DeferredPreparationEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSettingsDeferredPreparationEventTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FSettingsDeferredPreparationCommand(this)); return true; }

#endif
