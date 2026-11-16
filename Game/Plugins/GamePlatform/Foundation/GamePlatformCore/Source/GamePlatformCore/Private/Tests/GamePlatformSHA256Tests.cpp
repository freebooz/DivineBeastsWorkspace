#if WITH_DEV_AUTOMATION_TESTS
#include "Hash/GamePlatformSHA256.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSHA256Test, "GamePlatform.Core.SHA256",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSHA256Test::RunTest(const FString&)
{
    FString Hash;
    TestTrue(TEXT("空输入可散列"), GamePlatform::Hash::TrySha256Hex(nullptr, 0, Hash));
    TestEqual(TEXT("空字节固定向量"), Hash, FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    const ANSICHAR Abc[] = "abc";
    TestTrue(TEXT("abc可散列"), GamePlatform::Hash::TrySha256Hex(Abc, 3, Hash));
    TestEqual(TEXT("abc固定向量"), Hash, FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    uint8 MultiBlock[256]; for (int32 I = 0; I < UE_ARRAY_COUNT(MultiBlock); ++I) MultiBlock[I] = static_cast<uint8>(I);
    TestTrue(TEXT("非文本多块可散列"), GamePlatform::Hash::TrySha256Hex(MultiBlock, sizeof(MultiBlock), Hash));
    TestEqual(TEXT("0至255固定向量"), Hash, FString(TEXT("40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880")));
    TestFalse(TEXT("非空空指针拒绝"), GamePlatform::Hash::TrySha256Hex(nullptr, 1, Hash));
    TestTrue(TEXT("失败清空旧摘要"), Hash.IsEmpty());
    return true;
}
#endif
