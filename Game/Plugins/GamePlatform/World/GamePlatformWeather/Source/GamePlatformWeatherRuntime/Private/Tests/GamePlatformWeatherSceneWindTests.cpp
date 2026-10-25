// 地图全局风源仅覆盖首次预设的风字段；后续天气仍由唯一服务器快照裁决。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "API/GamePlatformWeatherWindSource.h"
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherSceneWindTest,
    "GamePlatform.Weather.Runtime.SceneWindSourceInitialOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherSceneWindTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    FGamePlatformWeatherState State; State.Type = EGamePlatformWeatherType::LightRain; State.RainIntensity = .7f;
    AWindDirectionalSource* Found = nullptr; FString Error;
    TestTrue(TEXT("无源不改原预设"), GamePlatformWeatherWindSource::Read(*World, State, Found, Error) == EGamePlatformSceneWindResult::Absent);
    auto* Wind = World->SpawnActor<AWindDirectionalSource>();
    Wind->GetComponent()->SetMobility(EComponentMobility::Movable);
    Wind->Tags.Add(GamePlatformWeatherWindSource::Tag());
    Wind->SetActorRotation(FRotator(0, 90, 0));
    Wind->GetComponent()->SetStrength(.45f); Wind->GetComponent()->SetSpeed(.15f);
    Wind->GetComponent()->SetMaximumGustAmount(.25f);
    TestTrue(TEXT("读取唯一地图风源"), GamePlatformWeatherWindSource::Read(*World, State, Found, Error) == EGamePlatformSceneWindResult::Applied);
    TestEqual(TEXT("方向为世界Yaw"), State.WindYawDegrees, 90.f);
    TestEqual(TEXT("强度"), State.WindIntensity, .45f);
    TestEqual(TEXT("只合并风不改降雨"), State.RainIntensity, .7f);
    Wind->SetActorRotation(FRotator(0, -.000001, 0));
    TestTrue(TEXT("整圈附近的float舍入合法"), GamePlatformWeatherWindSource::Read(*World, State, Found, Error) == EGamePlatformSceneWindResult::Applied);
    Wind->SetActorRotation(FRotator(0, 90, 0));
    auto* Preset = NewObject<UGamePlatformWeatherPresetDefinition>();
    FGamePlatformId::TryParse(TEXT("gp.weather.scene_wind_test@1"), Preset->LogicalId);
    Preset->Weather.State = State; Preset->Weather.State.WindIntensity = .8f;
    Preset->Weather.TransitionSeconds = 0.f;
    auto* Authority = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    TestTrue(TEXT("真实GameMode先激活"), Authority->ActivateWeather(FGamePlatformWeatherState()));
    TestTrue(TEXT("首发布前捕获地图风"), FMath::IsNearlyEqual(Authority->GetCurrentSnapshot().To.Decode().WindIntensity, .45f, 1.f/255));
    TestEqual(TEXT("首发布保留地图方向"), Authority->GetCurrentSnapshot().To.Decode().WindYawDegrees, 90.f);
    Wind->GetComponent()->SetStrength(.1f); // 模拟PIE客户端在异步预设完成前投影参数。
    TestTrue(TEXT("首次预设成功"), Authority->ApplyPreset(Preset));
    TestTrue(TEXT("首次风使用地图配置"), FMath::IsNearlyEqual(Authority->GetCurrentSnapshot().To.Decode().WindIntensity, .45f, 1.f/255));
    TestTrue(TEXT("后续预设成功"), Authority->ApplyPreset(Preset));
    TestTrue(TEXT("后续天气不被地图旧值覆盖"), FMath::IsNearlyEqual(Authority->GetCurrentSnapshot().To.Decode().WindIntensity, .8f, 1.f/255));
    auto* Duplicate = World->SpawnActor<AWindDirectionalSource>();
    Duplicate->Tags.Add(GamePlatformWeatherWindSource::Tag());
    TestTrue(TEXT("重复全局源明确拒绝"), GamePlatformWeatherWindSource::Read(*World, State, Found, Error) == EGamePlatformSceneWindResult::Invalid);
    TestNull(TEXT("重复不任意挑选"), Found);
    World->DestroyWorld(false);
    return true;
}
#endif
