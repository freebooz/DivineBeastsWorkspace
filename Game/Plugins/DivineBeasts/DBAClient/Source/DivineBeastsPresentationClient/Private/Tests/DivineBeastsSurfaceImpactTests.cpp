// 纯项目适配测试，不捏造技能数值、真实命中或GPU视觉资源。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Impact/DivineBeastsSurfaceImpactLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSurfaceImpactSemanticTest,
    "DivineBeasts.Presentation.SurfaceImpact.SemanticsAndSafeRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSurfaceImpactSemanticTest::RunTest(const FString&)
{
    const TPair<FName, EGamePlatformSurfaceMarkKind> Cases[] = {
        {TEXT("Impact.Projectile"), EGamePlatformSurfaceMarkKind::Bullet},
        {TEXT("Impact.Explosion"), EGamePlatformSurfaceMarkKind::Scorch},
        {TEXT("Impact.Slash"), EGamePlatformSurfaceMarkKind::Slash},
        {TEXT("Impact.Energy"), EGamePlatformSurfaceMarkKind::Fluid},
        {TEXT("Impact.Fracture"), EGamePlatformSurfaceMarkKind::Crack}
    };
    for (const auto& Case : Cases)
    {
        EGamePlatformSurfaceMarkKind Kind = EGamePlatformSurfaceMarkKind::Bullet;
        TestTrue(TEXT("五种明确项目表现语义均可映射"), UDivineBeastsSurfaceImpactLibrary::ResolveImpactVisualKind(Case.Key, Kind));
        TestEqual(TEXT("项目类别保持平台通用身份"), Kind, Case.Value);
    }
    EGamePlatformSurfaceMarkKind Unknown = EGamePlatformSurfaceMarkKind::Scorch;
    TestFalse(TEXT("未批准/未知语义不能假装有效命中"), UDivineBeastsSurfaceImpactLibrary::ResolveImpactVisualKind(TEXT("Impact.Unknown"), Unknown));

    FHitResult Empty;
    const auto Result = UDivineBeastsSurfaceImpactLibrary::SubmitConfirmedEnvironmentImpact(
        nullptr, Empty, TEXT("Impact.Projectile"), FVector::ForwardVector, FGuid::NewGuid());
    TestEqual(TEXT("无真实物理表面不能生成贴花"), Result.Status, EGamePlatformSurfaceMarkStatus::InvalidRequest);
    return true;
}
#endif
