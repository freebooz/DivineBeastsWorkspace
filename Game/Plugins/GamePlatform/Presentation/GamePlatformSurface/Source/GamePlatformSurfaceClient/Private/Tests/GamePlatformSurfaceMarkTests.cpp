// 离线合同测试不假装真实材质或场景测试：重点验证五类、表面回退、预算和非法输入。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"
#include "Marks/GamePlatformSurfaceMarkCatalog.h"
#include "Marks/GamePlatformSurfaceMarkBlueprintLibrary.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceMarkRequestTest,
    "GamePlatform.Surface.Marks.Request.FiveKindsAndInvalidValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceMarkRequestTest::RunTest(const FString&)
{
    FGamePlatformSurfaceMarkRequest Request;
    for (uint8 Kind = 0; Kind < 5; ++Kind)
    {
        Request.Kind = static_cast<EGamePlatformSurfaceMarkKind>(Kind);
        TestTrue(TEXT("五类客户端命中事实均支持"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    }
    Request.Kind = static_cast<EGamePlatformSurfaceMarkKind>(99);
    TestFalse(TEXT("未知类别不能降级为任意材质"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    Request.Kind = EGamePlatformSurfaceMarkKind::Crack;
    Request.Normal = FVector::ZeroVector;
    TestFalse(TEXT("零法线不能做无效投影"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    Request.Normal = FVector::UpVector;
    Request.SizeScale = -1.0f;
    TestFalse(TEXT("负尺寸不能进入GPU"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    Request.SizeScale = 9.0f;
    TestFalse(TEXT("超限尺寸拒绝"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    Request.SizeScale = 1.0f;
    Request.Location.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("非有限命中位置拒绝"), FGamePlatformSurfaceMarkPolicy::IsValidRequest(Request));
    const FHitResult Miss;
    const auto MissResult = UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMarkFromHit(
        nullptr, EGamePlatformSurfaceMarkKind::Bullet, Miss, FVector::ForwardVector, FGuid::NewGuid());
    TestEqual(TEXT("未命中不得在世界原点生成Decal"), MissResult.Status, EGamePlatformSurfaceMarkStatus::InvalidRequest);
    const auto WrongWorld = UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMark(nullptr, FGamePlatformSurfaceMarkRequest());
    TestEqual(TEXT("空世界必须报告InvalidWorld"), WrongWorld.Status, EGamePlatformSurfaceMarkStatus::InvalidWorld);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceMarkCatalogTest,
    "GamePlatform.Surface.Marks.Catalog.FallbackAndBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceMarkCatalogTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UGamePlatformSurfaceMarkCatalog> Catalog(NewObject<UGamePlatformSurfaceMarkCatalog>());
    for (uint8 Kind = 0; Kind < 5; ++Kind)
    {
        FGamePlatformSurfaceMarkStyle Style;
        Style.Kind = static_cast<EGamePlatformSurfaceMarkKind>(Kind);
        Style.Material = TSoftObjectPtr<UMaterialInterface>(
            FSoftObjectPath(TEXT("/GamePlatformSurface/Marks/TestAsset.TestAsset")));
        Catalog->Styles.Add(Style);
    }
    FString Error;
    TestTrue(TEXT("五类有效软资源合同"), Catalog->IsValidCatalog(Error));
    const auto* Fallback = Catalog->FindStyle(EGamePlatformSurfaceMarkKind::Slash, SurfaceType1);
    TestTrue(TEXT("非默认表面回退同类别默认"), Fallback && Fallback->Kind == EGamePlatformSurfaceMarkKind::Slash);
    FGamePlatformSurfaceMarkStyle Stone = Catalog->Styles[2];
    Stone.SurfaceType = SurfaceType1;
    Stone.SizeCm = FVector(9, 18, 40);
    Catalog->Styles.Add(Stone);
    const auto* Override = Catalog->FindStyle(EGamePlatformSurfaceMarkKind::Slash, SurfaceType1);
    TestTrue(TEXT("精确物理表面优先于默认"), Override && Override->SizeCm.Z == 40.0);
    Catalog->Styles.Add(Stone);
    TestFalse(TEXT("重复类别表面拒绝"), Catalog->IsValidCatalog(Error));
    Catalog->Styles.Pop();
    Catalog->MaxActiveMarks = 513;
    TestFalse(TEXT("超过硬预算拒绝"), Catalog->IsValidCatalog(Error));
    Catalog->MaxActiveMarks = 128;
    Catalog->Styles[0].Material.Reset();
    TestFalse(TEXT("无软材质不能充当可渲染目录"), Catalog->IsValidCatalog(Error));
    return true;
}
#endif
