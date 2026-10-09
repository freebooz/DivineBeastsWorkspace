// 项目客户端真实资源回归：完整UE5骨树接受，旧UE4骨架/空资源拒绝；不修改资产或权威角色。
// 由锁定Editor加载已保存公共资产执行，未取得资产时失败，不能用固定成功替代真实蒙皮验证。
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterSkeletonTest,
    "DivineBeasts.Presentation.Characters.SkeletonCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCharacterSkeletonTest::RunTest(const FString&)
{
    USkeleton* Correct = LoadObject<USkeleton>(nullptr, TEXT("/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin.SK_Mannequin"));
    USkeleton* Legacy = LoadObject<USkeleton>(nullptr, TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SK_Mannequin_Skeleton.SK_Mannequin_Skeleton"));
    if (!TestNotNull(TEXT("真实UE5骨架存在"), Correct) || !TestNotNull(TEXT("旧UE4骨架负例存在"), Legacy))
    {
        return false;
    }
    for (const TCHAR* Name : {TEXT("Manny"), TEXT("Quinn")})
    {
        const FString Path = FString::Printf(TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_%s_Simple.SKM_%s_Simple"), Name, Name);
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
        if (!TestNotNull(TEXT("真实蒙皮网格存在"), Mesh)) return false;
        FString Error;
        TestTrue(TEXT("完整骨树接受简化网格"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, Correct, Error));
        TestTrue(TEXT("成功清除旧错误"), Error.IsEmpty());
        TestFalse(TEXT("旧68骨树不可驱动UE5蒙皮"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, Legacy, Error));
        TestTrue(TEXT("失败给出骨骼原因"), Error.Contains(TEXT("骨骼")));
        TestFalse(TEXT("缺失骨架拒绝"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, nullptr, Error));
        TestTrue(TEXT("资产绑定也采用正确骨架"), Mesh->GetSkeleton() == Correct);
    }
    FString Error;
    TestFalse(TEXT("空网格拒绝"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(nullptr, Correct, Error));
    return true;
}
#endif
