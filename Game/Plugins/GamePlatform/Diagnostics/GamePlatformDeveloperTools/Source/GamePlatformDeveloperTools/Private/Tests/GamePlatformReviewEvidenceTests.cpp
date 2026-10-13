// 平台Editor证据回归：完整合同、路径逃逸、缺当前运行/二进制、旧无绑定报告均明确拒绝。
// 只读/瞬态测试，不写人工Passed或填真实审核人；原生运行待统一Editor构建。
#if WITH_DEV_AUTOMATION_TESTS
#include "Review/GamePlatformReviewTypes.h"
#include "Review/GamePlatformReviewEvidenceValidation.h"
#include "Misc/AutomationTest.h"
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
    return true;
}
#endif
