#include "Context/DivineBeastsApplicationFlowContext.h"

void UDivineBeastsApplicationFlowContext::ResetForNewRun(bool bInTryAutoLogin)
{
    check(IsInGameThread());

    Profile = {};
    CharacterRoster.Reset();
    SelectedCharacter = {};
    PendingCreateDraft = {};
    PendingSelection = {};
    Assignment = {};
    TargetExperienceId = NAME_None;
    PreferredRegion.Reset();
    LoadingObservationId.Invalidate();
    RecoveryAttempts = 0;
    bTryAutoLogin = bInTryAutoLogin;
    bHasSelectedCharacter = false;
    bHasPendingCreateDraft = false;
    bHasPendingSelection = false;
    ClearConnectionMaterial();
}

void UDivineBeastsApplicationFlowContext::SetSelectedCharacter(
    const FDivineBeastsCharacterSummary& Character)
{
    check(IsInGameThread());
    SelectedCharacter = Character;
    bHasSelectedCharacter = !Character.CharacterId.IsEmpty();
}

void UDivineBeastsApplicationFlowContext::SetPendingCreateDraft(
    const FDivineBeastsCharacterCreateDraft& Draft)
{
    check(IsInGameThread());
    PendingCreateDraft = Draft;
    bHasPendingCreateDraft = true;
}

bool UDivineBeastsApplicationFlowContext::ConsumePendingCreateDraft(
    FDivineBeastsCharacterCreateDraft& OutDraft)
{
    check(IsInGameThread());
    if (!bHasPendingCreateDraft)
    {
        OutDraft = {};
        return false;
    }

    OutDraft = MoveTemp(PendingCreateDraft);
    PendingCreateDraft = {};
    bHasPendingCreateDraft = false;
    return true;
}

void UDivineBeastsApplicationFlowContext::SetPendingSelection(
    const FDivineBeastsCharacterSummary& Character)
{
    check(IsInGameThread());
    PendingSelection = Character;
    bHasPendingSelection = !Character.CharacterId.IsEmpty();
}

bool UDivineBeastsApplicationFlowContext::ConsumePendingSelection(
    FDivineBeastsCharacterSummary& OutCharacter)
{
    check(IsInGameThread());
    if (!bHasPendingSelection)
    {
        OutCharacter = {};
        return false;
    }

    OutCharacter = MoveTemp(PendingSelection);
    PendingSelection = {};
    bHasPendingSelection = false;
    return true;
}

void UDivineBeastsApplicationFlowContext::SetTargetExperience(
    FName ExperienceId,
    FString InPreferredRegion)
{
    check(IsInGameThread());
    TargetExperienceId = ExperienceId;
    PreferredRegion = MoveTemp(InPreferredRegion);
}

void UDivineBeastsApplicationFlowContext::SetAssignment(
    const FDivineBeastsWorldAssignmentSummary& InSummary,
    FString InEndpoint,
    FString InTransferTicket)
{
    check(IsInGameThread());

    // 新分配到达时先擦除上一操作敏感材料，禁止旧票据跨操作复用。
    ClearConnectionMaterial();
    Assignment = InSummary;
    PendingEndpoint = MoveTemp(InEndpoint);
    PendingTransferTicket = MoveTemp(InTransferTicket);
}

bool UDivineBeastsApplicationFlowContext::ConsumeConnectionMaterial(
    FString& OutEndpoint,
    FString& OutTransferTicket)
{
    check(IsInGameThread());
    if (PendingEndpoint.IsEmpty() || PendingTransferTicket.IsEmpty())
    {
        OutEndpoint.Reset();
        OutTransferTicket.Reset();
        return false;
    }

    OutEndpoint = MoveTemp(PendingEndpoint);
    OutTransferTicket = MoveTemp(PendingTransferTicket);
    PendingEndpoint.Reset();
    PendingTransferTicket.Reset();
    return true;
}

void UDivineBeastsApplicationFlowContext::ClearConnectionMaterial()
{
    check(IsInGameThread());
    PendingEndpoint.Reset();
    PendingTransferTicket.Reset();
}
