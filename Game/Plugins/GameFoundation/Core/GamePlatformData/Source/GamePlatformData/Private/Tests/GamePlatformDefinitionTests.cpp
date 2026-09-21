#include "Definitions/GamePlatformDefinitionBase.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
// 若身份退回资产名称，重命名与不同物理文件的相同逻辑身份将不再等价。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformDefinitionIdentityTest,
    "GamePlatform.Data.Definition.IdentityAndVersion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformDefinitionIdentityTest::RunTest(const FString& Parameters)
{
    auto* First = NewObject<UGamePlatformDefinitionBase>();
    auto* Second = NewObject<UGamePlatformDefinitionBase>();
    FGamePlatformId::TryParse(TEXT("platform.probe@1"), First->LogicalId);
    Second->LogicalId = First->LogicalId;
    TestEqual(TEXT("独立对象使用相同稳定身份"), First->GetPrimaryAssetId(), Second->GetPrimaryAssetId());
    TestEqual(TEXT("规范主资产身份"), First->GetPrimaryAssetId().ToString(), FString(TEXT("GamePlatformDefinition:platform.probe@1")));
    TestTrue(TEXT("默认结构版本可读"), First->ValidateDefinition().IsSuccess());
    First->DataVersion.SchemaVersion = 2;
    TestFalse(TEXT("资产不能自行扩大类的可读范围"), First->ValidateDefinition().IsSuccess());
    First->DataVersion.SchemaVersion = 1;
    First->DataVersion.ContentRevision = 0;
    TestFalse(TEXT("非法内容修订被拒绝"), First->ValidateDefinition().IsSuccess());
    First->DataVersion.ContentRevision = 1;
    First->RequiredDefinitions.Add(FPrimaryAssetId());
    TestFalse(TEXT("非法依赖身份被拒绝"), First->ValidateDefinition().IsSuccess());
    return true;
}
#endif
