#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/DataAsset.h"
#include "GamePlatformReviewTypes.generated.h"

/** EGamePlatformReviewStatus（人工验收状态）。 */
UENUM()
enum class EGamePlatformReviewStatus : uint8
{
    NotRun,
    InProgress,
    Passed,
    Failed,
    Blocked
};

/** UGamePlatformReviewCase（游戏平台人工验收测试用例）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewCase : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName CaseId;
    UPROPERTY(EditAnywhere) FName Category;
    UPROPERTY(EditAnywhere) FString Description;
    UPROPERTY(EditAnywhere) TArray<FString> Preconditions;
    UPROPERTY(EditAnywhere) TArray<FString> Steps;
    UPROPERTY(EditAnywhere) FString ExpectedResult;
    UPROPERTY(EditAnywhere) TArray<FString> RequiredEvidence;

    bool IsCaseComplete(FString& OutError) const;
};

/** UGamePlatformReviewReport（游戏平台人工验收报告对象）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewReport : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere) FString RunId;
    UPROPERTY(VisibleAnywhere) FName CaseId;
    UPROPERTY(VisibleAnywhere) EGamePlatformReviewStatus Status = EGamePlatformReviewStatus::NotRun;
    UPROPERTY(VisibleAnywhere) FString Reviewer;
    UPROPERTY(VisibleAnywhere) FDateTime ReviewDate;
    UPROPERTY(VisibleAnywhere) FString Comment;
    UPROPERTY(VisibleAnywhere) TArray<FString> Evidence;
    UPROPERTY(VisibleAnywhere) TMap<FString, double> Metrics;
    UPROPERTY(VisibleAnywhere) FString BuildVersion;
    UPROPERTY(VisibleAnywhere) FString ContentRevision;
};

/** UGamePlatformReviewSubsystem（游戏平台人工验收子系统）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    UGamePlatformReviewReport* CreatePendingReport(
        UGamePlatformReviewCase* ReviewCase,
        const FString& RunId,
        const FString& BuildVersion,
        const FString& ContentRevision);

    bool ApplyHumanDecision(
        UGamePlatformReviewReport* Report,
        EGamePlatformReviewStatus NewStatus,
        const FString& Reviewer,
        const FString& Comment,
        const TArray<FString>& Evidence,
        bool bExplicitHumanConfirmation,
        FString& OutError);
};
