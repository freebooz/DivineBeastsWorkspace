#pragma once

#include "CoreMinimal.h"
#include "Validation/GamePlatformValidationTypes.h"

/**
 * FGamePlatformValidationService（平台验证服务）。
 * 只提供Editor/CI规则注册、豁免判断和证据报告，不持有Runtime业务状态。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationService
{
public:
    static void RegisterBuiltInRules();
    static void ResetForShutdown();

    static const TArray<FGamePlatformValidationRule>& GetRules();
    static const FGamePlatformValidationRule* FindRule(FName RuleId);

    static bool RegisterRuleProvider(
        const TSharedRef<IGamePlatformValidationRuleProvider>& Provider,
        FString& OutError);
    static void UnregisterRuleProvider(FName ProviderId);
    static TArray<FGamePlatformValidationRule> GetAllRulesSnapshot();
    static bool ShouldBlockResult(const FGamePlatformValidationResult& Result, FName GateStage);

    static bool IsAllowlisted(
        FName RuleId,
        const FString& Target,
        const TArray<FGamePlatformValidationAllowlistEntry>& Allowlist,
        const FDateTime& Now);

    static bool ReloadAllowlist(FString& OutError);
    static bool IsTargetAllowlisted(FName RuleId, const FString& Target);
    static void ApplyAllowlist(TArray<FGamePlatformValidationResult>& Results);
    static const TArray<FGamePlatformValidationAllowlistEntry>& GetAllowlist();

    static FString CreateRunId();
    static FString GetReportRoot();
    static bool ResolveSafeReportDirectory(const FString& RunId, FString& OutDirectory, FString& OutError);
    static bool WriteReports(const FGamePlatformValidationRunSummary& Summary, FString& OutDirectory, FString& OutError);

    static bool IsRemovedLegacyGameplayTag(const FString& TagText);
    static FString MaskSensitiveValue(const FString& Value);

private:
    static TArray<FGamePlatformValidationRule> Rules;
    static TMap<FName, TArray<FGamePlatformValidationRule>> ExtensionRules;
    static TArray<FGamePlatformValidationAllowlistEntry> AllowlistEntries;
    static bool bAllowlistLoaded;
};
