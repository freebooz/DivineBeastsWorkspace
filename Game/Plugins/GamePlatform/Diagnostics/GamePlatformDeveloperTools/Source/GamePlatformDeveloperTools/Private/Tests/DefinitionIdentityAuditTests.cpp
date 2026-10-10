// Editor定义审计回归：只创建内存测试对象，不保存uasset；验证真实LogicalId契约、重复所有者及版本失败。
#if WITH_DEV_AUTOMATION_TESTS
#include "Validation/GamePlatformGlobalAssetValidator.h"
#include "Validation/GamePlatformEditorValidators.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIdentityAuditTest, "GamePlatform.DeveloperTools.DefinitionIdentityContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIdentityAuditTest::RunTest(const FString&)
{
    TStrongObjectPtr<UPackage> Package(CreatePackage(*FString::Printf(TEXT("/Temp/GPDefinitionAudit_%s"), *FGuid::NewGuid().ToString())));
    TStrongObjectPtr<UGamePlatformDefinitionBase> First(NewObject<UGamePlatformDefinitionBase>(Package.Get(), TEXT("HeroOne"), RF_Public | RF_Standalone));
    TStrongObjectPtr<UGamePlatformDefinitionBase> Second(NewObject<UGamePlatformDefinitionBase>(Package.Get(), TEXT("HeroTwo"), RF_Public | RF_Standalone));
    FGamePlatformId::TryParse(TEXT("gameplatform.audit.hero@1"), First->LogicalId);
    Second->LogicalId = First->LogicalId;
    // 同时走引擎注册Validator实现，避免只验证全局聚合而遗漏逐资产入口仍使用过期字段。
    TStrongObjectPtr<UGamePlatformDefinitionValidator> Validator(NewObject<UGamePlatformDefinitionValidator>());
    TStrongObjectPtr<UGamePlatformStableIdValidator> IdValidator(NewObject<UGamePlatformStableIdValidator>());
    FDataValidationContext Context;
    TestTrue(TEXT("注册Validator按真实继承接纳定义"), Validator->CanValidateAsset_Implementation(FAssetData(First.Get()), First.Get(), Context));
    TestEqual(TEXT("逐资产校验承认LogicalId和DataVersion"), Validator->ValidateLoadedAsset_Implementation(FAssetData(First.Get()), First.Get(), Context), EDataValidationResult::Valid);
    TestEqual(TEXT("完整LogicalId不走旧字符串字符集"), IdValidator->ValidateLoadedAsset_Implementation(FAssetData(First.Get()), First.Get(), Context), EDataValidationResult::Valid);
    TArray<FGamePlatformValidationResult> Results;
    FGamePlatformGlobalAssetValidator::ValidateDefinitionsAndStableIds({ FAssetData(First.Get()) }, Results);
    TestEqual(TEXT("真实LogicalId定义不需要旧DefinitionId字段"), Results[0].Status, EGamePlatformValidationStatus::Passed);
    Results.Reset();
    FGamePlatformGlobalAssetValidator::ValidateDefinitionsAndStableIds({ FAssetData(First.Get()), FAssetData(Second.Get()) }, Results);
    TestEqual(TEXT("不同资产同主身份必须失败"), Results[0].Status, EGamePlatformValidationStatus::Failed);
    TestEqual(TEXT("重复身份单独反映于StableId门禁"), Results[1].Status, EGamePlatformValidationStatus::Failed);
    First->DataVersion.ContentRevision = 0;
    TStrongObjectPtr<UGamePlatformDefinitionValidator> InvalidValidator(NewObject<UGamePlatformDefinitionValidator>());
    FDataValidationContext InvalidContext;
    TestEqual(TEXT("逐资产校验拒绝非法内容修订"), InvalidValidator->ValidateLoadedAsset_Implementation(FAssetData(First.Get()), First.Get(), InvalidContext), EDataValidationResult::Invalid);
    Results.Reset();
    FGamePlatformGlobalAssetValidator::ValidateDefinitionsAndStableIds({ FAssetData(First.Get()) }, Results);
    TestEqual(TEXT("非法内容修订不允许通过聚合验收"), Results[0].Status, EGamePlatformValidationStatus::Failed);
    First->ClearFlags(RF_Public | RF_Standalone);
    Second->ClearFlags(RF_Public | RF_Standalone);
    return true;
}
#endif
