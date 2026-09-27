#pragma once

#include "CoreMinimal.h"
#include "GamePlatformValidationTypes.generated.h"

/** EGamePlatformValidationSeverity（平台验证严重级别）。 */
UENUM()
enum class EGamePlatformValidationSeverity : uint8
{
    Error,
    Warning,
    Info
};

/** EGamePlatformValidationStatus（平台验证结果状态）。 */
UENUM()
enum class EGamePlatformValidationStatus : uint8
{
    Passed,
    Failed,
    Warning,
    NotRun,
    Skipped
};

/** EGamePlatformAutoFixPolicy（自动修复策略）；第一版默认Disabled。 */
UENUM()
enum class EGamePlatformAutoFixPolicy : uint8
{
    Disabled,
    SuggestOnly,
    LowRisk
};

/** FGamePlatformValidationRule（统一验证规则模型）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationRule
{
    GENERATED_BODY()

    UPROPERTY() FName RuleId;
    UPROPERTY() int32 RuleVersion = 1;
    UPROPERTY() FName Category;
    UPROPERTY() EGamePlatformValidationSeverity Severity = EGamePlatformValidationSeverity::Error;
    UPROPERTY() FString Description;
    UPROPERTY() FString Rationale;
    UPROPERTY() FString Scope;
    UPROPERTY() bool bEnabled = true;
    UPROPERTY() bool bCIBlocking = true;
    UPROPERTY() EGamePlatformAutoFixPolicy AutoFixPolicy = EGamePlatformAutoFixPolicy::Disabled;
    UPROPERTY() FString Owner;
};

/**
 * IGamePlatformValidationRuleProvider（平台验证规则提供接口）。
 * MobaCommon/DivineBeasts Editor扩展可以单向实现并注册项目规则。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API IGamePlatformValidationRuleProvider
{
public:
    virtual ~IGamePlatformValidationRuleProvider() = default;
    virtual FName GetProviderId() const = 0;
    virtual void BuildRules(TArray<FGamePlatformValidationRule>& OutRules) const = 0;
};

/** FGamePlatformValidationResult（统一验证结果模型）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationResult
{
    GENERATED_BODY()

    UPROPERTY() FName RuleId;
    UPROPERTY() FString Target;
    UPROPERTY() EGamePlatformValidationStatus Status = EGamePlatformValidationStatus::NotRun;
    UPROPERTY() FString Message;
    UPROPERTY() FString Evidence;
    UPROPERTY() FString SuggestedFix;
};

/** FGamePlatformValidationAllowlistEntry（临时豁免项）；禁止ignore_all。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationAllowlistEntry
{
    GENERATED_BODY()

    UPROPERTY() FName RuleId;
    UPROPERTY() FString Target;
    UPROPERTY() FString Reason;
    UPROPERTY() FString Owner;
    UPROPERTY() FString ApprovedBy;
    UPROPERTY() FDateTime CreatedAt;
    UPROPERTY() FDateTime ExpiresAt;
    UPROPERTY() FString Ticket;

    bool IsActive(const FDateTime& Now) const
    {
        return !RuleId.IsNone()
            && !Target.IsEmpty()
            && !Reason.IsEmpty()
            && !Owner.IsEmpty()
            && !ApprovedBy.IsEmpty()
            && !Ticket.IsEmpty()
            && ExpiresAt > Now;
    }
};

/** FGamePlatformValidationRunSummary（一次门禁运行摘要）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformValidationRunSummary
{
    GENERATED_BODY()

    UPROPERTY() FString RunId;
    UPROPERTY() FDateTime StartedAt;
    UPROPERTY() FDateTime FinishedAt;
    UPROPERTY() TArray<FGamePlatformValidationResult> Results;
};
