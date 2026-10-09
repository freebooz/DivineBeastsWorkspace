#if WITH_DEV_AUTOMATION_TESTS
// 平台定义租约失败路径：未初始化GI没有Data服务，必须同步拒绝且无回调；不使用假资产/假租约。
#include "Misc/AutomationTest.h"
#include "Loading/GamePlatformHeroDefinitionLoader.h"
#include "Engine/GameInstance.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeroDefinitionResourceLeaseTest,
    "GamePlatform.Character.ResourceLease.UninitializedInstanceRejects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeroDefinitionResourceLeaseTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UGameInstance* Instance = NewObject<UGameInstance>();
    bool bCalled = false; FGamePlatformResult Result;
    const auto Lease = FGamePlatformHeroDefinitionLoader::AcquireDefinitionResources(*Instance,
        FPrimaryAssetId(TEXT("HeroDefinition"), TEXT("Missing")), EGamePlatformDataLifetime::Instance, Instance,
        [&bCalled](auto*, const auto&, const auto&) { bCalled = true; }, Result);
    TestFalse(TEXT("无作用域不签发租约"), Lease.IsValid());
    TestFalse(TEXT("同步拒绝明确失败"), Result.IsSuccess());
    TestFalse(TEXT("未接纳请求不产生异步完成"), bCalled);
    return true;
}
#endif
