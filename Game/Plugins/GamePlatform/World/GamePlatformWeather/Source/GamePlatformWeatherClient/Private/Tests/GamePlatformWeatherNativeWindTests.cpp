#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "API/GamePlatformWeatherWindSource.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherNativeWindTest,
    "GamePlatform.Weather.Client.NativeWindProjectionAndCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherNativeWindTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Wind = World->SpawnActor<AWindDirectionalSource>();
    Wind->Tags.Add(GamePlatformWeatherWindSource::Tag());
    Wind->GetComponent()->SetStrength(.45f); Wind->GetComponent()->SetSpeed(.15f);
    auto* Client = World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();
    auto* Authority = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    Client->OnWorldBeginPlay(*World);
    TestEqual(TEXT("首快照前不抹掉作者配置"), Wind->GetComponent()->Strength, .45f);
    FGamePlatformWeatherState Target; Target.WindYawDegrees = 180.f; Target.WindIntensity = .7f;
    Target.WindGustStrength = .3f; Target.WindGustFrequencyHz = .2f;
    TestTrue(TEXT("服务器更新天气"), Authority->SetWeather(Target, 0.f));
    TestTrue(TEXT("原生强度消费同一量化状态"), FMath::IsNearlyEqual(Wind->GetComponent()->Strength, .7f, 1.f/255));
    TestTrue(TEXT("方向消费同一世界Yaw"), FMath::IsNearlyEqual(FRotator::ClampAxis(Wind->GetActorRotation().Yaw), 180., .01));
    Client->Deinitialize();
    Target.WindIntensity = .2f; Authority->SetWeather(Target, 0.f);
    TestEqual(TEXT("退出后不再恢复原生风"), Wind->GetComponent()->Strength, 0.f);
    World->DestroyWorld(false);
    return true;
}
#endif
