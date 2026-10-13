// 平台Editor证据回归：完整合同、路径逃逸、缺当前运行/二进制、旧无绑定报告均明确拒绝。
// 只读/瞬态测试，不写人工Passed或填真实审核人；原生运行待统一Editor构建。
#if WITH_DEV_AUTOMATION_TESTS
#include "Review/GamePlatformReviewTypes.h"
#include "Review/GamePlatformReviewEvidenceValidation.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformReviewEvidenceTest, "GamePlatform.DeveloperTools.ReviewEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformReviewEvidenceTest::RunTest(const FString&)
{
    auto* Case = NewObject<UGamePlatformReviewCase>();
    Case->CaseId = TEXT("Review.Test"); Case->Category = TEXT("Diagnostics"); Case->Description = TEXT("证据门禁测试");
    Case->Steps = {TEXT("检查真实状态")}; Case->ExpectedResult = TEXT("版本和证据一致"); Case->RequiredEvidence = {TEXT("log")};
    FString Error, Path; const FString RunId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestFalse(TEXT("拒绝路径逃逸"), GamePlatform::Review::ResolveEvidencePath(RunId, TEXT("../outside.log"), Path, Error));
    TestFalse(TEXT("拒绝任意绝对路径"), GamePlatform::Review::ResolveEvidencePath(RunId, TEXT("C:/outside.log"), Path, Error));
    auto* Service = NewObject<UGamePlatformReviewSubsystem>();
    auto* Report = Service->CreatePendingReport(Case, RunId, TEXT("unverified"), TEXT("unverified"));
    TestNotNull(TEXT("完整用例创建待审"), Report); if (!Report) return false;
    TestFalse(TEXT("缺真实运行/二进制不能签收"), Service->ValidateReviewEvidence(*Case, *Report, Error));
    const auto OldFingerprint = Case->GetContractFingerprint(); Case->ExpectedResult = TEXT("变更后的预期");
    TestNotEqual(TEXT("预期变更改变合同指纹"), OldFingerprint, Case->GetContractFingerprint());
    TestFalse(TEXT("旧版本报告不计当前通过"), Service->ValidateReviewEvidence(*Case, *Report, Error));
    auto* Legacy = NewObject<UGamePlatformReviewReport>();
    TestFalse(TEXT("旧无绑定报告拒绝人工Passed"), Service->ApplyHumanDecision(Legacy, EGamePlatformReviewStatus::Passed, TEXT("仅测试审核者"), TEXT(""), {}, true, Error));
    // 只在内存构造证据字段，绝不落盘假运行。覆盖FName长名、旧运行后改预期再建新报告的路径。
    TestFalse(TEXT("超长外部JSON身份在FName前拒绝"), GamePlatform::Review::IsEvidenceIdentity(FString::ChrN(NAME_SIZE, TEXT('A'))));
    TestFalse(TEXT("保留空名在FName前拒绝"), GamePlatform::Review::IsEvidenceIdentity(TEXT("nOnE")));
    Case->FeatureId = TEXT("Related.Feature"); Case->ScenarioId = TEXT("Main.Scenario");
    auto Frozen = MakeShared<FJsonObject>();
    Frozen->SetStringField(TEXT("review_case_id"), Case->CaseId.ToString()); Frozen->SetNumberField(TEXT("version"), Case->CaseVersion);
    Frozen->SetStringField(TEXT("feature_id"), Case->FeatureId.ToString()); Frozen->SetStringField(TEXT("scenario_id"), Case->ScenarioId.ToString());
    Frozen->SetStringField(TEXT("category"), Case->Category.ToString()); Frozen->SetStringField(TEXT("description"), Case->Description);
    Frozen->SetStringField(TEXT("expected_result"), Case->ExpectedResult);
    TArray<TSharedPtr<FJsonValue>> Steps, Required;
    for (const auto& Step : Case->Steps) Steps.Add(MakeShared<FJsonValueString>(Step));
    for (const auto& Type : Case->RequiredEvidence) Required.Add(MakeShared<FJsonValueString>(Type));
    Frozen->SetArrayField(TEXT("steps"), Steps); Frozen->SetArrayField(TEXT("required_evidence"), Required); Frozen->SetArrayField(TEXT("preconditions"), {});
    TestTrue(TEXT("关联功能独立合同合法，不要求等于主功能"), GamePlatform::Review::IsFrozenCaseCurrent(Frozen, *Case));
    Case->ExpectedResult += TEXT("新的预期");
    TestFalse(TEXT("旧运行后改预期即使未递增版本也拒绝"), GamePlatform::Review::IsFrozenCaseCurrent(Frozen, *Case));
    auto State = MakeShared<FJsonObject>();
    State->SetStringField(TEXT("run_id"), RunId); State->SetStringField(TEXT("scenario_id"), TEXT("Main.Scenario")); State->SetStringField(TEXT("feature_id"), TEXT("Main.Feature"));
    State->SetNumberField(TEXT("run_state"), 4); State->SetNumberField(TEXT("completed_operations"), 1);
    auto Cleanup = MakeShared<FJsonObject>(); Cleanup->SetNumberField(TEXT("status"), 1); Cleanup->SetStringField(TEXT("code"), TEXT("None"));
    auto Terminal = MakeShared<FJsonObject>(); Terminal->SetNumberField(TEXT("status"), 1);
    State->SetObjectField(TEXT("cleanup_result"), Cleanup); State->SetObjectField(TEXT("terminal_result"), Terminal);
    TestTrue(TEXT("本Run完整终态字段接受"), GamePlatform::Review::IsRunStateCompleted(State, RunId, TEXT("Main.Scenario"), TEXT("Main.Feature")));
    State->SetNumberField(TEXT("run_state"), 2);
    TestFalse(TEXT("Running不能声称runtime_completed"), GamePlatform::Review::IsRunStateCompleted(State, RunId, TEXT("Main.Scenario"), TEXT("Main.Feature")));
    State->SetNumberField(TEXT("run_state"), 4); State->SetNumberField(TEXT("completed_operations"), 0);
    TestFalse(TEXT("仅准备/停止零操作不能冒充执行"), GamePlatform::Review::IsRunStateCompleted(State, RunId, TEXT("Main.Scenario"), TEXT("Main.Feature")));
    State->SetNumberField(TEXT("completed_operations"), 1);
    TestFalse(TEXT("其他Run状态拒绝"), GamePlatform::Review::IsRunStateCompleted(State, FGuid::NewGuid().ToString(), TEXT("Main.Scenario"), TEXT("Main.Feature")));
    return true;
}
#endif
