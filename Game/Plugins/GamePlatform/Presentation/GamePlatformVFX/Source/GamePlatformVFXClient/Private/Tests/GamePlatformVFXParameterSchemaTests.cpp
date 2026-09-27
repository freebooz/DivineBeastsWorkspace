#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformVFXParameters.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXParameterSchemaTest,
    "GamePlatform.VFX.Parameters.Schema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXParameterSchemaTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXParameterSchema Schema;
    Schema.MaxOverrideCount = 3;

    FGamePlatformVFXParameterRule& Radius = Schema.Rules.AddDefaulted_GetRef();
    Radius.Name = TEXT("User.Radius");
    Radius.Type = EGamePlatformVFXParameterType::Float;
    Radius.MinValue = 0.0f;
    Radius.MaxValue = 1000.0f;

    FGamePlatformVFXParameterRule& Target = Schema.Rules.AddDefaulted_GetRef();
    Target.Name = TEXT("User.TargetPosition");
    Target.Type = EGamePlatformVFXParameterType::Position;
    Target.bRequired = true;

    FText Reason;
    FGamePlatformVFXParameters Valid;
    Valid.FloatParameters.Add(Radius.Name, 200.0f);
    Valid.PositionParameters.Add(Target.Name, FVector(1000000.0, 2.0, 3.0));
    TestTrue(TEXT("Schema允许声明过的Float和Position"), Schema.Validate(Valid, Reason));

    FGamePlatformVFXParameters Unknown = Valid;
    Unknown.FloatParameters.Add(TEXT("User.Hack"), 1.0f);
    TestFalse(TEXT("未知参数必须拒绝"), Schema.Validate(Unknown, Reason));

    FGamePlatformVFXParameters OutOfRange = Valid;
    OutOfRange.FloatParameters[Radius.Name] = 5000.0f;
    TestFalse(TEXT("越界参数必须拒绝"), Schema.Validate(OutOfRange, Reason));

    FGamePlatformVFXParameters MissingRequired;
    MissingRequired.FloatParameters.Add(Radius.Name, 10.0f);
    TestFalse(TEXT("缺少Required Position必须拒绝"), Schema.Validate(MissingRequired, Reason));
    return true;
}

#endif
