#include "Review/GamePlatformReviewTypes.h"

bool UGamePlatformReviewCase::IsCaseComplete(FString& OutError) const
{
    if (CaseId.IsNone())
    {
        OutError = TEXT("CaseId不能为空。");
        return false;
    }
    if (Category.IsNone())
    {
        OutError = TEXT("Category不能为空。");
        return false;
    }
    if (Description.IsEmpty() || Steps.IsEmpty() || ExpectedResult.IsEmpty() || RequiredEvidence.IsEmpty())
    {
        OutError = TEXT("Description/Steps/ExpectedResult/RequiredEvidence必须完整。");
        return false;
    }
    return true;
}

UGamePlatformReviewReport* UGamePlatformReviewSubsystem::CreatePendingReport(
    UGamePlatformReviewCase* ReviewCase,
    const FString& RunId,
    const FString& BuildVersion,
    const FString& ContentRevision)
{
    if (!ReviewCase)
    {
        return nullptr;
    }

    UGamePlatformReviewReport* Report = NewObject<UGamePlatformReviewReport>(this);
    Report->RunId = RunId;
    Report->CaseId = ReviewCase->CaseId;
    Report->Status = EGamePlatformReviewStatus::NotRun;
    Report->BuildVersion = BuildVersion;
    Report->ContentRevision = ContentRevision;
    return Report;
}

bool UGamePlatformReviewSubsystem::ApplyHumanDecision(
    UGamePlatformReviewReport* Report,
    EGamePlatformReviewStatus NewStatus,
    const FString& Reviewer,
    const FString& Comment,
    const TArray<FString>& Evidence,
    bool bExplicitHumanConfirmation,
    FString& OutError)
{
    if (!Report)
    {
        OutError = TEXT("ReviewReport为空。");
        return false;
    }

    if (!bExplicitHumanConfirmation)
    {
        OutError = TEXT("人工结论必须由真实Reviewer显式确认；自动化/AI不可代签。");
        return false;
    }

    if (Reviewer.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("Reviewer不能为空。");
        return false;
    }

    if (NewStatus == EGamePlatformReviewStatus::Passed && Evidence.IsEmpty())
    {
        OutError = TEXT("Passed必须提供Required Evidence。");
        return false;
    }

    Report->Status = NewStatus;
    Report->Reviewer = Reviewer;
    Report->ReviewDate = FDateTime::UtcNow();
    Report->Comment = Comment;
    Report->Evidence = Evidence;
    return true;
}
