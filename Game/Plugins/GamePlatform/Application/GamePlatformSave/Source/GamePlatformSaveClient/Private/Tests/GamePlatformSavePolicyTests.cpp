#if WITH_DEV_AUTOMATION_TESTS

// GamePlatformSave（游戏平台本地存档插件）纯策略自动化测试。
// 本文件先定义期望行为：Key校验、版本化Envelope往返、CRC损坏检测与容量边界。
#include "Misc/AutomationTest.h"
#include "Policy/GamePlatformSavePolicy.h"

namespace
{
    FGamePlatformSaveRecord MakeValidRecord()
    {
        FGamePlatformSaveRecord Record;
        Record.Key.Namespace = TEXT("LocalTutorial");
        Record.Key.ProfileKey = TEXT("profile-A");
        Record.Key.SlotName = TEXT("main");
        Record.SchemaVersion = 3;
        Record.Payload = { 1, 2, 3, 4, 5, 6 };
        return Record;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSaveValidKeyTest,
    "GamePlatform.Save.Policy.AcceptsValidKey",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSaveValidKeyTest::RunTest(const FString& Parameters)
{
    const FGamePlatformResult Result =
        FGamePlatformSavePolicy::ValidateKey(MakeValidRecord().Key);
    TestTrue(TEXT("合法本地存档Key应通过严格校验"), Result.IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSaveRejectsEmptyKeyTest,
    "GamePlatform.Save.Policy.RejectsEmptyKey",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSaveRejectsEmptyKeyTest::RunTest(const FString& Parameters)
{
    FGamePlatformSaveKey Key;
    Key.Namespace = NAME_None;
    Key.ProfileKey = TEXT("../unsafe");
    Key.SlotName = TEXT("");

    const FGamePlatformResult Result =
        FGamePlatformSavePolicy::ValidateKey(Key);

    TestFalse(TEXT("空命名空间或空槽位必须Fail Closed"), Result.IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSaveEnvelopeRoundTripTest,
    "GamePlatform.Save.Policy.EnvelopeRoundTrip",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSaveEnvelopeRoundTripTest::RunTest(const FString& Parameters)
{
    const FGamePlatformSaveRecord Original = MakeValidRecord();

    TArray<uint8> Encoded;
    const FGamePlatformResult EncodeResult =
        FGamePlatformSavePolicy::EncodeRecord(Original, Encoded);
    TestTrue(TEXT("合法记录必须能够编码"), EncodeResult.IsSuccess());

    FGamePlatformSaveRecord Decoded;
    const FGamePlatformResult DecodeResult =
        FGamePlatformSavePolicy::DecodeRecord(
            Original.Key,
            Encoded,
            Decoded);

    TestTrue(TEXT("合法Envelope必须能够解码"), DecodeResult.IsSuccess());
    TestEqual(TEXT("SchemaVersion必须保持"), Decoded.SchemaVersion, Original.SchemaVersion);
    TestEqual(TEXT("Payload长度必须保持"), Decoded.Payload.Num(), Original.Payload.Num());
    TestTrue(TEXT("Payload内容必须保持"), Decoded.Payload == Original.Payload);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSaveDetectsCorruptionTest,
    "GamePlatform.Save.Policy.DetectsPayloadCorruption",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSaveDetectsCorruptionTest::RunTest(const FString& Parameters)
{
    const FGamePlatformSaveRecord Original = MakeValidRecord();
    TArray<uint8> Encoded;
    TestTrue(
        TEXT("测试前置编码必须成功"),
        FGamePlatformSavePolicy::EncodeRecord(Original, Encoded).IsSuccess());

    if (Encoded.Num() > 0)
    {
        Encoded.Last() ^= 0x7F;
    }

    FGamePlatformSaveRecord Decoded;
    const FGamePlatformResult DecodeResult =
        FGamePlatformSavePolicy::DecodeRecord(
            Original.Key,
            Encoded,
            Decoded);

    TestFalse(TEXT("CRC不匹配必须拒绝损坏Payload"), DecodeResult.IsSuccess());
    TestEqual(
        TEXT("损坏错误码必须稳定"),
        DecodeResult.Code,
        FName(TEXT("SavePayloadCorrupted")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSaveRejectsOversizedPayloadTest,
    "GamePlatform.Save.Policy.RejectsOversizedPayload",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSaveRejectsOversizedPayloadTest::RunTest(const FString& Parameters)
{
    FGamePlatformSaveRecord Record = MakeValidRecord();
    Record.Payload.SetNumZeroed(FGamePlatformSavePolicy::GetMaxPayloadBytes() + 1);

    const FGamePlatformResult Result =
        FGamePlatformSavePolicy::ValidateRecord(Record);

    TestFalse(TEXT("超过安全上限的本地Payload必须拒绝"), Result.IsSuccess());
    TestEqual(
        TEXT("容量错误码必须稳定"),
        Result.Code,
        FName(TEXT("SavePayloadTooLarge")));
    return true;
}

#endif
