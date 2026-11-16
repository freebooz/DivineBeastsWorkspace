#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"

/**
 * 平台定义主资产身份转换源码用例：供后续允许编译时运行。
 * 本轮只创建测试文件，不执行编译/UE自动化，也不加载任何数据资产。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDefinitionAssetIdResolutionTest,
    "GamePlatform.Data.Definitions.SharedAssetIdResolution",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDefinitionAssetIdResolutionTest::RunTest(const FString&)
{
    using EStatus = EGamePlatformDefinitionIdBuildStatus;
    FPrimaryAssetId Result;

    TestTrue(TEXT("合法小写逻辑编号转换成功"),
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(
            TEXT("world.weather@1"), Result) == EStatus::Success);
    TestTrue(TEXT("主资产类型沿用唯一的GamePlatformDefinition"),
        Result.PrimaryAssetType == UGamePlatformPrimaryDataAsset::DefinitionAssetType());
    TestEqual(TEXT("主资产名称使用规范逻辑身份"), Result.PrimaryAssetName,
        FName(TEXT("world.weather@1")));

    TestTrue(TEXT("合法大写输入也应规范化"),
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(
            TEXT("World.Weather@1"), Result) == EStatus::Success);
    TestEqual(TEXT("规范身份必须是小写"), Result.PrimaryAssetName,
        FName(TEXT("world.weather@1")));

    TestTrue(TEXT("无效路径不能充当逻辑身份"),
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(
            TEXT("/Game/Visuals/NS_VFX.NS_VFX"), Result) == EStatus::InvalidLogicalId);
    TestFalse(TEXT("失败必须清空之前的有效主资产ID"), Result.IsValid());

    TestTrue(TEXT("不存在的逻辑编号拒绝"),
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(
            NAME_None, Result) == EStatus::InvalidLogicalId);
    TestFalse(TEXT("空编号不可构造主资产ID"), Result.IsValid());
    TestTrue(TEXT("不允许带前导零的版本号"),
        UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(
            TEXT("world.weather@01"), Result) == EStatus::InvalidLogicalId);
    return true;
}

#endif
