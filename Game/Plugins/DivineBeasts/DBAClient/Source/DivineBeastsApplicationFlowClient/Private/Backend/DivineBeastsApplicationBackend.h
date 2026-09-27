#pragma once

#include "CoreMinimal.h"
#include "Flow/DivineBeastsFlowTypes.h"

class UGamePlatformOnlineClientSubsystem;

struct FDivineBeastsWorldAssignmentPayload
{
    FDivineBeastsWorldAssignmentSummary Summary;
    FString Endpoint;
    FString TransferTicket;
};

using FDivineBeastsProfileCompletion =
    TFunction<void(bool, FDivineBeastsPlayerProfile, EDivineBeastsFlowError)>;
using FDivineBeastsRosterCompletion =
    TFunction<void(bool, TArray<FDivineBeastsCharacterSummary>, EDivineBeastsFlowError)>;
using FDivineBeastsCharacterCompletion =
    TFunction<void(bool, FDivineBeastsCharacterSummary, EDivineBeastsFlowError)>;
using FDivineBeastsSelectionCompletion =
    TFunction<void(bool, FDivineBeastsValidatedSelection, EDivineBeastsFlowError)>;
using FDivineBeastsWorldAssignmentCompletion =
    TFunction<void(bool, FDivineBeastsWorldAssignmentPayload, EDivineBeastsFlowError)>;

/** IDivineBeastsApplicationBackend（神兽联盟应用流程后端端口）。 */
class IDivineBeastsApplicationBackend
{
public:
    virtual ~IDivineBeastsApplicationBackend() = default;

    virtual void LoadProfile(FDivineBeastsProfileCompletion Completion) = 0;
    virtual void LoadRoster(FDivineBeastsRosterCompletion Completion) = 0;
    virtual void CreateCharacter(
        const FDivineBeastsCharacterCreateDraft& Draft,
        const FGuid& OperationId,
        FDivineBeastsCharacterCompletion Completion) = 0;
    virtual void SelectPersistentCharacter(
        const FString& CharacterId,
        int64 ExpectedRevision,
        const FGuid& SelectionRequestId,
        FDivineBeastsSelectionCompletion Completion) = 0;
    virtual void RequestWorldAssignment(
        const FString& CharacterId,
        int64 ExpectedRevision,
        FName DesiredExperienceId,
        const FString& PreferredRegion,
        const FGuid& RequestId,
        FDivineBeastsWorldAssignmentCompletion Completion) = 0;
    virtual void CancelAll() = 0;
};

/** FDivineBeastsHttpApplicationBackend（Gateway HTTP私有适配器）。 */
class FDivineBeastsHttpApplicationBackend final
    : public IDivineBeastsApplicationBackend,
      public TSharedFromThis<FDivineBeastsHttpApplicationBackend>
{
public:
    explicit FDivineBeastsHttpApplicationBackend(
        UGamePlatformOnlineClientSubsystem* InOnline);

    virtual void LoadProfile(FDivineBeastsProfileCompletion Completion) override;
    virtual void LoadRoster(FDivineBeastsRosterCompletion Completion) override;
    virtual void CreateCharacter(
        const FDivineBeastsCharacterCreateDraft& Draft,
        const FGuid& OperationId,
        FDivineBeastsCharacterCompletion Completion) override;
    virtual void SelectPersistentCharacter(
        const FString& CharacterId,
        int64 ExpectedRevision,
        const FGuid& SelectionRequestId,
        FDivineBeastsSelectionCompletion Completion) override;
    virtual void RequestWorldAssignment(
        const FString& CharacterId,
        int64 ExpectedRevision,
        FName DesiredExperienceId,
        const FString& PreferredRegion,
        const FGuid& RequestId,
        FDivineBeastsWorldAssignmentCompletion Completion) override;
    virtual void CancelAll() override;

private:
    using FRawCompletion =
        TFunction<void(bool, const FString&, EDivineBeastsFlowError)>;

    void Send(
        const FString& Verb,
        const FString& RelativePath,
        const TSharedPtr<class FJsonObject>& Body,
        FRawCompletion Completion);

    FString MakeUrl(const FString& RelativePath) const;
    FString GetAuthHeader() const;

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    FString GatewayBaseUrl;
    TArray<TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;
};
