// 真实引擎灯光/高度雾组件回归；验证渐变、可逆性及独立World，不以截图代替运行状态。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Atmosphere/GamePlatformWeatherAtmosphere.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWeatherAtmosphereTest,
    "GamePlatform.Weather.Client.AtmosphereRestoration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWeatherAtmosphereTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Sun = Cast<UDirectionalLightComponent>(World->SpawnActor<ADirectionalLight>()->GetLightComponent());
    auto* Sky = World->SpawnActor<ASkyLight>()->GetLightComponent();
    auto* Fog = World->SpawnActor<AExponentialHeightFog>()->GetComponent();
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetIntensity(100.f);
    Sky->SetMobility(EComponentMobility::Movable); Sky->SetIntensity(2.f);
    Fog->SetFogDensity(.003f); Fog->SetStartDistance(1500.f);
    const FLinearColor OriginalColor = FLinearColor(Sun->LightColor);
    FGamePlatformWeatherAtmosphereProfile Profile; Profile.bEnabled = true;
    {
        FGamePlatformWeatherAtmosphere Adapter(*World, Profile);
        Adapter.Apply(.35f, .35f);
        TestTrue(TEXT("小雪降低照度"), Sun->Intensity < 100.f);
        const float LightSnowSun = Sun->Intensity;
        Adapter.Apply(.9f, .9f);
        TestTrue(TEXT("大雪比小雪照度更低"), Sun->Intensity < LightSnowSun);
        TestTrue(TEXT("天光保持可读但不抵消阴天降照度"), Sky->Intensity > 0.f && Sky->Intensity < 2.f);
        TestTrue(TEXT("降雪增加雾"), Fog->FogDensity > .003f);
        const float HeavyDensity = Fog->FogDensity;
        Adapter.Apply(.9f, .9f);
        TestEqual(TEXT("重复快照不叠乘"), Fog->FogDensity, HeavyDensity);
        Adapter.Apply(0.f, 0.f);
        TestEqual(TEXT("晴天恢复原始太阳"), Sun->Intensity, 100.f);
        TestEqual(TEXT("晴天恢复原始雾"), Fog->FogDensity, .003f);
        TestEqual(TEXT("晴天恢复雾起始距离"), Fog->StartDistance, 1500.f);
        TestTrue(TEXT("晴天恢复原始颜色"), FLinearColor(Sun->LightColor).Equals(OriginalColor));
        Adapter.Apply(1.f, 1.f);
    }
    TestEqual(TEXT("失活恢复灯光基线"), Sun->Intensity, 100.f);
    TestEqual(TEXT("失活恢复雾基线"), Fog->FogDensity, .003f);
    FGamePlatformWeatherState Cloudy; Cloudy.Type = EGamePlatformWeatherType::Cloudy;
    TestTrue(TEXT("没有雨雪的阴天同样降低照度"), FGamePlatformWeatherAtmosphere::CloudWeight(Cloudy) > .5f);
    World->DestroyWorld(false);
    UWorld* Empty = UWorld::CreateWorld(EWorldType::Game, false);
    {
        FGamePlatformWeatherAtmosphere Adapter(*Empty, Profile);
        Adapter.Apply(1.f, 1.f);
        int32 Count = 0; for (TActorIterator<AExponentialHeightFog> It(Empty); It; ++It) ++Count;
        TestEqual(TEXT("无雾场景创建一个本地临时雾"), Count, 1);
        for (TActorIterator<AExponentialHeightFog> It(Empty); It; ++It)
            TestEqual(TEXT("临时雾保留近场清晰距离"), It->GetComponent()->StartDistance, Profile.FogStartDistanceCm);
        auto* Level = NewObject<ULevel>(Empty); Level->OwningWorld = Empty;
        Empty->AddLevel(Level);
        FActorSpawnParameters Params; Params.OverrideLevel = Level;
        auto* StreamedFog = Empty->SpawnActor<AExponentialHeightFog>(Params);
        StreamedFog->GetComponent()->SetFogDensity(.006f);
        FWorldDelegates::LevelAddedToWorld.Broadcast(Level, Empty);
        TestTrue(TEXT("流送雾接管当前大雪"), StreamedFog->GetComponent()->FogDensity > .006f);
        FWorldDelegates::LevelRemovedFromWorld.Broadcast(Level, Empty);
        TestEqual(TEXT("卸载层恢复原始雾基线"), StreamedFog->GetComponent()->FogDensity, .006f);
        int32 LocalFogs = 0;
        for (TActorIterator<AExponentialHeightFog> It(Empty); It; ++It)
            if (It->GetLevel() == Empty->PersistentLevel && It->GetComponent()->FogDensity > 0.f) ++LocalFogs;
        TestEqual(TEXT("卸载真实雾后重新建立降雪临时雾"), LocalFogs, 1);
        StreamedFog->Destroy(); Empty->RemoveLevel(Level);
    }
    int32 Count = 0; for (TActorIterator<AExponentialHeightFog> It(Empty); It; ++It) ++Count;
    TestEqual(TEXT("失活销毁自有临时雾"), Count, 0);
    Empty->DestroyWorld(false);
    return true;
}
#endif
