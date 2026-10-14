// SSLM第三层Dedicated Server可信库存事件HTTP适配。
// 玩家身份只来自平台GamePlatformServer的当前已验证Admission；URL与强令牌仅部署环境注入。
// 本类不执行拾取碰撞/任务条件/治疗：相关玩法模块必须先做服务器权威判定。
// 返回Duplicate时不得再次执行玩法效果，Unknown要用相同世界事件ID重新核对。
#include "Inventory/DivineBeastsTrustedInventoryEventSubsystem.h"

#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Containers/StringConv.h"
#include "Containers/Ticker.h"

// 可信库存响应最大256KiB；HTTP的接收线程只写入此限界缓存，不访问任何UObject。
struct FDivineBeastsInventoryResponseBuffer
{
    FCriticalSection Mutex;
    TArray<uint8> Bytes;
    bool bOverflow = false;
};

// 每个HTTP事务只在该GameInstance内存活；没有持有APlayerController强引用。
// OperationId由后端根据真实来源事件计算，服务器不会自行用随机请求号代替幂等事件。
struct FDivineBeastsInventoryPendingRequest
{
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Http;
    TWeakObjectPtr<APlayerController> Controller;
    FGuid AdmissionId;
    FGuid ConnectionId;
    uint64 ConnectionGeneration = 0;
    uint64 SessionEpoch = 0;
    uint64 OwnerGeneration = 0;
    TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion;
};

bool UDivineBeastsTrustedInventoryEventSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return IsRunningDedicatedServer() && !IsRunningCommandlet() &&
        Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsTrustedInventoryEventSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bShuttingDown = false;
    ++Generation;
    // 缺少专用凭据/服务器Boot/世界内网PlayerData即保持禁用，不创建伪身份。
    PlayerDataBaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("PLAYERDATA_BASE_URL"));
    PlayerDataBaseUrl.RemoveFromEnd(TEXT("/"));
    GameID = FPlatformMisc::GetEnvironmentVariable(TEXT("INVENTORY_GAME_ID"));
    GameServerID = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_ID"));
    ServerBootID = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_BOOT_ID"));
    InternalBearer = FPlatformMisc::GetEnvironmentVariable(TEXT("INVENTORY_TRUSTED_EVENTS_TOKEN"));
    // 不能把内网API的真实URL、令牌或账号ID写入启动日志。
    if (!(PlayerDataBaseUrl.StartsWith(TEXT("http://")) ||
          PlayerDataBaseUrl.StartsWith(TEXT("https://"))) ||
        PlayerDataBaseUrl.Contains(TEXT("\r")) || PlayerDataBaseUrl.Contains(TEXT("\n")) ||
        GameID.IsEmpty() || GameServerID.IsEmpty() || ServerBootID.IsEmpty() ||
        InternalBearer.Len() < 32 || InternalBearer.Len() > 4096 ||
        InternalBearer.Contains(TEXT("\r")) || InternalBearer.Contains(TEXT("\n")) ||
        PlayerDataBaseUrl.Len() > 2048 ||
        PlayerDataBaseUrl.Contains(TEXT("?")) || PlayerDataBaseUrl.Contains(TEXT("#")))
    {
        PlayerDataBaseUrl.Reset();
    }
    if (!PlayerDataBaseUrl.IsEmpty())
    {
        if (UGameInstance* Instance = GetGameInstance())
        {
            if (auto* Admissions = Instance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>())
            {
                BoundAdmissions = Admissions;
                AdmissionChangedHandle = Admissions->OnAdmissionChanged().AddUObject(
                    this, &ThisClass::HandleVerifiedAdmissionChanged);
                LeaseTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateUObject(this, &ThisClass::TickPlayerLeaseRenewals), 10.0f);
            }
        }
    }
}

void UDivineBeastsTrustedInventoryEventSubsystem::Deinitialize()
{
    bShuttingDown = true;
    ++Generation;
    if (LeaseTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(LeaseTickerHandle);
        LeaseTickerHandle.Reset();
    }
    if (BoundAdmissions.IsValid() && AdmissionChangedHandle.IsValid())
    {
        BoundAdmissions->OnAdmissionChanged().Remove(AdmissionChangedHandle);
        AdmissionChangedHandle.Reset();
    }
    BoundAdmissions.Reset();
    ActiveLeaseControllers.Reset();
    for (auto& Pair : PendingLeaseRequests)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->OnProcessRequestComplete().Unbind();
            Pair.Value->CancelRequest();
        }
    }
    PendingLeaseRequests.Reset();
    for (auto& Pair : Pending)
    {
        if (Pair.Value.IsValid() && Pair.Value->Http.IsValid())
        {
            Pair.Value->Http->OnProcessRequestComplete().Unbind();
            Pair.Value->Http->CancelRequest();
        }
    }
    Pending.Reset();
    // 尽可能清零进程级临时敏感副本；实际Token原始来源由部署管理，不持久写盘。
    if (!InternalBearer.IsEmpty())
    {
        TArray<TCHAR>& Buffer = InternalBearer.GetCharArray();
        FMemory::Memzero(Buffer.GetData(), Buffer.Num() * sizeof(TCHAR));
    }
    InternalBearer.Reset();
    PlayerDataBaseUrl.Reset();
    GameID.Reset();
    GameServerID.Reset();
    ServerBootID.Reset();
    Super::Deinitialize();
}

void UDivineBeastsTrustedInventoryEventSubsystem::HandleVerifiedAdmissionChanged(
    const APlayerController* Controller,
    const FGamePlatformServerVerifiedAdmission& Admission, bool bAccepted)
{
    check(IsInGameThread());
    if (bShuttingDown || !IsValid(Controller)) { return; }
    const TWeakObjectPtr<APlayerController> Key(const_cast<APlayerController*>(Controller));
    if (bAccepted && Admission.IsStructurallyValid() &&
        !PlayerDataBaseUrl.IsEmpty() && Admission.ServerInstanceId == GameServerID &&
        Admission.ServerBootId == ServerBootID &&
        Admission.AuthorityUntil > FDateTime::UtcNow())
    {
        // 同一Controller换成新Admission时，立即撤销旧连接的在途续租。
        if (const FGuid* Existing = ActiveLeaseControllers.Find(Key);
            Existing && *Existing == Admission.AdmissionId) { return; }
        if (auto* Old = PendingLeaseRequests.Find(Key); Old && Old->IsValid())
        {
            (*Old)->OnProcessRequestComplete().Unbind();
            (*Old)->CancelRequest();
            PendingLeaseRequests.Remove(Key);
        }
        ActiveLeaseControllers.Add(Key, Admission.AdmissionId);
        RequestPlayerLeaseRenewal(*Controller, Admission);
        return;
    }
    // 已失效的旧准入撤销不能清除后来同Controller的新世代。
    if (const FGuid* Existing = ActiveLeaseControllers.Find(Key);
        Existing && *Existing == Admission.AdmissionId)
    {
        ActiveLeaseControllers.Remove(Key);
        if (auto* Old = PendingLeaseRequests.Find(Key); Old && Old->IsValid())
        {
            (*Old)->OnProcessRequestComplete().Unbind();
            (*Old)->CancelRequest();
            PendingLeaseRequests.Remove(Key);
        }
    }
}

bool UDivineBeastsTrustedInventoryEventSubsystem::TickPlayerLeaseRenewals(float DeltaSeconds)
{
    check(IsInGameThread());
    (void)DeltaSeconds;
    if (bShuttingDown) { return false; }
    const UGamePlatformServerAdmissionSubsystem* AdmissionSubsystem = BoundAdmissions.Get();
    if (!IsValid(AdmissionSubsystem) || PlayerDataBaseUrl.IsEmpty()) { return true; }

    TArray<TWeakObjectPtr<APlayerController>> Stale;
    for (const auto& Pair : ActiveLeaseControllers)
    {
        const TWeakObjectPtr<APlayerController> ControllerKey = Pair.Key;
        const APlayerController* Controller = ControllerKey.Get();
        FGamePlatformServerVerifiedAdmission Current;
        if (!IsValid(Controller) ||
            !AdmissionSubsystem->GetVerifiedAdmission(*Controller, Current) ||
            !Current.IsStructurallyValid() ||
            Current.AdmissionId != Pair.Value ||
            Current.ServerInstanceId != GameServerID ||
            Current.ServerBootId != ServerBootID ||
            Current.AuthorityUntil <= FDateTime::UtcNow())
        {
            Stale.Add(ControllerKey);
            continue;
        }
        if (!PendingLeaseRequests.Contains(ControllerKey))
        {
            RequestPlayerLeaseRenewal(*Controller, Current);
        }
    }
    for (const TWeakObjectPtr<APlayerController>& ControllerKey : Stale)
    {
        ActiveLeaseControllers.Remove(ControllerKey);
        if (auto* PendingRequest = PendingLeaseRequests.Find(ControllerKey);
            PendingRequest && PendingRequest->IsValid())
        {
            (*PendingRequest)->OnProcessRequestComplete().Unbind();
            (*PendingRequest)->CancelRequest();
            PendingLeaseRequests.Remove(ControllerKey);
        }
    }
    return true;
}

void UDivineBeastsTrustedInventoryEventSubsystem::RequestPlayerLeaseRenewal(
    const APlayerController& Controller,
    const FGamePlatformServerVerifiedAdmission& Admission)
{
    check(IsInGameThread());
    constexpr uint64 MaxExactJsonInteger = 9007199254740991ULL;
    if (bShuttingDown || PlayerDataBaseUrl.IsEmpty() || InternalBearer.Len() < 32 ||
        !IsValid(&Controller) || !Controller.HasAuthority() ||
        Admission.ServerInstanceId != GameServerID ||
        Admission.ServerBootId != ServerBootID ||
        Admission.SessionEpoch == 0 || Admission.SessionEpoch > MaxExactJsonInteger ||
        !Admission.ConnectionId.IsValid() || Admission.PlayerId.IsEmpty() ||
        Admission.AuthorityUntil <= FDateTime::UtcNow())
    {
        return;
    }
    const TWeakObjectPtr<APlayerController> Key(const_cast<APlayerController*>(&Controller));
    if (PendingLeaseRequests.Contains(Key)) { return; }

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("gameId"), GameID);
    Body->SetStringField(TEXT("playerId"), Admission.PlayerId);
    Body->SetStringField(TEXT("gameServerId"), GameServerID);
    Body->SetStringField(TEXT("serverBootId"), ServerBootID);
    Body->SetNumberField(TEXT("sessionEpoch"), static_cast<double>(Admission.SessionEpoch));
    Body->SetStringField(TEXT("connectionId"),
        Admission.ConnectionId.ToString(EGuidFormats::DigitsWithHyphensLower));
    Body->SetNumberField(TEXT("leaseSeconds"), 30);
    FString Json;
    if (!FJsonSerializer::Serialize(Body,TJsonWriterFactory<>::Create(&Json))) { return; }

    TSharedRef<IHttpRequest,ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject)) { return; }
#else
    return;
#endif
    Request->SetURL(PlayerDataBaseUrl + TEXT("/internal/v1/playerdata/inventory/session-renew"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + InternalBearer);
    Request->SetHeader(TEXT("X-Game-Server-Id"), GameServerID);
    Request->SetHeader(TEXT("X-Game-Server-Boot-Id"), ServerBootID);
    Request->SetContentAsString(Json);
    Request->SetTimeout(6.0f);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);

    // 续租返回只需要小于4KiB的JSON状态；真实网络接收先设硬限制再执行请求。
    constexpr int32 MaxRenewResponseBytes = 4096;
    const auto Buffer = MakeShared<FDivineBeastsInventoryResponseBuffer,ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 Receive = FHttpRequestStreamDelegateV2::CreateLambda(
        [Buffer,MaxRenewResponseBytes](void* Data,int64& InOutLength)
        {
            if (!Data || InOutLength<=0) { return; }
            FScopeLock Lock(&Buffer->Mutex);
            if (Buffer->bOverflow || InOutLength > MaxRenewResponseBytes - Buffer->Bytes.Num())
            {
                Buffer->bOverflow = true;
                InOutLength = 0;
                return;
            }
            Buffer->Bytes.Append(static_cast<const uint8*>(Data),static_cast<int32>(InOutLength));
        });
    if (!Request->SetResponseBodyReceiveStreamDelegateV2(MoveTemp(Receive))) { return; }

    const FGuid ExpectedAdmissionId = Admission.AdmissionId;
    const FString ExpectedPlayerId = Admission.PlayerId;
    const uint64 ExpectedSessionEpoch = Admission.SessionEpoch;
    const uint64 ExpectedGeneration = Generation;
    const TWeakObjectPtr<UDivineBeastsTrustedInventoryEventSubsystem> WeakSelf(this);
    Request->OnProcessRequestComplete().BindLambda(
        [WeakSelf,Key,ExpectedAdmissionId,ExpectedGeneration,ExpectedPlayerId,ExpectedSessionEpoch,Buffer](
            FHttpRequestPtr Finished,FHttpResponsePtr Response,bool bTransportOK)
        {
            UDivineBeastsTrustedInventoryEventSubsystem* Self = WeakSelf.Get();
            if (!IsValid(Self) || Self->bShuttingDown ||
                Self->Generation != ExpectedGeneration) { return; }
            const auto* Pending = Self->PendingLeaseRequests.Find(Key);
            if (!Pending || *Pending != Finished) { return; }
            Self->PendingLeaseRequests.Remove(Key);
            const FGuid* CurrentId = Self->ActiveLeaseControllers.Find(Key);
            if (!CurrentId || *CurrentId != ExpectedAdmissionId ||
                !Key.IsValid() || !bTransportOK || !Response ||
                Response->GetResponseCode()!=200)
            {
                // 失败或失联时不提升本地AuthorityUntil。
                // 下一个10秒周期使用相同当前Admission身份安全重试。
                return;
            }
            // 防止代理返回错误玩家/错误SessionEpoch的200回执。
            TArray<uint8> Bytes;
            {
                FScopeLock Lock(&Buffer->Mutex);
                if (Buffer->bOverflow || Buffer->Bytes.IsEmpty() || Buffer->Bytes.Num()>4096)
                {
                    return;
                }
                Bytes = MoveTemp(Buffer->Bytes);
            }
            const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),Bytes.Num());
            const FString JsonText(Converted.Length(),Converted.Get());
            TSharedPtr<FJsonObject> Body;
            FString ReturnedPlayerId,AuthorityUntilUtc;
            double ReturnedEpoch=0;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText),Body) ||
                !Body.IsValid() ||
                !Body->TryGetStringField(TEXT("playerId"),ReturnedPlayerId) ||
                ReturnedPlayerId!=ExpectedPlayerId ||
                !Body->TryGetNumberField(TEXT("sessionEpoch"),ReturnedEpoch) ||
                ReturnedEpoch!=static_cast<double>(ExpectedSessionEpoch) ||
                !Body->TryGetStringField(TEXT("authorityUntilUtc"),AuthorityUntilUtc) ||
                AuthorityUntilUtc.IsEmpty())
            {
                return;
            }
            // 续租回执不会直接提升本地身份；每次Grant/Consume仍在PG同事务校验活跃租约。
        });
    PendingLeaseRequests.Add(Key,Request);
    if (!Request->ProcessRequest())
    {
        Request->OnProcessRequestComplete().Unbind();
        PendingLeaseRequests.Remove(Key);
    }
}

FGuid UDivineBeastsTrustedInventoryEventSubsystem::SubmitWorldPickup(
    const APlayerController& Controller, FGuid SourceEventId, FName ItemDefinitionId,
    int32 Quantity, int64 ExpectedInventoryRevision,
    TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion)
{
    return SubmitConfirmedEvent(Controller, SourceEventId, TEXT("grant"), TEXT("pickup"),
        ItemDefinitionId, FString(), Quantity, ExpectedInventoryRevision, MoveTemp(Completion));
}

FGuid UDivineBeastsTrustedInventoryEventSubsystem::SubmitQuestReward(
    const APlayerController& Controller, FGuid SourceEventId, FName ItemDefinitionId,
    int32 Quantity, int64 ExpectedInventoryRevision,
    TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion)
{
    return SubmitConfirmedEvent(Controller, SourceEventId, TEXT("grant"), TEXT("quest_reward"),
        ItemDefinitionId, FString(), Quantity, ExpectedInventoryRevision, MoveTemp(Completion));
}

FGuid UDivineBeastsTrustedInventoryEventSubsystem::SubmitItemUse(
    const APlayerController& Controller, FGuid SourceEventId, FString ItemInstanceId,
    FName ItemDefinitionId, int32 Quantity, int64 ExpectedInventoryRevision,
    TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion)
{
    return SubmitConfirmedEvent(Controller, SourceEventId, TEXT("consume"), TEXT("item_use"),
        ItemDefinitionId, ItemInstanceId, Quantity, ExpectedInventoryRevision, MoveTemp(Completion));
}

FGuid UDivineBeastsTrustedInventoryEventSubsystem::SubmitConfirmedEvent(
    const APlayerController& Controller, FGuid SourceEventId, const TCHAR* EventKind,
    const TCHAR* ReasonCode, FName DefinitionId, const FString& InstanceId, int32 Quantity,
    int64 ExpectedInventoryRevision,
    TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion){
    check(IsInGameThread());
    constexpr uint64 MaxExactJsonInteger = 9007199254740991ULL;
    if (bShuttingDown || PlayerDataBaseUrl.IsEmpty() || InternalBearer.Len() < 32 ||
        !SourceEventId.IsValid() || DefinitionId.IsNone() ||
        Quantity < 1 || Quantity > 1000000 || ExpectedInventoryRevision < 0 ||
        static_cast<uint64>(ExpectedInventoryRevision) > MaxExactJsonInteger ||
        !IsValid(&Controller) || !Controller.HasAuthority() ||
        !Controller.GetWorld() ||
        Controller.GetWorld()->GetGameInstance() != GetGameInstance() ||
        (FCString::Strcmp(EventKind, TEXT("consume")) == 0 && InstanceId.IsEmpty()) ||
        (FCString::Strcmp(EventKind, TEXT("grant")) == 0 && !InstanceId.IsEmpty()))
    {
        return FGuid();
    }

    UGameInstance* Instance = GetGameInstance();
    UGamePlatformServerAdmissionSubsystem* AdmissionService = IsValid(Instance)
        ? Instance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>() : nullptr;
    FGamePlatformServerVerifiedAdmission Verified;
    if (!IsValid(AdmissionService) ||
        !AdmissionService->GetVerifiedAdmission(Controller, Verified) ||
        !Verified.IsStructurallyValid() || Verified.AuthorityUntil <= FDateTime::UtcNow() ||
        Verified.ServerInstanceId != GameServerID || Verified.ServerBootId != ServerBootID ||
        Verified.SessionEpoch == 0 || Verified.SessionEpoch > MaxExactJsonInteger ||
        !Verified.ConnectionId.IsValid() || Verified.PlayerId.IsEmpty())
    {
        return FGuid();
    }

    const FString SourceId = SourceEventId.ToString(EGuidFormats::DigitsWithHyphensLower);
    const FString Connection = Verified.ConnectionId.ToString(EGuidFormats::DigitsWithHyphensLower);
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("eventKind"), EventKind);
    Body->SetStringField(TEXT("gameId"), GameID);
    Body->SetStringField(TEXT("gameServerId"), GameServerID);
    Body->SetStringField(TEXT("serverBootId"), ServerBootID);
    Body->SetStringField(TEXT("playerId"), Verified.PlayerId);
    Body->SetNumberField(TEXT("sessionEpoch"), static_cast<double>(Verified.SessionEpoch));
    Body->SetStringField(TEXT("connectionId"), Connection);
    Body->SetStringField(TEXT("sourceEventId"), SourceId);
    // 0明确要求PlayerData在可信事件验证后重新读取最新背包Revision并执行乐观锁；
    // 非0版本仅能来自专服读取的权威PlayerData快照，绝不使用客户端自报值。
    Body->SetNumberField(TEXT("expectedRevision"), static_cast<double>(ExpectedInventoryRevision));
    Body->SetStringField(TEXT("itemDefinitionId"), DefinitionId.ToString());
    Body->SetStringField(TEXT("itemInstanceId"), InstanceId);
    Body->SetNumberField(TEXT("quantity"), Quantity);
    Body->SetStringField(TEXT("reasonCode"), ReasonCode);
    FString Json;
    if (!FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json))) return FGuid();

    const FGuid RequestId = FGuid::NewGuid();
    const uint64 CapturedGeneration = Generation;
    const TSharedPtr<FDivineBeastsInventoryPendingRequest, ESPMode::ThreadSafe> Entry =
        MakeShared<FDivineBeastsInventoryPendingRequest, ESPMode::ThreadSafe>();
    Entry->Controller = const_cast<APlayerController*>(&Controller);
    Entry->AdmissionId = Verified.AdmissionId;
    Entry->ConnectionId = Verified.ConnectionId;
    Entry->ConnectionGeneration = Verified.ConnectionGeneration;
    Entry->SessionEpoch = Verified.SessionEpoch;
    Entry->OwnerGeneration = CapturedGeneration;
    Entry->Completion = MoveTemp(Completion);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    // 内网专用Bearer令牌绝不能跟随302重定向传到另一服务器。
    // 若本引擎编译配置无法拒绝重定向，则失败关闭可信发奖请求。
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        return FGuid();
    }
#else
    return FGuid();
#endif
    Entry->Http = Request;
    Request->SetURL(PlayerDataBaseUrl + TEXT("/internal/v1/playerdata/inventory/trusted-events"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + InternalBearer);
    Request->SetHeader(TEXT("X-Game-Server-Id"), GameServerID);
    Request->SetHeader(TEXT("X-Game-Server-Boot-Id"), ServerBootID);
    Request->SetContentAsString(Json);
    Request->SetTimeout(8.0f);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);

    constexpr int32 MaxInventoryResponseBytes = 256 * 1024;
    const auto ResponseBuffer = MakeShared<FDivineBeastsInventoryResponseBuffer, ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 Receive = FHttpRequestStreamDelegateV2::CreateLambda(
        [ResponseBuffer, MaxInventoryResponseBytes](void* Data, int64& InOutLength)
        {
            if (!Data || InOutLength <= 0) return;
            FScopeLock Lock(&ResponseBuffer->Mutex);
            if (ResponseBuffer->bOverflow || InOutLength > MaxInventoryResponseBytes - ResponseBuffer->Bytes.Num())
            {
                ResponseBuffer->bOverflow = true;
                InOutLength = 0;
                return;
            }
            ResponseBuffer->Bytes.Append(static_cast<const uint8*>(Data), static_cast<int32>(InOutLength));
        });
    // 必须在接收前配置硬上限，不能先下载无限响应再检查GetContent().Num()。
    if (!Request->SetResponseBodyReceiveStreamDelegateV2(MoveTemp(Receive)))
    {
        return FGuid();
    }

    const TWeakObjectPtr<UDivineBeastsTrustedInventoryEventSubsystem> WeakSelf(this);
    Request->OnProcessRequestComplete().BindLambda(
        [WeakSelf, RequestId, CapturedGeneration, EventKindCopy = FString(EventKind), RequestedQuantity = Quantity, ResponseBuffer](
            FHttpRequestPtr Finished, FHttpResponsePtr Response, bool bTransportOK)
    {
        UDivineBeastsTrustedInventoryEventSubsystem* Self = WeakSelf.Get();
        if (!IsValid(Self) || Self->bShuttingDown ||
            Self->Generation != CapturedGeneration) return;

        const TSharedPtr<FDivineBeastsInventoryPendingRequest, ESPMode::ThreadSafe> Current =
            Self->Pending.FindRef(RequestId);
        if (!Current.IsValid() || Current->Http != Finished ||
            Current->OwnerGeneration != CapturedGeneration) return;
        Self->Pending.Remove(RequestId);
        const APlayerController* C = Current->Controller.Get();
        UGameInstance* GI = Self->GetGameInstance();
        UGamePlatformServerAdmissionSubsystem* Admissions = IsValid(GI)
            ? GI->GetSubsystem<UGamePlatformServerAdmissionSubsystem>() : nullptr;
        FGamePlatformServerVerifiedAdmission Now;
        if (!IsValid(C) || !IsValid(Admissions) ||
            !Admissions->GetVerifiedAdmission(*C, Now) ||
            !Now.IsStructurallyValid() ||
            Now.AdmissionId != Current->AdmissionId ||
            Now.ConnectionId != Current->ConnectionId ||
            Now.ConnectionGeneration != Current->ConnectionGeneration ||
            Now.SessionEpoch != Current->SessionEpoch ||
            Now.AuthorityUntil <= FDateTime::UtcNow()) return;

        EDivineBeastsInventoryEventOutcome Outcome =
            EDivineBeastsInventoryEventOutcome::Unknown;
        TArray<uint8> BodyBytes;
        bool bOverflow = false;
        {
            FScopeLock Lock(&ResponseBuffer->Mutex);
            bOverflow = ResponseBuffer->bOverflow;
            BodyBytes = MoveTemp(ResponseBuffer->Bytes);
        }
        if (bTransportOK && Response && Response->GetResponseCode() == 200 &&
            !bOverflow && !BodyBytes.IsEmpty() && BodyBytes.Num() <= 256 * 1024)
        {
            const FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(BodyBytes.GetData()), BodyBytes.Num());
            const FString ResponseText(Decoded.Length(), Decoded.Get());
            TSharedPtr<FJsonObject> Parsed;
            if (FJsonSerializer::Deserialize(
                    TJsonReaderFactory<>::Create(ResponseText), Parsed) &&
                Parsed.IsValid())
            {
                FString OperationId;
                bool bDuplicate = false;
                const TSharedPtr<FJsonObject>* Snapshot = nullptr;
                const FString Prefix = EventKindCopy == TEXT("grant") ? TEXT("grant-") : TEXT("consume-");
                const bool bHasOperationId =
                    Parsed->TryGetStringField(TEXT("operationId"), OperationId);
                bool bValidOperationId = bHasOperationId &&
                    OperationId.Len() == Prefix.Len() + 32 &&
                    OperationId.StartsWith(Prefix, ESearchCase::CaseSensitive);
                if (bValidOperationId)
                {
                    // 后端操作号为16字节摘要的32个小写十六进制字符；
                    // 其他前缀或任意“成功”字样不得触发实际消耗后玩法反馈。
                    for (int32 Index = Prefix.Len(); Index < OperationId.Len(); ++Index)
                    {
                        const TCHAR Character = OperationId[Index];
                        if (!((Character >= TEXT('0') && Character <= TEXT('9')) ||
                              (Character >= TEXT('a') && Character <= TEXT('f'))))
                        {
                            bValidOperationId = false;
                            break;
                        }
                    }
                }
                double MovedQuantity = -1.0;
                double RemainingQuantity = -1.0;
                double InventoryRevision = 0.0;
                if (bValidOperationId &&
                    Parsed->TryGetBoolField(TEXT("duplicate"), bDuplicate) &&
                    Parsed->TryGetNumberField(TEXT("movedQuantity"), MovedQuantity) &&
                    Parsed->TryGetNumberField(TEXT("remainingQuantity"), RemainingQuantity) &&
                    Parsed->TryGetObjectField(TEXT("snapshot"), Snapshot) &&
                    Snapshot && Snapshot->IsValid() &&
                    (*Snapshot)->TryGetNumberField(TEXT("inventoryRevision"), InventoryRevision) &&
                    InventoryRevision >= 1.0 &&
                    InventoryRevision <= 9007199254740991.0 &&
                    MovedQuantity == static_cast<double>(RequestedQuantity) &&
                    RemainingQuantity >= 0.0 &&
                    (EventKindCopy == TEXT("consume") || RemainingQuantity == 0.0))
                {
                    Outcome = bDuplicate ? EDivineBeastsInventoryEventOutcome::AlreadyCommitted
                                         : EDivineBeastsInventoryEventOutcome::FirstCommitted;
                }
            }
        }
        // Unknown不能触发实际消耗后的玩法效果；Duplicate也须先按来源事件完成效果去重。
        if (Current->Completion)
        {
            TFunction<void(EDivineBeastsInventoryEventOutcome)> Callback = MoveTemp(Current->Completion);
            Callback(Outcome);
        }
    });

    Pending.Add(RequestId, Entry);
    if (!Request->ProcessRequest())
    {
        Request->OnProcessRequestComplete().Unbind();
        Pending.Remove(RequestId);
        return FGuid();
    }
    return RequestId;
}

bool UDivineBeastsTrustedInventoryEventSubsystem::CancelLocalWait(FGuid RequestId)
{
    check(IsInGameThread());
    if (const TSharedPtr<FDivineBeastsInventoryPendingRequest, ESPMode::ThreadSafe> Item =
            Pending.FindRef(RequestId); Item.IsValid())
    {
        Pending.Remove(RequestId);
        if (Item->Http.IsValid())
        {
            Item->Http->OnProcessRequestComplete().Unbind();
            Item->Http->CancelRequest();
        }
        // 仅本地取消等待，后端有可能已经接受；同一SourceEventId对账才是权威。
        return true;
    }
    return false;
}
