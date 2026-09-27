#include "Interfaces/IGamePlatformDataService.h"
#include "Loading/GamePlatformAssetManager.h"
#include "Loading/DataNextTick.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
void ShutdownTestInstance(UGameInstance* Instance)
{
    if (!Instance) return;
    UWorld* InstanceWorld = Instance->GetWorld();
    if (InstanceWorld)
    {
        InstanceWorld->DestroyWorld(false);
    }
    // Shutdown会通知全部真实子系统；其间保留WorldContext，避免其他子系统访问悬空上下文。
    Instance->Shutdown();
    if (InstanceWorld) GEngine->DestroyWorldContext(InstanceWorld);
}
/** 实际InitializeStandalone创建实例子系统后走公开门面，不手动创建数据子系统，不改写源资产。 */
class FDataLeaseIntegrationCommand final : public IAutomationLatentCommand
{
public:
    FDataLeaseIntegrationCommand(FAutomationTestBase* InTest, FPrimaryAssetId InId)
        : Test(InTest), Id(InId), StartedSeconds(FPlatformTime::Seconds()) {}
    virtual ~FDataLeaseIntegrationCommand() override { CallbackAlive.Reset(); Cleanup(); }
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test->AddError(TEXT("真实资产租约集成超过30秒，不能判为成功。")); Cleanup(); return true; }
        auto* Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
        if (!Manager) { Test->AddError(TEXT("正式工程必须先配置GamePlatformAssetManager。")); Cleanup(); return true; }
        if (Phase == 0)
        {
            if (Manager->GetPrimaryAssetPath(Id).IsNull())
            { Test->AddError(TEXT("指定测试定义未被正式主资产扫描发现。")); return true; }
            Manager->GetPrimaryAssetHandle(Id, false, &BaselineBundles);
            bWasInitiallyLoaded = Manager->GetPrimaryAssetObject(Id) != nullptr || Manager->GetPrimaryAssetHandle(Id).IsValid();
            TArray<FName> CurrentBundles;
            Manager->GetPrimaryAssetHandle(Id, true, &CurrentBundles);
            for (FName Bundle : CurrentBundles) BaselineBundles.AddUnique(Bundle);
            InstanceA.Reset(NewObject<UGameInstance>(GEngine));
            InstanceB.Reset(NewObject<UGameInstance>(GEngine));
            InstanceA->InitializeStandalone(FName(*(TEXT("DataTestA_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            InstanceB->InitializeStandalone(FName(*(TEXT("DataTestB_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            bIsInitialized = true;
            ServiceA = IGamePlatformDataService::Get(*InstanceA);
            ServiceB = IGamePlatformDataService::Get(*InstanceB);
            if (!ServiceA || !ServiceB) { Test->AddError(TEXT("实际实例子系统集合未提供数据公开门面。")); Cleanup(); return true; }
            FGamePlatformResult Accepted;
            LeaseA = ServiceA->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("UI"), TEXT("Core")},
                EGamePlatformDataLifetime::Instance, InstanceA.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountA; ResultA = Result; } }, Accepted);
            Test->TestTrue(TEXT("A接纳后尚未同步通知"), Accepted.IsSuccess() && CountA == 0);
            Test->TestEqual(TEXT("重复分组规范化"), LeaseA.Bundles.Num(), 2);
            LeaseB = ServiceB->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("core"), TEXT("Server")},
                EGamePlatformDataLifetime::Instance, InstanceB.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountB; ResultB = Result; } }, Accepted);
            Test->TestTrue(TEXT("B接纳后尚未同步通知"), Accepted.IsSuccess() && CountB == 0);
            Test->TestFalse(TEXT("错实例不能释放A"), ServiceB->ReleaseDefinition(LeaseA).IsSuccess());
            auto Forged = LeaseA; ++Forged.Generation;
            Test->TestFalse(TEXT("错代次不能释放A"), ServiceA->ReleaseDefinition(Forged).IsSuccess());
            auto CancelLease = ServiceA->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {},
                EGamePlatformDataLifetime::Instance, InstanceA.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountCancelled; Test->TestTrue(TEXT("取消抢先时仅取消终态"), Result.Status == EGamePlatformResultStatus::Cancelled); } }, Accepted);
            Test->TestTrue(TEXT("首次释放成功"), ServiceA->ReleaseDefinition(CancelLease).IsSuccess());
            Test->TestTrue(TEXT("重复释放幂等"), ServiceA->ReleaseDefinition(CancelLease).IsSuccess());
            Test->TestEqual(TEXT("取消不同步重入"), CountCancelled, 0);
            // 实际世界销毁只清理对应世界租约，实例租约在同一事件后仍继续加载。
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
            World->SetGameInstance(InstanceA.Get());
            auto WorldLease = ServiceA->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {},
                EGamePlatformDataLifetime::World, World.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) Test->TestFalse(TEXT("旧世界绝不迟到成功"), Result.IsSuccess()); }, Accepted);
            Test->TestTrue(TEXT("世界请求已接纳"), WorldLease.IsValid() && Accepted.IsSuccess());
            World->DestroyWorld(false);
            Test->TestTrue(TEXT("世界销毁撤销对应租约"), ServiceA->GetLeaseState(WorldLease) == EGamePlatformDataRequestState::Released);
            World.Reset();
            Phase = 1;
            return false;
        }
        if (Phase == 1)
        {
            if (CountA == 0 || CountB == 0 || CountCancelled == 0) return false;
            Test->TestTrue(TEXT("A真实加载成功"), ResultA.IsSuccess());
            Test->TestTrue(TEXT("B真实加载成功"), ResultB.IsSuccess());
            if (!ResultA.IsSuccess() || !ResultB.IsSuccess()) { Cleanup(); return true; }
            const auto* LoadedA = ServiceA->GetLoadedDefinition(LeaseA);
            Test->TestNotNull(TEXT("A成功后可读取实际对象"), LoadedA);
            Test->TestTrue(TEXT("A/B读取同一引擎对象"), LoadedA && LoadedA == ServiceB->GetLoadedDefinition(LeaseB));
            TArray<FName> Union;
            Manager->GetPrimaryAssetHandle(Id, true, &Union);
            Test->TestTrue(TEXT("共同需求包含Core/UI/Server"), Union.Contains(TEXT("Core")) && Union.Contains(TEXT("UI")) && Union.Contains(TEXT("Server")));
            Test->TestTrue(TEXT("成功租约可释放"), ServiceA->ReleaseDefinition(LeaseA).IsSuccess());
            ShutdownTestInstance(InstanceA.Get()); bIsAClosed = true; ServiceA = nullptr;
            Phase = 2;
            return false;
        }
        if (Phase == 2)
        {
            const auto Pending = Manager->GetPrimaryAssetHandle(Id);
            if (Pending && !Pending->HasLoadCompleted()) return false;
            Test->TestNotNull(TEXT("A退出后B仍可读"), ServiceB->GetLoadedDefinition(LeaseB));
            TArray<FName> Union;
            Manager->GetPrimaryAssetHandle(Id, true, &Union);
            Test->TestTrue(TEXT("释放A保留B的Core/Server"), Union.Contains(TEXT("Core")) && Union.Contains(TEXT("Server")));
            if (!BaselineBundles.Contains(TEXT("UI"))) Test->TestFalse(TEXT("无外部UI需求时移除A的UI"), Union.Contains(TEXT("UI")));
            FGamePlatformResult Accepted;
            CacheLease = ServiceB->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("Server")},
                EGamePlatformDataLifetime::Instance, InstanceB.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountCache; Test->TestTrue(TEXT("缓存/无新工作仍真实成功"), Result.IsSuccess()); } }, Accepted);
            Test->TestTrue(TEXT("缓存申请接纳"), Accepted.IsSuccess());
            Test->TestEqual(TEXT("缓存也必须延后通知"), CountCache, 0);
            StartedCancelLease = ServiceB->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("CancelAfterStart")},
                EGamePlatformDataLifetime::Instance, InstanceB.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountStartedCancel; Test->TestTrue(TEXT("引擎请求开始后取消仍只有取消终态"), Result.Status == EGamePlatformResultStatus::Cancelled); } }, Accepted);
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
            World->SetGameInstance(InstanceB.Get());
            ReadyWorldLease = ServiceB->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("Core"), TEXT("Server")},
                EGamePlatformDataLifetime::World, World.Get(), [this, Alive = TWeakPtr<int32>(CallbackAlive)](const auto&, const auto& Result)
                { if (Alive.IsValid()) { ++CountReadyWorld; Test->TestTrue(TEXT("世界租约先真实完成"), Result.IsSuccess()); } }, Accepted);
            Phase = 3;
            return false;
        }
        if (Phase == 3)
        {
            if (!bHasCancelledStartedRequest)
            {
                TArray<FName> PendingBundles;
                Manager->GetPrimaryAssetHandle(Id, false, &PendingBundles);
                if (!PendingBundles.Contains(TEXT("CancelAfterStart"))) return false;
                Test->TestTrue(TEXT("引擎已有此请求分组但尚未发终态"), ServiceB->GetLeaseState(StartedCancelLease) == EGamePlatformDataRequestState::Loading);
                ServiceB->ReleaseDefinition(StartedCancelLease);
                bHasCancelledStartedRequest = true;
            }
            if (CountCache == 0 || CountStartedCancel == 0 || CountReadyWorld == 0) return false;
            Test->TestNotNull(TEXT("缓存租约可读取"), ServiceB->GetLoadedDefinition(CacheLease));
            Test->TestNotNull(TEXT("成功世界租约在清理前可读"), ServiceB->GetLoadedDefinition(ReadyWorldLease));
            World->DestroyWorld(false);
            World.Reset();
            Test->TestTrue(TEXT("成功世界租约在销毁后已释放"), ServiceB->GetLeaseState(ReadyWorldLease) == EGamePlatformDataRequestState::Released);
            Test->TestNotNull(TEXT("同实例跨图租约不受世界销毁影响"), ServiceB->GetLoadedDefinition(LeaseB));
            Test->TestEqual(TEXT("外部卸载不能抢走B资源"), Manager->UnloadPrimaryAsset(Id), 0);
            // 真实外部引擎入口，发生在数据服务开始持有之后；最后释放不能丢失其新分组。
            ExternalLoad = Manager->LoadPrimaryAsset(Id, {TEXT("ExternalAfterLease")});
            ServiceB->ReleaseDefinition(CacheLease); ServiceB->ReleaseDefinition(LeaseB);
            GamePlatform::Data::NextTick([this, Alive = TWeakPtr<int32>(CallbackAlive)]()
            { if (Alive.IsValid()) bMayInspectExternal = true; });
            Phase = 4;
            return false;
        }
        if (!bMayInspectExternal) return false;
        if (const auto Pending = Manager->GetPrimaryAssetHandle(Id); Pending && !Pending->HasLoadCompleted()) return false;
        TArray<FName> ExternalBundles;
        Manager->GetPrimaryAssetHandle(Id, true, &ExternalBundles);
        Test->TestTrue(TEXT("释放最后租约仍保留后来外部分组"), ExternalBundles.Contains(TEXT("ExternalAfterLease")));
        Test->TestTrue(TEXT("内部收缩不能取消后来外部请求"), !ExternalLoad || !ExternalLoad->WasCanceled());
        Test->TestNotNull(TEXT("后来外部加载根对象仍存在"), Manager->GetPrimaryAssetObject(Id));
        Test->TestEqual(TEXT("A仅一次请求终态"), CountA, 1);
        Test->TestEqual(TEXT("B仅一次请求终态"), CountB, 1);
        Test->TestEqual(TEXT("缓存仅一次请求终态"), CountCache, 1);
        Test->TestEqual(TEXT("取消仅一次请求终态"), CountCancelled, 1);
        Test->TestEqual(TEXT("开始加载后取消仅一次终态"), CountStartedCancel, 1);
        Test->TestEqual(TEXT("成功世界清理不重复终态"), CountReadyWorld, 1);
        Test->TestNull(TEXT("释放后不能继续读取"), ServiceB->GetLoadedDefinition(LeaseB));
        Test->TestEqual(TEXT("B无遗留有效租约"), ServiceB->GetDiagnostics().ActiveLeases, 0);
        Cleanup();
        return true;
    }
private:
    void Cleanup()
    {
        if (World.IsValid()) { World->DestroyWorld(false); World.Reset(); }
        if (bIsInitialized)
        {
            if (!bIsAClosed) ShutdownTestInstance(InstanceA.Get());
            ShutdownTestInstance(InstanceB.Get()); bIsInitialized = false;
            ServiceA = nullptr; ServiceB = nullptr;
        }
        if (ExternalLoad.IsValid() || bMayInspectExternal)
        {
            if (auto* Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized()))
            {
                // 测试参数须指定独占开发夹具；显式撤销本测试的外部使用，保留测试前的外部分组。
                if (bWasInitiallyLoaded) Manager->LoadPrimaryAsset(Id, BaselineBundles);
                else Manager->UnloadPrimaryAsset(Id);
            }
            ExternalLoad.Reset(); bMayInspectExternal = false;
        }
    }
    FAutomationTestBase* Test;
    // 潜伏命令超时销毁后，下一轮取消通知不得访问已析构的测试命令。
    TSharedPtr<int32> CallbackAlive = MakeShared<int32>(0);
    FPrimaryAssetId Id;
    double StartedSeconds;
    int32 Phase = 0, CountA = 0, CountB = 0, CountCancelled = 0, CountCache = 0, CountStartedCancel = 0, CountReadyWorld = 0;
    bool bIsInitialized = false, bIsAClosed = false;
    bool bHasCancelledStartedRequest = false, bMayInspectExternal = false, bWasInitiallyLoaded = false;
    FGamePlatformResult ResultA, ResultB;
    FGamePlatformDataLease LeaseA, LeaseB, CacheLease, StartedCancelLease, ReadyWorldLease;
    TSharedPtr<FStreamableHandle> ExternalLoad;
    TArray<FName> BaselineBundles;
    TStrongObjectPtr<UGameInstance> InstanceA, InstanceB;
    IGamePlatformDataService* ServiceA = nullptr;
    IGamePlatformDataService* ServiceB = nullptr;
    TStrongObjectPtr<UWorld> World;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDataLeaseIntegrationTest, "GamePlatform.Data.Runtime.RealAssetLeases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDataLeaseIntegrationTest::RunTest(const FString& Parameters)
{
    FString DefinitionText;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GamePlatformDataTestDefinition="), DefinitionText))
    {
        AddError(TEXT("须显式提供-GamePlatformDataTestDefinition=GamePlatformDefinition:<合法逻辑身份>并生成真实测试资产；缺少资产不能跳过并报告通过。"));
        return false;
    }
    const FPrimaryAssetId Id(DefinitionText);
    if (!Id.IsValid()) { AddError(TEXT("测试主资产身份非法。")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FDataLeaseIntegrationCommand(this, Id));
    return true;
}
#endif
