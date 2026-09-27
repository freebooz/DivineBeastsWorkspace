#include "Validation/GamePlatformValidationService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/GamePlatformValidationSettings.h"

TArray<FGamePlatformValidationRule> FGamePlatformValidationService::Rules;
TMap<FName, TArray<FGamePlatformValidationRule>> FGamePlatformValidationService::ExtensionRules;
TArray<FGamePlatformValidationAllowlistEntry> FGamePlatformValidationService::AllowlistEntries;
bool FGamePlatformValidationService::bAllowlistLoaded = false;

namespace
{
    FGamePlatformValidationRule MakeRule(
        const TCHAR* Id,
        const TCHAR* Category,
        EGamePlatformValidationSeverity Severity,
        const TCHAR* Description,
        const TCHAR* Rationale,
        const TCHAR* Scope,
        bool bBlocking = true)
    {
        FGamePlatformValidationRule Rule;
        Rule.RuleId = FName(Id);
        Rule.Category = FName(Category);
        Rule.Severity = Severity;
        Rule.Description = Description;
        Rule.Rationale = Rationale;
        Rule.Scope = Scope;
        Rule.bCIBlocking = bBlocking;
        Rule.AutoFixPolicy = EGamePlatformAutoFixPolicy::Disabled;
        Rule.Owner = TEXT("GamePlatformDeveloperTools（游戏平台开发者工具）");
        return Rule;
    }

    FString StatusToString(EGamePlatformValidationStatus Status)
    {
        switch (Status)
        {
        case EGamePlatformValidationStatus::Passed: return TEXT("通过");
        case EGamePlatformValidationStatus::Failed: return TEXT("失败");
        case EGamePlatformValidationStatus::Warning: return TEXT("警告");
        case EGamePlatformValidationStatus::Skipped: return TEXT("跳过");
        default: return TEXT("未执行");
        }
    }
}

void FGamePlatformValidationService::RegisterBuiltInRules()
{
    if (!Rules.IsEmpty())
    {
        return;
    }

    Rules =
    {
        MakeRule(TEXT("GP.Naming"), TEXT("Naming"), EGamePlatformValidationSeverity::Error, TEXT("平台/项目资产与代码命名必须符合约定。"), TEXT("稳定命名降低跨插件复用和资产定位成本。"), TEXT("Editor assets + source metadata")),
        MakeRule(TEXT("GP.Dependency"), TEXT("Architecture"), EGamePlatformValidationSeverity::Error, TEXT("三层依赖只允许向基础层方向且禁止循环。"), TEXT("防止GamePlatform反向依赖MobaCommon/DivineBeasts。"), TEXT("Build.cs + .uplugin + module graph")),
        MakeRule(TEXT("GP.InheritanceBoundary"), TEXT("Architecture"), EGamePlatformValidationSeverity::Error, TEXT("三层类型继承与Public API必须只向基础层扩展。"), TEXT("防止GamePlatform/MobaCommon公开类型被项目层反向污染。"), TEXT("C++ Public headers + type graph; assets via DataValidation")),
        MakeRule(TEXT("GP.ClientLeak"), TEXT("Cook"), EGamePlatformValidationSeverity::Error, TEXT("Client构建/Cook不得包含ServerOnly内容。"), TEXT("端侧隔离与秘密最小暴露。"), TEXT("real client receipt/cook manifest")),
        MakeRule(TEXT("GP.ServerLeak"), TEXT("Cook"), EGamePlatformValidationSeverity::Error, TEXT("Server构建/Cook不得包含无必要客户端表现内容。"), TEXT("降低Dedicated Server包体与加载开销。"), TEXT("real server receipt/cook manifest")),
        MakeRule(TEXT("GP.Content"), TEXT("Content"), EGamePlatformValidationSeverity::Error, TEXT("资产引用、路径、Primary Asset、Chunk与开发内容必须有效。"), TEXT("保证Cook可重复且资源边界清晰。"), TEXT("Asset Registry/Asset Manager")),
        MakeRule(TEXT("GP.Definition"), TEXT("Definition"), EGamePlatformValidationSeverity::Error, TEXT("Definition必须具有稳定ID、版本与有效依赖。"), TEXT("Definition是跨玩法数据契约。"), TEXT("Definition assets")),
        MakeRule(TEXT("GP.StableId"), TEXT("Identity"), EGamePlatformValidationSeverity::Error, TEXT("稳定ID不能为空、重复或使用非法格式。"), TEXT("防止存档、网络与后端契约漂移。"), TEXT("definitions and stable ids")),
        MakeRule(TEXT("GP.GameplayTag"), TEXT("GameplayTag"), EGamePlatformValidationSeverity::Error, TEXT("玩法标签必须有效且不得恢复已取消旧系统标签。"), TEXT("保持现行玩法语义一致。"), TEXT("GameplayTags")),
        MakeRule(TEXT("GP.ServerAssetSafety"), TEXT("ServerSafety"), EGamePlatformValidationSeverity::Error, TEXT("Server-safe资产不得硬依赖Client-only表现资源。"), TEXT("保证专用服务器资源隔离。"), TEXT("Asset Registry dependency graph")),
        MakeRule(TEXT("GP.RPCAuthority"), TEXT("Network"), EGamePlatformValidationSeverity::Warning, TEXT("RPC/Authority静态门禁检查命名、频率风险与Debug Shipping gate。"), TEXT("静态分析不能证明授权正确，仍需Integration/Security tests。"), TEXT("source scan"), false),
        MakeRule(TEXT("GP.SecretScan"), TEXT("Security"), EGamePlatformValidationSeverity::Error, TEXT("客户端/Shipping工件不得携带Secret。"), TEXT("避免密钥与生产配置泄漏。"), TEXT("source + packaged artifacts")),
        MakeRule(TEXT("GP.ManualReview"), TEXT("Review"), EGamePlatformValidationSeverity::Error, TEXT("人工验收必须有真实Reviewer与Evidence。"), TEXT("AI/Codex不能代签人工结论。"), TEXT("review report")),
        MakeRule(TEXT("GP.Performance"), TEXT("Performance"), EGamePlatformValidationSeverity::Warning, TEXT("性能结果只和兼容硬件/场景基线比较。"), TEXT("避免错误回归结论。"), TEXT("performance profile"), false)
    };
    FString AllowlistError;
    if (!ReloadAllowlist(AllowlistError) && !AllowlistError.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("DeveloperTools Allowlist加载失败：%s"), *AllowlistError);
    }
}

void FGamePlatformValidationService::ResetForShutdown()
{
    Rules.Reset();
    ExtensionRules.Reset();
    AllowlistEntries.Reset();
    bAllowlistLoaded = false;
}

const TArray<FGamePlatformValidationRule>& FGamePlatformValidationService::GetRules()
{
    RegisterBuiltInRules();
    return Rules;
}

const FGamePlatformValidationRule* FGamePlatformValidationService::FindRule(FName RuleId)
{
    if (const FGamePlatformValidationRule* BuiltIn =
        GetRules().FindByPredicate([RuleId](const FGamePlatformValidationRule& Rule)
        {
            return Rule.RuleId == RuleId;
        }))
    {
        return BuiltIn;
    }

    for (const TPair<FName, TArray<FGamePlatformValidationRule>>& Pair : ExtensionRules)
    {
        if (const FGamePlatformValidationRule* Extension =
            Pair.Value.FindByPredicate([RuleId](const FGamePlatformValidationRule& Rule)
            {
                return Rule.RuleId == RuleId;
            }))
        {
            return Extension;
        }
    }

    return nullptr;
}

bool FGamePlatformValidationService::RegisterRuleProvider(
    const TSharedRef<IGamePlatformValidationRuleProvider>& Provider,
    FString& OutError)
{
    const FName ProviderId = Provider->GetProviderId();
    if (ProviderId.IsNone())
    {
        OutError = TEXT("Rule Provider的ProviderId不能为空。");
        return false;
    }

    if (ExtensionRules.Contains(ProviderId))
    {
        OutError = FString::Printf(
            TEXT("Rule Provider重复注册：%s"),
            *ProviderId.ToString());
        return false;
    }

    TArray<FGamePlatformValidationRule> ProviderRules;
    Provider->BuildRules(ProviderRules);

    TSet<FName> SeenRuleIds;
    for (const FGamePlatformValidationRule& Rule : ProviderRules)
    {
        if (Rule.RuleId.IsNone())
        {
            OutError = TEXT("扩展规则RuleId不能为空。");
            return false;
        }

        if (SeenRuleIds.Contains(Rule.RuleId) || FindRule(Rule.RuleId) != nullptr)
        {
            OutError = FString::Printf(
                TEXT("扩展规则RuleId重复：%s"),
                *Rule.RuleId.ToString());
            return false;
        }

        SeenRuleIds.Add(Rule.RuleId);
    }

    ExtensionRules.Add(ProviderId, MoveTemp(ProviderRules));
    return true;
}

void FGamePlatformValidationService::UnregisterRuleProvider(FName ProviderId)
{
    ExtensionRules.Remove(ProviderId);
}

TArray<FGamePlatformValidationRule>
FGamePlatformValidationService::GetAllRulesSnapshot()
{
    TArray<FGamePlatformValidationRule> Snapshot = GetRules();
    for (const TPair<FName, TArray<FGamePlatformValidationRule>>& Pair : ExtensionRules)
    {
        Snapshot.Append(Pair.Value);
    }
    return Snapshot;
}

bool FGamePlatformValidationService::ShouldBlockResult(
    const FGamePlatformValidationResult& Result,
    FName GateStage)
{
    if (Result.Status == EGamePlatformValidationStatus::Passed
        || Result.Status == EGamePlatformValidationStatus::Skipped)
    {
        return false;
    }

    const FGamePlatformValidationRule* Rule = FindRule(Result.RuleId);
    if (Rule && !Rule->bEnabled)
    {
        return false;
    }

    if (Result.Status == EGamePlatformValidationStatus::Warning)
    {
        const UGamePlatformValidationSettings* Settings =
            GetDefault<UGamePlatformValidationSettings>();

        if (GateStage == TEXT("Release"))
        {
            return Settings->bWarningsBlockRelease;
        }
        if (GateStage == TEXT("Nightly"))
        {
            return Settings->bWarningsBlockNightly;
        }
        if (GateStage == TEXT("PreSubmit"))
        {
            return Settings->bWarningsBlockPreSubmit;
        }

        return Rule == nullptr || Rule->bCIBlocking;
    }

    if (Result.Status == EGamePlatformValidationStatus::Failed
        || Result.Status == EGamePlatformValidationStatus::NotRun)
    {
        return Rule == nullptr || Rule->bCIBlocking;
    }

    return false;
}

bool FGamePlatformValidationService::IsAllowlisted(
    FName RuleId,
    const FString& Target,
    const TArray<FGamePlatformValidationAllowlistEntry>& Allowlist,
    const FDateTime& Now)
{
    return Allowlist.ContainsByPredicate([&](const FGamePlatformValidationAllowlistEntry& Entry)
    {
        return Entry.RuleId == RuleId
            && Entry.Target.Equals(Target, ESearchCase::CaseSensitive)
            && Entry.IsActive(Now);
    });
}

bool FGamePlatformValidationService::ReloadAllowlist(FString& OutError)
{
    AllowlistEntries.Reset();
    bAllowlistLoaded = true;
    OutError.Reset();

    const FString WorkspaceRoot =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
    const FString Path =
        WorkspaceRoot / TEXT("Build/Rules/developer-tools-allowlist.json");

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *Path))
    {
        OutError = FString::Printf(TEXT("Allowlist文件不存在或不可读：%s"), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = TEXT("Allowlist JSON解析失败。");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("entries"), Entries) || Entries == nullptr)
    {
        OutError = TEXT("Allowlist缺少entries数组。");
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        const TSharedPtr<FJsonObject> EntryObject = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!EntryObject.IsValid())
        {
            continue;
        }

        FString RuleIdText;
        FString Target;
        FString Reason;
        FString Owner;
        FString ApprovedBy;
        FString CreatedAtText;
        FString ExpiresAtText;
        FString Ticket;

        EntryObject->TryGetStringField(TEXT("RuleId"), RuleIdText);
        EntryObject->TryGetStringField(TEXT("Target"), Target);
        EntryObject->TryGetStringField(TEXT("Reason"), Reason);
        EntryObject->TryGetStringField(TEXT("Owner"), Owner);
        EntryObject->TryGetStringField(TEXT("ApprovedBy"), ApprovedBy);
        EntryObject->TryGetStringField(TEXT("CreatedAt"), CreatedAtText);
        EntryObject->TryGetStringField(TEXT("ExpiresAt"), ExpiresAtText);
        EntryObject->TryGetStringField(TEXT("Ticket"), Ticket);

        FGamePlatformValidationAllowlistEntry Entry;
        Entry.RuleId = FName(*RuleIdText);
        Entry.Target = Target;
        Entry.Reason = Reason;
        Entry.Owner = Owner;
        Entry.ApprovedBy = ApprovedBy;
        Entry.Ticket = Ticket;

        const bool bCreatedValid =
            FDateTime::ParseIso8601(*CreatedAtText, Entry.CreatedAt);
        const bool bExpiresValid =
            FDateTime::ParseIso8601(*ExpiresAtText, Entry.ExpiresAt);

        if (Entry.RuleId.IsNone()
            || Entry.Target.IsEmpty()
            || Entry.Reason.IsEmpty()
            || Entry.Owner.IsEmpty()
            || Entry.ApprovedBy.IsEmpty()
            || Entry.Ticket.IsEmpty()
            || !bCreatedValid
            || !bExpiresValid)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("忽略不完整Allowlist条目：RuleId=%s Target=%s"),
                *RuleIdText,
                *Target);
            continue;
        }

        AllowlistEntries.Add(MoveTemp(Entry));
    }

    return true;
}

const TArray<FGamePlatformValidationAllowlistEntry>&
FGamePlatformValidationService::GetAllowlist()
{
    if (!bAllowlistLoaded)
    {
        FString Error;
        ReloadAllowlist(Error);
    }
    return AllowlistEntries;
}

bool FGamePlatformValidationService::IsTargetAllowlisted(
    FName RuleId,
    const FString& Target)
{
    return IsAllowlisted(
        RuleId,
        Target,
        GetAllowlist(),
        FDateTime::UtcNow());
}

void FGamePlatformValidationService::ApplyAllowlist(
    TArray<FGamePlatformValidationResult>& Results)
{
    const FDateTime Now = FDateTime::UtcNow();
    const TArray<FGamePlatformValidationAllowlistEntry>& Entries = GetAllowlist();

    for (FGamePlatformValidationResult& Result : Results)
    {
        if (Result.Status != EGamePlatformValidationStatus::Failed
            && Result.Status != EGamePlatformValidationStatus::Warning)
        {
            continue;
        }

        const FGamePlatformValidationAllowlistEntry* Match =
            Entries.FindByPredicate([&](const FGamePlatformValidationAllowlistEntry& Entry)
            {
                return Entry.RuleId == Result.RuleId
                    && Entry.Target.Equals(Result.Target, ESearchCase::CaseSensitive)
                    && Entry.IsActive(Now);
            });

        if (!Match)
        {
            continue;
        }

        Result.Status = EGamePlatformValidationStatus::Skipped;
        Result.Message += FString::Printf(
            TEXT(" [Allowlist有效至%s，Ticket=%s，ApprovedBy=%s]"),
            *Match->ExpiresAt.ToIso8601(),
            *Match->Ticket,
            *Match->ApprovedBy);
    }
}

FString FGamePlatformValidationService::CreateRunId()
{
    return FString::Printf(
        TEXT("%s-%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
}

FString FGamePlatformValidationService::GetReportRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("GamePlatformValidation"));
}

bool FGamePlatformValidationService::ResolveSafeReportDirectory(
    const FString& RunId,
    FString& OutDirectory,
    FString& OutError)
{
    if (RunId.IsEmpty() || RunId.Contains(TEXT("..")) || RunId.Contains(TEXT("/")) || RunId.Contains(TEXT("\\")))
    {
        OutError = TEXT("RunId非法，禁止路径逃逸。");
        return false;
    }

    FString Root = GetReportRoot();
    FPaths::NormalizeDirectoryName(Root);
    FString Candidate = FPaths::ConvertRelativePathToFull(Root / RunId);
    FPaths::NormalizeDirectoryName(Candidate);

    if (!Candidate.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase)
        && !Candidate.StartsWith(Root + TEXT("\\"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("报告路径逃逸Workspace Saved目录。");
        return false;
    }

    IFileManager::Get().MakeDirectory(*Candidate, true);
    OutDirectory = Candidate;
    return true;
}

bool FGamePlatformValidationService::WriteReports(
    const FGamePlatformValidationRunSummary& Summary,
    FString& OutDirectory,
    FString& OutError)
{
    if (!ResolveSafeReportDirectory(Summary.RunId, OutDirectory, OutError))
    {
        return false;
    }

    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetStringField(TEXT("runId"), Summary.RunId);
    RootObject->SetStringField(TEXT("startedAt"), Summary.StartedAt.ToIso8601());
    RootObject->SetStringField(TEXT("finishedAt"), Summary.FinishedAt.ToIso8601());

    TArray<TSharedPtr<FJsonValue>> Issues;
    int32 BlockingFailures = 0;

    for (const FGamePlatformValidationResult& Result : Summary.Results)
    {
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("ruleId"), Result.RuleId.ToString());
        Item->SetStringField(TEXT("target"), Result.Target);
        Item->SetStringField(TEXT("status"), StatusToString(Result.Status));
        Item->SetStringField(TEXT("message"), Result.Message);
        Item->SetStringField(TEXT("evidence"), Result.Evidence);
        Item->SetStringField(TEXT("suggestedFix"), Result.SuggestedFix);
        Issues.Add(MakeShared<FJsonValueObject>(Item));

        if (Result.Status == EGamePlatformValidationStatus::Failed)
        {
            if (const FGamePlatformValidationRule* Rule = FindRule(Result.RuleId))
            {
                BlockingFailures += Rule->bCIBlocking ? 1 : 0;
            }
        }
    }

    RootObject->SetNumberField(TEXT("blockingFailures"), BlockingFailures);
    RootObject->SetArrayField(TEXT("results"), Issues);

    FString JsonText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
    FJsonSerializer::Serialize(RootObject, Writer);

    FString Markdown = FString::Printf(
        TEXT("# GamePlatformValidation（游戏平台验证）\n\n- RunId：%s\n- BlockingFailures：%d\n\n"),
        *Summary.RunId,
        BlockingFailures);

    for (const FGamePlatformValidationResult& Result : Summary.Results)
    {
        Markdown += FString::Printf(
            TEXT("- %s｜%s｜%s｜%s\n"),
            *Result.RuleId.ToString(),
            *StatusToString(Result.Status),
            *Result.Target,
            *Result.Message);
    }

    const bool bJson =
        FFileHelper::SaveStringToFile(JsonText, *(OutDirectory / TEXT("summary.json")));
    const bool bIssues =
        FFileHelper::SaveStringToFile(JsonText, *(OutDirectory / TEXT("issues.json")));
    const bool bMarkdown =
        FFileHelper::SaveStringToFile(Markdown, *(OutDirectory / TEXT("summary.md")));

    if (!bJson || !bIssues || !bMarkdown)
    {
        OutError = TEXT("写入summary.json/issues.json/summary.md失败。");
        return false;
    }

    return true;
}

bool FGamePlatformValidationService::IsRemovedLegacyGameplayTag(const FString& TagText)
{
    const UGamePlatformValidationSettings* Settings = GetDefault<UGamePlatformValidationSettings>();
    return Settings->RemovedGameplayTagPrefixes.ContainsByPredicate([&](const FString& Prefix)
    {
        return !Prefix.IsEmpty() && TagText.StartsWith(Prefix);
    });
}

FString FGamePlatformValidationService::MaskSensitiveValue(const FString& Value)
{
    if (Value.IsEmpty())
    {
        return TEXT("<empty>");
    }

    if (Value.Len() <= 4)
    {
        return TEXT("****");
    }

    return Value.Left(2) + TEXT("***") + Value.Right(2);
}
