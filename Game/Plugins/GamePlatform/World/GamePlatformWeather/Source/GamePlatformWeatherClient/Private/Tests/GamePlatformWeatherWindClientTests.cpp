// 真实World回归：同步Surface通知重入、GameState复制时钟校正和旧来源解绑。
// 无GameInstance/表现资源；GUID为明确测试夹具，只观测取消所有权，不伪造VFX播放。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "API/GamePlatformWeatherWindSource.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/WindDirectionalSourceComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherWindRefreshReentryTest,
    "GamePlatform.Weather.Client.WindRefreshRejectsReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherWindRefreshReentryTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Authority = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    auto* Client = World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();
    auto* Surface = IGamePlatformSurfaceService::Get(*World);
    if (!TestNotNull(TEXT("天气权威"), Authority) || !TestNotNull(TEXT("天气客户端"), Client) ||
        !TestNotNull(TEXT("Surface"), Surface)) { World->DestroyWorld(false); return false; }
    Authority->ActivateWeather(FGamePlatformWeatherState());
    Client->OnWorldBeginPlay(*World);
    auto Altered = Surface->GetEnvironmentState(); Altered.Wind.Timing.W = 1.;
    Surface->ApplyEnvironmentState(Altered);
    bool bReentered = false;
    const FGuid NewRequest = FGuid::NewGuid();
    const auto Handler = Surface->AddStateChangedHandler(FGamePlatformSurfaceStateChanged::FDelegate::CreateLambda(
        [&](const FGamePlatformSurfaceEnvironmentState&, int32)
        {
            if (bReentered) return;
            bReentered = true;
            FGamePlatformWeatherState Next; Next.WindIntensity = .6f;
            Authority->SetWeather(Next, 0.f);
            Client->ActiveVfxRequestId = NewRequest;
            Client->ActiveVfxTag = TEXT("WindReentryFixture");
        }));
    Client->RefreshPresentationAfterContentActivation(true);
    TestTrue(TEXT("Surface通知确实同步重入"), bReentered);
    TestEqual(TEXT("旧刷新不能取消新请求"), Client->ActiveVfxRequestId, NewRequest);
    Surface->RemoveStateChangedHandler(Handler);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherWindClockTest,
    "GamePlatform.Weather.Client.WindClockCorrectionAndOldSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherWindClockTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Authority = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    auto* Client = World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();
    auto* Surface = IGamePlatformSurfaceService::Get(*World);
    auto* A = World->SpawnActor<AGamePlatformGameStateBase>();
    auto* B = World->SpawnActor<AGamePlatformGameStateBase>();
    if (!Authority || !Client || !Surface || !A || !B) { World->DestroyWorld(false); return false; }
    World->SetGameState(A);
    auto* Wind = World->SpawnActor<AWindDirectionalSource>();
    Wind->Tags.Add(GamePlatformWeatherWindSource::Tag()); Wind->GetComponent()->SetStrength(.45f);
    FGamePlatformWeatherState Initial; Initial.WindIntensity = .4f;
    Authority->ActivateWeather(Initial); Client->OnWorldBeginPlay(*World);
    // 测试夹具模拟网络属性到达，真正调用引擎OnRep；不新增生产端时钟写入API。
    FDoubleProperty* ReplicatedTime = FindFProperty<FDoubleProperty>(AGameStateBase::StaticClass(), TEXT("ReplicatedWorldTimeSecondsDouble"));
    if (!TestNotNull(TEXT("引擎复制时钟字段"), ReplicatedTime)) { World->DestroyWorld(false); return false; }
    const auto Receive = [ReplicatedTime](AGamePlatformGameStateBase* State, double Seconds)
    {
        ReplicatedTime->SetPropertyValue_InContainer(State, Seconds);
        // 本夹具没有InitializeActorsForPlay，AActor::ProcessEvent会静默拦截。
        // 限定调用UObject分发仍执行真实引擎thunk及虚OnRep，避免改变生产权限或全局脚本开关。
        State->UObject::ProcessEvent(State->FindFunctionChecked(TEXT("OnRep_ReplicatedWorldTimeSecondsDouble")), nullptr);
    };
    int32 ClockNotifications = 0;
    const auto ClockHandler = A->AddServerTimeSynchronizedHandler(FGamePlatformServerTimeSynchronized::FDelegate::CreateLambda(
        [&ClockNotifications](double) { ++ClockNotifications; }));
    Wind->GetComponent()->SetStrength(0.f); // 模拟初始时钟等待期间的原生风暂停。
    Receive(A, 200.);
    TestTrue(TEXT("时钟通知也恢复原生风投影"), FMath::IsNearlyEqual(Wind->GetComponent()->Strength,
        Client->ActiveSnapshot.To.Decode().WindIntensity, .0001f));
    AddInfo(FString::Printf(TEXT("WindClock: server=%.6f bound=%d revision=%d offset=%.6f notifications=%d"),
        A->GetServerWorldTimeSeconds(), Client->ServerClockSource.Get() == A ? 1 : 0,
        Client->ActiveSnapshot.Revision, Surface->GetEnvironmentState().Wind.Timing.W, ClockNotifications));
    TestEqual(TEXT("真实OnRep发布时钟通知"), ClockNotifications, 1);
    TestTrue(TEXT("时钟校正事件重新投影风偏移"), FMath::IsNearlyEqual(Surface->GetEnvironmentState().Wind.Timing.W, 200., .001));
    World->SetGameState(B);
    const double BeforeOld = Surface->GetEnvironmentState().Wind.Timing.W;
    Receive(A, 400.);
    TestEqual(TEXT("旧GameState不能改写当前风"), Surface->GetEnvironmentState().Wind.Timing.W, BeforeOld);
    Receive(B, 300.);
    TestTrue(TEXT("新GameState继续校准"), FMath::IsNearlyEqual(Surface->GetEnvironmentState().Wind.Timing.W, 300., .001));
    Client->Deinitialize();
    const double BeforeClosed = Surface->GetEnvironmentState().Wind.Timing.W;
    Receive(B, 500.);
    TestEqual(TEXT("关闭后订阅已解绑"), Surface->GetEnvironmentState().Wind.Timing.W, BeforeClosed);
    A->RemoveServerTimeSynchronizedHandler(ClockHandler);
    World->DestroyWorld(false);
    return true;
}
#endif
