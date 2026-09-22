#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/GamePlatformExperienceDefinition.h"
#include "Definitions/GamePlatformPawnDefinition.h"
#include "Misc/AutomationTest.h"
#include "Settings/GamePlatformGameplaySettings.h"
#include "Types/GamePlatformExperienceState.h"
#include "Types/GamePlatformGameplayReadiness.h"
#include "Types/GamePlatformPlayerLifecycle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformGameplaySafeDefaultsTest,
    "GamePlatform.Gameplay.Contracts.SafeDefaults",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformGameplaySafeDefaultsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FGamePlatformExperienceSnapshot Experience;
    TestEqual(TEXT("默认体验阶段必须是未选定"), Experience.Stage, EGamePlatformExperienceStage::Unassigned);
    TestFalse(TEXT("默认体验快照不能表示已经分配"), Experience.IsAssigned());

    const FGamePlatformPlayerLifecycleSnapshot Player;
    TestEqual(TEXT("默认玩家阶段必须是未登记"), Player.Stage, EGamePlatformPlayerStage::Unregistered);
    TestFalse(TEXT("默认玩家快照不能表示服务器已激活"), Player.IsServerActive());

    const FGamePlatformPreparationToken Token;
    TestFalse(TEXT("默认准备令牌必须无效"), Token.IsValid());

    FGamePlatformLocalPreparationFacts Facts;
    Facts.bClientExperiencePrepared = true;
    Facts.bClientPawnBound = true;
    Facts.bInputProfilePrepared = true;
    Facts.bBindingsReady = true;
    TestTrue(TEXT("开放前四项本地事实齐备时可以提交准备报告"), Facts.IsComplete());
    TestFalse(TEXT("开放前准备事实不得包含GameplayInputEnabled的循环前置"), Facts.bGameplayInputEnabled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformGameplayDefinitionValidationTest,
    "GamePlatform.Gameplay.Definitions.Validation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformGameplayDefinitionValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UGamePlatformExperienceDefinition* Experience = NewObject<UGamePlatformExperienceDefinition>();
    TestNotNull(TEXT("应能创建体验定义夹具"), Experience);
    if (!Experience)
    {
        return false;
    }

    FGamePlatformId::TryParse(TEXT("foundation.explore@1"), Experience->LogicalId);
    Experience->DataVersion.SchemaVersion = 1;
    Experience->DataVersion.ContentRevision = 1;
    Experience->Purpose = TEXT("FoundationExplore");
    FGamePlatformId WorldId;
    FGamePlatformId::TryParse(TEXT("foundation.world@1"), WorldId);
    Experience->SupportedWorldIds.Add(WorldId);
    Experience->SpawnPolicyId = TEXT("PlayerStart");

    const FGamePlatformResult MissingPawnResult = Experience->ValidateDefinition();
    TestEqual(TEXT("缺少Pawn定义必须给出稳定错误码"), MissingPawnResult.Code, FName(TEXT("MissingPawnDefinition")));

    const FPrimaryAssetId PawnId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("foundation.pawn@1"));
    Experience->DefaultPawnDefinitionId = PawnId;
    Experience->RequiredDefinitions.Add(PawnId);

    FGamePlatformExperienceAssemblyEntry First;
    First.AssemblyId = TEXT("Rules");
    First.FactoryId = TEXT("RulesFactory");
    Experience->AssemblyEntries.Add(First);
    Experience->AssemblyEntries.Add(First);
    const FGamePlatformResult DuplicateResult = Experience->ValidateDefinition();
    TestEqual(TEXT("重复装配项必须拒绝"), DuplicateResult.Code, FName(TEXT("DuplicateAssemblyId")));

    Experience->AssemblyEntries.Reset();
    FGamePlatformExperienceAssemblyEntry A;
    A.AssemblyId = TEXT("A");
    A.FactoryId = TEXT("FactoryA");
    A.Dependencies.Add(TEXT("B"));
    FGamePlatformExperienceAssemblyEntry B;
    B.AssemblyId = TEXT("B");
    B.FactoryId = TEXT("FactoryB");
    B.Dependencies.Add(TEXT("A"));
    Experience->AssemblyEntries = {A, B};
    const FGamePlatformResult CycleResult = Experience->ValidateDefinition();
    TestEqual(TEXT("装配依赖环必须拒绝"), CycleResult.Code, FName(TEXT("AssemblyDependencyCycle")));

    UGamePlatformPawnDefinition* Pawn = NewObject<UGamePlatformPawnDefinition>();
    FGamePlatformId::TryParse(TEXT("foundation.pawn@1"), Pawn->LogicalId);
    Pawn->DataVersion.SchemaVersion = 1;
    Pawn->DataVersion.ContentRevision = 1;
    const FGamePlatformResult MissingClassResult = Pawn->ValidateDefinition();
    TestEqual(TEXT("缺少Pawn类必须拒绝"), MissingClassResult.Code, FName(TEXT("MissingPawnClass")));

    const UGamePlatformGameplaySettings* Settings = GetDefault<UGamePlatformGameplaySettings>();
    TestNotNull(TEXT("玩法设置默认对象必须存在"), Settings);
    TestTrue(TEXT("设置安全上限必须合法"), Settings && Settings->ValidateSettings().IsSuccess());
    return true;
}

#endif
