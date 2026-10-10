// 平台客户端业务JSON适配；线程、生命周期与迁移合同见同名公开头。
#include "Transport/GamePlatformProgressionGatewayHttpTransport.h"

#include "Dom/JsonObject.h"
#include "JsonIntegerPolicy.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 游戏线程中的本领域所有权账本；不保存Token，也不拥有Online/HTTP对象。
struct FGamePlatformProgressionGatewayHttpTransport::FRuntime
{
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TArray<FGamePlatformOnlineRequestHandle> ActiveRequests;
    uint64 CancellationGeneration = 0;
};

FGamePlatformProgressionGatewayHttpTransport::FGamePlatformProgressionGatewayHttpTransport(UGamePlatformOnlineClientSubsystem* InOnlineSubsystem)
    : Runtime(MakeUnique<FRuntime>())
{
    check(IsInGameThread());
    Runtime->Online = InOnlineSubsystem;
}

FGamePlatformProgressionGatewayHttpTransport::FGamePlatformProgressionGatewayHttpTransport(FString InGatewayBaseUrl, FString InAccessToken)
    : Runtime(MakeUnique<FRuntime>())
{
    // 旧消费者须迁入Online组合根；不把传入票据复制到长期对象或日志。
    (void)InGatewayBaseUrl;
    InAccessToken.Reset();
}

FGamePlatformProgressionGatewayHttpTransport::~FGamePlatformProgressionGatewayHttpTransport()
{
    CancelAllRequests();
}

bool FGamePlatformProgressionGatewayHttpTransport::IsConfigured() const
{
    check(IsInGameThread());
    const auto* Online = Runtime ? Runtime->Online.Get() : nullptr;
    if (!IsValid(Online)) { return false; }
    const auto State = Online->GetSnapshot().State;
    return State == EGamePlatformAuthState::Authenticated || State == EGamePlatformAuthState::Refreshing;
}

void FGamePlatformProgressionGatewayHttpTransport::CancelAllRequests()
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

void FGamePlatformProgressionGatewayHttpTransport::UnregisterRequest(const FGuid& RequestId)
{
    Runtime->ActiveRequests.RemoveAll([&RequestId](const auto& Handle) { return Handle.RequestId == RequestId; });
}

bool FGamePlatformProgressionGatewayHttpTransport::StartRequest(
    const FString& Path,
    TFunction<void(int32, const FString&, EGamePlatformProgressionError)> Completion)
{
    check(IsInGameThread());
    if (!Completion || !IsConfigured() || Runtime->ActiveRequests.Num() >= 8) { return false; }
    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = TEXT("GET");
    Request.RelativePath = Path;
    // 只读请求允许Online重试；写命令没有端点级幂等合同前禁止自动重放。
    Request.bIdempotent = Request.Verb == TEXT("GET") || Request.Verb == TEXT("HEAD");
    const uint64 ExpectedCancellationGeneration = Runtime->CancellationGeneration;
    const TWeakPtr<FGamePlatformProgressionGatewayHttpTransport, ESPMode::ThreadSafe> WeakSelf = AsShared();
    const auto HandleBox = MakeShared<FGamePlatformOnlineRequestHandle, ESPMode::ThreadSafe>();
    auto Handle = Runtime->Online->SendAuthenticatedRequest(
        MoveTemp(Request), FGamePlatformOnlineRequestOptions(),
        [WeakSelf, HandleBox, ExpectedCancellationGeneration, Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            const auto Self = WeakSelf.Pin();
            if (!Self || !Self->Runtime || Self->Runtime->CancellationGeneration != ExpectedCancellationGeneration) { return; }
            Self->UnregisterRequest(HandleBox->RequestId);
            EGamePlatformProgressionError Error = EGamePlatformProgressionError::None;
            if (!Response.IsSuccess())
            {
                if (Response.Error == EGamePlatformAuthError::AuthExpired ||
                    Response.Error == EGamePlatformAuthError::InvalidCredentials ||
                    Response.Error == EGamePlatformAuthError::Forbidden)
                { Error = EGamePlatformProgressionError::Unauthorized; }
                else if (Response.Error == EGamePlatformAuthError::Cancelled)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformProgressionError::OutcomeUnknown : EGamePlatformProgressionError::Cancelled; }
                else if (Response.Error == EGamePlatformAuthError::TimedOut)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformProgressionError::OutcomeUnknown : EGamePlatformProgressionError::TimedOut; }
                else if (Response.Error == EGamePlatformAuthError::OutcomeUnknown)
                { Error = EGamePlatformProgressionError::OutcomeUnknown; }
                else { Error = MapHttpError(Response.HttpStatusCode); }
            }
            Completion(Response.HttpStatusCode, Response.Body, Error);
        });
    *HandleBox = Handle;
    if (!Handle.RequestId.IsValid()) { return false; }
    Runtime->ActiveRequests.Add(Handle);
    return true;
}

bool FGamePlatformProgressionGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformProgressionSnapshotCompletion Completion)
{
    if (!Completion) { return false; }
    return StartRequest(
        TEXT("/v1/progression"),
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body,
            EGamePlatformProgressionError TransportError) mutable
        {
            if (TransportError != EGamePlatformProgressionError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion({}, TransportError != EGamePlatformProgressionError::None ? TransportError : MapHttpError(StatusCode));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformProgressionSnapshot Snapshot;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToSnapshot(Json, Snapshot))
            {
                Completion(
                    {},
                    EGamePlatformProgressionError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Snapshot),
                EGamePlatformProgressionError::None);
        });
}

bool FGamePlatformProgressionGatewayHttpTransport::JsonToSnapshot(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformProgressionSnapshot& OutSnapshot)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString GeneratedAt;
    if (!ParseInt64String(
            Json,
            TEXT("progression_revision"),
            OutSnapshot.ProgressionRevision) ||
        OutSnapshot.ProgressionRevision < 1 ||
        !Json->TryGetStringField(
            TEXT("generated_at"),
            GeneratedAt) ||
        !FDateTime::ParseIso8601(
            *GeneratedAt,
            OutSnapshot.GeneratedAtUtc))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Json->TryGetArrayField(TEXT("tracks"), Values) || !Values)
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

        FString SubjectType;
        FString SubjectId;
        FString TrackId;
        double Level = 0.0;
        double MaxLevel = 0.0;
        double CurveVersion = 0.0;

        FGamePlatformProgressionTrackState Track;

        if (!Object->TryGetStringField(
                TEXT("subject_type"),
                SubjectType) ||
            !Object->TryGetStringField(
                TEXT("subject_id"),
                SubjectId) ||
            !Object->TryGetStringField(
                TEXT("track_id"),
                TrackId) ||
            !Object->TryGetNumberField(
                TEXT("level"),
                Level) ||
            !Object->TryGetNumberField(
                TEXT("max_level"),
                MaxLevel) ||
            !Object->TryGetNumberField(
                TEXT("curve_version"),
                CurveVersion) ||
            !ParseInt64String(
                Object,
                TEXT("total_xp"),
                Track.TotalXP) ||
            !ParseInt64String(
                Object,
                TEXT("revision"),
                Track.Revision))
        {
            return false;
        }

        // 数字先验证再转int32，拒绝溢出/小数/非有限输入，不能靠转换后的截断值通过领域校验。
        if (!FMath::IsFinite(Level) || !FMath::IsFinite(MaxLevel) || !FMath::IsFinite(CurveVersion) ||
            Level < 1.0 || MaxLevel < Level || CurveVersion < 1.0 ||
            Level > MAX_int32 || MaxLevel > MAX_int32 || CurveVersion > MAX_int32 ||
            FMath::FloorToDouble(Level) != Level || FMath::FloorToDouble(MaxLevel) != MaxLevel || FMath::FloorToDouble(CurveVersion) != CurveVersion)
        { return false; }

        Track.SubjectType =
            SubjectType == TEXT("character")
                ? EGamePlatformProgressionSubjectType::Character
                : SubjectType == TEXT("player")
                    ? EGamePlatformProgressionSubjectType::Player
                    : EGamePlatformProgressionSubjectType::Unknown;

        Track.SubjectId = SubjectId;
        Track.ProgressionTrackId = FName(*TrackId);
        Track.Level = static_cast<int32>(Level);
        Track.MaxLevel = static_cast<int32>(MaxLevel);
        Track.CurveVersion = static_cast<int32>(CurveVersion);

        if (Track.SubjectType ==
                EGamePlatformProgressionSubjectType::Unknown ||
            Track.SubjectId.IsEmpty() ||
            Track.ProgressionTrackId.IsNone() ||
            Track.TotalXP < 0 ||
            Track.Level < 1 ||
            Track.MaxLevel < Track.Level ||
            Track.CurveVersion < 1 ||
            Track.Revision < 1)
        {
            return false;
        }

        OutSnapshot.Tracks.Add(MoveTemp(Track));
    }

    return true;
}

bool FGamePlatformProgressionGatewayHttpTransport::ParseInt64String(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    int64& OutValue)
{
    FString Value;
    if (!Json.IsValid() || !Json->HasTypedField<EJson::String>(Field) ||
        !Json->TryGetStringField(Field, Value) ||
        Value.IsEmpty())
    {
        return false;
    }

    std::int64_t Parsed = 0;
    if (!GPIntegerPolicy::ParseDecimalInteger(std::basic_string_view<TCHAR>(*Value, Value.Len()), Parsed)) { return false; }
    OutValue = Parsed; return true;
}

EGamePlatformProgressionError
FGamePlatformProgressionGatewayHttpTransport::MapHttpError(
    int32 StatusCode)
{
    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformProgressionError::Unauthorized;
    }
    if (StatusCode == 0 || StatusCode >= 500)
    {
        return EGamePlatformProgressionError::BackendUnavailable;
    }
    return EGamePlatformProgressionError::OutcomeUnknown;
}
