#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Validation/GamePlatformVFXDefinitionValidator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXMissingNiagaraValidationTest,
    "GamePlatform.VFX.Editor.Validation.MissingNiagara",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXMissingNiagaraValidationTest::RunTest(const FString& Parameters)
{
    const UGamePlatformVFXInstantDefinition* Definition = NewObject<UGamePlatformVFXInstantDefinition>();
    TArray<FGamePlatformVFXValidationIssue> Issues;
    FGamePlatformVFXDefinitionValidator::Validate(*Definition, Issues);

    const bool bFound = Issues.ContainsByPredicate([](const FGamePlatformVFXValidationIssue& Issue)
    {
        return Issue.RuleId == TEXT("GPVFX.Definition.MissingNiagara");
    });

    TestTrue(TEXT("缺失 Niagara 必须产生验证错误"), bFound);
    return true;
}

#endif
