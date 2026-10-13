// 平台Editor人工审核入口：完整合同、实际证据和追加修订，不能自动填写人工通过。
// 限游戏线程；写盘先于内存结论提交，失败保持原始运行结果和历史记录。
#include "Review/GamePlatformReviewTypes.h"
#include "Review/GamePlatformReviewEvidenceValidation.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
/** 稳定FName仅允许ASCII标识，已发布英文身份保留，不执行用户文本。 */
bool IsReviewIdentity(FName Name)
{
    const FString Text = Name.ToString();
    if (Name.IsNone()) return false;
    for (TCHAR C : Text)
        if (!(C >= TEXT('a') && C <= TEXT('z')) && !(C >= TEXT('A') && C <= TEXT('Z')) &&
            !(C >= TEXT('0') && C <= TEXT('9')) && C != TEXT('_') && C != TEXT('.') && C != TEXT('-')) return false;
    return true;
}
/** 已知中立证据类型，大小写仅为旧合同兼容，重复类型不产生额外要求。 */
bool IsReviewEvidenceType(const FString& Type)
{
    const FString Key = Type.ToLower();
    return Key == TEXT("log") || Key == TEXT("screenshot") || Key == TEXT("state_dump") ||
        Key == TEXT("performance") || Key == TEXT("asset_manifest") || Key == TEXT("network_trace");
}
}
bool UGamePlatformReviewCase::IsCaseComplete(FString& Error) const
{
    if (!IsReviewIdentity(CaseId) || !IsReviewIdentity(Category) || CaseVersion < 1)
    { Error = TEXT("用例身份、分类或正版本不完整。"); return false; }
    if (Description.TrimStartAndEnd().IsEmpty() || Steps.IsEmpty() || ExpectedResult.TrimStartAndEnd().IsEmpty() || RequiredEvidence.IsEmpty())
    { Error = TEXT("中文目的、步骤、预期或必需证据缺失。"); return false; }
    for (const auto& Step : Steps) if (Step.TrimStartAndEnd().IsEmpty()) { Error = TEXT("用例步骤含空白。"); return false; }
    for (const auto& Type : RequiredEvidence) if (!IsReviewEvidenceType(Type)) { Error = TEXT("用例证据类型未登记。"); return false; }
    return true;
}
FString UGamePlatformReviewCase::GetContractFingerprint() const { return GamePlatform::Review::ContractFingerprint(*this); }
UGamePlatformReviewReport* UGamePlatformReviewSubsystem::CreatePendingReport(UGamePlatformReviewCase* Case,
    const FString& RunId, const FString& BuildVersion, const FString& ContentRevision)
{
    check(IsInGameThread()); FString Error; FGuid Id;
    if (!Case || !Case->IsCaseComplete(Error) || !FGuid::ParseExact(RunId, EGuidFormats::DigitsWithHyphens, Id) ||
        !Id.IsValid() || RunId != Id.ToString(EGuidFormats::DigitsWithHyphens)) return nullptr;
    const FString Fingerprint = Case->GetContractFingerprint(); if (Fingerprint.IsEmpty()) return nullptr;
    auto* Report = NewObject<UGamePlatformReviewReport>(this);
    Report->RunId = RunId; Report->CaseId = Case->CaseId; Report->BoundCase = Case; Report->CaseVersion = Case->CaseVersion;
    Report->CaseFingerprint = Fingerprint; Report->FeatureId = Case->FeatureId; Report->ScenarioId = Case->ScenarioId;
    Report->BuildVersion = BuildVersion; Report->ContentRevision = ContentRevision;
    return Report;
}
bool UGamePlatformReviewSubsystem::ApplyHumanDecision(UGamePlatformReviewReport* Report, EGamePlatformReviewStatus Status,
    const FString& Reviewer, const FString& Comment, const TArray<FString>& Evidence, bool bConfirmed, FString& Error)
{
    check(IsInGameThread()); Error.Reset();
    if (!Report || !Report->BoundCase) { Error = TEXT("报告没有绑定完整用例，旧无绑定报告不可签收。"); return false; }
    if (!bConfirmed || Reviewer.TrimStartAndEnd().IsEmpty())
    { Error = TEXT("人工结论必须由具名真实审核者显式确认，自动化/AI不能代签。"); return false; }
    if (Status != EGamePlatformReviewStatus::Passed && Status != EGamePlatformReviewStatus::Failed &&
        Status != EGamePlatformReviewStatus::Blocked && Status != EGamePlatformReviewStatus::InProgress)
    { Error = TEXT("人工修订状态不合法。"); return false; }
    if (!Report->BoundCase->IsCaseComplete(Error) || Report->CaseVersion != Report->BoundCase->CaseVersion ||
        Report->CaseFingerprint != Report->BoundCase->GetContractFingerprint())
    { Error = TEXT("当前用例合同已变化，请重新建立当前版本报告。"); return false; }
    if (Status == EGamePlatformReviewStatus::Passed)
    {
        const auto Previous = Report->Evidence;
        Report->Evidence = Evidence;
        const bool bValid = ValidateReviewEvidence(*Report->BoundCase, *Report, Error);
        Report->Evidence = Previous;
        if (!bValid) return false;
    }
    else if (!GamePlatform::Review::ValidateEvidenceReferences(*Report->BoundCase, *Report, Evidence, false, Error)) return false;
    if ((Status == EGamePlatformReviewStatus::Failed || Status == EGamePlatformReviewStatus::Blocked) && Comment.TrimStartAndEnd().IsEmpty())
    { Error = TEXT("失败/阻塞人工结论必须说明原因。"); return false; }
    const int32 Revision = Report->Revisions.Num() + 1;
    const FString Relative = FString::Printf(TEXT("Decisions/%s.%d.json"), *Report->CaseId.ToString(), Revision);
    FString Path;
    if (!GamePlatform::Review::ResolveEvidencePath(Report->RunId, Relative, Path, Error)) return false;
    if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path)))
    { Error = TEXT("无法创建本运行人工修订目录。"); return false; }
    auto Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("schema_version"), 1); Object->SetNumberField(TEXT("revision"), Revision);
    Object->SetStringField(TEXT("run_id"), Report->RunId); Object->SetStringField(TEXT("case_id"), Report->CaseId.ToString());
    Object->SetNumberField(TEXT("case_version"), Report->CaseVersion); Object->SetStringField(TEXT("case_fingerprint"), Report->CaseFingerprint);
    Object->SetNumberField(TEXT("human_status"), static_cast<uint8>(Status)); Object->SetStringField(TEXT("reviewer"), Reviewer.TrimStartAndEnd());
    const auto Date = FDateTime::UtcNow(); Object->SetStringField(TEXT("reviewed_utc"), Date.ToIso8601()); Object->SetStringField(TEXT("comment"), Comment);
    Object->SetStringField(TEXT("source_fingerprint"), Report->SourceFingerprint); Object->SetStringField(TEXT("content_fingerprint"), Report->ContentFingerprint);
    Object->SetStringField(TEXT("build_version"), Report->BuildVersion); Object->SetStringField(TEXT("run_evidence_hash"), Report->RunEvidenceHash);
    if (!Report->Revisions.IsEmpty()) Object->SetStringField(TEXT("supersedes"), Report->Revisions.Last().SavedPath);
    TArray<TSharedPtr<FJsonValue>> References;
    for (const auto& Item : Evidence) References.Add(MakeShared<FJsonValueString>(Item));
    Object->SetArrayField(TEXT("evidence"), References);
    FString Text;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text)) ||
        !FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_NoReplaceExisting))
    { Error = TEXT("人工修订追加失败或同版文件已存在；未覆盖旧结论。"); return false; }
    FGamePlatformReviewRevision Entry;
    Entry.Revision = Revision; Entry.Status = Status; Entry.Reviewer = Reviewer.TrimStartAndEnd(); Entry.Date = Date;
    Entry.Comment = Comment; Entry.Evidence = Evidence; Entry.SavedPath = Path; Report->Revisions.Add(Entry);
    Report->Status = Status; Report->Reviewer = Entry.Reviewer; Report->ReviewDate = Date; Report->Comment = Comment; Report->Evidence = Evidence;
    return true;
}
