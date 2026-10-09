#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsCombatFeedbackCatalog.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformId.h"

/**
 * 测试项目层技能反馈映射作为统一数据服务Definition的真实契约：
 * 合法英雄+技能键可查；主资产身份、重复映射和无效数值必须在发布前拒绝。
 * 不创建或伪造任何.uasset，不能代替引擎资产烘焙与联机功能测试。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsCombatFeedbackDefinitionTest,
    "DivineBeasts.Presentation.CombatFeedback.DefinitionContract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCombatFeedbackDefinitionTest::RunTest(const FString&)
{
    UDivineBeastsCombatFeedbackCatalog* Catalog =
        NewObject<UDivineBeastsCombatFeedbackCatalog>(GetTransientPackage());
    UGamePlatformHitFeedbackProfile* Profile =
        NewObject<UGamePlatformHitFeedbackProfile>(GetTransientPackage());

    FGamePlatformId CatalogLogicalId;
    FGamePlatformId ProfileLogicalId;
    TestTrue(TEXT("目录逻辑身份可解析"),
        FGamePlatformId::TryParse(TEXT("divinebeasts.combat.feedback@1"), CatalogLogicalId));
    TestTrue(TEXT("Profile逻辑身份可解析"),
        FGamePlatformId::TryParse(TEXT("platform.combat.feedback@1"), ProfileLogicalId));
    Catalog->LogicalId = CatalogLogicalId;
    Profile->LogicalId = ProfileLogicalId;
    TestTrue(TEXT("默认反馈参数满足平台定义校验"),
        Profile->ValidateDefinition().IsSuccess());

    FDivineBeastsCombatFeedbackEntry Entry;
    Entry.HeroDefinitionId = TEXT("Hero.Fixture");
    Entry.AbilityDefinitionId = TEXT("Ability.Fixture");
    Entry.ProfileDefinitionId = Profile->GetPrimaryAssetId();
    Catalog->Entries.Add(Entry);

    TArray<FString> Errors;
    TestTrue(TEXT("唯一有效映射通过校验"),
        Catalog->ValidateMappings(Errors));
    TestTrue(TEXT("目录纳入统一主资产Definition校验"),
        Catalog->ValidateDefinition().IsSuccess());

    FDivineBeastsCombatFeedbackEntry Resolved;
    TestTrue(TEXT("精确英雄及技能ID能够查询"),
        Catalog->TryResolve(Entry.HeroDefinitionId, Entry.AbilityDefinitionId, Resolved));
    TestEqual(TEXT("查询必须保持主资产逻辑身份"),
        Resolved.ProfileDefinitionId, Entry.ProfileDefinitionId);

    Catalog->Entries.Add(Entry);
    TestFalse(TEXT("重复键应失败而非按数组位置任意覆盖"),
        Catalog->TryResolve(Entry.HeroDefinitionId, Entry.AbilityDefinitionId, Resolved));
    TestFalse(TEXT("重复目录必须拒绝发布"),
        Catalog->ValidateDefinition().IsSuccess());

    Catalog->Entries.SetNum(1);
    Catalog->Entries[0].ProfileDefinitionId = FPrimaryAssetId();
    TestFalse(TEXT("无效Profile主资产身份不能发布"),
        Catalog->ValidateMappings(Errors));

    Profile->Tuning.CameraStrength = 3.0f;
    TestFalse(TEXT("非法镜头强度须在编辑器资产验证阶段拒绝"),
        Profile->ValidateDefinition().IsSuccess());
    return true;
}
#endif
