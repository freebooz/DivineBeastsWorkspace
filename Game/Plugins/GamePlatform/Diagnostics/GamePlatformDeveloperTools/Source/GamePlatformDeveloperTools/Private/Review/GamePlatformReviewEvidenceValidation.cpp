// 平台Editor证据拒绝路径：固定Run根、实际文件摘要、用例/版本及真实运行证明，全部游戏线程。
// 文件与JSON是证据数据而非命令；未实现链接审计的平台明确拒绝，人工结论不能覆盖原始运行结果。
#include "Review/GamePlatformReviewEvidenceValidation.h"
#include "Review/GamePlatformReviewTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace GamePlatform::Review
{
namespace
{
/** 六种中立证据类型；旧Screenshot大小写兼容，但不能把任意扩展名当类型。 */
bool IsEvidenceType(const FString& Type)
{
    return Type == TEXT("log") || Type == TEXT("screenshot") || Type == TEXT("state_dump") ||
        Type == TEXT("performance") || Type == TEXT("asset_manifest") || Type == TEXT("network_trace");
}
/** 根GUID必须规范，拒绝任意路径/旧Run-1进入新的签收真源。 */
bool IsRunId(const FString& Text)
{
    FGuid Id; return FGuid::ParseExact(Text, EGuidFormats::DigitsWithHyphens, Id) && Id.IsValid() && Text == Id.ToString(EGuidFormats::DigitsWithHyphens);
}
/** 检查实际存在的每个祖先，不允许目录junction把证据读到固定根外。 */
bool IsPlainPath(FString Path)
{
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    while (!Path.IsEmpty())
    {
        if ((Files.FileExists(*Path) || Files.DirectoryExists(*Path)) && Files.IsSymlink(*Path) != ESymlinkResult::NonSymlink) return false;
        const FString Parent = FPaths::GetPath(Path); if (Parent == Path) break; Path = Parent;
    }
    return true;
}
/** 无溢出/脚本的ASCII相对路径；路径合法还需实际文件及哈希检查。 */
bool IsRelativeReference(const FString& Path)
{
    if (Path.IsEmpty() || Path.StartsWith(TEXT("/")) || Path.Contains(TEXT("\\")) || Path.Contains(TEXT(":"))) return false;
    TArray<FString> Parts; Path.ParseIntoArray(Parts, TEXT("/"), false);
    for (const auto& Part : Parts) if (Part.IsEmpty() || Part == TEXT(".") || Part == TEXT("..")) return false;
    for (TCHAR C : Path)
        if (!(C >= TEXT('a') && C <= TEXT('z')) && !(C >= TEXT('A') && C <= TEXT('Z')) && !(C >= TEXT('0') && C <= TEXT('9')) &&
            C != TEXT('_') && C != TEXT('-') && C != TEXT('.') && C != TEXT('/')) return false;
    return true;
}
/** 指纹只接受真实算法形状；unverified只允许留在待审报告，Passed不能消费。 */
bool IsHash(const FString& Text)
{
    if (Text.Len() != 64) return false;
    for (TCHAR C : Text) if (!(C >= TEXT('0') && C <= TEXT('9')) && !(C >= TEXT('a') && C <= TEXT('f'))) return false;
    return true;
}
/** 外部索引最多4MiB，单证据最多64MiB；这是读入防滥用边界，不宣称游戏预算达标。 */
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
    const int64 Size = IFileManager::Get().FileSize(*Path);
    FString Text; return Size > 0 && Size <= 4 * 1024 * 1024 && FFileHelper::LoadFileToString(Text, *Path) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
/** 文本字段必须存在且非空，读取失败返回false而不是触发Json检查断言。 */
bool ReadText(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FString& Out)
{ return Object.IsValid() && Object->TryGetStringField(Name, Out) && !Out.TrimStartAndEnd().IsEmpty(); }
/** 证据身份先检查字符串再构造FName，4MiB文件限制不能保护单个超长名字。 */
bool IsIdentity(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() >= NAME_SIZE || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase)) return false;
    const auto Letter = [](TCHAR C) { return (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')); };
    if (!Letter(Value[0])) return false;
    for (TCHAR C : Value) if (!Letter(C) && !(C >= TEXT('0') && C <= TEXT('9')) && C != TEXT('_') && C != TEXT('.') && C != TEXT('-')) return false;
    return true;
}
/** 冻结合同的有序文本必须逐项一致；步骤/前提不得靠版本号猜测未变。 */
bool SameTexts(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TArray<FString>& Values, bool bLower = false)
{
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (!Object->TryGetArrayField(Field, Array) || Array->Num() != Values.Num()) return false;
    for (int32 I = 0; I < Values.Num(); ++I)
    {
        FString Value;
        if (!(*Array)[I].IsValid() || !(*Array)[I]->TryGetString(Value) ||
            (bLower ? Value.ToLower() != Values[I].ToLower() : Value != Values[I])) return false;
    }
    return true;
}
/** 运行事实必须来自实际状态转储，不接受索引单一布尔。终态允许预期失败/取消，清理必须确认。 */
bool IsCompletedState(const TSharedPtr<FJsonObject>& State, const FString& RunId, const FString& Scenario, const FString& MainFeature)
{
    FString Id, ScenarioId, FeatureId;
    double Phase = 0, Commands = 0, CleanupStatus = 0, TerminalStatus = 0;
    const TSharedPtr<FJsonObject>* Cleanup = nullptr; const TSharedPtr<FJsonObject>* Terminal = nullptr;
    return ReadText(State, TEXT("run_id"), Id) && Id == RunId &&
        ReadText(State, TEXT("scenario_id"), ScenarioId) && ScenarioId == Scenario &&
        ReadText(State, TEXT("feature_id"), FeatureId) && FeatureId == MainFeature &&
        State->TryGetNumberField(TEXT("run_state"), Phase) && (Phase == 4 || Phase == 5 || Phase == 6) &&
        State->TryGetNumberField(TEXT("completed_operations"), Commands) && FMath::IsFinite(Commands) && Commands > 0 && Commands <= 9007199254740991.0 && FMath::FloorToDouble(Commands) == Commands &&
        State->TryGetObjectField(TEXT("cleanup_result"), Cleanup) && (*Cleanup)->TryGetNumberField(TEXT("status"), CleanupStatus) && CleanupStatus == 1 &&
        State->TryGetObjectField(TEXT("terminal_result"), Terminal) && (*Terminal)->TryGetNumberField(TEXT("status"), TerminalStatus) && TerminalStatus >= 1 && TerminalStatus <= 3;
}
}
bool ResolveEvidencePath(const FString& RunId, const FString& Relative, FString& Out, FString& Error)
{
    Out.Reset();
    if (!IsRunId(RunId) || !IsRelativeReference(Relative)) { Error = TEXT("运行GUID或证据相对路径不合法。"); return false; }
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GamePlatformValidation"), RunId, TEXT("Review")));
    const FString Path = FPaths::Combine(Root, Relative);
    if (!IsPlainPath(Path)) { Error = TEXT("证据路径含符号链接/junction或平台不能审计链接。"); return false; }
    Out = Path; return true;
}
FString HashEvidenceFile(const FString& Path)
{
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    const int64 Size = Files.FileSize(*Path); const auto Time = Files.GetTimeStamp(*Path);
    if (Size <= 0 || Size > 64 * 1024 * 1024 || !IsPlainPath(Path)) return FString();
    TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != Size ||
        Files.FileSize(*Path) != Size || Files.GetTimeStamp(*Path) != Time) return FString();
    FSHA256Signature Signature;
    return FPlatformMisc::GetSHA256Signature(Bytes.GetData(), Bytes.Num(), Signature) ? Signature.ToString().ToLower() : FString();
}
FString ContractFingerprint(const UGamePlatformReviewCase& Case)
{
    FString Canonical;
    const auto Add = [&Canonical](const FString& Value) { Canonical += FString::Printf(TEXT("%d:"), Value.Len()) + Value; };
    Add(Case.CaseId.ToString()); Add(LexToString(Case.CaseVersion)); Add(Case.FeatureId.ToString()); Add(Case.ScenarioId.ToString());
    Add(Case.Category.ToString()); Add(Case.Description); Add(LexToString(Case.Preconditions.Num()));
    for (const auto& Value : Case.Preconditions) Add(Value);
    Add(LexToString(Case.Steps.Num())); for (const auto& Value : Case.Steps) Add(Value);
    Add(Case.ExpectedResult); Add(LexToString(Case.RequiredEvidence.Num())); for (const auto& Value : Case.RequiredEvidence) Add(Value.ToLower());
    FTCHARToUTF8 Utf8(*Canonical); FSHA256Signature Signature;
    return FPlatformMisc::GetSHA256Signature(Utf8.Get(), Utf8.Length(), Signature) ? Signature.ToString().ToLower() : FString();
}
bool ValidateEvidenceReferences(const UGamePlatformReviewCase& Case, const UGamePlatformReviewReport& Report,
    const TArray<FString>& References, bool bRequireAll, FString& Error)
{
    TSet<FString> Types, Seen;
    for (const auto& Reference : References)
    {
        FString Type, Relative, Path;
        if (!Reference.Split(TEXT(":"), &Type, &Relative) || !IsEvidenceType(Type.ToLower()) ||
            !ResolveEvidencePath(Report.RunId, Relative, Path, Error)) { Error = TEXT("证据必须为已知type:仓库规范相对路径。"); return false; }
        Type = Type.ToLower(); const FString Key = Type + TEXT(":") + Relative;
        if (Seen.Contains(Key.ToLower())) { Error = TEXT("证据引用重复。"); return false; } Seen.Add(Key.ToLower());
        const FString Hash = HashEvidenceFile(Path);
        if (Hash.IsEmpty()) { Error = TEXT("实际证据缺失、为空、超限、变化或SHA256不可用。"); return false; }
        if (const FString* Imported = Report.ImportedEvidenceHashes.Find(Key); Imported && *Imported != Hash)
        { Error = TEXT("证据文件自导入后发生变化。"); return false; }
        if (Type == TEXT("screenshot"))
        {
            TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 8 ||
                Bytes[0] != 137 || Bytes[1] != 'P' || Bytes[2] != 'N' || Bytes[3] != 'G' || Bytes[4] != 13 || Bytes[5] != 10 || Bytes[6] != 26 || Bytes[7] != 10)
            { Error = TEXT("截图不是有效PNG证据，禁止文本改扩展名占位。"); return false; }
        }
        if (Type == TEXT("state_dump") || Type == TEXT("asset_manifest") || Type == TEXT("performance"))
        { TSharedPtr<FJsonObject> Object; if (!ReadJson(Path, Object)) { Error = TEXT("结构化证据不是完整JSON对象。"); return false; } }
        Types.Add(Type);
    }
    if (bRequireAll) for (const auto& Required : Case.RequiredEvidence)
        if (!Types.Contains(Required.ToLower())) { Error = TEXT("缺少必需证据类型：") + Required; return false; }
    return true;
}
bool ValidateEvidence(const UGamePlatformReviewCase& Case, const UGamePlatformReviewReport& Report, FString& Error)
{
    if (!Case.IsCaseComplete(Error) || Report.BoundCase != &Case || Report.CaseId != Case.CaseId ||
        Report.CaseVersion != Case.CaseVersion || Report.CaseFingerprint != ContractFingerprint(Case))
    { Error = TEXT("用例合同/版本与待审报告不匹配。"); return false; }
    if (!Report.bRuntimeCompleted || !Report.bBinaryVerified || !IsHash(Report.SourceFingerprint) || !IsHash(Report.ContentFingerprint) ||
        Report.SourceFingerprint != Report.CurrentSourceFingerprint || Report.ContentFingerprint != Report.CurrentContentFingerprint)
    { Error = TEXT("缺当前版本实际终态、匹配二进制或源/内容指纹证明。"); return false; }
    FString IndexPath;
    if (!ResolveEvidencePath(Report.RunId, Report.RunEvidenceIndex, IndexPath, Error) ||
        Report.RunEvidenceHash.IsEmpty() || HashEvidenceFile(IndexPath) != Report.RunEvidenceHash)
    { Error = TEXT("运行索引缺失或导入后已变化。"); return false; }
    FString ProofPath;
    if (!ResolveEvidencePath(Report.RunId, Report.BinaryVerificationPath, ProofPath, Error) ||
        Report.BinaryVerificationHash.IsEmpty() || HashEvidenceFile(ProofPath) != Report.BinaryVerificationHash)
    { Error = TEXT("独立二进制证明缺失或导入后已变化。"); return false; }
    // 原始索引/状态/证明必须全部重读，即使人工提交只选必需类型，不能绕过已导入事实。
    for (const auto& Pair : Report.ImportedEvidenceHashes)
    {
        FString Type, Relative, Path;
        if (!Pair.Key.Split(TEXT(":"), &Type, &Relative) || !ResolveEvidencePath(Report.RunId, Relative, Path, Error) || HashEvidenceFile(Path) != Pair.Value)
        { Error = TEXT("运行冻结证据自导入后已变化。"); return false; }
    }
    return ValidateEvidenceReferences(Case, Report, Report.Evidence, true, Error);
}
}

bool UGamePlatformReviewSubsystem::BindRunEvidence(UGamePlatformReviewReport* Report, const FString& InputPath,
    const FString& CurrentSource, const FString& CurrentContent, FString& Error)
{
    check(IsInGameThread());
    if (!Report || !Report->BoundCase) { Error = TEXT("待审报告没有绑定完整用例。"); return false; }
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GamePlatformValidation"), Report->RunId, TEXT("Review"))) + TEXT("/");
    FString Relative = InputPath;
    if (!FPaths::IsRelative(Relative))
    {
        Relative = FPaths::ConvertRelativePathToFull(Relative); FPaths::MakeStandardFilename(Relative);
        if (!Relative.StartsWith(Root, ESearchCase::IgnoreCase)) { Error = TEXT("索引不属于本RunId固定证据根。"); return false; }
        Relative.RightChopInline(Root.Len());
    }
    FString Path;
    if (!GamePlatform::Review::ResolveEvidencePath(Report->RunId, Relative, Path, Error)) return false;
    TSharedPtr<FJsonObject> Index;
    const int64 Size = IFileManager::Get().FileSize(*Path); FString Text;
    if (Size <= 0 || Size > 4 * 1024 * 1024 || !FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Index) || !Index.IsValid())
    { Error = TEXT("运行索引缺失、超限或JSON不完整。"); return false; }
    double Schema = 0; FString RunId, Source, Content, Scenario, Feature, Build;
    bool bCompleted = false, bVerified = false;
    if (!Index->TryGetNumberField(TEXT("schema_version"), Schema) || Schema != 1 ||
        !Index->TryGetStringField(TEXT("run_id"), RunId) || RunId != Report->RunId ||
        !Index->TryGetStringField(TEXT("source_fingerprint"), Source) || !Index->TryGetStringField(TEXT("content_fingerprint"), Content) ||
        !Index->TryGetStringField(TEXT("scenario_id"), Scenario) || !Index->TryGetStringField(TEXT("feature_id"), Feature) ||
        !Index->TryGetStringField(TEXT("build_version"), Build) || !Index->TryGetBoolField(TEXT("runtime_completed"), bCompleted) ||
        !Index->TryGetBoolField(TEXT("binary_verified"), bVerified))
    { Error = TEXT("运行索引必需身份/版本/实际状态不完整。"); return false; }
    if (!GamePlatform::Review::IsIdentity(Scenario) || !GamePlatform::Review::IsIdentity(Feature) || FName(*Scenario) != Report->BoundCase->ScenarioId)
    { Error = TEXT("运行身份非法或案例与用例不匹配。"); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr; bool bCaseMatched = false;
    if (!Index->TryGetArrayField(TEXT("review_cases"), Cases)) { Error = TEXT("运行缺独立用例列表。"); return false; }
    for (const auto& Value : *Cases)
    {
        const auto Case = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        FString Id, CaseFeature, CaseScenario, Category, Description, Expected; double Version = 0;
        if (!Case || !GamePlatform::Review::ReadText(Case, TEXT("review_case_id"), Id) || !GamePlatform::Review::IsIdentity(Id))
        { Error = TEXT("运行用例身份非法。"); return false; }
        if (FName(*Id) != Report->CaseId) continue;
        // 核对该独立用例的功能，而不是强制关联功能等于主功能；拒绝重复匹配或未递增版本的合同变化。
        const auto& Current = *Report->BoundCase;
        if (bCaseMatched || !Case->TryGetNumberField(TEXT("version"), Version) || Version != Report->CaseVersion ||
            !GamePlatform::Review::ReadText(Case, TEXT("feature_id"), CaseFeature) || !GamePlatform::Review::IsIdentity(CaseFeature) || FName(*CaseFeature) != Current.FeatureId ||
            !GamePlatform::Review::ReadText(Case, TEXT("scenario_id"), CaseScenario) || CaseScenario != Scenario ||
            !GamePlatform::Review::ReadText(Case, TEXT("category"), Category) || Category != Current.Category.ToString() ||
            !GamePlatform::Review::ReadText(Case, TEXT("description"), Description) || Description != Current.Description ||
            !GamePlatform::Review::ReadText(Case, TEXT("expected_result"), Expected) || Expected != Current.ExpectedResult ||
            !GamePlatform::Review::SameTexts(Case, TEXT("steps"), Current.Steps) || !GamePlatform::Review::SameTexts(Case, TEXT("preconditions"), Current.Preconditions) ||
            !GamePlatform::Review::SameTexts(Case, TEXT("required_evidence"), Current.RequiredEvidence, true))
        { Error = TEXT("独立用例的运行冻结合同与当前合同不匹配或重复。"); return false; }
        bCaseMatched = true;
    }
    if (!bCaseMatched) { Error = TEXT("运行没有本用例的匹配版本。"); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr; TArray<FString> References; TMap<FString, FString> Hashes;
    if (!Index->TryGetArrayField(TEXT("evidence"), Entries)) { Error = TEXT("运行缺真实证据引用。"); return false; }
    bool bStateCompleted = false; int32 StateCount = 0;
    for (const auto& Value : *Entries)
    {
        const auto Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr; FString Type, File, Hash, FilePath;
        if (!Entry || !Entry->TryGetStringField(TEXT("type"), Type) || !Entry->TryGetStringField(TEXT("path"), File) || !Entry->TryGetStringField(TEXT("sha256"), Hash) ||
            !GamePlatform::Review::IsEvidenceType(Type.ToLower()) || !GamePlatform::Review::IsHash(Hash) ||
            !GamePlatform::Review::ResolveEvidencePath(Report->RunId, File, FilePath, Error) || GamePlatform::Review::HashEvidenceFile(FilePath) != Hash)
        { Error = TEXT("证据引用或实际SHA256不匹配。"); return false; }
        const FString Key = Type.ToLower() + TEXT(":") + File;
        if (Hashes.Contains(Key)) { Error = TEXT("重复证据引用。"); return false; } References.Add(Key); Hashes.Add(Key, Hash);
        if (Type == TEXT("state_dump"))
        {
            TSharedPtr<FJsonObject> State;
            if (++StateCount != 1 || !GamePlatform::Review::ReadJson(FilePath, State)) { Error = TEXT("原始状态转储缺失/重复/无效。"); return false; }
            bStateCompleted = GamePlatform::Review::IsCompletedState(State, RunId, Scenario, Feature);
        }
    }
    if (StateCount != 1 || bCompleted != bStateCompleted) { Error = TEXT("实跑声明不等于本Run原始终态/命令数/清理事实。"); return false; }
    const FString IndexHash = GamePlatform::Review::HashEvidenceFile(Path);
    if (IndexHash.IsEmpty()) { Error = TEXT("运行索引实际摘要不可读。"); return false; }
    // binary_verified不能仅由客户端布尔字段决定；需要后续无构建运行器生成的独立验证证明。
    FString Verification, ProofHash;
    if (bVerified)
    {
        FString VerificationPath; TSharedPtr<FJsonObject> Proof; FString ProofText;
        if (!Index->TryGetStringField(TEXT("binary_verification"), Verification) ||
            !GamePlatform::Review::ResolveEvidencePath(Report->RunId, Verification, VerificationPath, Error) ||
            IFileManager::Get().FileSize(*VerificationPath) > 4 * 1024 * 1024 || !FFileHelper::LoadFileToString(ProofText, *VerificationPath) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProofText), Proof) || !Proof.IsValid())
        { Error = TEXT("客户端标记已验证，但缺独立二进制验证证明。"); return false; }
        bool bPassed = false; FString Kind, ProofSource, ProofContent, Artifact, ProofRun, RawIndex, RawHash, RawPath;
        const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
        if (!Proof->TryGetBoolField(TEXT("passed"), bPassed) || !bPassed || !Proof->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("PluginDemoBinaryVerification") ||
            !Proof->TryGetStringField(TEXT("source_fingerprint"), ProofSource) || ProofSource != Source ||
            !Proof->TryGetStringField(TEXT("content_fingerprint"), ProofContent) || ProofContent != Content ||
            !Proof->TryGetStringField(TEXT("artifact_sha256"), Artifact) || Artifact.ToLower() != Build.ToLower() ||
            !Proof->TryGetStringField(TEXT("run_id"), ProofRun) || ProofRun != RunId ||
            !Proof->TryGetStringField(TEXT("raw_index_path"), RawIndex) || !Proof->TryGetStringField(TEXT("raw_index_sha256"), RawHash) ||
            !GamePlatform::Review::ResolveEvidencePath(RunId, RawIndex, RawPath, Error) || GamePlatform::Review::HashEvidenceFile(RawPath) != RawHash ||
            !Proof->TryGetArrayField(TEXT("tests"), Tests) || Tests->IsEmpty())
        { Error = TEXT("独立验证证明的源/内容/产物或真实测试集合不匹配。"); return false; }
        TSet<FString> TestNames;
        for (const auto& TestValue : *Tests)
        {
            const auto Test = TestValue.IsValid() && TestValue->Type == EJson::Object ? TestValue->AsObject() : nullptr; FString State, Name;
            if (!Test || !Test->TryGetStringField(TEXT("state"), State) || State != TEXT("Success") || !Test->TryGetStringField(TEXT("fullTestPath"), Name) || TestNames.Contains(Name))
            { Error = TEXT("二进制验证存在缺失/失败原生测试。"); return false; }
            TestNames.Add(Name);
        }
        for (const TCHAR* Name : {TEXT("Definition"), TEXT("Catalog"), TEXT("Lifecycle"), TEXT("Menu"), TEXT("BirdScenario"), TEXT("Samples"), TEXT("Evidence"), TEXT("LaunchPolicy")})
            if (!TestNames.Contains(FString(TEXT("DivineBeasts.PluginDemos.")) + Name)) { Error = TEXT("独立证明缺当前演示必需测试。"); return false; }
        ProofHash = GamePlatform::Review::HashEvidenceFile(VerificationPath);
        if (ProofHash.IsEmpty()) { Error = TEXT("独立证明摘要不可读。"); return false; }
        Hashes.Add(TEXT("raw_index:") + RawIndex, RawHash);
    }
    Report->SourceFingerprint = Source; Report->ContentFingerprint = Content; Report->BuildVersion = Build;
    Report->FeatureId = Report->BoundCase->FeatureId; Report->ScenarioId = FName(*Scenario); Report->bRuntimeCompleted = bStateCompleted; Report->bBinaryVerified = bVerified;
    Report->BinaryVerificationPath = Verification; Report->BinaryVerificationHash = ProofHash;
    Report->CurrentSourceFingerprint = CurrentSource; Report->CurrentContentFingerprint = CurrentContent;
    Report->RunEvidenceIndex = Relative; Report->RunEvidenceHash = IndexHash; Report->ImportedEvidenceHashes = MoveTemp(Hashes); Report->Evidence = MoveTemp(References);
    return true;
}
bool UGamePlatformReviewSubsystem::ValidateReviewEvidence(const UGamePlatformReviewCase& Case, const UGamePlatformReviewReport& Report, FString& Error) const
{ return GamePlatform::Review::ValidateEvidence(Case, Report, Error); }
