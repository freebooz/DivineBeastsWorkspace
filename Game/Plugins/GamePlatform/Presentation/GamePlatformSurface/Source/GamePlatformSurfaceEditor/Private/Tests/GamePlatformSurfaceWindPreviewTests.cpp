#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "API/GamePlatformSurfaceWindPreview.h"
#include "Validation/GamePlatformSurfaceAssetContract.h"
#include "Types/GamePlatformSurfaceParameterNames.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceWindPreviewTest,
    "GamePlatform.Surface.Editor.WindPreviewIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceWindPreviewTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> Editor(UWorld::CreateWorld(EWorldType::Editor, false));
    TStrongObjectPtr<UWorld> Game(UWorld::CreateWorld(EWorldType::Game, false));
    FGamePlatformSurfaceWindTransition Wind; Wind.From = Wind.To = FVector4(90, .5, .2, .15);
    FString Error;
    TestFalse(TEXT("不能覆盖运行世界MPC"), GamePlatformSurfaceWindPreview::Apply(*Game, Wind, Error));
    auto* Collection = Cast<UMaterialParameterCollection>(FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionObjectPath().TryLoad());
    if (!TestNotNull(TEXT("真实核心MPC"), Collection)) { Game->DestroyWorld(false); Editor->DestroyWorld(false); return false; }
    const auto Defaults = Collection->VectorParameters;
    auto* Instance = Editor->GetParameterCollectionInstance(Collection);
    Instance->SetScalarParameterValue(GamePlatformSurfaceParameters::GlobalWetness, .63f);
    TestTrue(TEXT("编辑器应用"), GamePlatformSurfaceWindPreview::Apply(*Editor, Wind, Error));
    FGamePlatformSurfaceWindTransition Read;
    TestTrue(TEXT("编辑器回读"), GamePlatformSurfaceWindPreview::Read(*Editor, Read, Error));
    TestTrue(TEXT("端点正确"), Read.To.Equals(Wind.To, .0001));
    float Wetness = 0.f; Instance->GetScalarParameterValue(GamePlatformSurfaceParameters::GlobalWetness, Wetness);
    TestEqual(TEXT("预览保留原湿润"), Wetness, .63f);
    TestEqual(TEXT("不改变MPC资产参数数量"), Collection->VectorParameters.Num(), Defaults.Num());
    for (int32 Index = 0; Index < Defaults.Num(); ++Index)
        TestTrue(TEXT("不改变MPC默认值"), Collection->VectorParameters[Index].DefaultValue.Equals(Defaults[Index].DefaultValue));
    Game->DestroyWorld(false); Editor->DestroyWorld(false);
    return true;
}
#endif
