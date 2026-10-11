// 编辑器自动化：鸟群可配置数量、非法输入、防止超过512和确定性随机散布边界。
// 这是纯数学合同测试；不等价于真实Niagara、客户端Cook或GPU性能验收。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Environment/GamePlatformBirdFlockProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformBirdFlockParametersTest,
    "GamePlatform.VFX.BirdFlock.CountAndSeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

#endif
