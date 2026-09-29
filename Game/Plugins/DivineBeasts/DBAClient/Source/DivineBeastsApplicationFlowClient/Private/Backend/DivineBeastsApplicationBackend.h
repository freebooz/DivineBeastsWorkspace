#pragma once

#include "CoreMinimal.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Types/GamePlatformOnlineRequests.h"

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

/**
 * FDivineBeastsHttpApplicationBackend（Gateway业务适配器）。
 *
 * 项目层只负责业务相对路径和JSON DTO；认证、Token刷新、401单次重放、
 * 并发/队列/超时预算及HTTP安全策略统一由GamePlatformOnlineClient承担。
 */
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
        bool bIdempotent,
        FString IdempotencyKey,
        FRawCompletion Completion);

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TArray<FGamePlatformOnlineRequestHandle> ActiveRequests;
};

