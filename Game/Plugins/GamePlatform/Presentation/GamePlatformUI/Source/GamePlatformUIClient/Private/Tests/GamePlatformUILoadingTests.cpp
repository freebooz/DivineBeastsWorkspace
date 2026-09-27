#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Loading/GamePlatformLoadingScreenService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUILoadingTokenTest,
    "GamePlatform.UI.Loading.TokenRefCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUILoadingTokenTest::RunTest(const FString& Parameters)
{
    UGamePlatformLoadingScreenService* Service =
        NewObject<UGamePlatformLoadingScreenService>();

    const FGamePlatformLoadingToken A =
        Service->AcquireToken(FText::FromString(TEXT("A")), -1.0f);
    const FGamePlatformLoadingToken B =
        Service->AcquireToken(FText::FromString(TEXT("B")), 1.5f);

    FGamePlatformUILoadingSnapshot Snapshot = Service->GetSnapshot();
    TestTrue(TEXT("存在Loading"), Snapshot.bIsLoading);
    TestEqual(TEXT("两个Token"), Snapshot.ActiveTokenCount, 2);
    TestEqual(TEXT("超过1的真实进度被夹紧"), Snapshot.Progress, 1.0f);

    TestTrue(
        TEXT("未知进度可更新"),
        Service->UpdateToken(B, FText::FromString(TEXT("B2")), -0.5f));
    Snapshot = Service->GetSnapshot();
    TestEqual(TEXT("负进度统一表示未知"), Snapshot.Progress, -1.0f);

    TestTrue(TEXT("释放A"), Service->ReleaseToken(A));
    TestTrue(TEXT("释放B"), Service->ReleaseToken(B));
    Snapshot = Service->GetSnapshot();
    TestFalse(TEXT("全部释放后结束Loading"), Snapshot.bIsLoading);
    TestEqual(TEXT("Token归零"), Snapshot.ActiveTokenCount, 0);
    return true;
}

#endif
