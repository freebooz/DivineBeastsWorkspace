#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsFlowTypes.generated.h"

/** EDivineBeastsOnboardingState（神兽联盟新手引导状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsOnboardingState : uint8
{
    New,
    TutorialRequired,
    TutorialInProgress,
    OnboardingComplete
};

/** EDivineBeastsFlowError（神兽联盟流程错误）。 */
UENUM(BlueprintType)
enum class EDivineBeastsFlowError : uint8
{
    None,
    FlowNotInitialized,
    AuthenticationRequired,
    AuthenticationFailed,
    InvalidCredentials,
    AccountLocked,
    Maintenance,
    NetworkUnavailable,
    AuthExpired,
    Cancelled,
    TimedOut,
    ProfileNotFound,
    ProfileUnavailable,
    CharacterRosterUnavailable,
    CharacterNotFound,
    CharacterNotOwned,
    CharacterCreateRejected,
    CharacterCreateOutcomeUnknown,
    CharacterSelectionRejected,
    HeroNotEntitled,
    HeroCatalogUnavailable,
    InvalidAppearance,
    OnboardingRequired,
    ExperienceNotAllowed,
    WorldAssignmentUnavailable,
    NoServerCapacity,
    TransferTicketExpired,
    TransferTicketInvalid,
    TransferTicketConsumed,
    TravelFailed,
    AdmissionFailed,
    WorldMismatch,
    WorldReadinessTimedOut,
    SessionDisconnected,
    ReconnectExhausted,
    ContractIncompatible,
    StaleOperation
};

/** EDivineBeastsFlowAction（UI允许触发的项目流程动作）。 */
UENUM(BlueprintType)
enum class EDivineBeastsFlowAction : uint8
{
    TryAutoLogin,
    Login,
    Refresh,
    CreateCharacter,
    SelectPersistentCharacter,
    Retry,
    Logout
};

/** FDivineBeastsPlayerProfile（玩家资料投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsPlayerProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) int64 ProfileRevision = 0;
    UPROPERTY(BlueprintReadOnly) EDivineBeastsOnboardingState OnboardingState =
        EDivineBeastsOnboardingState::New;
    UPROPERTY(BlueprintReadOnly) FString LastSelectedCharacterId;
    UPROPERTY(BlueprintReadOnly) FName LastExperienceId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName LastWorldId = NAME_None;
};

/** FDivineBeastsCharacterSummary（持久角色摘要，不等于Arena选人）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsCharacterSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) FName HeroDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FString CharacterName;
    UPROPERTY(BlueprintReadOnly) int64 CharacterRevision = 0;
    UPROPERTY(BlueprintReadOnly) EDivineBeastsOnboardingState OnboardingState =
        EDivineBeastsOnboardingState::New;
    UPROPERTY(BlueprintReadOnly) FName Status = NAME_None;
};

/** FDivineBeastsCharacterCreateDraft（当前客户端会话中的临时角色创建草稿）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsCharacterCreateDraft
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CharacterName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, FString> AppearanceSelection;

    bool IsLocallyValid() const
    {
        return !HeroDefinitionId.IsNone() &&
            !CharacterName.TrimStartAndEnd().IsEmpty() &&
            CharacterName.Len() <= 24;
    }
};

/** FDivineBeastsValidatedSelection（后端校验后的持久角色选择）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsValidatedSelection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString SelectionRequestId;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsCharacterSummary Character;
    UPROPERTY(BlueprintReadOnly) int64 ProfileRevision = 0;
};

/** FDivineBeastsWorldAssignmentSummary（世界分配公开摘要，不含TransferTicket原文）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsWorldAssignmentSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString AssignmentId;
    UPROPERTY(BlueprintReadOnly) FString GameServerId;
    UPROPERTY(BlueprintReadOnly) FName ServerRoleId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName ExperienceId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName WorldId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName MapId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName RegionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FString TicketId;
    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) FString SessionId;
};

/** FDivineBeastsFlowViewState（UI只读流程投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSAPPLICATIONFLOWCLIENT_API FDivineBeastsFlowViewState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid FlowRunId;
    UPROPERTY(BlueprintReadOnly) FName CurrentStep = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 NodeGeneration = 0;
    UPROPERTY(BlueprintReadOnly) bool bBusy = false;
    UPROPERTY(BlueprintReadOnly) EDivineBeastsFlowError Error =
        EDivineBeastsFlowError::None;
    UPROPERTY(BlueprintReadOnly) TArray<EDivineBeastsFlowAction> AllowedActions;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsPlayerProfile Profile;
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsCharacterSummary> CharacterRoster;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsCharacterSummary SelectedCharacter;
    UPROPERTY(BlueprintReadOnly) bool bHasSelectedCharacter = false;
    UPROPERTY(BlueprintReadOnly) FString LoadingSummary;
    /** 本次世界切换的回调身份；异步完成通知必须原样携带，旧操作不能推进新屏障。 */
    UPROPERTY(BlueprintReadOnly) FGuid LoadingObservationId;
    UPROPERTY(BlueprintReadOnly) FString ConnectionSummary;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsWorldAssignmentSummary Assignment;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsFlowViewStateChangedNative,
    const FDivineBeastsFlowViewState&);
