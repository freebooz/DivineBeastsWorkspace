// 项目层客户端真实MID回归：使用真实共享母材质及六张UE贴图，测试只创建自有World/组件。
// 租约为绑定单元夹具，不冒充平台签发；真实Data异步、64组件与GPU另由PIE/发布包检查。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"
#include "Settings/DivineBeastsVillageSnowSettings.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Types/GamePlatformResult.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageGroundMIDTest,
    "DivineBeasts.Presentation.VillageGround.RealMIDAndCancelledBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageGroundMIDTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    if (!TestNotNull(TEXT("自有世界"), World.Get())) return false;
    auto* Landscape = World->SpawnActor<ALandscape>();
    auto* Component = NewObject<ULandscapeComponent>(Landscape);
    Landscape->LandscapeComponents.Add(Component);
    Landscape->bUseDynamicMaterialInstance = true;
    Landscape->LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge.M_DBA_Village_LandscapeBridge"));
    if (!TestNotNull(TEXT("必须有真实共享母材质"), Landscape->LandscapeMaterial.Get()))
    { World->DestroyWorld(false); return false; }
    auto* Mid = UMaterialInstanceDynamic::Create(Landscape->LandscapeMaterial, Component);
    Component->MaterialInstancesDynamic.Add(Mid);
    const TStrongObjectPtr<UDivineBeastsVillageSnowWorldSubsystem> Service(
        NewObject<UDivineBeastsVillageSnowWorldSubsystem>(World.Get()));
    Service->bVillageWorld = true;
    Service->bTexturesReady = true;
    const auto* Config = GetDefault<UDivineBeastsVillageSnowSettings>();
    // 故意反转角色路径；任何按数组下标绑定的回归都会产生真实MID错纹理。
    const TArray<FSoftObjectPath> Paths = {Config->GroundORMTexture, Config->SnowNormalTexture,
        Config->GroundAlbedoTexture, Config->SnowAlbedoTexture, Config->GroundNormalTexture, Config->SnowCoverageTexture};
    TArray<TStrongObjectPtr<UTexture2D>> Textures;
    for (const auto& Path : Paths)
    {
        Textures.Emplace(Cast<UTexture2D>(Path.TryLoad())); // 测试夹具允许同步加载，生产路径禁止。
        TestNotNull(TEXT("六张真实Texture2D都必须存在"), Textures.Last().Get());
    }
    Service->TextureLease.ResourcePaths = Paths;
    Service->CurrentSnowAmount = .23f;
    Service->CurrentWetness = .35f;
    TestTrue(TEXT("完整六纹理绑定成功"), Service->ApplyToLandscape(Landscape));
    TestTrue(TEXT("草地颜色按语义路径而非下标"),
        Mid->K2_GetTextureParameterValue(TEXT("VillageGroundAlbedoTexture")) == Config->GroundAlbedoTexture.ResolveObject());
    TestTrue(TEXT("草地法线不是雪法线"),
        Mid->K2_GetTextureParameterValue(TEXT("VillageGroundNormalTexture")) == Config->GroundNormalTexture.ResolveObject());
    TestTrue(TEXT("草地ORM线性资源正确"),
        Mid->K2_GetTextureParameterValue(TEXT("VillageGroundORMTexture")) == Config->GroundORMTexture.ResolveObject());
    TestTrue(TEXT("扫描草地不叠加旧泥土乘色"), Mid->K2_GetVectorParameterValue(TEXT("VillageDryColor")).Equals(FLinearColor::White));
    TestTrue(TEXT("雪覆盖仍保留"), FMath::IsNearlyEqual(Mid->K2_GetScalarParameterValue(TEXT("VillageSnowAmount")), .23f));
    // 失败和关闭不会把已加载的对象当新租约资源；旧回调不得复活本世界绑定。
    Service->TexturesAppliedLandscapes.Reset();
    Service->TextureLease.ResourcePaths.Remove(Config->GroundNormalTexture);
    TestFalse(TEXT("租约缺一张纹理拒绝"), Service->ApplyToLandscape(Landscape));
    Service->bClosing = true;
    Service->HandleTexturesReady(0, Service->TextureLease, FGamePlatformResult::Success());
    TestFalse(TEXT("关闭时不能绑定"), Service->ApplyToLandscape(Landscape));
    Service->ReleaseResources();
    TestFalse(TEXT("清理关闭纹理就绪状态"), Service->bTexturesReady);
    TestEqual(TEXT("清理撤销所有绑定标识"), Service->TexturesAppliedLandscapes.Num(), 0);
    World->DestroyWorld(false);
    return true;
}
#endif
