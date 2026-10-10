// 平台客户端业务JSON适配；线程、生命周期与迁移合同见同名公开头。
#include "Transport/GamePlatformEntitlementGatewayHttpTransport.h"

#include "Dom/JsonObject.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 游戏线程中的本领域所有权账本；不保存Token，也不拥有Online/HTTP对象。
struct FGamePlatformEntitlementGatewayHttpTransport::FRuntime
{
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TArray<FGamePlatformOnlineRequestHandle> ActiveRequests;
    uint64 CancellationGeneration = 0;
};

FGamePlatformEntitlementGatewayHttpTransport::FGamePlatformEntitlementGatewayHttpTransport(UGamePlatformOnlineClientSubsystem* InOnlineSubsystem)
    : Runtime(MakeUnique<FRuntime>())
{
    check(IsInGameThread());
    Runtime->Online = InOnlineSubsystem;
}

FGamePlatformEntitlementGatewayHttpTransport::FGamePlatformEntitlementGatewayHttpTransport(FString InGatewayBaseUrl, FString InAccessToken)
    : Runtime(MakeUnique<FRuntime>())
{
    // 旧消费者须迁入Online组合根；不把传入票据复制到长期对象或日志。
    (void)InGatewayBaseUrl;
    InAccessToken.Reset();
}

FGamePlatformEntitlementGatewayHttpTransport::~FGamePlatformEntitlementGatewayHttpTransport()
{
    CancelAllRequests();
}

bool FGamePlatformEntitlementGatewayHttpTransport::IsConfigured() const
{
    check(IsInGameThread());
    const auto* Online = Runtime ? Runtime->Online.Get() : nullptr;
    if (!IsValid(Online)) { return false; }
    const auto State = Online->GetSnapshot().State;
    return State == EGamePlatformAuthState::Authenticated || State == EGamePlatformAuthState::Refreshing;
}

void FGamePlatformEntitlementGatewayHttpTransport::CancelAllRequests()
{
    check(IsInGameThread());
    if (!Runtime) { return; }
    ++Runtime->CancellationGeneration;
    auto Requests = MoveTemp(Runtime->ActiveRequests);
    Runtime->ActiveRequests.Reset();
    if (auto* Online = Runtime->Online.Get())
    {
        for (const auto& Request : Requests) { Online->Cancel(Request); }
    }
}

void FGamePlatformEntitlementGatewayHttpTransport::UnregisterRequest(const FGuid& RequestId)
{
    Runtime->ActiveRequests.RemoveAll([&RequestId](const auto& Handle) { return Handle.RequestId == RequestId; });
}

bool FGamePlatformEntitlementGatewayHttpTransport::StartJsonRequest(
    const FString& Path,
    TFunction<void(int32, const FString&, EGamePlatformEntitlementError)> Completion)
{
    check(IsInGameThread());
    if (!Completion || !IsConfigured() || Runtime->ActiveRequests.Num() >= 8) { return false; }
    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = TEXT("GET");
    Request.RelativePath = Path;
    // 只读请求允许Online重试；写命令没有端点级幂等合同前禁止自动重放。
    Request.bIdempotent = Request.Verb == TEXT("GET") || Request.Verb == TEXT("HEAD");
    const uint64 ExpectedCancellationGeneration = Runtime->CancellationGeneration;
    const TWeakPtr<FGamePlatformEntitlementGatewayHttpTransport, ESPMode::ThreadSafe> WeakSelf = AsShared();
    const auto HandleBox = MakeShared<FGamePlatformOnlineRequestHandle, ESPMode::ThreadSafe>();
    auto Handle = Runtime->Online->SendAuthenticatedRequest(
        MoveTemp(Request), FGamePlatformOnlineRequestOptions(),
        [WeakSelf, HandleBox, ExpectedCancellationGeneration, Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            const auto Self = WeakSelf.Pin();
            if (!Self || !Self->Runtime || Self->Runtime->CancellationGeneration != ExpectedCancellationGeneration) { return; }
            Self->UnregisterRequest(HandleBox->RequestId);
            EGamePlatformEntitlementError Error = EGamePlatformEntitlementError::None;
            if (!Response.IsSuccess())
            {
                if (Response.Error == EGamePlatformAuthError::AuthExpired ||
                    Response.Error == EGamePlatformAuthError::InvalidCredentials ||
                    Response.Error == EGamePlatformAuthError::Forbidden)
                { Error = EGamePlatformEntitlementError::Unauthorized; }
                else if (Response.Error == EGamePlatformAuthError::Cancelled)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformEntitlementError::OutcomeUnknown : EGamePlatformEntitlementError::Cancelled; }
                else if (Response.Error == EGamePlatformAuthError::TimedOut)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformEntitlementError::OutcomeUnknown : EGamePlatformEntitlementError::TimedOut; }
                else if (Response.Error == EGamePlatformAuthError::OutcomeUnknown)
                { Error = EGamePlatformEntitlementError::OutcomeUnknown; }
                else { Error = MapHttpError(Response.HttpStatusCode); }
            }
            Completion(Response.HttpStatusCode, Response.Body, Error);
        });
    *HandleBox = Handle;
    if (!Handle.RequestId.IsValid()) { return false; }
    Runtime->ActiveRequests.Add(Handle);
    return true;
}

bool FGamePlatformEntitlementGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformEntitlementSnapshotCompletion Completion)
{
    if (!Completion) { return false; }
    return StartJsonRequest(
        TEXT("/v1/entitlements"),
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body,
            EGamePlatformEntitlementError TransportError) mutable
        {
            if (TransportError != EGamePlatformEntitlementError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion({}, TransportError != EGamePlatformEntitlementError::None ? TransportError : MapHttpError(StatusCode));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformEntitlementSnapshot Snapshot;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToSnapshot(Json, Snapshot))
            {
                Completion(
                    {},
                    EGamePlatformEntitlementError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Snapshot),
                EGamePlatformEntitlementError::None);
        });
}

bool FGamePlatformEntitlementGatewayHttpTransport::JsonToSnapshot(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformEntitlementSnapshot& OutSnapshot)
{
    if (!Json.IsValid())
    {
        return false;
    }

    double Revision = 0.0;
    FString GeneratedAt;
    if (!Json->TryGetNumberField(TEXT("revision"), Revision) ||
        !FMath::IsFinite(Revision) || Revision < 1.0 || Revision > 9007199254740991.0 || FMath::FloorToDouble(Revision) != Revision ||
        !Json->TryGetStringField(TEXT("generated_at"), GeneratedAt) ||
        !FDateTime::ParseIso8601(*GeneratedAt, OutSnapshot.GeneratedAtUtc))
    {
        return false;
    }

    OutSnapshot.Revision = static_cast<int64>(Revision);

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Json->TryGetArrayField(TEXT("entitlements"), Values) || !Values)
    {
        // 空数组是合法清空；缺字段或错误类型不是空快照，不得覆盖已有只读状态。
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Object =
            Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            return false;
        }

        FString EntitlementId;
        FString Category;
        FString TargetType;
        FString TargetId;
        FString Status;

        if (!Object->TryGetStringField(TEXT("entitlement_id"), EntitlementId) ||
            !Object->TryGetStringField(TEXT("category"), Category) ||
            !Object->TryGetStringField(TEXT("target_type"), TargetType) ||
            !Object->TryGetStringField(TEXT("target_id"), TargetId) ||
            !Object->TryGetStringField(TEXT("status"), Status))
        {
            return false;
        }

        FGamePlatformEntitlementEntry Entry;
        Entry.EntitlementId = FName(*EntitlementId);
        Entry.Category = FName(*Category);
        Entry.TargetId = FName(*TargetId);

        if (TargetType == TEXT("hero"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Hero;
        else if (TargetType == TEXT("skin"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Skin;
        else if (TargetType == TEXT("cosmetic"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Cosmetic;
        else if (TargetType == TEXT("feature"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Feature;
        else
            Entry.TargetType = EGamePlatformEntitlementTargetType::Unknown;

        if (Status == TEXT("active"))
            Entry.Status = EGamePlatformEntitlementStatus::Active;
        else if (Status == TEXT("not_started"))
            Entry.Status = EGamePlatformEntitlementStatus::NotStarted;
        else if (Status == TEXT("expired"))
            Entry.Status = EGamePlatformEntitlementStatus::Expired;
        else
            Entry.Status = EGamePlatformEntitlementStatus::Revoked;

        FString Optional;
        if (Object->TryGetStringField(TEXT("starts_at"), Optional) &&
            !Optional.IsEmpty())
        {
            Entry.bHasStartsAt =
                FDateTime::ParseIso8601(*Optional, Entry.StartsAtUtc);
        }

        if (Object->TryGetStringField(TEXT("expires_at"), Optional) &&
            !Optional.IsEmpty())
        {
            Entry.bHasExpiresAt =
                FDateTime::ParseIso8601(*Optional, Entry.ExpiresAtUtc);
        }

        if (Entry.EntitlementId.IsNone() ||
            Entry.TargetType == EGamePlatformEntitlementTargetType::Unknown ||
            Entry.TargetId.IsNone())
        {
            return false;
        }

        OutSnapshot.Entitlements.Add(MoveTemp(Entry));
    }

    return true;
}

EGamePlatformEntitlementError
FGamePlatformEntitlementGatewayHttpTransport::MapHttpError(
    int32 StatusCode)
{
    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformEntitlementError::Unauthorized;
    }

    if (StatusCode == 0 || StatusCode >= 500)
    {
        return EGamePlatformEntitlementError::BackendUnavailable;
    }

    return EGamePlatformEntitlementError::OutcomeUnknown;
}
