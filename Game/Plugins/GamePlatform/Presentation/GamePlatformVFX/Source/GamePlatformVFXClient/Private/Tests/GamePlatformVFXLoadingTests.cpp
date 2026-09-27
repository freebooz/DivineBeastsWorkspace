#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Preloading/GamePlatformVFXPreloadCoordinator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXInvalidPreloadTest,
    "GamePlatform.VFX.Loading.InvalidDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXInvalidPreloadTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXPreloadCoordinator Coordinator;
    bool bCompletionCalled = false;

    const FGamePlatformVFXPreloadHandle Handle = Coordinator.RequestDefinition(
        TSoftObjectPtr<UGamePlatformVFXDefinition>(),
        [this, &bCompletionCalled](UGamePlatformVFXDefinition* Definition)
        {
            bCompletionCalled = true;
            TestNull(TEXT("无效 Definition 返回空对象"), Definition);
        });

    TestFalse(TEXT("无效路径不产生预加载句柄"), Handle.IsValid());
    TestTrue(TEXT("失败完成回调被调用"), bCompletionCalled);
    return true;
}

#endif
