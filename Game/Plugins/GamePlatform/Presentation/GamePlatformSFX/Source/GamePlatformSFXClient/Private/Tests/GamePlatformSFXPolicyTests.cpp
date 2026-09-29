#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Policy/GamePlatformSFXPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSFXValidDefinitionIdTest,
    "GamePlatform.SFX.Policy.ValidDefinitionId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSFXValidDefinitionIdTest::RunTest(const FString& Parameters)
{
    FPrimaryAssetId AssetId;
    const FGamePlatformResult Result =
        FGamePlatformSFXPolicy::BuildDefinitionAssetId(
            TEXT("presentation.sfx.hit@1"),
            AssetId);

    TestTrue(TEXT("规范SFX逻辑身份应被接受"), Result.IsSuccess());
    TestTrue(TEXT("应生成有效GamePlatformDefinition主资产ID"), AssetId.IsValid());
    TestEqual(
        TEXT("主资产名应保持规范逻辑身份"),
        AssetId.PrimaryAssetName,
        FName(TEXT("presentation.sfx.hit@1")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSFXRejectsAssetPathTest,
    "GamePlatform.SFX.Policy.RejectsAssetPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSFXRejectsAssetPathTest::RunTest(const FString& Parameters)
{
    FPrimaryAssetId AssetId;
    const FGamePlatformResult Result =
        FGamePlatformSFXPolicy::BuildDefinitionAssetId(
            TEXT("/Game/Audio/S_Hit.S_Hit"),
            AssetId);

    TestFalse(TEXT("SFX公共请求不得直接携带资产路径"), Result.IsSuccess());
    TestFalse(TEXT("非法请求不得生成主资产ID"), AssetId.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSFXRejectsInvalidParametersTest,
    "GamePlatform.SFX.Policy.RejectsInvalidParameters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSFXRejectsInvalidParametersTest::RunTest(const FString& Parameters)
{
    FGamePlatformSFXRequest Request;
    Request.DefinitionId = TEXT("presentation.sfx.hit@1");
    Request.VolumeMultiplier = 5.0f;

    const FGamePlatformResult Result =
        FGamePlatformSFXPolicy::ValidateRequest(Request);
    TestFalse(TEXT("超出平台安全范围的音量倍数必须拒绝"), Result.IsSuccess());
    return true;
}

#endif
