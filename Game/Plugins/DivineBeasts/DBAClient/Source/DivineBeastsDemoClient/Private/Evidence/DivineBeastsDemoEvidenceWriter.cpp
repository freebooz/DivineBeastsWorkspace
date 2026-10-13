// SSLM脱敏证据追加实现：固定Game/Saved/GamePlatformValidation/<RunId>/Review，拒绝链接路径和覆盖。
// 只保存实际状态/码和版本合同；没有新二进制证明时binary_verified=false，不能签收通过。
#include "Evidence/DivineBeastsDemoEvidenceWriter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "HAL/FileManager.h"
#include "Containers/UnrealString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"

namespace DivineBeasts::Demo
{
namespace
{
/** 源指纹必须是作者原始字节的规范小写SHA256；只满足长度不能冒充可追溯身份。 */
bool IsSourceFingerprint(const FString& Fingerprint)
{
    if (Fingerprint.Len() != 64) return false;
    for (TCHAR C : Fingerprint)
        if (!(C >= TEXT('0') && C <= TEXT('9')) && !(C >= TEXT('a') && C <= TEXT('f'))) return false;
    return true;
}
/** 不写外部诊断正文，避免服务错误中的账号/URL/令牌进入持久证据；原始状态与码保留。 */
TSharedRef<FJsonObject> ResultObject(const FGamePlatformResult& Result)
{
    auto Object = MakeShared<FJsonObject>(); Object->SetNumberField(TEXT("status"), static_cast<uint8>(Result.Status));
    Object->SetStringField(TEXT("code"), Result.Code.ToString());
    Object->SetStringField(TEXT("message"), TEXT("持久证据只保留原始状态和错误码；动态诊断正文已省略。"));
    return Object;
}
/** 从目标目录逐级检查真实文件系统；链接检查未实现时明确失败，不猜路径安全。 */
bool IsPlainPath(const FString& Path)
{
    auto& Files = FPlatformFileManager::Get().GetPlatformFile(); FString Current = Path;
    while (!Current.IsEmpty())
    {
        if ((Files.DirectoryExists(*Current) || Files.FileExists(*Current)) && Files.IsSymlink(*Current) != ESymlinkResult::NonSymlink) return false;
        const FString Parent = FPaths::GetPath(Current); if (Parent == Current) break; Current = Parent;
    }
    return true;
}
/** 同目录追加唯一文件；NoReplace防止竞争覆盖。索引只有全部本次文件写完才产生。 */
bool SaveJson(const TSharedRef<FJsonObject>& Object, const FString& Path)
{
    FString Text; if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text))) return false;
    return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(), FILEWRITE_NoReplaceExisting);
}
/** 哈希固定小型生成文件；SHA256不可用时失败，不伪造摘要。 */
FString FileHash(const FString& Path)
{
    TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return FString();
    FSHA256Signature Signature;
    return FPlatformMisc::GetSHA256Signature(Bytes.GetData(), Bytes.Num(), Signature) ? Signature.ToString().ToLower() : FString();
}
}
bool WriteRunEvidence(const FDivineBeastsDemoRunEvidence& Input, FString& OutIndex, FString& OutError)
{
    check(IsInGameThread()); OutIndex.Reset(); OutError.Reset();
    if (!Input.Run.Token.IsValid() || Input.Run.ScenarioId != Input.Scenario.ScenarioId ||
        !IsSourceFingerprint(Input.SourceFingerprint) || Input.ReviewCases.IsEmpty() ||
        Input.Run.CompletedOperations < 0 || Input.Run.CompletedOperations > 9007199254740991LL)
    { OutError = TEXT("运行身份、源指纹或独立用例合同缺失，不能写待审证据。"); return false; }
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GamePlatformValidation"), Input.Run.Token.RunId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("Review")));
    if (!IsPlainPath(Root) || !Files.CreateDirectoryTree(*Root)) { OutError = TEXT("证据目录含链接、不能审计或创建失败。"); return false; }
    const FString Revision = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString StateName = TEXT("RunState.") + Revision + TEXT(".json"), LogName = TEXT("RunEvents.") + Revision + TEXT(".log");
    // 日志文件路径必须与引擎的全局LogPath日志类别区分；文件身份和追加语义保持原合同。
    const FString StatePath = FPaths::Combine(Root, StateName), EventLogPath = FPaths::Combine(Root, LogName);
    auto State = MakeShared<FJsonObject>();
    State->SetStringField(TEXT("run_id"), Input.Run.Token.RunId.ToString(EGuidFormats::DigitsWithHyphens));
    State->SetStringField(TEXT("world_id"), Input.Run.Token.WorldId.ToString(EGuidFormats::DigitsWithHyphens));
    State->SetStringField(TEXT("run_generation"), LexToString(Input.Run.Token.Generation));
    State->SetStringField(TEXT("scenario_id"), Input.Run.ScenarioId.ToString()); State->SetStringField(TEXT("feature_id"), Input.Run.FeatureId.ToString());
    State->SetNumberField(TEXT("run_state"), static_cast<uint8>(Input.Run.State)); State->SetNumberField(TEXT("completed_operations"), Input.Run.CompletedOperations);
    State->SetObjectField(TEXT("operation_result"), ResultObject(Input.Run.OperationResult));
    State->SetObjectField(TEXT("cleanup_result"), ResultObject(Input.Run.CleanupResult)); State->SetObjectField(TEXT("terminal_result"), ResultObject(Input.Run.TerminalResult));
    if (!SaveJson(State, StatePath)) { OutError = TEXT("状态证据追加失败；已有文件未覆盖。"); return false; }
    FString Events = TEXT("SSLM实际演示事件；不含用户输入、外部诊断正文或人工结论。\n");
    for (const auto& Event : Input.EventRecords) Events += Event + TEXT("\n");
    if (!FFileHelper::SaveStringToFile(Events, *EventLogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_NoReplaceExisting))
    { OutError = TEXT("事件日志追加失败，状态文件保留用于诊断，本次索引未发布。"); return false; }
    auto Index = MakeShared<FJsonObject>(); Index->SetNumberField(TEXT("schema_version"), 1);
    Index->SetStringField(TEXT("run_id"), Input.Run.Token.RunId.ToString(EGuidFormats::DigitsWithHyphens));
    Index->SetStringField(TEXT("scenario_id"), Input.Run.ScenarioId.ToString()); Index->SetStringField(TEXT("feature_id"), Input.Run.FeatureId.ToString());
    Index->SetStringField(TEXT("source_fingerprint"), Input.SourceFingerprint); Index->SetStringField(TEXT("content_fingerprint"), TEXT("unverified"));
    Index->SetStringField(TEXT("build_version"), TEXT("unverified")); Index->SetBoolField(TEXT("binary_verified"), false);
    Index->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString()); Index->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
    Index->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId()); Index->SetStringField(TEXT("recorded_utc"), FDateTime::UtcNow().ToIso8601());
    const bool bTerminal = Input.Run.State == EDivineBeastsDemoRunState::Completed || Input.Run.State == EDivineBeastsDemoRunState::Failed || Input.Run.State == EDivineBeastsDemoRunState::Cancelled;
    Index->SetBoolField(TEXT("runtime_completed"), bTerminal && Input.Run.CompletedOperations > 0 && Input.Run.CleanupResult.IsSuccess());
    Index->SetObjectField(TEXT("raw_state"), State);
    TArray<TSharedPtr<FJsonValue>> Cases;
    for (const auto& Case : Input.ReviewCases)
    {
        const auto* Feature = Input.Features.FindByPredicate([&Case](const auto& F) { return F.FeatureId == Case.FeatureId; });
        if (!Feature || Feature->Purpose.TrimStartAndEnd().IsEmpty()) { OutError = TEXT("用例缺运行时冻结的功能目的，不能用当前资产替代历史合同。"); return false; }
        auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("review_case_id"), Case.ReviewCaseId.ToString());
        Entry->SetNumberField(TEXT("version"), Case.Version); Entry->SetStringField(TEXT("feature_id"), Case.FeatureId.ToString());
        Entry->SetStringField(TEXT("scenario_id"), Case.ScenarioId.ToString()); TArray<TSharedPtr<FJsonValue>> Required;
        for (FName Type : Case.RequiredEvidence) Required.Add(MakeShared<FJsonValueString>(Type.ToString()));
        Entry->SetArrayField(TEXT("required_evidence"), Required);
        Entry->SetStringField(TEXT("category"), TEXT("PluginDemo")); Entry->SetStringField(TEXT("description"), Feature->Purpose);
        Entry->SetStringField(TEXT("expected_result"), Case.ExpectedResult);
        TArray<TSharedPtr<FJsonValue>> Steps, Preconditions;
        for (const auto& Step : Case.Steps) Steps.Add(MakeShared<FJsonValueString>(Step));
        for (const auto& Condition : Input.Scenario.Preconditions) Preconditions.Add(MakeShared<FJsonValueString>(Condition));
        Entry->SetArrayField(TEXT("steps"), Steps); Entry->SetArrayField(TEXT("preconditions"), Preconditions);
        Cases.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Index->SetArrayField(TEXT("review_cases"), Cases); TArray<TSharedPtr<FJsonValue>> Evidence;
    for (const auto& Pair : TArray<TPair<FString, FString>>{{TEXT("state_dump"), StateName}, {TEXT("log"), LogName}})
    {
        auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("type"), Pair.Key); Entry->SetStringField(TEXT("path"), Pair.Value);
        const FString Hash = FileHash(FPaths::Combine(Root, Pair.Value));
        if (Hash.Len() != 64) { OutError = TEXT("实际证据SHA256计算失败，本次索引未发布。"); return false; }
        Entry->SetStringField(TEXT("sha256"), Hash); Evidence.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Index->SetArrayField(TEXT("evidence"), Evidence);
    const FString Path = FPaths::Combine(Root, TEXT("RunEvidence.") + Revision + TEXT(".json"));
    if (!SaveJson(Index, Path)) { OutError = TEXT("待审索引追加失败，既有证据保留。"); return false; }
    OutIndex = Path; return true;
}
}
