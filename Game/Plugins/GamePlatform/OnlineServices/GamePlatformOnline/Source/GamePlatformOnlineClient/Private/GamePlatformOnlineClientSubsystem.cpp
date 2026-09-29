#include "GamePlatformOnlineClientSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Dom/JsonObject.h"
#include "Misc/LexFromString.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Transport/GamePlatformGatewayAuthProvider.h"

namespace
{
constexpr int32 MaxLoginNameChars = 256;
constexpr int32 MaxPasswordChars = 4096;
constexpr double ProactiveRefreshLeadSeconds = 5.0;
constexpr float RequestTickerIntervalSeconds = 0.1f;

EGamePlatformOnlineError ToOnlineError(EGamePlatformAuthError Error)
{
    switch (Error)
    {
    case EGamePlatformAuthError::None: return EGamePlatformOnlineError::None;
    case EGamePlatformAuthError::ProviderUnavailable:
    case EGamePlatformAuthError::NetworkUnavailable:
        return EGamePlatformOnlineError::ConnectionFailure;
    case EGamePlatformAuthError::InvalidCredentials:
    case EGamePlatformAuthError::AuthExpired:
        return EGamePlatformOnlineError::Unauthenticated;
    case EGamePlatformAuthError::AccountLocked:
    case EGamePlatformAuthError::Forbidden:
        return EGamePlatformOnlineError::Forbidden;
    case EGamePlatformAuthError::Maintenance:
    case EGamePlatformAuthError::ServiceUnavailable:
        return EGamePlatformOnlineError::ServiceUnavailable;
    case EGamePlatformAuthError::ContractIncompatible:
        return EGamePlatformOnlineError::IncompatibleProtocol;
    case EGamePlatformAuthError::Cancelled:
        return EGamePlatformOnlineError::Cancelled;
    case EGamePlatformAuthError::TimedOut:
        return EGamePlatformOnlineError::Timeout;
    case EGamePlatformAuthError::QueueFull:
        return EGamePlatformOnlineError::QueueFull;
    case EGamePlatformAuthError::AuthenticationBusy:
        return EGamePlatformOnlineError::AuthenticationBusy;
    case EGamePlatformAuthError::Conflict:
        return EGamePlatformOnlineError::Conflict;
    case EGamePlatformAuthError::NotFound:
        return EGamePlatformOnlineError::NotFound;
    case EGamePlatformAuthError::RateLimited:
        return EGamePlatformOnlineError::RateLimited;
    case EGamePlatformAuthError::OutcomeUnknown:
        return EGamePlatformOnlineError::OutcomeUnknown;
    case EGamePlatformAuthError::InvalidRequest:
        return EGamePlatformOnlineError::InvalidArgument;
    case EGamePlatformAuthError::InvalidResponse:
    case EGamePlatformAuthError::Unknown:
    default:
        return EGamePlatformOnlineError::InvalidResponse;
    }
}

FName OnlineErrorCode(EGamePlatformAuthError Error)
{
    switch (Error)
    {
    case EGamePlatformAuthError::None: return NAME_None;
    case EGamePlatformAuthError::ProviderUnavailable: return TEXT("OnlineProviderUnavailable");
    case EGamePlatformAuthError::InvalidCredentials: return TEXT("OnlineInvalidCredentials");
    case EGamePlatformAuthError::AccountLocked: return TEXT("OnlineAccountLocked");
    case EGamePlatformAuthError::Maintenance: return TEXT("OnlineMaintenance");
    case EGamePlatformAuthError::NetworkUnavailable: return TEXT("OnlineNetworkUnavailable");
    case EGamePlatformAuthError::AuthExpired: return TEXT("OnlineAuthExpired");
    case EGamePlatformAuthError::ContractIncompatible: return TEXT("OnlineContractIncompatible");
    case EGamePlatformAuthError::Cancelled: return TEXT("OnlineCancelled");
    case EGamePlatformAuthError::TimedOut: return TEXT("OnlineTimedOut");
    case EGamePlatformAuthError::QueueFull: return TEXT("OnlineQueueFull");
    case EGamePlatformAuthError::AuthenticationBusy: return TEXT("OnlineAuthenticationBusy");
    case EGamePlatformAuthError::InvalidRequest: return TEXT("OnlineInvalidRequest");
    case EGamePlatformAuthError::InvalidResponse: return TEXT("OnlineInvalidResponse");
    case EGamePlatformAuthError::Forbidden: return TEXT("OnlineForbidden");
    case EGamePlatformAuthError::Conflict: return TEXT("OnlineConflict");
    case EGamePlatformAuthError::NotFound: return TEXT("OnlineNotFound");
    case EGamePlatformAuthError::RateLimited: return TEXT("OnlineRateLimited");
    case EGamePlatformAuthError::ServiceUnavailable: return TEXT("OnlineServiceUnavailable");
    case EGamePlatformAuthError::OutcomeUnknown: return TEXT("OnlineOutcomeUnknown");
    default: return TEXT("OnlineUnknown");
    }
}

FGamePlatformResult ToCoreResult(EGamePlatformAuthError Error)
{
    if (Error == EGamePlatformAuthError::None)
    {
        return FGamePlatformResult::Success();
    }
    if (Error == EGamePlatformAuthError::Cancelled)
    {
        return FGamePlatformResult::Cancelled(TEXT("在线操作已取消。"));
    }
    return FGamePlatformResult::Failure(
        OnlineErrorCode(Error),
        TEXT("在线操作未成功完成。"));
}

bool IsPrintableAsciiToken(const FString& Value, int32 MaxChars)
{
    if (Value.IsEmpty() || Value.Len() > MaxChars)
    {
        return false;
    }
    for (TCHAR C : Value)
    {
        if (C < 33 || C > 126)
        {
            return false;
        }
    }
    return true;
}

bool DeserializeObjectPreservingIntegers(
    const FString& Text,
    TSharedPtr<FJsonObject>& Out)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    return FJsonSerializer::Deserialize(
               Reader,
               Out,
               FJsonSerializer::EFlags::StoreNumbersAsStrings) &&
        Out.IsValid();
}

bool ParseProfilePayload(
    const FString& Text,
    const FString& ExpectedPlayerId,
    const FString& ExpectedGameId,
    FGamePlatformOnlineProfile& Out)
{
    TSharedPtr<FJsonObject> Json;
    if (!DeserializeObjectPreservingIntegers(Text, Json))
    {
        return false;
    }

    FString DataVersionText;
    FString RevisionText;
    const TArray<TSharedPtr<FJsonValue>>* OwnedValues = nullptr;
    if (!Json->TryGetStringField(TEXT("playerId"), Out.PlayerId) ||
        !Json->TryGetStringField(TEXT("gameId"), Out.GameId) ||
        !Json->TryGetStringField(TEXT("displayName"), Out.DisplayName) ||
        !Json->TryGetStringField(TEXT("dataVersion"), DataVersionText) ||
        !Json->TryGetStringField(TEXT("revision"), RevisionText) ||
        !Json->TryGetBoolField(TEXT("tutorialCompleted"), Out.bTutorialCompleted) ||
        !Json->TryGetArrayField(TEXT("ownedCharacterIds"), OwnedValues) ||
        !OwnedValues ||
        !LexTryParseString(Out.DataVersion, *DataVersionText) ||
        !LexTryParseString(Out.Revision, *RevisionText) ||
        Out.DataVersion < 1 ||
        Out.Revision < 0 ||
        Out.PlayerId != ExpectedPlayerId ||
        Out.GameId != ExpectedGameId)
    {
        return false;
    }

    Json->TryGetStringField(TEXT("defaultWorldId"), Out.DefaultWorldId);
    Out.OwnedCharacterIds.Reset(OwnedValues->Num());
    for (const TSharedPtr<FJsonValue>& Value : *OwnedValues)
    {
        FString CharacterId;
        if (!Value.IsValid() || !Value->TryGetString(CharacterId) || CharacterId.IsEmpty())
        {
            return false;
        }
        Out.OwnedCharacterIds.Add(MoveTemp(CharacterId));
    }
    return true;
}

bool SerializeProfileUpdatePayload(
    const FString& DisplayName,
    int64 ExpectedRevision,
    FString& Out)
{
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("displayName"), DisplayName);
    Writer->WriteValue(TEXT("expectedRevision"), ExpectedRevision);
    Writer->WriteObjectEnd();
    return Writer->Close();
}
}

struct UGamePlatformOnlineClientSubsystem::FRuntime
{
    struct FPendingRequest
    {
        FGamePlatformAuthenticatedRequest Request;
        FGamePlatformOnlineRequestOptions Options;
        FGamePlatformAuthenticatedCompletion Completion;
        double DeadlineSeconds = 0.0;
        bool bStarted = false;
        bool bWaitingRefresh = false;
        bool bAuthReplay = false;
        bool bRequiresAuthentication = true;
    };

    struct FPendingRefreshOperation
    {
        FGamePlatformOnlineRequestOptions Options;
        FGamePlatformOnlineAuthenticationCompletion Completion;
        double StartedSeconds = 0.0;
    };

    struct FPendingLogoutOperation
    {
        FGamePlatformOnlineLogoutCompletion Completion;
        double StartedSeconds = 0.0;
        bool bHadAuthentication = false;
    };

    TSharedPtr<IGamePlatformOnlineAuthProvider> Provider;
    TMap<FGuid, TSharedPtr<FPendingRequest>> Requests;
    TArray<FGuid> ReadyQueue;
    int32 ReadyQueueHead = 0;
    TArray<FGuid> RefreshWaiters;
    TMap<FGuid, FPendingRefreshOperation> RefreshOperations;
    TMap<FGuid, FPendingLogoutOperation> LogoutOperations;
    FGuid LoginOperationId;
    FGamePlatformOnlineAuthenticationCompletion LoginCompletion;
    FGamePlatformOnlineRequestOptions LoginOptions;
    double LoginStartedSeconds = 0.0;
    FTSTicker::FDelegateHandle TickerHandle;
    int32 ActiveRequests = 0;
    uint64 RefreshAttempts = 0;
    uint64 CompletedRequests = 0;
    uint64 AuthGenerationCounter = 0;
    uint64 TokenVersion = 0;
    EGamePlatformAuthError LastError = EGamePlatformAuthError::None;
    bool bRefreshInFlight = false;
};

UGamePlatformOnlineClientSubsystem::UGamePlatformOnlineClientSubsystem() = default;
UGamePlatformOnlineClientSubsystem::~UGamePlatformOnlineClientSubsystem() = default;

void UGamePlatformOnlineClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    check(IsInGameThread());

    Runtime = MakeUnique<FRuntime>();
    Runtime->Provider = MakeShared<FGamePlatformGatewayAuthProvider>();
    InstanceScopeId = FGuid::NewGuid();
    Snapshot = {};
    Snapshot.AuthGeneration = FGuid::NewGuid();
    Runtime->AuthGenerationCounter = 1;

    UGameInstance* Instance = GetGameInstance();
    ensureMsgf(
        Instance && IGamePlatformOnlineService::RegisterInstanceService(*Instance, *this),
        TEXT("GamePlatformOnlineClient 未能注册当前 GameInstance 在线服务门面。"));
}

void UGamePlatformOnlineClientSubsystem::Deinitialize()
{
    check(IsInGameThread());
    StopTicker();

    if (UGameInstance* Instance = GetGameInstance())
    {
        IGamePlatformOnlineService::UnregisterInstanceService(*Instance, *this);
    }

    if (Runtime)
    {
        if (Runtime->Provider.IsValid())
        {
            Runtime->Provider->CancelAll();
        }
        Runtime->Requests.Reset();
        Runtime->ReadyQueue.Reset();
    Runtime->ReadyQueueHead = 0;
        Runtime->RefreshWaiters.Reset();
        Runtime->RefreshOperations.Reset();
        Runtime->LogoutOperations.Reset();
        Runtime->LoginCompletion = {};
        Runtime->LoginOperationId.Invalidate();
        Runtime.Reset();
    }

    bConfigured = false;
    Configuration = {};
    Snapshot = {};
    CachedProfile.Reset();
    InstanceScopeId.Invalidate();
    AuthStateChanged.Clear();
    Super::Deinitialize();
}

FGamePlatformResult UGamePlatformOnlineClientSubsystem::Configure(
    const FGamePlatformOnlineConfiguration& InConfiguration)
{
    check(IsInGameThread());

    if (!Runtime || !Runtime->Provider.IsValid())
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineProviderUnavailable"),
            TEXT("平台在线客户端传输提供者不可用。"));
    }
    if (!Runtime->Requests.IsEmpty() ||
        Runtime->bRefreshInFlight ||
        Snapshot.State == EGamePlatformAuthState::LoggingIn ||
        Snapshot.State == EGamePlatformAuthState::Authenticated ||
        Snapshot.State == EGamePlatformAuthState::Refreshing)
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineConfigurationBusy"),
            TEXT("认证或在线请求进行中时不能重新配置。"));
    }

    const FGamePlatformResult Validated =
        IGamePlatformOnlineService::ValidateConfiguration(InConfiguration);
    if (!Validated.IsSuccess())
    {
        return Validated;
    }

    const FGamePlatformResult ProviderResult =
        Runtime->Provider->Configure(InConfiguration);
    if (!ProviderResult.IsSuccess())
    {
        return ProviderResult;
    }

    Configuration = InConfiguration;
    bConfigured = true;
    ResetAuthentication(
        EGamePlatformAuthState::LoggedOut,
        EGamePlatformAuthError::None,
        true);
    return FGamePlatformResult::Success();
}

void UGamePlatformOnlineClientSubsystem::SetProvider(
    TSharedPtr<IGamePlatformOnlineAuthProvider> InProvider)
{
    check(IsInGameThread());
    if (!Runtime || Runtime->Provider == InProvider)
    {
        return;
    }

    TArray<FGuid> Existing;
    Runtime->Requests.GetKeys(Existing);
    for (const FGuid& RequestId : Existing)
    {
        FGamePlatformAuthenticatedResponse Cancelled;
        Cancelled.Error = EGamePlatformAuthError::Cancelled;
        CompleteRequest(RequestId, MoveTemp(Cancelled));
    }

    if (Runtime->Provider.IsValid())
    {
        Runtime->Provider->CancelAll();
    }

    Runtime->Provider = MoveTemp(InProvider);
    Runtime->bRefreshInFlight = false;
    Runtime->RefreshWaiters.Reset();
    Runtime->ReadyQueue.Reset();
    Runtime->ReadyQueueHead = 0;

    if (bConfigured && Runtime->Provider.IsValid())
    {
        const FGamePlatformResult Result =
            Runtime->Provider->Configure(Configuration);
        if (!Result.IsSuccess())
        {
            bConfigured = false;
        }
    }

    ResetAuthentication(
        bConfigured
            ? EGamePlatformAuthState::LoggedOut
            : EGamePlatformAuthState::Failed,
        bConfigured
            ? EGamePlatformAuthError::None
            : EGamePlatformAuthError::ProviderUnavailable,
        true);
}

void UGamePlatformOnlineClientSubsystem::TryAutoLogin()
{
    check(IsInGameThread());

    if (!Runtime || !Runtime->Provider.IsValid() || !bConfigured)
    {
        ResetAuthentication(
            EGamePlatformAuthState::Failed,
            EGamePlatformAuthError::ProviderUnavailable,
            true);
        return;
    }
    if (Snapshot.State == EGamePlatformAuthState::LoggingIn ||
        Snapshot.State == EGamePlatformAuthState::Authenticated ||
        Snapshot.State == EGamePlatformAuthState::Refreshing ||
        Snapshot.State == EGamePlatformAuthState::LoggingOut)
    {
        Snapshot.Error = EGamePlatformAuthError::AuthenticationBusy;
        BroadcastSnapshot();
        return;
    }

    ResetAuthentication(
        EGamePlatformAuthState::LoggingIn,
        EGamePlatformAuthError::None,
        true);
    const FGuid Generation = Snapshot.AuthGeneration;
    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);

    Runtime->Provider->TryAutoLogin(
        [WeakThis, Generation](FGamePlatformAuthProviderResult Result) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, Generation, Result = MoveTemp(Result)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->CompleteAuthentication(
                            Generation,
                            MoveTemp(Result),
                            false);
                    }
                });
        });
}

void UGamePlatformOnlineClientSubsystem::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password)
{
    check(IsInGameThread());

    if (!Runtime || !Runtime->Provider.IsValid() || !bConfigured)
    {
        ResetAuthentication(
            EGamePlatformAuthState::Failed,
            EGamePlatformAuthError::ProviderUnavailable,
            true);
        return;
    }
    if (Snapshot.State == EGamePlatformAuthState::LoggingIn ||
        Snapshot.State == EGamePlatformAuthState::Authenticated ||
        Snapshot.State == EGamePlatformAuthState::Refreshing ||
        Snapshot.State == EGamePlatformAuthState::LoggingOut)
    {
        // 不推进代次：被拒绝的并发登录没有资格取消正在进行中的合法认证。
        Snapshot.Error = EGamePlatformAuthError::AuthenticationBusy;
        BroadcastSnapshot();
        return;
    }

    const FString Normalized = LoginName.TrimStartAndEnd();
    if (Normalized.IsEmpty() ||
        Password.IsEmpty() ||
        Normalized.Len() > MaxLoginNameChars ||
        Password.Len() > MaxPasswordChars)
    {
        ResetAuthentication(
            EGamePlatformAuthState::Failed,
            EGamePlatformAuthError::InvalidCredentials,
            true);
        return;
    }

    ResetAuthentication(
        EGamePlatformAuthState::LoggingIn,
        EGamePlatformAuthError::None,
        true);
    const FGuid Generation = Snapshot.AuthGeneration;
    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);

    Runtime->Provider->LoginWithCredentials(
        Normalized,
        Password,
        [WeakThis, Generation](FGamePlatformAuthProviderResult Result) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, Generation, Result = MoveTemp(Result)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->CompleteAuthentication(
                            Generation,
                            MoveTemp(Result),
                            false);
                    }
                });
        });
}

void UGamePlatformOnlineClientSubsystem::Refresh()
{
    check(IsInGameThread());
    if (Snapshot.State == EGamePlatformAuthState::Refreshing)
    {
        return;
    }
    if (Snapshot.State != EGamePlatformAuthState::Authenticated)
    {
        ResetAuthentication(
            EGamePlatformAuthState::Failed,
            EGamePlatformAuthError::AuthExpired,
            true);
        return;
    }
    BeginRefreshSingleFlight();
}

void UGamePlatformOnlineClientSubsystem::Logout()
{
    check(IsInGameThread());

    if (!Runtime)
    {
        return;
    }

    TArray<FGuid> Existing;
    Runtime->Requests.GetKeys(Existing);
    for (const FGuid& RequestId : Existing)
    {
        if (TSharedPtr<FRuntime::FPendingRequest>* Pending =
                Runtime->Requests.Find(RequestId);
            Pending && (*Pending)->bStarted &&
            Runtime->Provider.IsValid())
        {
            Runtime->Provider->CancelRequest(RequestId);
            (*Pending)->bStarted = false;
            Runtime->ActiveRequests =
                FMath::Max(0, Runtime->ActiveRequests - 1);
        }

        FGamePlatformAuthenticatedResponse Cancelled;
        Cancelled.Error = EGamePlatformAuthError::Cancelled;
        CompleteRequest(RequestId, MoveTemp(Cancelled));
    }

    Runtime->bRefreshInFlight = false;
    Runtime->RefreshWaiters.Reset();
    Runtime->ReadyQueue.Reset();
    Runtime->ReadyQueueHead = 0;

    // 本地退出立即生效；远端撤销是尽力而为，失败不能恢复旧认证。
    ResetAuthentication(
        EGamePlatformAuthState::LoggedOut,
        EGamePlatformAuthError::None,
        true);

    if (Runtime->Provider.IsValid())
    {
        Runtime->Provider->Logout([](FGamePlatformAuthProviderLogoutResult) {});
    }
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::SendAuthenticatedRequest(
    FGamePlatformAuthenticatedRequest Request,
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformAuthenticatedCompletion Completion)
{
    return SendRequestInternal(
        MoveTemp(Request),
        Options,
        MoveTemp(Completion),
        true);
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::SendRequestInternal(
    FGamePlatformAuthenticatedRequest Request,
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformAuthenticatedCompletion Completion,
    bool bRequiresAuthentication)
{
    check(IsInGameThread());

    FGamePlatformOnlineRequestHandle Handle;
    Handle.InstanceScopeId = InstanceScopeId;
    Handle.RequestId = FGuid::NewGuid();

    auto CompleteLater =
        [Completion = MoveTemp(Completion)](
            EGamePlatformAuthError Error) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Completion), Error]() mutable
                {
                    if (Completion)
                    {
                        FGamePlatformAuthenticatedResponse Result;
                        Result.Error = Error;
                        Completion(MoveTemp(Result));
                    }
                });
        };

    if (!Runtime ||
        !Runtime->Provider.IsValid() ||
        !bConfigured)
    {
        CompleteLater(EGamePlatformAuthError::ProviderUnavailable);
        return Handle;
    }
    const bool bSafeRead =
        Request.Verb == TEXT("GET") || Request.Verb == TEXT("HEAD");
    const bool bIdempotencyKeyValid =
        Request.IdempotencyKey.IsEmpty() ||
        IsPrintableAsciiToken(Request.IdempotencyKey, 128);
    if (!Completion ||
        Request.Verb.IsEmpty() ||
        Request.RelativePath.IsEmpty() ||
        !bIdempotencyKeyValid ||
        (!bSafeRead && Request.bIdempotent && Request.IdempotencyKey.IsEmpty()) ||
        !FMath::IsFinite(Options.DeadlineSeconds) ||
        Options.DeadlineSeconds < 0.0 ||
        Options.DeadlineSeconds > Configuration.RequestDeadlineSeconds)
    {
        CompleteLater(EGamePlatformAuthError::InvalidRequest);
        return Handle;
    }

    const int32 OutstandingBudget =
        Configuration.MaxConcurrentRequests +
        Configuration.MaxQueuedRequests;
    if (Runtime->Requests.Num() >= OutstandingBudget)
    {
        CompleteLater(EGamePlatformAuthError::QueueFull);
        return Handle;
    }

    if (bRequiresAuthentication &&
        Snapshot.State != EGamePlatformAuthState::Authenticated &&
        Snapshot.State != EGamePlatformAuthState::Refreshing)
    {
        CompleteLater(EGamePlatformAuthError::AuthExpired);
        return Handle;
    }

    const TSharedPtr<FRuntime::FPendingRequest> Pending =
        MakeShared<FRuntime::FPendingRequest>();
    Pending->Request = MoveTemp(Request);
    Pending->Options = Options;
    Pending->Completion = MoveTemp(Completion);
    Pending->bRequiresAuthentication = bRequiresAuthentication;
    Pending->DeadlineSeconds =
        FPlatformTime::Seconds() +
        (Options.DeadlineSeconds > 0.0
            ? Options.DeadlineSeconds
            : Configuration.RequestDeadlineSeconds);

    Runtime->Requests.Add(Handle.RequestId, Pending);
    EnsureTicker();

    const bool bTokenNearExpiry =
        bRequiresAuthentication &&
        (Snapshot.AccessExpiresAt.GetTicks() <= 0 ||
         Snapshot.AccessExpiresAt <=
             FDateTime::UtcNow() +
             FTimespan::FromSeconds(ProactiveRefreshLeadSeconds));

    if (bRequiresAuthentication &&
        (Snapshot.State == EGamePlatformAuthState::Refreshing ||
         bTokenNearExpiry))
    {
        QueueForRefresh(Handle.RequestId);
        BeginRefreshSingleFlight();
    }
    else
    {
        Runtime->ReadyQueue.Add(Handle.RequestId);
        PumpRequests();
    }

    return Handle;
}

bool UGamePlatformOnlineClientSubsystem::Cancel(
    const FGamePlatformOnlineRequestHandle& Request)
{
    check(IsInGameThread());

    if (!Runtime ||
        Request.InstanceScopeId != InstanceScopeId ||
        !Request.RequestId.IsValid())
    {
        return false;
    }

    TSharedPtr<FRuntime::FPendingRequest>* Pending =
        Runtime->Requests.Find(Request.RequestId);
    if (!Pending)
    {
        return false;
    }

    if ((*Pending)->bStarted && Runtime->Provider.IsValid())
    {
        Runtime->Provider->CancelRequest(Request.RequestId);
        (*Pending)->bStarted = false;
        Runtime->ActiveRequests =
            FMath::Max(0, Runtime->ActiveRequests - 1);
    }

    FGamePlatformAuthenticatedResponse Result;
    Result.Error = EGamePlatformAuthError::Cancelled;
    CompleteRequest(Request.RequestId, MoveTemp(Result));
    return true;
}

bool UGamePlatformOnlineClientSubsystem::ApplyAuthorization(
    IHttpRequest& Request) const
{
    check(IsInGameThread());
    return Runtime &&
        Runtime->Provider.IsValid() &&
        Snapshot.State == EGamePlatformAuthState::Authenticated &&
        Runtime->Provider->ApplyAuthorization(Request);
}

FGamePlatformOnlineDiagnostics
UGamePlatformOnlineClientSubsystem::GetDiagnostics() const
{
    FGamePlatformOnlineDiagnostics Result;
    Result.InstanceScopeId = InstanceScopeId;
    Result.bConfigured = bConfigured;

    switch (Snapshot.State)
    {
    case EGamePlatformAuthState::Authenticated:
        Result.AuthState = EGamePlatformOnlineAuthState::SignedIn;
        break;
    case EGamePlatformAuthState::LoggingIn:
        Result.AuthState = EGamePlatformOnlineAuthState::SigningIn;
        break;
    case EGamePlatformAuthState::Refreshing:
        Result.AuthState = EGamePlatformOnlineAuthState::Refreshing;
        break;
    case EGamePlatformAuthState::Failed:
        Result.AuthState =
            Snapshot.Error == EGamePlatformAuthError::AuthExpired
                ? EGamePlatformOnlineAuthState::ReauthenticationRequired
                : EGamePlatformOnlineAuthState::SignedOut;
        break;
    default:
        Result.AuthState = EGamePlatformOnlineAuthState::SignedOut;
        break;
    }

    if (Runtime)
    {
        Result.ActiveRequests = Runtime->ActiveRequests;
        Result.QueuedRequests =
            FMath::Max(
                0,
                Runtime->Requests.Num() -
                    Runtime->ActiveRequests -
                    Runtime->RefreshWaiters.Num());
        Result.RefreshWaiters = Runtime->RefreshWaiters.Num();
        Result.RefreshAttempts = Runtime->RefreshAttempts;
        Result.CompletedRequests = Runtime->CompletedRequests;
        Result.LastError = ToOnlineError(Runtime->LastError);
    }
    return Result;
}

void UGamePlatformOnlineClientSubsystem::ResetAuthentication(
    EGamePlatformAuthState NewState,
    EGamePlatformAuthError Error,
    bool bAdvanceGeneration)
{
    if (bAdvanceGeneration || !Snapshot.AuthGeneration.IsValid())
    {
        Snapshot.AuthGeneration = FGuid::NewGuid();
    }

    Snapshot.State = NewState;
    Snapshot.Error = Error;

    if (NewState != EGamePlatformAuthState::Authenticated &&
        NewState != EGamePlatformAuthState::Refreshing &&
        NewState != EGamePlatformAuthState::LoggingIn)
    {
        Snapshot.AccountId.Reset();
        Snapshot.SessionId.Reset();
        Snapshot.AccessExpiresAt = FDateTime();
        Snapshot.RefreshExpiresAt = FDateTime();
    }

    BroadcastSnapshot();
}

void UGamePlatformOnlineClientSubsystem::CompleteAuthentication(
    const FGuid& ExpectedGeneration,
    FGamePlatformAuthProviderResult Result,
    bool bIsRefresh)
{
    check(IsInGameThread());

    if (!Runtime || Snapshot.AuthGeneration != ExpectedGeneration)
    {
        return;
    }

    if (bIsRefresh)
    {
        Runtime->bRefreshInFlight = false;
    }

    const bool bIdentityValid =
        Result.bSuccess &&
        !Result.AccountId.TrimStartAndEnd().IsEmpty() &&
        !Result.SessionId.TrimStartAndEnd().IsEmpty() &&
        Result.AccessExpiresAt > FDateTime::UtcNow() &&
        Result.RefreshExpiresAt > Result.AccessExpiresAt;

    if (bIdentityValid &&
        (!bIsRefresh ||
         (Result.AccountId == Snapshot.AccountId &&
          Result.SessionId == Snapshot.SessionId)))
    {
        Snapshot.State = EGamePlatformAuthState::Authenticated;
        Snapshot.Error = EGamePlatformAuthError::None;
        Snapshot.AccountId = MoveTemp(Result.AccountId);
        Snapshot.SessionId = MoveTemp(Result.SessionId);
        Snapshot.AccessExpiresAt = Result.AccessExpiresAt;
        Snapshot.RefreshExpiresAt = Result.RefreshExpiresAt;
        BroadcastSnapshot();

        if (bIsRefresh)
        {
            const TArray<FGuid> Waiters =
                MoveTemp(Runtime->RefreshWaiters);
            Runtime->RefreshWaiters.Reset();
            for (const FGuid& RequestId : Waiters)
            {
                if (TSharedPtr<FRuntime::FPendingRequest>* Pending =
                        Runtime->Requests.Find(RequestId))
                {
                    (*Pending)->bWaitingRefresh = false;
                    Runtime->ReadyQueue.Add(RequestId);
                }
            }
            PumpRequests();
        }
        return;
    }

    const EGamePlatformAuthError Error =
        Result.Error == EGamePlatformAuthError::None
            ? EGamePlatformAuthError::ContractIncompatible
            : Result.Error;

    Snapshot.State = EGamePlatformAuthState::Failed;
    Snapshot.Error = Error;
    Snapshot.AccountId.Reset();
    Snapshot.SessionId.Reset();
    Snapshot.AccessExpiresAt = FDateTime();
    Snapshot.RefreshExpiresAt = FDateTime();
    BroadcastSnapshot();

    if (bIsRefresh)
    {
        const TArray<FGuid> Waiters =
            MoveTemp(Runtime->RefreshWaiters);
        Runtime->RefreshWaiters.Reset();
        for (const FGuid& RequestId : Waiters)
        {
            if (TSharedPtr<FRuntime::FPendingRequest>* Pending =
                    Runtime->Requests.Find(RequestId))
            {
                (*Pending)->bWaitingRefresh = false;
            }
            FGamePlatformAuthenticatedResponse Failed;
            Failed.Error = Error;
            CompleteRequest(RequestId, MoveTemp(Failed));
        }
    }
}

void UGamePlatformOnlineClientSubsystem::BeginRefreshSingleFlight()
{
    check(IsInGameThread());

    if (!Runtime ||
        Runtime->bRefreshInFlight ||
        !Runtime->Provider.IsValid())
    {
        return;
    }
    if (Snapshot.State != EGamePlatformAuthState::Authenticated &&
        Snapshot.State != EGamePlatformAuthState::Refreshing)
    {
        return;
    }

    Runtime->bRefreshInFlight = true;
    ++Runtime->RefreshAttempts;
    Snapshot.State = EGamePlatformAuthState::Refreshing;
    Snapshot.Error = EGamePlatformAuthError::None;
    BroadcastSnapshot();

    const FGuid Generation = Snapshot.AuthGeneration;
    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
    Runtime->Provider->Refresh(
        [WeakThis, Generation](FGamePlatformAuthProviderResult Result) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, Generation, Result = MoveTemp(Result)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->CompleteAuthentication(
                            Generation,
                            MoveTemp(Result),
                            true);
                    }
                });
        });
}

void UGamePlatformOnlineClientSubsystem::QueueForRefresh(
    const FGuid& RequestId)
{
    if (!Runtime)
    {
        return;
    }

    TSharedPtr<FRuntime::FPendingRequest>* Pending =
        Runtime->Requests.Find(RequestId);
    if (!Pending || (*Pending)->bWaitingRefresh)
    {
        return;
    }

    if (Runtime->RefreshWaiters.Num() >=
        Configuration.MaxRefreshWaiters)
    {
        FGamePlatformAuthenticatedResponse Failed;
        Failed.Error = EGamePlatformAuthError::QueueFull;
        CompleteRequest(RequestId, MoveTemp(Failed));
        return;
    }

    (*Pending)->bWaitingRefresh = true;
    Runtime->RefreshWaiters.Add(RequestId);
}

void UGamePlatformOnlineClientSubsystem::PumpRequests()
{
    check(IsInGameThread());

    if (!Runtime ||
        !Runtime->Provider.IsValid() ||
        Snapshot.State != EGamePlatformAuthState::Authenticated)
    {
        return;
    }

    while (Runtime->ActiveRequests <
               Configuration.MaxConcurrentRequests &&
           !Runtime->ReadyQueue.IsEmpty())
    {
        const FGuid RequestId = Runtime->ReadyQueue[0];
        Runtime->ReadyQueue.RemoveAt(0, 1, EAllowShrinking::No);

        TSharedPtr<FRuntime::FPendingRequest>* Found =
            Runtime->Requests.Find(RequestId);
        if (!Found)
        {
            continue;
        }

        const TSharedPtr<FRuntime::FPendingRequest> Pending = *Found;
        if (Pending->bWaitingRefresh || Pending->bStarted)
        {
            continue;
        }
        if (!IsRequestAlive(Pending->Options))
        {
            FGamePlatformAuthenticatedResponse Cancelled;
            Cancelled.Error = EGamePlatformAuthError::Cancelled;
            CompleteRequest(RequestId, MoveTemp(Cancelled));
            continue;
        }
        if (FPlatformTime::Seconds() >= Pending->DeadlineSeconds)
        {
            FGamePlatformAuthenticatedResponse TimedOut;
            TimedOut.Error = EGamePlatformAuthError::TimedOut;
            CompleteRequest(RequestId, MoveTemp(TimedOut));
            continue;
        }

        Pending->bStarted = true;
        ++Runtime->ActiveRequests;

        const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
        Runtime->Provider->SendAuthenticatedRequest(
            RequestId,
            Pending->Request,
            [WeakThis, RequestId](
                FGamePlatformAuthenticatedResponse Response) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, RequestId, Response = MoveTemp(Response)]() mutable
                    {
                        UGamePlatformOnlineClientSubsystem* Self =
                            WeakThis.Get();
                        if (!Self || !Self->Runtime)
                        {
                            return;
                        }

                        TSharedPtr<FRuntime::FPendingRequest>* FoundPending =
                            Self->Runtime->Requests.Find(RequestId);
                        if (!FoundPending)
                        {
                            return;
                        }

                        const TSharedPtr<FRuntime::FPendingRequest> Pending =
                            *FoundPending;
                        if (Pending->bStarted)
                        {
                            Pending->bStarted = false;
                            Self->Runtime->ActiveRequests =
                                FMath::Max(
                                    0,
                                    Self->Runtime->ActiveRequests - 1);
                        }

                        if (Response.HttpStatusCode == 401 ||
                            Response.Error ==
                                EGamePlatformAuthError::AuthExpired)
                        {
                            if (!Pending->bAuthReplay)
                            {
                                Pending->bAuthReplay = true;
                                const bool bReplaySafe =
                                    Pending->Request.Verb == TEXT("GET") ||
                                    Pending->Request.Verb == TEXT("HEAD") ||
                                    Pending->Request.bIdempotent;

                                if (bReplaySafe)
                                {
                                    Self->QueueForRefresh(RequestId);
                                    Self->BeginRefreshSingleFlight();
                                    Self->PumpRequests();
                                    return;
                                }

                                // 非幂等写请求不自动重放。先刷新后续认证上下文，
                                // 当前请求以AuthExpired结束，由业务层根据自身事务语义决定是否重提。
                                Self->BeginRefreshSingleFlight();
                                Response.Error =
                                    EGamePlatformAuthError::AuthExpired;
                            }

                            if (Self->Runtime->Provider.IsValid())
                            {
                                Self->Runtime->Provider->CancelAll();
                            }
                            Self->ResetAuthentication(
                                EGamePlatformAuthState::Failed,
                                EGamePlatformAuthError::AuthExpired,
                                true);
                            Response.Error =
                                EGamePlatformAuthError::AuthExpired;
                        }

                        Self->CompleteRequest(
                            RequestId,
                            MoveTemp(Response));
                        Self->PumpRequests();
                    });
            });
    }

    EnsureTicker();
}

void UGamePlatformOnlineClientSubsystem::CompleteRequest(
    const FGuid& RequestId,
    FGamePlatformAuthenticatedResponse Response)
{
    check(IsInGameThread());
    if (!Runtime)
    {
        return;
    }

    TSharedPtr<FRuntime::FPendingRequest> Pending;
    if (!Runtime->Requests.RemoveAndCopyValue(RequestId, Pending) ||
        !Pending.IsValid())
    {
        return;
    }

    Runtime->RefreshWaiters.Remove(RequestId);
    ++Runtime->CompletedRequests;
    Runtime->LastError = Response.Error;

    FGamePlatformAuthenticatedCompletion Completion =
        MoveTemp(Pending->Completion);
    if (Completion)
    {
        Completion(MoveTemp(Response));
    }

    if (Runtime->Requests.IsEmpty() &&
        !Runtime->bRefreshInFlight)
    {
        StopTicker();
    }
}

bool UGamePlatformOnlineClientSubsystem::TickRequests(float)
{
    check(IsInGameThread());
    if (!Runtime)
    {
        return false;
    }

    const double Now = FPlatformTime::Seconds();
    TArray<FGuid> RequestIds;
    Runtime->Requests.GetKeys(RequestIds);

    for (const FGuid& RequestId : RequestIds)
    {
        TSharedPtr<FRuntime::FPendingRequest>* Found =
            Runtime->Requests.Find(RequestId);
        if (!Found)
        {
            continue;
        }

        const TSharedPtr<FRuntime::FPendingRequest> Pending = *Found;
        if (IsRequestAlive(Pending->Options) &&
            Now < Pending->DeadlineSeconds)
        {
            continue;
        }

        if (Pending->bStarted && Runtime->Provider.IsValid())
        {
            Runtime->Provider->CancelRequest(RequestId);
            Pending->bStarted = false;
            Runtime->ActiveRequests =
                FMath::Max(0, Runtime->ActiveRequests - 1);
        }

        FGamePlatformAuthenticatedResponse Result;
        Result.Error = IsRequestAlive(Pending->Options)
            ? EGamePlatformAuthError::TimedOut
            : EGamePlatformAuthError::Cancelled;
        CompleteRequest(RequestId, MoveTemp(Result));
    }

    PumpRequests();

    if (Runtime->Requests.IsEmpty() &&
        !Runtime->bRefreshInFlight)
    {
        Runtime->TickerHandle.Reset();
        return false;
    }
    return true;
}

void UGamePlatformOnlineClientSubsystem::EnsureTicker()
{
    if (!Runtime || Runtime->TickerHandle.IsValid())
    {
        return;
    }
    if (Runtime->Requests.IsEmpty() &&
        !Runtime->bRefreshInFlight)
    {
        return;
    }

    Runtime->TickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
     