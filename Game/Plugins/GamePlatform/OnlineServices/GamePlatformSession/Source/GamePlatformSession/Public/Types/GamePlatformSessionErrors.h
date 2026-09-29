#pragma once

#include "CoreMinimal.h"

/**
 * GamePlatformSessionErrors（平台会话稳定错误码）。
 *
 * 统一错误身份供Session、项目流程、日志与Telemetry映射使用；错误码本身不得携带Endpoint、Ticket、Token或玩家隐私。
 */
namespace GamePlatformSessionErrors
{
inline const FName OperationIdInvalid(TEXT("SessionOperationIdInvalid"));
inline const FName IntentInvalid(TEXT("SessionIntentInvalid"));
inline const FName TransferRequestIncomplete(TEXT("SessionTransferRequestIncomplete"));
inline const FName TransferRequestTooLarge(TEXT("SessionTransferRequestTooLarge"));
inline const FName TransferEndpointInvalid(TEXT("SessionTransferEndpointInvalid"));
inline const FName TransferTimeoutInvalid(TEXT("SessionTransferTimeoutInvalid"));
inline const FName Unavailable(TEXT("SessionUnavailable"));
inline const FName TransportUnavailable(TEXT("SessionTransportUnavailable"));
inline const FName TransferRejected(TEXT("SessionTransferRejected"));
inline const FName NoActiveTransfer(TEXT("SessionNoActiveTransfer"));
inline const FName CancelRejected(TEXT("SessionCancelRejected"));
inline const FName AuthoritativeFactRequired(TEXT("SessionAuthoritativeFactRequired"));
inline const FName StaleOperation(TEXT("SessionStaleOperation"));
inline const FName FactRejected(TEXT("SessionFactRejected"));
inline const FName FactInvalid(TEXT("SessionFactInvalid"));
inline const FName BindingRejected(TEXT("SessionBindingRejected"));
inline const FName TravelCommitRejected(TEXT("SessionTravelCommitRejected"));
inline const FName TransportFailed(TEXT("SessionTransportFailed"));
inline const FName ReconciliationRejected(TEXT("SessionReconciliationRejected"));
inline const FName DisconnectRejected(TEXT("SessionDisconnectRejected"));
inline const FName LeaveRejected(TEXT("SessionLeaveRejected"));
inline const FName Cancelled(TEXT("SessionCancelled"));
inline const FName TimedOut(TEXT("SessionTimedOut"));
inline const FName OutcomeUncertain(TEXT("SessionOutcomeUncertain"));
inline const FName AuthChanged(TEXT("SessionAuthChanged"));
inline const FName Failed(TEXT("SessionFailed"));
}
