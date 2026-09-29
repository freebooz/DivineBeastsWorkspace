#include "GamePlatformOnlineClientSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Interfaces/IHttpRequest.h"
#include "Dom/JsonObject.h"
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

bool IsValidDisplayName(const FString& Value)
{
    const FString Trimmed = Value.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    int32 ScalarCount = 0;
    for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
    {
        const uint32 Unit = static_cast<uint32>(Trimmed[Index]);
        if (Unit < 0x20u || Unit == 0x7fu)
        {
            return false;
        }

        if (Unit >= 0xd800u && Unit <= 0xdbffu)
        {
            if (Index + 1 >= Trimmed.Len())
            {
                return false;
            }
            const uint32 Low =
                static_cast<uint32>(Trimmed[Index + 1]);
            if (Low < 0xdc00u || Low > 0xdfffu)
            {
                return false;
            }
            ++Index;
        }
        else if (Unit >= 0xdc00u && Unit <= 0xdfffu)
        {
            return false;
        }

        if (++ScalarCount > 24)
        {
            return false;
        }
    }
    return ScalarCount > 0;
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
    FString SelectedCharacterId;
    const TArray<TSharedPtr<FJsonValue>>* OwnedValues = nullptr;
    if (!Json->TryGetStringField(TEXT("playerId"), Out.PlayerId) ||
        !Json->TryGetStringField(TEXT("gameId"), Out.GameId) ||
        !Json->TryGetStringField(TEXT("displayName"), Out.DisplayName) ||
        !Json->TryGetStringField(TEXT("dataVersion"), DataVersionText) ||
        !Json->TryGetStringField(TEXT("revision"), RevisionText) ||
        !Json->TryGetBoolField(TEXT("tutorialCompleted"), Out.bTutorialCompleted) ||
        !Json->TryGetStringField(TEXT("selectedCharacterId"), SelectedCharacterId) ||
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
        double ReadyAtSeconds = 0.0;
        int32 RetryCount = 0;
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
UGamePlatformOnlineClientSubsystem::UGamePlatformOnlineClientSubsystem(FVTableHelper& Helper)
    : Super(Helper)
{
}
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


FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::ProbeService(
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformOnlineProbeCompletion Completion)
{
    check(IsInGameThread());

    const double Started = FPlatformTime::Seconds();
    const TSharedRef<FGamePlatformOnlineRequestHandle> Handle =
        MakeShared<FGamePlatformOnlineRequestHandle>();

    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = TEXT("GET");
    Request.RelativePath = TEXT("/v1/online/probe");
    Request.bIdempotent = true;

    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
    *Handle = SendRequestInternal(
        MoveTemp(Request),
        Options,
        [WeakThis, Handle, Started, Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            FGamePlatformOnlineProbeResult Result;
            Result.Request = *Handle;
            Result.ElapsedSeconds =
                FMath::Max(0.0, FPlatformTime::Seconds() - Started);
            Result.Error = ToOnlineError(Response.Error);
            Result.Result = ToCoreResult(Response.Error);

            if (Response.IsSuccess())
            {
                TSharedPtr<FJsonObject> Json;
                bool bReady = false;
                FString ContractVersion;
                FString Service;
                const TSharedRef<TJsonReader<>> Reader =
                    TJsonReaderFactory<>::Create(Response.Body);
                if (!FJsonSerializer::Deserialize(Reader, Json) ||
                    !Json.IsValid() ||
                    !Json->TryGetBoolField(TEXT("ready"), bReady) ||
                    !Json->TryGetStringField(
                        TEXT("contractVersion"),
                        ContractVersion) ||
                    !Json->TryGetStringField(TEXT("service"), Service) ||
                    !bReady ||
                    Service != TEXT("gatewayservice"))
                {
                    Result.Error = EGamePlatformOnlineError::InvalidResponse;
                    Result.Result = FGamePlatformResult::Failure(
                        TEXT("OnlineProbeInvalidResponse"),
                        TEXT("在线探测响应结构或服务身份无效。"));
                    Self->ServiceState =
                        EGamePlatformOnlineServiceState::Unavailable;
                }
                else if (
                    ContractVersion !=
                    Self->Configuration.RequiredContractVersion)
                {
                    Result.Error =
                        EGamePlatformOnlineError::IncompatibleProtocol;
                    Result.Result = FGamePlatformResult::Failure(
                        TEXT("OnlineContractIncompatible"),
                        TEXT("在线服务契约版本与客户端要求不一致。"));
                    Result.ContractVersion = MoveTemp(ContractVersion);
                    Self->ServiceState =
                        EGamePlatformOnlineServiceState::Incompatible;
                }
                else
                {
                    Result.bReady = true;
                    Result.ContractVersion = MoveTemp(ContractVersion);
                    Result.Error = EGamePlatformOnlineError::None;
                    Result.Result = FGamePlatformResult::Success();
                    Self->ServiceState =
                        EGamePlatformOnlineServiceState::Ready;
                }
            }
            else
            {
                Self->ServiceState =
                    EGamePlatformOnlineServiceState::Unavailable;
            }

            if (Completion)
            {
                Completion(Result);
            }
        },
        false);

    return *Handle;
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::Login(
    FGamePlatformOnlineLoginRequest Request,
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformOnlineAuthenticationCompletion Completion)
{
    check(IsInGameThread());

    FGamePlatformOnlineRequestHandle Handle;
    Handle.InstanceScopeId = InstanceScopeId;
    Handle.RequestId = FGuid::NewGuid();
    const double Started = FPlatformTime::Seconds();

    if (!Completion)
    {
        return Handle;
    }

    auto CompleteLater =
        [this, Handle, Started, &Completion](
            EGamePlatformAuthError Error) mutable
        {
            FGamePlatformOnlineAuthenticationCompletion Deferred =
                MoveTemp(Completion);
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this),
                 Handle,
                 Started,
                 Completion = MoveTemp(Deferred),
                 Error]() mutable
                {
                    UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                    if (!Self || !Completion)
                    {
                        return;
                    }

                    FGamePlatformOnlineAuthenticationResult Result;
                    Result.Request = Handle;
                    Result.ElapsedSeconds =
                        FMath::Max(
                            0.0,
                            FPlatformTime::Seconds() - Started);
                    Result.Error = ToOnlineError(Error);
                    Result.Result = ToCoreResult(Error);
                    Result.Authentication = Self->GetAuthentication();
                    Completion(Result);
                });
        };

    if (!Runtime ||
        !Runtime->Provider.IsValid() ||
        !bConfigured)
    {
        CompleteLater(EGamePlatformAuthError::ProviderUnavailable);
        return Handle;
    }

    if (!IsRequestAlive(Options) ||
        !FMath::IsFinite(Options.DeadlineSeconds) ||
        Options.DeadlineSeconds < 0.0 ||
        Options.DeadlineSeconds > Configuration.RequestDeadlineSeconds)
    {
        CompleteLater(EGamePlatformAuthError::InvalidRequest);
        return Handle;
    }

    // 先判断认证上下文是否繁忙，再校验第二次调用的凭据内容。
    // 被拒绝的并发登录（即使参数本身无效）没有资格推进认证代次或清理首个合法登录。
    if (Runtime->LoginOperationId.IsValid() ||
        Snapshot.State == EGamePlatformAuthState::LoggingIn ||
        Snapshot.State == EGamePlatformAuthState::Authenticated ||
        Snapshot.State == EGamePlatformAuthState::Refreshing ||
        Snapshot.State == EGamePlatformAuthState::LoggingOut)
    {
        Snapshot.Error = EGamePlatformAuthError::AuthenticationBusy;
        BroadcastSnapshot();
        CompleteLater(EGamePlatformAuthError::AuthenticationBusy);
        return Handle;
    }

    const FString Normalized = Request.AccountName.TrimStartAndEnd();
    if (Normalized.IsEmpty() ||
        Request.Credential.IsEmpty() ||
        Normalized.Len() > MaxLoginNameChars ||
        Request.Credential.Len() > MaxPasswordChars)
    {
        ResetAuthentication(
            EGamePlatformAuthState::Failed,
            EGamePlatformAuthError::InvalidCredentials,
            true);
        CompleteLater(EGamePlatformAuthError::InvalidCredentials);
        return Handle;
    }

    Runtime->LoginOperationId = Handle.RequestId;
    Runtime->LoginCompletion = MoveTemp(Completion);
    Runtime->LoginOptions = Options;
    Runtime->LoginStartedSeconds = Started;

    ResetAuthentication(
        EGamePlatformAuthState::LoggingIn,
        EGamePlatformAuthError::None,
        true);
    const FGuid Generation = Snapshot.AuthGeneration;
    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);

    Runtime->Provider->LoginWithCredentials(
        Normalized,
        Request.Credential,
        [WeakThis, Generation, RequestId = Handle.RequestId](
            FGamePlatformAuthProviderResult ProviderResult) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 Generation,
                 RequestId,
                 ProviderResult = MoveTemp(ProviderResult)]() mutable
                {
                    UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                    if (!Self ||
                        !Self->Runtime ||
                        Self->Runtime->LoginOperationId != RequestId ||
                        Self->Snapshot.AuthGeneration != Generation)
                    {
                        return;
                    }

                    FGamePlatformOnlineAuthenticationCompletion Done =
                        MoveTemp(Self->Runtime->LoginCompletion);
                    const FGamePlatformOnlineRequestOptions Options =
                        Self->Runtime->LoginOptions;
                    const double Started =
                        Self->Runtime->LoginStartedSeconds;
                    Self->Runtime->LoginOperationId.Invalidate();
                    Self->Runtime->LoginCompletion = {};

                    if (!Self->IsRequestAlive(Options))
                    {
                        if (Self->Runtime->Provider.IsValid())
                        {
                            Self->Runtime->Provider->CancelAll();
                        }
                        Self->ResetAuthentication(
                            EGamePlatformAuthState::LoggedOut,
                            EGamePlatformAuthError::Cancelled,
                            true);
                    }
                    else
                    {
                        Self->CompleteAuthentication(
                            Generation,
                            MoveTemp(ProviderResult),
                            false);
                    }

                    if (Done)
                    {
                        const EGamePlatformAuthError FinalError =
                            Self->Snapshot.State ==
                                    EGamePlatformAuthState::Authenticated
                                ? EGamePlatformAuthError::None
                                : (Self->Snapshot.Error ==
                                           EGamePlatformAuthError::None
                                       ? EGamePlatformAuthError::Unknown
                                       : Self->Snapshot.Error);

                        FGamePlatformOnlineAuthenticationResult Result;
                        Result.Request.InstanceScopeId =
                            Self->InstanceScopeId;
                        Result.Request.RequestId = RequestId;
                        Result.ElapsedSeconds =
                            FMath::Max(
                                0.0,
                                FPlatformTime::Seconds() - Started);
                        Result.Error = ToOnlineError(FinalError);
                        Result.Result = ToCoreResult(FinalError);
                        Result.Authentication = Self->GetAuthentication();
                        Done(Result);
                    }
                });
        });

    EnsureTicker();
    return Handle;
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::GetCurrentPlayerProfile(
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformOnlineProfileCompletion Completion)
{
    check(IsInGameThread());

    const double Started = FPlatformTime::Seconds();
    const FString ExpectedPlayerId = Snapshot.AccountId;
    const FString ExpectedGameId = Configuration.GameId;
    const TSharedRef<FGamePlatformOnlineRequestHandle> Handle =
        MakeShared<FGamePlatformOnlineRequestHandle>();

    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = TEXT("GET");
    Request.RelativePath = TEXT("/v1/player/profile");
    Request.bIdempotent = true;

    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
    *Handle = SendAuthenticatedRequest(
        MoveTemp(Request),
        Options,
        [WeakThis,
         Handle,
         Started,
         ExpectedPlayerId,
         ExpectedGameId,
         Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            FGamePlatformOnlineProfileResult Result;
            Result.Request = *Handle;
            Result.ElapsedSeconds =
                FMath::Max(0.0, FPlatformTime::Seconds() - Started);
            Result.Error = ToOnlineError(Response.Error);
            Result.Result = ToCoreResult(Response.Error);

            if (Response.IsSuccess())
            {
                FGamePlatformOnlineProfile Parsed;
                if (!ParseProfilePayload(
                        Response.Body,
                        ExpectedPlayerId,
                        ExpectedGameId,
                        Parsed))
                {
                    Result.Error = EGamePlatformOnlineError::InvalidResponse;
                    Result.Result = FGamePlatformResult::Failure(
                        TEXT("OnlineProfileInvalidResponse"),
                        TEXT("玩家资料响应字段、主体、游戏身份或修订号无效。"));
                }
                else
                {
                    Result.Profile = Parsed;
                    if (!Self->CachedProfile.IsSet() ||
                        Parsed.Revision >=
                            Self->CachedProfile->Revision)
                    {
                        Self->CachedProfile = MoveTemp(Parsed);
                    }
                }
            }

            if (Completion)
            {
                Completion(Result);
            }
        });

    return *Handle;
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::UpdateCurrentPlayerProfile(
    const FGamePlatformOnlineProfileUpdateRequest& Request,
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformOnlineProfileCompletion Completion)
{
    check(IsInGameThread());

    const FString DisplayName = Request.DisplayName.TrimStartAndEnd();
    FString Body;
    if (!IsValidDisplayName(DisplayName) ||
        Request.ExpectedRevision < 0 ||
        !IsPrintableAsciiToken(Request.IdempotencyKey, 128) ||
        !SerializeProfileUpdatePayload(
            DisplayName,
            Request.ExpectedRevision,
            Body))
    {
        FGamePlatformOnlineRequestHandle Handle;
        Handle.InstanceScopeId = InstanceScopeId;
        Handle.RequestId = FGuid::NewGuid();
        AsyncTask(
            ENamedThreads::GameThread,
            [Handle, Completion = MoveTemp(Completion)]() mutable
            {
                if (!Completion)
                {
                    return;
                }
                FGamePlatformOnlineProfileResult Result;
                Result.Request = Handle;
                Result.Error = EGamePlatformOnlineError::InvalidArgument;
                Result.Result = FGamePlatformResult::Failure(
                    TEXT("OnlineProfileUpdateInvalid"),
                    TEXT("资料更新参数、修订号或幂等键无效。"));
                Completion(Result);
            });
        return Handle;
    }

    const double Started = FPlatformTime::Seconds();
    const FString ExpectedPlayerId = Snapshot.AccountId;
    const FString ExpectedGameId = Configuration.GameId;
    const TSharedRef<FGamePlatformOnlineRequestHandle> Handle =
        MakeShared<FGamePlatformOnlineRequestHandle>();

    FGamePlatformAuthenticatedRequest TransportRequest;
    TransportRequest.Verb = TEXT("PATCH");
    TransportRequest.RelativePath = TEXT("/v1/player/profile");
    TransportRequest.Body = MoveTemp(Body);
    TransportRequest.IdempotencyKey = Request.IdempotencyKey;
    TransportRequest.bIdempotent = true;

    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
    *Handle = SendAuthenticatedRequest(
        MoveTemp(TransportRequest),
        Options,
        [WeakThis,
         Handle,
         Started,
         ExpectedPlayerId,
         ExpectedGameId,
         Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            FGamePlatformOnlineProfileResult Result;
            Result.Request = *Handle;
            Result.ElapsedSeconds =
                FMath::Max(0.0, FPlatformTime::Seconds() - Started);
            Result.Error = ToOnlineError(Response.Error);
            Result.Result = ToCoreResult(Response.Error);

            if (Response.IsSuccess())
            {
                FGamePlatformOnlineProfile Parsed;
                if (!ParseProfilePayload(
                        Response.Body,
                        ExpectedPlayerId,
                        ExpectedGameId,
                        Parsed))
                {
                    Result.Error = EGamePlatformOnlineError::InvalidResponse;
                    Result.Result = FGamePlatformResult::Failure(
                        TEXT("OnlineProfileInvalidResponse"),
                        TEXT("资料更新成功响应结构或修订号无效。"));
                }
                else
                {
                    Result.Profile = Parsed;
                    if (!Self->CachedProfile.IsSet() ||
                        Parsed.Revision >=
                            Self->CachedProfile->Revision)
                    {
                        Self->CachedProfile = MoveTemp(Parsed);
                    }
                }
            }

            if (Completion)
            {
                Completion(Result);
            }
        });

    return *Handle;
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::RefreshAuthentication(
    const FGamePlatformOnlineRequestOptions& Options,
    FGamePlatformOnlineAuthenticationCompletion Completion)
{
    check(IsInGameThread());

    FGamePlatformOnlineRequestHandle Handle;
    Handle.InstanceScopeId = InstanceScopeId;
    Handle.RequestId = FGuid::NewGuid();

    if (!Completion)
    {
        return Handle;
    }

    auto CompleteLater =
        [this, Handle, &Completion](
            EGamePlatformAuthError Error) mutable
        {
            FGamePlatformOnlineAuthenticationCompletion Deferred =
                MoveTemp(Completion);
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this),
                 Handle,
                 Completion = MoveTemp(Deferred),
                 Error]() mutable
                {
                    UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                    if (!Self || !Completion)
                    {
                        return;
                    }
                    FGamePlatformOnlineAuthenticationResult Result;
                    Result.Request = Handle;
                    Result.Error = ToOnlineError(Error);
                    Result.Result = ToCoreResult(Error);
                    Result.Authentication = Self->GetAuthentication();
                    Completion(Result);
                });
        };

    if (!Runtime ||
        !Runtime->Provider.IsValid() ||
        !bConfigured)
    {
        CompleteLater(EGamePlatformAuthError::ProviderUnavailable);
        return Handle;
    }
    if (!IsRequestAlive(Options) ||
        !FMath::IsFinite(Options.DeadlineSeconds) ||
        Options.DeadlineSeconds < 0.0 ||
        Options.DeadlineSeconds > Configuration.RequestDeadlineSeconds)
    {
        CompleteLater(EGamePlatformAuthError::InvalidRequest);
        return Handle;
    }
    if (Snapshot.State != EGamePlatformAuthState::Authenticated &&
        Snapshot.State != EGamePlatformAuthState::Refreshing)
    {
        CompleteLater(EGamePlatformAuthError::AuthExpired);
        return Handle;
    }
    if (Runtime->RefreshOperations.Num() +
            Runtime->RefreshWaiters.Num() >=
        Configuration.MaxRefreshWaiters)
    {
        CompleteLater(EGamePlatformAuthError::QueueFull);
        return Handle;
    }

    FRuntime::FPendingRefreshOperation Pending;
    Pending.Options = Options;
    Pending.Completion = MoveTemp(Completion);
    Pending.StartedSeconds = FPlatformTime::Seconds();
    Runtime->RefreshOperations.Add(Handle.RequestId, MoveTemp(Pending));
    BeginRefreshSingleFlight();
    EnsureTicker();
    return Handle;
}

FGamePlatformOnlineRequestHandle
UGamePlatformOnlineClientSubsystem::Logout(
    FGamePlatformOnlineLogoutCompletion Completion)
{
    check(IsInGameThread());

    FGamePlatformOnlineRequestHandle Handle;
    Handle.InstanceScopeId = InstanceScopeId;
    Handle.RequestId = FGuid::NewGuid();

    if (!Runtime)
    {
        AsyncTask(
            ENamedThreads::GameThread,
            [Handle, Completion = MoveTemp(Completion)]() mutable
            {
                if (!Completion)
                {
                    return;
                }
                FGamePlatformOnlineLogoutResult Result;
                Result.Request = Handle;
                Result.Error = EGamePlatformOnlineError::InvalidConfiguration;
                Result.Result = FGamePlatformResult::Failure(
                    TEXT("OnlineRuntimeUnavailable"),
                    TEXT("在线运行时不存在。"));
                Completion(Result);
            });
        return Handle;
    }

    FRuntime::FPendingLogoutOperation PendingLogout;
    PendingLogout.Completion = MoveTemp(Completion);
    PendingLogout.StartedSeconds = FPlatformTime::Seconds();
    PendingLogout.bHadAuthentication =
        Snapshot.State == EGamePlatformAuthState::Authenticated ||
        Snapshot.State == EGamePlatformAuthState::Refreshing;
    Runtime->LogoutOperations.Add(
        Handle.RequestId,
        MoveTemp(PendingLogout));
    EnsureTicker();

    // 本地退出先完成：之后所有旧操作终态都只能观察到新代次未登录状态。
    ResetAuthentication(
        EGamePlatformAuthState::LoggedOut,
        EGamePlatformAuthError::None,
        true);

    if (Runtime->LoginOperationId.IsValid())
    {
        FGamePlatformOnlineAuthenticationCompletion LoginDone =
            MoveTemp(Runtime->LoginCompletion);
        const FGuid LoginRequestId = Runtime->LoginOperationId;
        Runtime->LoginOperationId.Invalidate();
        Runtime->LoginCompletion = {};
        if (LoginDone)
        {
            FGamePlatformOnlineAuthenticationResult Cancelled;
            Cancelled.Request.InstanceScopeId = InstanceScopeId;
            Cancelled.Request.RequestId = LoginRequestId;
            Cancelled.Error = EGamePlatformOnlineError::Cancelled;
            Cancelled.Result =
                FGamePlatformResult::Cancelled(TEXT("退出登录取消了登录等待。"));
            Cancelled.Authentication = GetAuthentication();
            AsyncTask(
                ENamedThreads::GameThread,
                [LoginDone = MoveTemp(LoginDone),
                 Cancelled = MoveTemp(Cancelled)]() mutable
                {
                    LoginDone(Cancelled);
                });
        }
    }

    TMap<FGuid, FRuntime::FPendingRefreshOperation> RefreshOperations =
        MoveTemp(Runtime->RefreshOperations);
    Runtime->RefreshOperations.Reset();
    for (TPair<FGuid, FRuntime::FPendingRefreshOperation>& Pair :
         RefreshOperations)
    {
        if (!Pair.Value.Completion)
        {
            continue;
        }
        FGamePlatformOnlineAuthenticationResult Cancelled;
        Cancelled.Request.InstanceScopeId = InstanceScopeId;
        Cancelled.Request.RequestId = Pair.Key;
        Cancelled.Error = EGamePlatformOnlineError::Cancelled;
        Cancelled.Result =
            FGamePlatformResult::Cancelled(TEXT("退出登录取消了刷新等待。"));
        Cancelled.Authentication = GetAuthentication();
        FGamePlatformOnlineAuthenticationCompletion Done =
            MoveTemp(Pair.Value.Completion);
        AsyncTask(
            ENamedThreads::GameThread,
            [Done = MoveTemp(Done),
             Cancelled = MoveTemp(Cancelled)]() mutable
            {
                Done(Cancelled);
            });
    }

    TArray<FGuid> Existing;
    Runtime->Requests.GetKeys(Existing);
    for (const FGuid& RequestId : Existing)
    {
        if (TSharedPtr<FRuntime::FPendingRequest>* ExistingRequest =
                Runtime->Requests.Find(RequestId);
            ExistingRequest &&
            (*ExistingRequest)->bStarted &&
            Runtime->Provider.IsValid())
        {
            Runtime->Provider->CancelRequest(RequestId);
            (*ExistingRequest)->bStarted = false;
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

    if (!Runtime->Provider.IsValid())
    {
        FRuntime::FPendingLogoutOperation Operation;
        if (Runtime->LogoutOperations.RemoveAndCopyValue(
                Handle.RequestId,
                Operation) &&
            Operation.Completion)
        {
            FGamePlatformOnlineLogoutResult Result;
            Result.Request = Handle;
            Result.Error = EGamePlatformOnlineError::ConnectionFailure;
            Result.Result = FGamePlatformResult::Failure(
                TEXT("OnlineLogoutProviderUnavailable"),
                TEXT("本地已退出，但无法确认服务端会话撤销。"));
            Result.Disposition =
                EGamePlatformOnlineLogoutDisposition::RevocationUnconfirmed;
            FGamePlatformOnlineLogoutCompletion Done =
                MoveTemp(Operation.Completion);
            AsyncTask(
                ENamedThreads::GameThread,
                [Done = MoveTemp(Done),
                 Result = MoveTemp(Result)]() mutable
                {
                    Done(Result);
                });
        }
        return Handle;
    }

    const TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakThis(this);
    Runtime->Provider->Logout(
        [WeakThis, Handle](
            FGamePlatformAuthProviderLogoutResult ProviderResult) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, Handle, ProviderResult]() mutable
                {
                    UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                    if (!Self || !Self->Runtime)
                    {
                        return;
                    }

                    FRuntime::FPendingLogoutOperation Operation;
                    if (!Self->Runtime->LogoutOperations.RemoveAndCopyValue(
                            Handle.RequestId,
                            Operation))
                    {
                        return;
                    }

                    FGamePlatformOnlineLogoutResult Result;
                    Result.Request = Handle;
                    Result.ElapsedSeconds =
                        FMath::Max(
                            0.0,
                            FPlatformTime::Seconds() -
                                Operation.StartedSeconds);

                    if (!Operation.bHadAuthentication)
                    {
                        Result.Result = FGamePlatformResult::Success();
                        Result.Disposition =
                            EGamePlatformOnlineLogoutDisposition::LocalSignedOut;
                    }
                    else if (ProviderResult.bServerRevoked)
                    {
                        Result.Result = FGamePlatformResult::Success();
                        Result.Disposition =
                            EGamePlatformOnlineLogoutDisposition::ServerRevoked;
                    }
                    else
                    {
                        const EGamePlatformAuthError Error =
                            ProviderResult.Error ==
                                    EGamePlatformAuthError::None
                                ? EGamePlatformAuthError::NetworkUnavailable
                                : ProviderResult.Error;
                        Result.Error = ToOnlineError(Error);
                        Result.Result = ToCoreResult(Error);
                        Result.Disposition =
                            EGamePlatformOnlineLogoutDisposition::RevocationUnconfirmed;
                    }

                    if (Operation.Completion)
                    {
                        Operation.Completion(Result);
                    }
                });
        });

    return Handle;
}

FGamePlatformOnlineAuthSnapshot
UGamePlatformOnlineClientSubsystem::GetAuthentication() const
{
    FGamePlatformOnlineAuthSnapshot Result;
    Result.AuthContextId = Snapshot.AuthGeneration;
    Result.PlayerId = Snapshot.AccountId;
    Result.AccessExpiresAt = Snapshot.AccessExpiresAt;
    Result.RefreshExpiresAt = Snapshot.RefreshExpiresAt;

    if (Runtime)
    {
        Result.AuthGeneration = Runtime->AuthGenerationCounter;
        Result.TokenVersion = Runtime->TokenVersion;
    }

    switch (Snapshot.State)
    {
    case EGamePlatformAuthState::LoggingIn:
        Result.State = EGamePlatformOnlineAuthState::SigningIn;
        break;
    case EGamePlatformAuthState::Authenticated:
        Result.State = EGamePlatformOnlineAuthState::SignedIn;
        break;
    case EGamePlatformAuthState::Refreshing:
        Result.State = EGamePlatformOnlineAuthState::Refreshing;
        break;
    case EGamePlatformAuthState::Failed:
        Result.State =
            (Snapshot.Error == EGamePlatformAuthError::AuthExpired ||
             Snapshot.Error == EGamePlatformAuthError::OutcomeUnknown)
                ? EGamePlatformOnlineAuthState::ReauthenticationRequired
                : EGamePlatformOnlineAuthState::SignedOut;
        break;
    default:
        Result.State = EGamePlatformOnlineAuthState::SignedOut;
        break;
    }
    return Result;
}

TOptional<FGamePlatformOnlineProfile>
UGamePlatformOnlineClientSubsystem::GetCachedProfile() const
{
    return CachedProfile;
}

void UGamePlatformOnlineClientSubsystem::SetProvider(
    TSharedPtr<IGamePlatformOnlineAuthProvider> InProvider)
{
    check(IsInGameThread());
    if (!Runtime || Runtime->Provider == InProvider)
    {
        return;
    }


    if (Runtime->LoginOperationId.IsValid())
    {
        FGamePlatformOnlineRequestHandle Handle;
        Handle.InstanceScopeId = InstanceScopeId;
        Handle.RequestId = Runtime->LoginOperationId;
        Cancel(Handle);
    }

    TArray<FGuid> RefreshOperationIds;
    Runtime->RefreshOperations.GetKeys(RefreshOperationIds);
    for (const FGuid& OperationId : RefreshOperationIds)
    {
        FGamePlatformOnlineRequestHandle Handle;
        Handle.InstanceScopeId = InstanceScopeId;
        Handle.RequestId = OperationId;
        Cancel(Handle);
    }

    TArray<FGuid> LogoutOperationIds;
    Runtime->LogoutOperations.GetKeys(LogoutOperationIds);
    for (const FGuid& OperationId : LogoutOperationIds)
    {
        FGamePlatformOnlineRequestHandle Handle;
        Handle.InstanceScopeId = InstanceScopeId;
        Handle.RequestId = OperationId;
        Cancel(Handle);
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
    FGamePlatformOnlineLoginRequest Request;
    Request.AccountName = LoginName;
    Request.Credential = Password;
    Login(
        MoveTemp(Request),
        FGamePlatformOnlineRequestOptions{},
        [](const FGamePlatformOnlineAuthenticationResult&) {});
}

void UGamePlatformOnlineClientSubsystem::Refresh()
{
    RefreshAuthentication(
        FGamePlatformOnlineRequestOptions{},
        [](const FGamePlatformOnlineAuthenticationResult&) {});
}

void UGamePlatformOnlineClientSubsystem::Logout()
{
    Logout([](const FGamePlatformOnlineLogoutResult&) {});
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

    if (!Completion)
    {
        return Handle;
    }

    auto CompleteLater =
        [&Completion](EGamePlatformAuthError Error) mutable
        {
            FGamePlatformAuthenticatedCompletion Deferred =
                MoveTemp(Completion);
            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Deferred), Error]() mutable
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
    if (Request.Verb.IsEmpty() ||
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

    if (Runtime->LoginOperationId == Request.RequestId)
    {
        FGamePlatformOnlineAuthenticationCompletion Done =
            MoveTemp(Runtime->LoginCompletion);
        Runtime->LoginOperationId.Invalidate();
        Runtime->LoginCompletion = {};
        if (Runtime->Provider.IsValid())
        {
            Runtime->Provider->InvalidateAuthenticationOperation();
        }
        ResetAuthentication(
            EGamePlatformAuthState::LoggedOut,
            EGamePlatformAuthError::Cancelled,
            true);

        AsyncTask(
            ENamedThreads::GameThread,
            [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this),
             Request,
             Done = MoveTemp(Done)]() mutable
            {
                UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                if (!Self || !Done)
                {
                    return;
                }

                FGamePlatformOnlineAuthenticationResult Result;
                Result.Request = Request;
                Result.Error = EGamePlatformOnlineError::Cancelled;
                Result.Result =
                    FGamePlatformResult::Cancelled(TEXT("登录等待已取消。"));
                Result.Authentication = Self->GetAuthentication();
                Done(Result);
            });
        return true;
    }

    FRuntime::FPendingRefreshOperation RefreshOperation;
    if (Runtime->RefreshOperations.RemoveAndCopyValue(
            Request.RequestId,
            RefreshOperation))
    {
        AsyncTask(
            ENamedThreads::GameThread,
            [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this),
             Request,
             Operation = MoveTemp(RefreshOperation)]() mutable
            {
                UGamePlatformOnlineClientSubsystem* Self = WeakThis.Get();
                if (!Self || !Operation.Completion)
                {
                    return;
                }

                FGamePlatformOnlineAuthenticationResult Result;
                Result.Request = Request;
                Result.ElapsedSeconds =
                    FMath::Max(
                        0.0,
                        FPlatformTime::Seconds() -
                            Operation.StartedSeconds);
                Result.Error = EGamePlatformOnlineError::Cancelled;
                Result.Result =
                    FGamePlatformResult::Cancelled(TEXT("刷新等待已取消。"));
                Result.Authentication = Self->GetAuthentication();
                Operation.Completion(Result);
            });
        return true;
    }

    FRuntime::FPendingLogoutOperation LogoutOperation;
    if (Runtime->LogoutOperations.RemoveAndCopyValue(
            Request.RequestId,
            LogoutOperation))
    {
        // 本地退出已经不可回滚；取消仅停止调用方等待远端撤销结果。
        AsyncTask(
            ENamedThreads::GameThread,
            [Request, Operation = MoveTemp(LogoutOperation)]() mutable
            {
                if (!Operation.Completion)
                {
                    return;
                }

                FGamePlatformOnlineLogoutResult Result;
                Result.Request = Request;
                Result.ElapsedSeconds =
                    FMath::Max(
                        0.0,
                        FPlatformTime::Seconds() -
                            Operation.StartedSeconds);
                Result.Error = EGamePlatformOnlineError::Cancelled;
                Result.Result = FGamePlatformResult::Cancelled(
                    TEXT("退出等待已取消；本地退出保持有效。"));
                Result.Disposition =
                    EGamePlatformOnlineLogoutDisposition::RevocationUnconfirmed;
                Operation.Completion(Result);
            });
        return true;
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

    if (!Runtime ||
        !Runtime->Provider.IsValid() ||
        Snapshot.State != EGamePlatformAuthState::Authenticated ||
        Configuration.ServiceOrigin.IsEmpty())
    {
        return false;
    }

    // 只允许给当前已配置 Gateway 的同源请求附加认证。
    // 即使调用方属于可信平台模块，也不能把 Bearer Token 注入任意外部 URL。
    const FString RequestUrl = Request.GetURL();
    const FString SameOriginPrefix = Configuration.ServiceOrigin + TEXT("/");
    if (!RequestUrl.StartsWith(SameOriginPrefix, ESearchCase::CaseSensitive))
    {
        return false;
    }

    return Runtime->Provider->ApplyAuthorization(Request);
}

FGamePlatformOnlineDiagnostics
UGamePlatformOnlineClientSubsystem::GetDiagnostics() const
{
    FGamePlatformOnlineDiagnostics Result;
    Result.InstanceScopeId = InstanceScopeId;
    Result.bConfigured = bConfigured;
    Result.ServiceState = ServiceState;

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
            (Snapshot.Error == EGamePlatformAuthError::AuthExpired ||
             Snapshot.Error == EGamePlatformAuthError::OutcomeUnknown)
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
        Result.RefreshWaiters =
            Runtime->RefreshWaiters.Num() +
            Runtime->RefreshOperations.Num();
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
        if (Runtime)
        {
            ++Runtime->AuthGenerationCounter;
            Runtime->TokenVersion = 0;
        }
        CachedProfile.Reset();
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
        if (Runtime)
        {
            if (bIsRefresh)
            {
                ++Runtime->TokenVersion;
            }
            else
            {
                Runtime->TokenVersion = 1;
            }
            Runtime->LastError = EGamePlatformAuthError::None;
        }
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
            TMap<FGuid, FRuntime::FPendingRefreshOperation> Operations =
                MoveTemp(Runtime->RefreshOperations);
            Runtime->RefreshOperations.Reset();
            const double Now = FPlatformTime::Seconds();
            for (TPair<FGuid, FRuntime::FPendingRefreshOperation>& Pair : Operations)
            {
                if (!Pair.Value.Completion)
                {
                    continue;
                }

                FGamePlatformOnlineAuthenticationResult PublicResult;
                PublicResult.Request.InstanceScopeId = InstanceScopeId;
                PublicResult.Request.RequestId = Pair.Key;
                PublicResult.ElapsedSeconds =
                    FMath::Max(0.0, Now - Pair.Value.StartedSeconds);
                const bool bAlive = IsRequestAlive(Pair.Value.Options);
                PublicResult.Error = bAlive
                    ? EGamePlatformOnlineError::None
                    : EGamePlatformOnlineError::Cancelled;
                PublicResult.Result = bAlive
                    ? FGamePlatformResult::Success()
                    : FGamePlatformResult::Cancelled(
                        TEXT("刷新等待者生命周期已结束。"));
                PublicResult.Authentication = GetAuthentication();
                Pair.Value.Completion(PublicResult);
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
    CachedProfile.Reset();
    if (Runtime)
    {
        Runtime->TokenVersion = 0;
        Runtime->LastError = Error;
    }
    BroadcastSnapshot();

    if (bIsRefresh)
    {
        TMap<FGuid, FRuntime::FPendingRefreshOperation> Operations =
            MoveTemp(Runtime->RefreshOperations);
        Runtime->RefreshOperations.Reset();
        const double Now = FPlatformTime::Seconds();
        for (TPair<FGuid, FRuntime::FPendingRefreshOperation>& Pair : Operations)
        {
            if (!Pair.Value.Completion)
            {
                continue;
            }

            FGamePlatformOnlineAuthenticationResult PublicResult;
            PublicResult.Request.InstanceScopeId = InstanceScopeId;
            PublicResult.Request.RequestId = Pair.Key;
            PublicResult.ElapsedSeconds =
                FMath::Max(0.0, Now - Pair.Value.StartedSeconds);
            const EGamePlatformAuthError FinalError =
                IsRequestAlive(Pair.Value.Options)
                    ? Error
                    : EGamePlatformAuthError::Cancelled;
            PublicResult.Error = ToOnlineError(FinalError);
            PublicResult.Result = ToCoreResult(FinalError);
            PublicResult.Authentication = GetAuthentication();
            Pair.Value.Completion(PublicResult);
        }

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

    if (Runtime->RefreshWaiters.Num() +
            Runtime->RefreshOperations.Num() >=
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

    if (!Runtime || !Runtime->Provider.IsValid())
    {
        return;
    }

    while (Runtime->ActiveRequests <
               Configuration.MaxConcurrentRequests &&
           Runtime->ReadyQueueHead < Runtime->ReadyQueue.Num())
    {
        const FGuid RequestId =
            Runtime->ReadyQueue[Runtime->ReadyQueueHead++];

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

        if (Pending->bRequiresAuthentication &&
            Snapshot.State != EGamePlatformAuthState::Authenticated)
        {
            if (Snapshot.State == EGamePlatformAuthState::Refreshing)
            {
                QueueForRefresh(RequestId);
            }
            else
            {
                FGamePlatformAuthenticatedResponse Failed;
                Failed.Error = EGamePlatformAuthError::AuthExpired;
                CompleteRequest(RequestId, MoveTemp(Failed));
            }
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
        auto Completion =
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

                        const bool bSafeRead =
                            Pending->Request.Verb == TEXT("GET") ||
                            Pending->Request.Verb == TEXT("HEAD");
                        const bool bTransientReadFailure =
                            Response.Error ==
                                EGamePlatformAuthError::NetworkUnavailable ||
                            Response.Error ==
                                EGamePlatformAuthError::TimedOut ||
                            Response.Error ==
                                EGamePlatformAuthError::RateLimited ||
                            Response.Error ==
                                EGamePlatformAuthError::ServiceUnavailable;
                        if (bSafeRead &&
                            bTransientReadFailure &&
                            Pending->RetryCount <
                                Self->Configuration.MaxReadRetries)
                        {
                            const double Jitter =
                                0.8 +
                                static_cast<double>(
                                    (GetTypeHash(RequestId) +
                                     static_cast<uint32>(
                                         Pending->RetryCount * 131u)) %
                                    401u) /
                                    1000.0;
                            const double PolicyDelay =
                                Self->Configuration.RetryBaseDelaySeconds *
                                FMath::Pow(
                                    2.0,
                                    Pending->RetryCount) *
                                Jitter;
                            const double Delay =
                                FMath::Max(
                                    PolicyDelay,
                                    Response.RetryAfterSeconds);
                            const double Now = FPlatformTime::Seconds();
                            if (FMath::IsFinite(Delay) &&
                                Delay > 0.0 &&
                                Delay <=
                                    Self->Configuration.MaxRetryDelaySeconds &&
                                Now + Delay < Pending->DeadlineSeconds)
                            {
                                ++Pending->RetryCount;
                                Pending->ReadyAtSeconds = Now + Delay;
                                Self->EnsureTicker();
                                Self->PumpRequests();
                                return;
                            }
                        }

                        if (Pending->bRequiresAuthentication &&
                            (Response.HttpStatusCode == 401 ||
                             Response.Error ==
                                 EGamePlatformAuthError::AuthExpired))
                        {
                            if (!Pending->bAuthReplay)
                            {
                                Pending->bAuthReplay = true;
                                const bool bReplaySafe =
                                    Pending->Request.Verb == TEXT("GET") ||
                                    Pending->Request.Verb == TEXT("HEAD") ||
                                    (Pending->Request.bIdempotent &&
                                     !Pending->Request.IdempotencyKey.IsEmpty());

                                if (bReplaySafe)
                                {
                                    Self->QueueForRefresh(RequestId);
                                    Self->BeginRefreshSingleFlight();
                                    Self->PumpRequests();
                                    return;
                                }

                                // 非幂等写请求不自动重放；只刷新后续认证上下文。
                                // 当前写请求以 AuthExpired 结束，由业务层决定查询结果或补偿。
                                Self->BeginRefreshSingleFlight();
                                Response.Error =
                                    EGamePlatformAuthError::AuthExpired;
                                Self->CompleteRequest(
                                    RequestId,
                                    MoveTemp(Response));
                                Self->PumpRequests();
                                return;
                            }

                            // 刷新后同一逻辑请求再次401，认证上下文不可继续使用。
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
            };

        if (Pending->bRequiresAuthentication)
        {
            Runtime->Provider->SendAuthenticatedRequest(
                RequestId,
                Pending->Request,
                MoveTemp(Completion));
        }
        else
        {
            Runtime->Provider->SendUnauthenticatedRequest(
                RequestId,
                Pending->Request,
                MoveTemp(Completion));
        }
    }

    // 采用Head索引避免每次RemoveAt(0)搬移整个队列；批量消费后再有界压缩。
    if (Runtime->ReadyQueueHead > 64 &&
        Runtime->ReadyQueueHead * 2 >= Runtime->ReadyQueue.Num())
    {
        Runtime->ReadyQueue.RemoveAt(
            0,
            Runtime->ReadyQueueHead,
            EAllowShrinking::No);
        Runtime->ReadyQueueHead = 0;
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
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion),
             Response = MoveTemp(Response)]() mutable
            {
                Completion(MoveTemp(Response));
            });
    }

    if (Runtime->Requests.IsEmpty() &&
        !Runtime->bRefreshInFlight &&
        !Runtime->LoginOperationId.IsValid() &&
        Runtime->RefreshOperations.IsEmpty() &&
        Runtime->LogoutOperations.IsEmpty())
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
    if (Runtime->LoginOperationId.IsValid())
    {
        const double LoginBudget =
            Runtime->LoginOptions.DeadlineSeconds > 0.0
                ? Runtime->LoginOptions.DeadlineSeconds
                : Configuration.RequestDeadlineSeconds;
        const bool bAlive = IsRequestAlive(Runtime->LoginOptions);
        if (!bAlive ||
            Now >= Runtime->LoginStartedSeconds + LoginBudget)
        {
            const FGuid LoginRequestId = Runtime->LoginOperationId;
            const double LoginStartedSeconds =
                Runtime->LoginStartedSeconds;
            FGamePlatformOnlineAuthenticationCompletion Done =
                MoveTemp(Runtime->LoginCompletion);
            Runtime->LoginOperationId.Invalidate();
            Runtime->LoginCompletion = {};
            if (Runtime->Provider.IsValid())
            {
                Runtime->Provider->InvalidateAuthenticationOperation();
            }

            const EGamePlatformAuthError Error = bAlive
                ? EGamePlatformAuthError::TimedOut
                : EGamePlatformAuthError::Cancelled;
            ResetAuthentication(
                bAlive
                    ? EGamePlatformAuthState::Failed
                    : EGamePlatformAuthState::LoggedOut,
                Error,
                true);

            if (Done)
            {
                FGamePlatformOnlineAuthenticationResult Result;
                Result.Request.InstanceScopeId = InstanceScopeId;
                Result.Request.RequestId = LoginRequestId;
                Result.ElapsedSeconds =
                    FMath::Max(0.0, Now - LoginStartedSeconds);
                Result.Error = ToOnlineError(Error);
                Result.Result = ToCoreResult(Error);
                Result.Authentication = GetAuthentication();
                Done(Result);
            }
        }
    }

    TArray<FGuid> RefreshOperationIds;
    Runtime->RefreshOperations.GetKeys(RefreshOperationIds);
    for (const FGuid& OperationId : RefreshOperationIds)
    {
        FRuntime::FPendingRefreshOperation* Pending =
            Runtime->RefreshOperations.Find(OperationId);
        if (!Pending)
        {
            continue;
        }

        const double Budget =
            Pending->Options.DeadlineSeconds > 0.0
                ? Pending->Options.DeadlineSeconds
                : Configuration.RequestDeadlineSeconds;
        const bool bAlive = IsRequestAlive(Pending->Options);
        if (bAlive && Now < Pending->StartedSeconds + Budget)
        {
            continue;
        }

        FRuntime::FPendingRefreshOperation Operation;
        if (!Runtime->RefreshOperations.RemoveAndCopyValue(
                OperationId,
                Operation) ||
            !Operation.Completion)
        {
            continue;
        }

        const EGamePlatformAuthError Error = bAlive
            ? EGamePlatformAuthError::TimedOut
            : EGamePlatformAuthError::Cancelled;
        FGamePlatformOnlineAuthenticationResult Result;
        Result.Request.InstanceScopeId = InstanceScopeId;
        Result.Request.RequestId = OperationId;
        Result.ElapsedSeconds =
            FMath::Max(0.0, Now - Operation.StartedSeconds);
        Result.Error = ToOnlineError(Error);
        Result.Result = ToCoreResult(Error);
        Result.Authentication = GetAuthentication();
        Operation.Completion(Result);
    }

    TArray<FGuid> LogoutOperationIds;
    Runtime->LogoutOperations.GetKeys(LogoutOperationIds);
    for (const FGuid& OperationId : LogoutOperationIds)
    {
        FRuntime::FPendingLogoutOperation* Pending =
            Runtime->LogoutOperations.Find(OperationId);
        if (!Pending ||
            Now < Pending->StartedSeconds +
                Configuration.RevocationDeadlineSeconds)
        {
            continue;
        }

        FRuntime::FPendingLogoutOperation Operation;
        if (!Runtime->LogoutOperations.RemoveAndCopyValue(
                OperationId,
                Operation) ||
            !Operation.Completion)
        {
            continue;
        }

        FGamePlatformOnlineLogoutResult Result;
        Result.Request.InstanceScopeId = InstanceScopeId;
        Result.Request.RequestId = OperationId;
        Result.ElapsedSeconds =
            FMath::Max(0.0, Now - Operation.StartedSeconds);
        Result.Error = EGamePlatformOnlineError::Timeout;
        Result.Result = FGamePlatformResult::Failure(
            TEXT("OnlineLogoutRevocationTimeout"),
            TEXT("本地已退出，但远端撤销在截止时间内未确认。"));
        Result.Disposition =
            EGamePlatformOnlineLogoutDisposition::RevocationUnconfirmed;
        Operation.Completion(Result);
    }

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
            if (!Pending->bStarted &&
                !Pending->bWaitingRefresh &&
                Pending->ReadyAtSeconds > 0.0 &&
                Now >= Pending->ReadyAtSeconds)
            {
                Pending->ReadyAtSeconds = 0.0;
                Runtime->ReadyQueue.Add(RequestId);
            }
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
        !Runtime->bRefreshInFlight &&
        !Runtime->LoginOperationId.IsValid() &&
        Runtime->RefreshOperations.IsEmpty() &&
        Runtime->LogoutOperations.IsEmpty())
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
        !Runtime->bRefreshInFlight &&
        !Runtime->LoginOperationId.IsValid() &&
        Runtime->RefreshOperations.IsEmpty() &&
        Runtime->LogoutOperations.IsEmpty())
    {
        return;
    }

    Runtime->TickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UGamePlatformOnlineClientSubsystem::TickRequests),
            RequestTickerIntervalSeconds);
}

void UGamePlatformOnlineClientSubsystem::StopTicker()
{
    if (Runtime && Runtime->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(
            Runtime->TickerHandle);
        Runtime->TickerHandle.Reset();
    }
}

bool UGamePlatformOnlineClientSubsystem::IsRequestAlive(
    const FGamePlatformOnlineRequestOptions& Options) const
{
    if (!Options.Owner.IsExplicitlyNull() &&
        !Options.Owner.IsValid())
    {
        return false;
    }

    if (Options.Lifetime ==
        EGamePlatformOnlineRequestLifetime::World)
    {
        UWorld* World = Options.World.Get();
        return World &&
            World->GetGameInstance() == GetGameInstance();
    }

    return true;
}

void UGamePlatformOnlineClientSubsystem::BroadcastSnapshot()
{
    AuthStateChanged.Broadcast(Snapshot);
}

