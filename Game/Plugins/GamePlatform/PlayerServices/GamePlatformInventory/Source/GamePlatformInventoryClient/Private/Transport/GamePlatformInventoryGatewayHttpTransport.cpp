#include "Transport/GamePlatformInventoryGatewayHttpTransport.h"

#include "Dom/JsonObject.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 MaxConcurrentInventoryRequests = 8;
constexpr int32 MaxTransportInventoryContainers = 64;
constexpr int32 MaxTransportInventoryItems = 10000;
constexpr int32 MaxTransportInventoryQuickbarSlots = 12;

bool TryGetExactInt32(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    int32& OutValue)
{
    double Number = 0.0;
    if (!Json.IsValid() ||
        !Json->TryGetNumberField(Field, Number) ||
        Number < static_cast<double>(MIN_int32) ||
        Number > static_cast<double>(MAX_int32))
    {
        return false;
    }

    const int64 Integral = static_cast<int64>(Number);
    if (static_cast<double>(Integral) != Number)
    {
        return false;
    }

    OutValue = static_cast<int32>(Integral);
    return true;
}

bool TryGetPositiveRevision(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    int64& OutValue)
{
    FString Text;
    if (!Json.IsValid() ||
        !Json->TryGetStringField(Field, Text) ||
        Text.IsEmpty() ||
        Text != Text.TrimStartAndEnd())
    {
        return false;
    }

    TCHAR* End = nullptr;
    const int64 Value = FCString::Strtoi64(*Text, &End, 10);
    if (Value <= 0 ||
        End == nullptr ||
        *End != TEXT('\0') ||
        FString::Printf(TEXT("%lld"), static_cast<long long>(Value)) != Text)
    {
        return false;
    }

    OutValue = Value;
    return true;
}

EGamePlatformInventoryError MapOnlineTransportError(
    EGamePlatformAuthError Error,
    bool bMayHaveReachedServer)
{
    switch (Error)
    {
    case EGamePlatformAuthError::None:
        return EGamePlatformInventoryError::None;
    case EGamePlatformAuthError::AuthExpired:
    case EGamePlatformAuthError::InvalidCredentials:
    case EGamePlatformAuthError::Forbidden:
        return EGamePlatformInventoryError::Unauthorized;
    case EGamePlatformAuthError::Cancelled:
        return bMayHaveReachedServer
            ? EGamePlatformInventoryError::OutcomeUnknown
            : EGamePlatformInventoryError::Cancelled;
    case EGamePlatformAuthError::TimedOut:
        return bMayHaveReachedServer
            ? EGamePlatformInventoryError::OutcomeUnknown
            : EGamePlatformInventoryError::TimedOut;
    case EGamePlatformAuthError::OutcomeUnknown:
        return EGamePlatformInventoryError::OutcomeUnknown;
    case EGamePlatformAuthError::NetworkUnavailable:
        return bMayHaveReachedServer
            ? EGamePlatformInventoryError::OutcomeUnknown
            : EGamePlatformInventoryError::BackendUnavailable;
    case EGamePlatformAuthError::ProviderUnavailable:
    case EGamePlatformAuthError::Maintenance:
    case EGamePlatformAuthError::QueueFull:
    case EGamePlatformAuthError::ServiceUnavailable:
        return EGamePlatformInventoryError::BackendUnavailable;
    default:
        return EGamePlatformInventoryError::InvalidResponse;
    }
}
}

struct FGamePlatformInventoryGatewayHttpTransport::FRuntime
{
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> OnlineSubsystem;
    FCriticalSection ActiveRequestsMutex;
    TArray<FGamePlatformOnlineRequestHandle> ActiveRequests;
};

FGamePlatformInventoryGatewayHttpTransport::
FGamePlatformInventoryGatewayHttpTransport(
    UGamePlatformOnlineClientSubsystem* InOnlineSubsystem)
    : Runtime(MakeUnique<FRuntime>())
{
    Runtime->OnlineSubsystem = InOnlineSubsystem;
}

FGamePlatformInventoryGatewayHttpTransport::
~FGamePlatformInventoryGatewayHttpTransport()
{
    CancelAllRequests();
}

bool FGamePlatformInventoryGatewayHttpTransport::IsConfigured() const
{
    if (!Runtime)
    {
        return false;
    }

    const UGamePlatformOnlineClientSubsystem* Online =
        Runtime->OnlineSubsystem.Get();
    if (!IsValid(Online))
    {
        return false;
    }

    const EGamePlatformAuthState State = Online->GetSnapshot().State;
    return State == EGamePlatformAuthState::Authenticated ||
           State == EGamePlatformAuthState::Refreshing;
}

void FGamePlatformInventoryGatewayHttpTransport::CancelAllRequests()
{
    if (!Runtime)
    {
        return;
    }

    TArray<FGamePlatformOnlineRequestHandle> Requests;
    {
        FScopeLock Lock(&Runtime->ActiveRequestsMutex);
        Requests = MoveTemp(Runtime->ActiveRequests);
        Runtime->ActiveRequests.Reset();
    }

    if (UGamePlatformOnlineClientSubsystem* Online =
            Runtime->OnlineSubsystem.Get())
    {
        for (const FGamePlatformOnlineRequestHandle& Request : Requests)
        {
            if (Request.RequestId.IsValid())
            {
                Online->Cancel(Request);
            }
        }
    }
}

void FGamePlatformInventoryGatewayHttpTransport::UnregisterRequest(
    const FGuid& RequestId)
{
    if (!Runtime || !RequestId.IsValid())
    {
        return;
    }

    FScopeLock Lock(&Runtime->ActiveRequestsMutex);
    Runtime->ActiveRequests.RemoveAll(
        [&RequestId](const FGamePlatformOnlineRequestHandle& Handle)
        {
            return Handle.RequestId == RequestId;
        });
}

FString FGamePlatformInventoryGatewayHttpTransport::GuidString(
    const FGuid& Guid)
{
    return Guid.IsValid()
        ? Guid.ToString(EGuidFormats::DigitsWithHyphensLower)
        : FString();
}

FString FGamePlatformInventoryGatewayHttpTransport::RevisionString(
    int64 Revision)
{
    return Revision > 0
        ? FString::Printf(TEXT("%lld"), static_cast<long long>(Revision))
        : FString();
}

bool FGamePlatformInventoryGatewayHttpTransport::StartJsonRequest(
    const FString& Verb,
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    const FString& IdempotencyKey,
    TFunction<void(
        int32,
        const FString&,
        EGamePlatformInventoryError)> Completion)
{
    if (!Runtime || !Completion || !IsConfigured())
    {
        return false;
    }

    UGamePlatformOnlineClientSubsystem* Online =
        Runtime->OnlineSubsystem.Get();
    if (!IsValid(Online))
    {
        return false;
    }

    {
        FScopeLock Lock(&Runtime->ActiveRequestsMutex);
        if (Runtime->ActiveRequests.Num() >= MaxConcurrentInventoryRequests)
        {
            return false;
        }
    }

    FString Payload;
    if (Body.IsValid())
    {
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Payload);
        if (!FJsonSerializer::Serialize(Body.ToSharedRef(), Writer))
        {
            return false;
        }
    }

    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = Verb;
    Request.RelativePath = Path;
    Request.Body = MoveTemp(Payload);
    Request.IdempotencyKey = IdempotencyKey;
    Request.bIdempotent =
        Verb == TEXT("GET") ||
        Verb == TEXT("HEAD") ||
        !IdempotencyKey.IsEmpty();

    // 使用Online实例统一配置的请求总截止时间，不在背包插件复制第二套超时参数。
    FGamePlatformOnlineRequestOptions Options;

    TSharedRef<FGamePlatformOnlineRequestHandle, ESPMode::ThreadSafe>
        RequestHandleBox =
            MakeShared<
                FGamePlatformOnlineRequestHandle,
                ESPMode::ThreadSafe>();

    TWeakPtr<
        FGamePlatformInventoryGatewayHttpTransport,
        ESPMode::ThreadSafe> WeakSelf = AsShared();

    FGamePlatformOnlineRequestHandle Handle =
        Online->SendAuthenticatedRequest(
            MoveTemp(Request),
            Options,
            [WeakSelf,
             RequestHandleBox,
             Completion = MoveTemp(Completion)](
                FGamePlatformAuthenticatedResponse Response) mutable
            {
                if (const TSharedPtr<
                        FGamePlatformInventoryGatewayHttpTransport,
                        ESPMode::ThreadSafe> Self = WeakSelf.Pin())
                {
                    Self->UnregisterRequest(
                        RequestHandleBox->RequestId);
                }

                EGamePlatformInventoryError InventoryError =
                    EGamePlatformInventoryError::None;

                if (!Response.IsSuccess())
                {
                    InventoryError =
                        Response.HttpStatusCode > 0
                            ? FGamePlatformInventoryGatewayHttpTransport::
                                  MapHttpError(
                                      Response.HttpStatusCode,
                                      Response.Body)
                            : MapOnlineTransportError(
                                  Response.Error,
                                  Response.bMayHaveReachedServer);
                }

                Completion(
                    Response.HttpStatusCode,
                    Response.Body,
                    InventoryError);
            });

    *RequestHandleBox = Handle;
    if (!Handle.RequestId.IsValid())
    {
        return false;
    }

    {
        FScopeLock Lock(&Runtime->ActiveRequestsMutex);
        Runtime->ActiveRequests.Add(Handle);
    }
    return true;
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformInventorySnapshotCompletion Completion)
{
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/inventory"),
        nullptr,
        FString(),
        [Completion = MoveTemp(Completion)](
            int32,
            const FString& Body,
            EGamePlatformInventoryError Error) mutable
        {
            if (Error != EGamePlatformInventoryError::None)
            {
                Completion({}, Error);
                return;
            }

            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformInventorySnapshot Snapshot;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToSnapshot(Json, Snapshot))
            {
                Completion(
                    {},
                    EGamePlatformInventoryError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Snapshot),
                EGamePlatformInventoryError::None);
        });
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginGetOperation(
    const FGuid& OperationId,
    FGamePlatformInventoryMutationCompletion Completion)
{
    if (!OperationId.IsValid())
    {
        return false;
    }

    return StartJsonRequest(
        TEXT("GET"),
        FString::Printf(
            TEXT("/v1/inventory/operations/%s"),
            *GuidString(OperationId)),
        nullptr,
        FString(),
        [Completion = MoveTemp(Completion)](
            int32,
            const FString& Body,
            EGamePlatformInventoryError Error) mutable
        {
            if (Error != EGamePlatformInventoryError::None)
            {
                Completion({}, Error);
                return;
            }

            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformInventoryMutationResult Result;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToMutationResult(Json, Result))
            {
                Completion(
                    {},
                    EGamePlatformInventoryError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Result),
                EGamePlatformInventoryError::None);
        });
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginMove(
    const FGamePlatformInventoryMoveRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("operationId"),
        GuidString(Request.OperationId));
    Body->SetStringField(
        TEXT("expectedRevision"),
        RevisionString(Request.ExpectedRevision));
    Body->SetStringField(
        TEXT("itemInstanceId"),
        Request.ItemInstanceId);
    Body->SetStringField(
        TEXT("targetContainerId"),
        Request.TargetContainerId.ToString());
    Body->SetNumberField(
        TEXT("targetSlotIndex"),
        Request.TargetSlotIndex);

    return BeginMutation(
        TEXT("/v1/inventory/move"),
        Request.OperationId,
        Body,
        MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginSplit(
    const FGamePlatformInventorySplitRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("operationId"),
        GuidString(Request.OperationId));
    Body->SetStringField(
        TEXT("expectedRevision"),
        RevisionString(Request.ExpectedRevision));
    Body->SetStringField(
        TEXT("sourceItemInstanceId"),
        Request.SourceItemInstanceId);
    Body->SetNumberField(
        TEXT("splitQuantity"),
        Request.SplitQuantity);
    Body->SetStringField(
        TEXT("targetContainerId"),
        Request.TargetContainerId.ToString());
    Body->SetNumberField(
        TEXT("targetSlotIndex"),
        Request.TargetSlotIndex);

    return BeginMutation(
        TEXT("/v1/inventory/split"),
        Request.OperationId,
        Body,
        MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginMerge(
    const FGamePlatformInventoryMergeRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("operationId"),
        GuidString(Request.OperationId));
    Body->SetStringField(
        TEXT("expectedRevision"),
        RevisionString(Request.ExpectedRevision));
    Body->SetStringField(
        TEXT("sourceItemInstanceId"),
        Request.SourceItemInstanceId);
    Body->SetStringField(
        TEXT("targetItemInstanceId"),
        Request.TargetItemInstanceId);

    return BeginMutation(
        TEXT("/v1/inventory/merge"),
        Request.OperationId,
        Body,
        MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginSetQuickbar(
    const FGamePlatformInventoryQuickbarRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("operationId"),
        GuidString(Request.OperationId));
    Body->SetStringField(
        TEXT("expectedRevision"),
        RevisionString(Request.ExpectedRevision));
    Body->SetNumberField(TEXT("slotIndex"), Request.SlotIndex);
    Body->SetStringField(
        TEXT("itemInstanceId"),
        Request.ItemInstanceId);

    return BeginMutation(
        TEXT("/v1/inventory/quickbar/set"),
        Request.OperationId,
        Body,
        MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginClearQuickbar(
    const FGamePlatformInventoryQuickbarRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("operationId"),
        GuidString(Request.OperationId));
    Body->SetStringField(
        TEXT("expectedRevision"),
        RevisionString(Request.ExpectedRevision));
    Body->SetNumberField(TEXT("slotIndex"), Request.SlotIndex);

    return BeginMutation(
        TEXT("/v1/inventory/quickbar/clear"),
        Request.OperationId,
        Body,
        MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginMutation(
    const FString& Path,
    const FGuid& OperationId,
    const TSharedPtr<FJsonObject>& Body,
    FGamePlatformInventoryMutationCompletion Completion)
{
    if (!OperationId.IsValid())
    {
        return false;
    }

    const FString IdempotencyKey = GuidString(OperationId);
    return StartJsonRequest(
        TEXT("POST"),
        Path,
        Body,
        IdempotencyKey,
        [Completion = MoveTemp(Completion)](
            int32,
            const FString& ResponseBody,
            EGamePlatformInventoryError Error) mutable
        {
            if (Error != EGamePlatformInventoryError::None)
            {
                Completion({}, Error);
                return;
            }

            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformInventoryMutationResult Result;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToMutationResult(Json, Result))
            {
                Completion(
                    {},
                    EGamePlatformInventoryError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Result),
                EGamePlatformInventoryError::None);
        });
}

bool FGamePlatformInventoryGatewayHttpTransport::JsonToSnapshot(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformInventorySnapshot& OutSnapshot)
{
    if (!Json.IsValid())
    {
        return false;
    }

    OutSnapshot = {};
    if (!TryGetPositiveRevision(
            Json,
            TEXT("inventoryRevision"),
            OutSnapshot.InventoryRevision))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Containers = nullptr;
    if (!Json->TryGetArrayField(TEXT("containers"), Containers) ||
        !Containers ||
        Containers->Num() > MaxTransportInventoryContainers)
    {
        return false;
    }

    OutSnapshot.Containers.Reserve(Containers->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Containers)
    {
        const TSharedPtr<FJsonObject> ContainerJson =
            Value.IsValid()
                ? Value->AsObject()
                : nullptr;

        FString ContainerId;
        FGamePlatformInventoryContainerSnapshot Container;
        if (!ContainerJson.IsValid() ||
            !ContainerJson->TryGetStringField(
                TEXT("containerId"),
                ContainerId) ||
            !TryGetExactInt32(
                ContainerJson,
                TEXT("capacity"),
                Container.Capacity))
        {
            return false;
        }

        Container.ContainerId = FName(*ContainerId);
        if (!Container.IsValid())
        {
            return false;
        }
        OutSnapshot.Containers.Add(MoveTemp(Container));
    }

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Json->TryGetArrayField(TEXT("items"), Items) ||
        !Items ||
        Items->Num() > MaxTransportInventoryItems)
    {
        return false;
    }

    OutSnapshot.Items.Reserve(Items->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Items)
    {
        const TSharedPtr<FJsonObject> ItemJson =
            Value.IsValid()
                ? Value->AsObject()
                : nullptr;

        FGamePlatformInventoryItemInstance Item;
        FString ItemDefinitionId;
        FString ContainerId;
        FString InstanceState;

        if (!ItemJson.IsValid() ||
            !ItemJson->TryGetStringField(
                TEXT("itemInstanceId"),
                Item.ItemInstanceId) ||
            !ItemJson->TryGetStringField(
                TEXT("itemDefinitionId"),
                ItemDefinitionId) ||
            !TryGetExactInt32(
                ItemJson,
                TEXT("quantity"),
                Item.Quantity) ||
            !ItemJson->TryGetStringField(
                TEXT("containerId"),
                ContainerId) ||
            !TryGetExactInt32(
                ItemJson,
                TEXT("slotIndex"),
                Item.SlotIndex) ||
            !TryGetPositiveRevision(
                ItemJson,
                TEXT("revision"),
                Item.Revision) ||
            !ItemJson->TryGetStringField(
                TEXT("instanceState"),
                InstanceState) ||
            !TryGetExactInt32(
                ItemJson,
                TEXT("maxStackSize"),
                Item.MaxStackSize))
        {
            return false;
        }

        Item.ItemDefinitionId = FName(*ItemDefinitionId);
        Item.ContainerId = FName(*ContainerId);
        Item.InstanceState = FName(*InstanceState);

        if (!Item.IsValid())
        {
            return false;
        }

        OutSnapshot.Items.Add(MoveTemp(Item));
    }

    const TArray<TSharedPtr<FJsonValue>>* Quickbar = nullptr;
    if (!Json->TryGetArrayField(TEXT("quickbar"), Quickbar) ||
        !Quickbar ||
        Quickbar->Num() > MaxTransportInventoryQuickbarSlots)
    {
        return false;
    }

    OutSnapshot.Quickbar.Reserve(Quickbar->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Quickbar)
    {
        const TSharedPtr<FJsonObject> SlotJson =
            Value.IsValid()
                ? Value->AsObject()
                : nullptr;

        FGamePlatformInventoryQuickbarSlot Slot;
        if (!SlotJson.IsValid() ||
            !TryGetExactInt32(
                SlotJson,
                TEXT("slotIndex"),
                Slot.SlotIndex) ||
            !SlotJson->TryGetStringField(
                TEXT("itemInstanceId"),
                Slot.ItemInstanceId) ||
            !TryGetPositiveRevision(
                SlotJson,
                TEXT("revision"),
                Slot.Revision) ||
            Slot.SlotIndex < 0 ||
            Slot.ItemInstanceId.IsEmpty())
        {
            return false;
        }

        OutSnapshot.Quickbar.Add(MoveTemp(Slot));
    }

    return true;
}

bool FGamePlatformInventoryGatewayHttpTransport::JsonToMutationResult(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformInventoryMutationResult& OutResult)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString OperationId;
    const TSharedPtr<FJsonObject>* SnapshotJson = nullptr;

    if (!Json->TryGetStringField(
            TEXT("operationId"),
            OperationId) ||
        !FGuid::Parse(OperationId, OutResult.OperationId) ||
        !Json->TryGetObjectField(
            TEXT("snapshot"),
            SnapshotJson) ||
        !SnapshotJson ||
        !JsonToSnapshot(*SnapshotJson, OutResult.Snapshot) ||
        !TryGetExactInt32(
            Json,
            TEXT("movedQuantity"),
            OutResult.MovedQuantity) ||
        !TryGetExactInt32(
            Json,
            TEXT("remainingQuantity"),
            OutResult.RemainingQuantity) ||
        OutResult.MovedQuantity < 0 ||
        OutResult.RemainingQuantity < 0)
    {
        return false;
    }

    bool bDuplicate = false;
    if (!Json->TryGetBoolField(TEXT("duplicate"), bDuplicate))
    {
        return false;
    }
    OutResult.bDuplicate = bDuplicate;

    return true;
}

EGamePlatformInventoryError
FGamePlatformInventoryGatewayHttpTransport::MapHttpError(
    int32 StatusCode,
    const FString& Body)
{
    FString ErrorCode;
    if (!Body.IsEmpty())
    {
        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(Reader, Json) &&
            Json.IsValid())
        {
            Json->TryGetStringField(
                TEXT("errorCode"),
                ErrorCode);
        }
    }

    if (ErrorCode == TEXT("INVENTORY_ITEM_NOT_FOUND"))
        return EGamePlatformInventoryError::ItemNotFound;
    if (ErrorCode == TEXT("INVENTORY_DEFINITION_NOT_FOUND"))
        return EGamePlatformInventoryError::DefinitionNotFound;
    if (ErrorCode == TEXT("INVENTORY_INVALID_QUANTITY"))
        return EGamePlatformInventoryError::InvalidQuantity;
    if (ErrorCode == TEXT("INVENTORY_STACK_LIMIT_EXCEEDED"))
        return EGamePlatformInventoryError::StackLimitExceeded;
    if (ErrorCode == TEXT("INVENTORY_SLOT_OUT_OF_RANGE"))
        return EGamePlatformInventoryError::SlotOutOfRange;
    if (ErrorCode == TEXT("INVENTORY_SLOT_OCCUPIED"))
        return EGamePlatformInventoryError::SlotOccupied;
    if (ErrorCode == TEXT("INVENTORY_CONTAINER_NOT_FOUND"))
        return EGamePlatformInventoryError::ContainerNotFound;
    if (ErrorCode == TEXT("INVENTORY_REVISION_CONFLICT"))
        return EGamePlatformInventoryError::RevisionConflict;
    if (ErrorCode == TEXT("IDEMPOTENCY_CONFLICT"))
        return EGamePlatformInventoryError::DuplicateOperation;
    if (ErrorCode == TEXT("INVENTORY_OPERATION_IN_PROGRESS"))
        return EGamePlatformInventoryError::OperationInProgress;
    if (ErrorCode == TEXT("INVENTORY_OPERATION_NOT_FOUND"))
        return EGamePlatformInventoryError::OperationNotFound;
    if (ErrorCode == TEXT("INVENTORY_FULL"))
        return EGamePlatformInventoryError::InventoryFull;
    if (ErrorCode == TEXT("INVENTORY_CONSUME_NOT_ALLOWED"))
        return EGamePlatformInventoryError::ConsumeNotAllowed;
    if (ErrorCode == TEXT("INVENTORY_INSUFFICIENT_QUANTITY"))
        return EGamePlatformInventoryError::InsufficientQuantity;
    if (ErrorCode == TEXT("AUTH_SESSION_INVALID") ||
        ErrorCode == TEXT("AUTH_TOKEN_EXPIRED") ||
        ErrorCode == TEXT("AUTH_FORBIDDEN"))
        return EGamePlatformInventoryError::Unauthorized;
    if (ErrorCode == TEXT("SERVICE_UNAVAILABLE"))
        return EGamePlatformInventoryError::BackendUnavailable;

    if (StatusCode == 401 || StatusCode == 403)
        return EGamePlatformInventoryError::Unauthorized;
    if (StatusCode == 408)
        return EGamePlatformInventoryError::TimedOut;
    if (StatusCode == 0 || StatusCode >= 500)
        return EGamePlatformInventoryError::BackendUnavailable;

    return EGamePlatformInventoryError::InvalidResponse;
}
