// 平台鸟群自动化：数量及种子执行纯算法断言；Actor生命周期使用真实GameWorld与ISM组件。
// 在没有PIE的编辑器测试宿主中创建独立瞬态游戏世界，退出时回收，不替代真实Niagara/视觉/Cook性能验收。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Environment/GamePlatformBirdFlockProfile.h"
#include "Environment/GamePlatformBirdFlockActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"
// NewObject的Outer转换需要UPackage完整定义，不能依赖其他模块的传递包含。
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformBirdFlockParametersTest,
    "GamePlatform.VFX.BirdFlock.CountAndSeed",
    // 同一参数合同可在编辑器或专用客户端开发构建中独立运行。
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformBirdFlockParametersTest::RunTest(const FString& Parameters)
{
    UGamePlatformBirdFlockProfile* Profile =
        NewObject<UGamePlatformBirdFlockProfile>(GetTransientPackage());
    Profile->DefaultBirdCount = 32;
    Profile->MaxBirdCount = 512;

    TestEqual(TEXT("未指定覆盖时使用配置默认数量"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, -1, 1.0f), 32);
    TestEqual(TEXT("数量为0时不保留隐藏实例"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 0, 1.0f), 0);
    TestEqual(TEXT("数量为1时支持单鸟"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 1, 1.0f), 1);
    TestEqual(TEXT("数量上限支持512"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 512, 1.0f), 512);
    TestEqual(TEXT("越过512硬上限直接拒绝"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 513, 1.0f), INDEX_NONE);
    TestEqual(TEXT("负数量直接拒绝"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, -2, 1.0f), INDEX_NONE);
    TestEqual(TEXT("质量0不保留实例"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 128, 0.0f), 0);
    TestEqual(TEXT("质量减半的实际实例数"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 128, 0.5f), 64);
    TestEqual(TEXT("大于1的质量比例不合法"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 128, 1.1f), INDEX_NONE);
    Profile->MaxBirdCount = 40;
    TestEqual(TEXT("按物种上限裁剪请求数量"),
        FGamePlatformBirdFlockMath::ResolveBirdCount(*Profile, 128, 1.0f), 40);

    const float A = FGamePlatformBirdFlockMath::SeedToUnit(4, 25, 123);
    TestEqual(TEXT("相同种子结果可复现"), A,
        FGamePlatformBirdFlockMath::SeedToUnit(4, 25, 123));
    TestTrue(TEXT("散布随机分布在半开区间[0,1)"), A >= 0.0f && A < 1.0f);
    TestNotEqual(TEXT("不同个体不采用完全相同的随机相位"), A,
        FGamePlatformBirdFlockMath::SeedToUnit(4, 26, 123));
    return true;
}


/** 无渲染设备环境也能检查真实运行Actor数量/生命周期，不依赖游戏项目鸟种资源。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformBirdFlockActorRuntimeTest,
    "GamePlatform.VFX.BirdFlock.ActorInstanceLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformBirdFlockActorRuntimeTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("运行中引擎不能为空"), GEngine))
    {
        return false;
    }
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (UWorld* Candidate = Context.World())
        {
            if (Candidate->IsGameWorld())
            {
                World = Candidate;
                break;
            }
        }
    }
    // 编辑器命令行自动化通常不存在PIE World。只在没有可用游戏世界时建立隔离的真实GameWorld，
    // 仍通过真实SpawnActor/ISM和销毁入口验收，不注入模拟Actor或伪造实例计数。
    TStrongObjectPtr<UWorld> OwnedWorld;
    if (!World)
    {
        OwnedWorld.Reset(UWorld::CreateWorld(EWorldType::Game, false));
        World = OwnedWorld.Get();
        // 引擎销毁Actor会查WorldContext；测试世界也必须完整登记，不能留下无上下文警告。
        if (World) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    }
    ON_SCOPE_EXIT
    {
        if (OwnedWorld.IsValid())
        {
            UWorld* Owned = OwnedWorld.Get();
            OwnedWorld->DestroyWorld(false);
            GEngine->DestroyWorldContext(Owned);
        }
    };
    if (!TestNotNull(TEXT("必须有真实游戏世界"), World) ||
        !TestTrue(TEXT("世界类型必须支持游戏Actor"), World->IsGameWorld()))
    {
        return false;
    }
    AGamePlatformBirdFlockActor* Actor =
        World->SpawnActor<AGamePlatformBirdFlockActor>(
            FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("在游戏世界中创建鸟群批量实例Actor"), Actor))
    {
        return false;
    }

    // 测试中只加载Engine内置形状/材质，不引入项目内容包或真实鸟类强依赖。
    UGamePlatformBirdFlockProfile* Profile =
        NewObject<UGamePlatformBirdFlockProfile>(Actor);
    Profile->BirdMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    Profile->BirdMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    Profile->MaxBirdCount = 512;
    Profile->DefaultBirdCount = 32;
    if (!TestNotNull(TEXT("测试静态网格"), Profile->BirdMesh.Get()) ||
        !TestNotNull(TEXT("测试材质"), Profile->BirdMaterial.Get()))
    {
        Actor->Destroy();
        return false;
    }
    TestTrue(TEXT("运行中替换物种预设"), Actor->SetBirdProfile(Profile));
    TestEqual(TEXT("默认预设构建的实际实例数"), Actor->GetEffectiveBirdCount(), 32);

    TestTrue(TEXT("数量为0时更新成功"), Actor->SetBirdCount(0));
    TestEqual(TEXT("关闭鸟群立即释放实例"), Actor->GetEffectiveBirdCount(), 0);
    TestTrue(TEXT("切换单鸟"), Actor->SetBirdCount(1));
    TestEqual(TEXT("单鸟实际实例数"), Actor->GetEffectiveBirdCount(), 1);
    TestTrue(TEXT("切换128鸟"), Actor->SetBirdCount(128));
    TestEqual(TEXT("128鸟实际实例数"), Actor->GetEffectiveBirdCount(), 128);
    TestTrue(TEXT("切换512鸟"), Actor->SetBirdCount(512));
    TestEqual(TEXT("512鸟实际实例数"), Actor->GetEffectiveBirdCount(), 512);
    TestTrue(TEXT("降低画质一半"), Actor->SetQualityFraction(0.5f));
    TestEqual(TEXT("512鸟降低画质后实际256只"), Actor->GetEffectiveBirdCount(), 256);
    TestFalse(TEXT("超过硬限制必须拒绝"), Actor->SetBirdCount(513));
    TestEqual(TEXT("非法参数不改变可见实例"), Actor->GetEffectiveBirdCount(), 256);
    TestTrue(TEXT("画质0成功清除可见鸟"), Actor->SetQualityFraction(0.0f));
    TestEqual(TEXT("画质0后无实例残留"), Actor->GetEffectiveBirdCount(), 0);
    Actor->Destroy();
    return true;
}

#endif
