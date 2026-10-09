// F21：显式刷新重新解析MPC配置；Transient集合测试不生成/保存资产，也不验证实际渲染。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Types/GamePlatformSurfaceParameterNames.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceBindingRefreshTest, "GamePlatform.Surface.Binding.RefreshReadsConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceBindingRefreshTest::RunTest(const FString&)
{
    auto* Settings=GetMutableDefault<UGamePlatformSurfaceSettings>();
    TGuardValue<TSoftObjectPtr<UMaterialParameterCollection>> RestoreCollection(Settings->GlobalParameterCollection,Settings->GlobalParameterCollection);
    TGuardValue<bool> RestoreWarn(Settings->bWarnOnMaterialBindingFailure,false);
    TStrongObjectPtr<UMaterialParameterCollection> First(NewObject<UMaterialParameterCollection>());
    TStrongObjectPtr<UMaterialParameterCollection> Second(NewObject<UMaterialParameterCollection>());
    const FName Names[] = { GamePlatformSurfaceParameters::GlobalWetness,GamePlatformSurfaceParameters::GlobalSnowAmount,
        GamePlatformSurfaceParameters::GlobalSnowHeightCm,GamePlatformSurfaceParameters::GlobalMossInfluence,
        GamePlatformSurfaceParameters::GlobalPuddleAmount,GamePlatformSurfaceParameters::RainIntensity,
        GamePlatformSurfaceParameters::SnowIntensity,GamePlatformSurfaceParameters::TemperatureCelsius };
    for (const FName Name:Names)
    {
        FCollectionScalarParameter Parameter; Parameter.ParameterName=Name;
        First->ScalarParameters.Add(Parameter); Second->ScalarParameters.Add(Parameter);
    }
    Settings->GlobalParameterCollection=First.Get();
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,false,ERHIFeatureLevel::Num,&Values));
    auto* Service=NewObject<UGamePlatformSurfaceWorldSubsystem>(World.Get());
    Service->RefreshMaterialBinding(); TestEqual(TEXT("首先解析集合A"),Service->BoundCollection.Get(),First.Get());
    Settings->GlobalParameterCollection=Second.Get(); Service->RefreshMaterialBinding();
    TestEqual(TEXT("刷新后解析集合B"),Service->BoundCollection.Get(),Second.Get());
    Settings->GlobalParameterCollection.Reset(); Service->RefreshMaterialBinding();
    TestNull(TEXT("空配置不能保留旧绑定"),Service->BoundCollection.Get());
    World->DestroyWorld(false); return true;
}
#endif
