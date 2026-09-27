#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commandlets/WorldValidationResultGate.h"
#include "Commandlets/GamePlatformWorldValidationCommandlet.h"
#include "Editor.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GamePlatformWorldDefinitionValidator.h"

namespace
{
// 仅构造结果门禁的输入，不模拟验证器或宣称真实资产已经通过验证。
FValidateAssetsResults CompleteWorldResults(const TArray<FString>& Paths)
{
    FValidateAssetsResults Results;
    Results.NumRequested = Paths.Num();
    Results.NumChecked = Paths.Num();
    Results.NumValid = Paths.Num();
    Results.ValidatorStatistics.FindOrAdd(UGamePlatformWorldDefinitionValidator::StaticClass()->GetClassPathName()).AssetsValidated = Paths.Num();
    for (const FString& Path : Paths)
    {
        Results.AssetsDetails.FindOrAdd(Path).Result = EDataValidationResult::Valid;
    }
    return Results;
}
}

// 防止恢复原生固定true行为：空扫描、零执行与不完整执行绝不能产生通过结果。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommandletCoverageTest, "GamePlatform.World.Editor.Commandlet.NonEmptyCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCommandletCoverageTest::RunTest(const FString&)
{
    const TArray<FString> Paths{ TEXT("/Game/TestWorld.TestWorld"), TEXT("/Game/TestRegion.TestRegion") };
    const TArray<FString> Empty;
    TestFalse(TEXT("空集合不能通过"), GamePlatformWorldValidation::PassesResultGate(CompleteWorldResults(Empty), Empty));
    TestFalse(TEXT("有扫描无执行不能通过"), GamePlatformWorldValidation::PassesResultGate(FValidateAssetsResults{}, Paths));
    auto Results = CompleteWorldResults(Paths);
    TestTrue(TEXT("所有选中对象真实结果齐全时通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    --Results.NumChecked;
    TestFalse(TEXT("少执行一个不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.NumSkipped = 1;
    TestFalse(TEXT("跳过不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.NumUnableToValidate = 1;
    TestFalse(TEXT("未验证不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.bAssetLimitReached = true;
    TestFalse(TEXT("数量上限截断不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    return true;
}

// 捕获只看NumChecked或总数、忽略Invalid及逐资产结果的错误修改。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommandletInvalidTest, "GamePlatform.World.Editor.Commandlet.InvalidFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCommandletInvalidTest::RunTest(const FString&)
{
    const TArray<FString> Paths{ TEXT("/Game/TestWorld.TestWorld") };
    auto Results = CompleteWorldResults(Paths); Results.NumInvalid = 1;
    TestFalse(TEXT("Invalid必须失败"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.AssetsDetails.FindChecked(Paths[0]).Result = EDataValidationResult::Invalid;
    TestFalse(TEXT("总数不能遮盖逐资产失败"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.AssetsDetails.FindChecked(Paths[0]).Result = EDataValidationResult::NotValidated;
    TestFalse(TEXT("逐资产未验证必须失败"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.ValidatorMessages.Add(FTokenizedMessage::Create(EMessageSeverity::Error, FText::FromString(TEXT("验证器整体失败"))));
    TestFalse(TEXT("非资产级验证错误也失败"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    return true;
}

// 其他验证器的Valid不能替代World地图/父链验证；旧日志同数量对象也不能替代本次身份。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommandletEvidenceTest, "GamePlatform.World.Editor.Commandlet.ExactEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCommandletEvidenceTest::RunTest(const FString&)
{
    const TArray<FString> Paths{ TEXT("/Game/TestWorld.TestWorld") };
    auto Results = CompleteWorldResults(Paths); Results.ValidatorStatistics.Reset();
    TestFalse(TEXT("世界验证器未执行不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.ValidatorStatistics.FindChecked(UGamePlatformWorldDefinitionValidator::StaticClass()->GetClassPathName()).AssetsValidated = 0;
    TestFalse(TEXT("只有登记没有执行不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); Results.AssetsDetails.Reset();
    Results.AssetsDetails.FindOrAdd(TEXT("/Game/OldWorld.OldWorld")).Result = EDataValidationResult::Valid;
    TestFalse(TEXT("其他对象不能顶替当前扫描身份"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    Results = CompleteWorldResults(Paths); ++Results.NumRequested;
    TestFalse(TEXT("额外领域资产混入不能通过"), GamePlatformWorldValidation::PassesResultGate(Results, Paths));
    return true;
}

// 走实际反射Commandlet的Main，不仅检查布尔辅助门禁；固定领域不可被通用AssetType参数改写。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommandletEntryTest, "GamePlatform.World.Editor.Commandlet.RejectScopeOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCommandletEntryTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实编辑器上下文必须存在，不允许前置缺失冒充参数门禁通过"), GEditor)) { return false; }
    TStrongObjectPtr<UGamePlatformWorldValidationCommandlet> Commandlet(NewObject<UGamePlatformWorldValidationCommandlet>());
    TestEqual(TEXT("改变扫描领域明确返回失败码1而不是原生2或成功0"),
        Commandlet->Main(TEXT("-AssetType=Texture2D")), 1);
    return true;
}
#endif
