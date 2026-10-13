// SSLM三层样板集成回归：UI读取真实Monolith WBP，Data经正式实例服务加载Engine Cube。
// 仅隔离临时游戏实例，无玩家存档/HTTP/支付；缺真实资产明确失败，不跳过或伪造替身为通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Components/GamePlatformResourceBarWidget.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
namespace
{
/** 正式公开服务的两个独立需求；完成弱门保障清理后不再访问测试对象。 */
class FDemoSamplesCommand final : public IAutomationLatentCommand
{
public:
    explicit FDemoSamplesCommand(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
    ~FDemoSamplesCommand() override
    {
        Alive.Reset();
        for (auto* Instance : {A.Get(), B.Get()})
        {
            if (!Instance) continue;
            if (auto* Data = IGamePlatformDataService::Get(*Instance))
                for (const auto& Lease : {LeaseA, LeaseB, Cancelled})
                    if (Lease.IsValid() && Data->GetDiagnostics().ScopeId == Lease.ScopeId) Data->ReleaseResources(Lease);
            auto* World = Instance->GetWorld(); if (World) World->DestroyWorld(false);
            Instance->Shutdown(); if (World) GEngine->DestroyWorldContext(World);
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 30.0) { Test->AddError(TEXT("真实Data样板测试超过用例30秒期限，未确认完成。")); return true; }
        if (!bStarted)
        {
            bStarted = true;
            A.Reset(NewObject<UGameInstance>(GEngine)); B.Reset(NewObject<UGameInstance>(GEngine));
            A->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); B->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            auto* SA = IGamePlatformDataService::Get(*A); auto* SB = IGamePlatformDataService::Get(*B);
            if (!SA || !SB) { Test->AddError(TEXT("真实实例中央Data服务未建立。")); return true; }
            auto* Class = LoadClass<UGamePlatformResourceBarWidget>(nullptr, TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoResourceBar.WBP_DBA_PluginDemoResourceBar_C"));
            if (!Class) { Test->AddError(TEXT("缺真实Monolith资源条WBP，不能用C++临时视觉树代替。")); return true; }
            TStrongObjectPtr<UGamePlatformResourceBarWidget> Widget(CreateWidget<UGamePlatformResourceBarWidget>(A.Get(), Class));
            if (!Widget.IsValid()) { Test->AddError(TEXT("实际资源条Widget实例创建失败。")); return true; }
            Widget->TakeWidget();
            auto* Progress = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("ProgressBar")));
            auto* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("ResourceValueText")));
            if (!Progress || !Text) { Test->AddError(TEXT("真实资源条命名控件缺失。")); return true; }
            FGamePlatformUIResourceBarState State; State.CurrentValue = 50; State.MaximumValue = 100;
            Widget->ApplyResourceState(State);
            Test->TestEqual(TEXT("实际进度条50/100为0.5"), Progress->GetPercent(), 0.5f);
            State.MaximumValue = 0; Widget->ApplyResourceState(State);
            Test->TestEqual(TEXT("最大值0不除零"), Progress->GetPercent(), 0.0f);
            Test->TestTrue(TEXT("最大值0真实文本清空"), Text->GetText().IsEmpty());
            const auto Guard = TWeakPtr<int32>(Alive); FGamePlatformResult Accepted;
            const TArray<FSoftObjectPath> Cube = {FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))};
            LeaseA = SA->AcquireResources(Cube, EGamePlatformDataLifetime::Instance, A.Get(), [this, Guard](const auto&, const auto& Result) { if (Guard.IsValid()) { ++CountA; ResultA = Result; } }, Accepted);
            Test->TestTrue(TEXT("A真实受理且不立即回调"), Accepted.IsSuccess() && CountA == 0);
            LeaseB = SB->AcquireResources(Cube, EGamePlatformDataLifetime::Instance, B.Get(), [this, Guard](const auto&, const auto& Result) { if (Guard.IsValid()) { ++CountB; ResultB = Result; } }, Accepted);
            Test->TestTrue(TEXT("B真实受理"), Accepted.IsSuccess());
            Cancelled = SA->AcquireResources(Cube, EGamePlatformDataLifetime::Instance, A.Get(), [this, Guard](const auto&, const auto& Result) { if (Guard.IsValid()) { ++CountCancelled; ResultCancelled = Result; } }, Accepted);
            Test->TestTrue(TEXT("取消前受理"), Accepted.IsSuccess()); SA->ReleaseResources(Cancelled);
            return false;
        }
        if (!CountA || !CountB || !CountCancelled) return false;
        auto* SA = IGamePlatformDataService::Get(*A); auto* SB = IGamePlatformDataService::Get(*B);
        Test->TestTrue(TEXT("实际共享资源加载成功"), ResultA.IsSuccess() && ResultB.IsSuccess());
        Test->TestTrue(TEXT("A释放受理"), SA->ReleaseResources(LeaseA).IsSuccess());
        Test->TestEqual(TEXT("B仍拥有Ready资源"), SB->GetLeaseState(LeaseB), EGamePlatformDataRequestState::Succeeded);
        Test->TestNotNull(TEXT("真实Cube仍驻留"), FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")).ResolveObject());
        Test->TestEqual(TEXT("真实取消结果未改为成功"), ResultCancelled.Status, EGamePlatformResultStatus::Cancelled);
        Test->TestEqual(TEXT("A一次终态"), CountA, 1); Test->TestEqual(TEXT("B一次终态"), CountB, 1); Test->TestEqual(TEXT("取消一次终态"), CountCancelled, 1);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started;
    bool bStarted = false;
    TStrongObjectPtr<UGameInstance> A, B;
    TSharedPtr<int32> Alive = MakeShared<int32>(0);
    FGamePlatformDataLease LeaseA, LeaseB, Cancelled;
    FGamePlatformResult ResultA, ResultB, ResultCancelled;
    int32 CountA = 0, CountB = 0, CountCancelled = 0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoSamplesTest, "DivineBeasts.PluginDemos.Samples",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoSamplesTest::RunTest(const FString&)
{
    if (!GEngine) { AddError(TEXT("需要真实正式Editor宿主。")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FDemoSamplesCommand(this));
    return true;
}
#endif
