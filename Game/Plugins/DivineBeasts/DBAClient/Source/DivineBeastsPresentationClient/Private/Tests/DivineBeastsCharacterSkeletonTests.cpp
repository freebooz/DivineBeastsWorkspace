// 项目客户端真实资源回归：完整UE5骨树接受，旧UE4骨架/空资源拒绝；不修改资产或权威角色。
// 由锁定Editor加载已保存公共资产执行，未取得资产时失败，不能用固定成功替代真实蒙皮验证。
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h" // 显式提供UPackage继承完整定义，瞬态Outer才能按真实类型转换为UObject；不依赖Unity间接包含。
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

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
    // 原生GeneratedClass真实实现IAnimClassInterface；模板目标为空不等于Mesh骨架缺失。
    // 仅构造Transient元数据并设置真实父类，不编译/保存蓝图、不改ClassFlags、不创建动画实例。
    const TStrongObjectPtr<USkeleton> KeepCorrect(Correct);
    const TStrongObjectPtr<USkeleton> KeepLegacy(Legacy);
    const TStrongObjectPtr<UAnimBlueprintGeneratedClass> TemplateClass(
        NewObject<UAnimBlueprintGeneratedClass>(GetTransientPackage(), NAME_None, RF_Transient));
    TemplateClass->SetSuperStruct(UAnimInstance::StaticClass());
    const IAnimClassInterface* TemplateInterface = IAnimClassInterface::GetFromClass(TemplateClass.Get());
    if (!TestNotNull(TEXT("原生模板类具有真实动画接口"), TemplateInterface)) return false;
    TestTrue(TEXT("原生模板类目标骨架确实为空"), TemplateInterface->GetTargetSkeleton() == nullptr);
    for (const TCHAR* Name : {TEXT("Manny"), TEXT("Quinn")})
    {
        const FString Path = FString::Printf(TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_%s_Simple.SKM_%s_Simple"), Name, Name);
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
        if (!TestNotNull(TEXT("真实蒙皮网格存在"), Mesh)) return false;
        const TStrongObjectPtr<USkeletalMesh> KeepMesh(Mesh);
        FString Error;
        TestTrue(TEXT("完整骨树接受简化网格"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, Correct, Error));
        TestTrue(TEXT("成功清除旧错误"), Error.IsEmpty());
        TestFalse(TEXT("旧68骨树不可驱动UE5蒙皮"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, Legacy, Error));
        TestTrue(TEXT("失败给出骨骼原因"), Error.Contains(TEXT("骨骼")));
        TestFalse(TEXT("缺失骨架拒绝"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, nullptr, Error));
        TestTrue(TEXT("资产绑定也采用正确骨架"), Mesh->GetSkeleton() == Correct);
        // 直接调用两个生产装配入口共享的校验，不在测试复制骨树算法或伪造受理结果。
        TemplateClass->TargetSkeleton = nullptr;
        TestTrue(TEXT("合法目标为空的模板动画使用网格实际骨架"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, TemplateClass.Get(), Error));
        TestTrue(TEXT("模板回退成功清除旧错误"), Error.IsEmpty());
        TemplateClass->TargetSkeleton = Correct;
        TestTrue(TEXT("显式正确目标骨架仍通过完整骨树门禁"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, TemplateClass.Get(), Error));
        TemplateClass->TargetSkeleton = Legacy;
        TestFalse(TEXT("模板兼容修复不能放行显式旧骨架"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, TemplateClass.Get(), Error));
        TestTrue(TEXT("动画目标不兼容仍给出骨骼原因"), Error.Contains(TEXT("骨骼")));
        TemplateClass->TargetSkeleton = nullptr;
        TestTrue(TEXT("原生动画类遵守已经验证的网格骨树"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, UAnimInstance::StaticClass(), Error));
        TestTrue(TEXT("可选动画类为空仍必须先通过网格门禁"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, nullptr, Error));
        TestFalse(TEXT("已加载但不是动画实例的类拒绝"),
            UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(Mesh, UObject::StaticClass(), Error));
    }
    FString Error;
    TestFalse(TEXT("空网格拒绝"), UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(nullptr, Correct, Error));
    TestFalse(TEXT("模板回退不能绕过空网格"),
        UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(nullptr, TemplateClass.Get(), Error));
    const TStrongObjectPtr<USkeletalMesh> EmptyMesh(
        NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient));
    TestFalse(TEXT("模板回退不能绕过网格实际骨架缺失"),
        UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(EmptyMesh.Get(), TemplateClass.Get(), Error));
    return true;
}
#endif
