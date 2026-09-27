#include "Transport/GamePlatformInventoryGatewayHttpTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FGamePlatformInventoryGatewayHttpTransport::
FGamePlatformInventoryGatewayHttpTransport(
    FString InGatewayBaseUrl,
    FString InAccessToken)
    : GatewayBaseUrl(MoveTemp(InGatewayBaseUrl))
    , AccessToken(MoveTemp(InAccessToken))
{
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool FGamePlatformInventoryGatewayHttpTransport::IsConfigured() const
{
    return !GatewayBaseUrl.IsEmpty() &&
           !AccessToken.IsEmpty();
}

void FGamePlatformInventoryGatewayHttpTransport::CancelAllRequests()
{
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> Requests;

    {
        FScopeLock Lock(&ActiveRequestsMutex);
        Requests = ActiveRequests;
        ActiveRequests.Reset();
    }

    for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request :
         Requests)
    {
        if (Request.IsValid())
        {
            Request->CancelRequest();
        }
    }
}

void FGamePlatformInventoryGatewayHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&ActiveRequestsMutex);
    ActiveRequests.Remove(Request);
}

FString FGamePlatformInventoryGatewayHttpTransport::GuidString(
    const FGuid& Guid)
{
    return Guid.IsValid()
        ? Guid.ToString(EGuidFormats::DigitsWithHyphensLower)
        : FString();
}

bool FGamePlatformInventoryGatewayHttpTransport::StartJsonRequest(
    const FString& Verb,
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    TFunction<void(int32, const FString&)> Completion)
{
    if (!IsConfigured() || !Completion)
    {
        return false;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> RequestPtr = Request;

    {
        FScopeLock Lock(&ActiveRequestsMutex);
        constexpr int32 MaxConcurrentHttpRequests = 8;
        if (ActiveRequests.Num() >= MaxConcurrentHttpRequests)
        {
            return false;
        }
        ActiveRequests.Add(RequestPtr);
    }

    TSharedRef<FGamePlatformInventoryGatewayHttpTransport, ESPMode::ThreadSafe>
        Self = AsShared();

    Request->SetURL(GatewayBaseUrl + Path);
    Request->SetVerb(Verb);
    Request->SetHeader(
        TEXT("Authorization"),
        FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json"));

    if (Body.IsValid())
    {
        FString Payload;
        TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Payload);

        if (!FJsonSerializer::Serialize(
                Body.ToSharedRef(),
                Writer))
        {
            UnregisterRequest(RequestPtr);
            return false;
        }

        Request->SetHeader(
            TEXT("Content-Type"),
            TEXT("application/json"));
        Request->SetContentAsString(Payload);
    }

    Request->OnProcessRequestComplete().BindLambda(
        [Self,
         RequestPtr,
         Completion = MoveTemp(Completion)](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bConnectedSuccessfully) mutable
        {
            const int32 StatusCode =
                bConnectedSuccessfully && Response.IsValid()
                    ? Response->GetResponseCode()
                    : 0;

            const FString BodyText =
                Response.IsValid()
                    ? Response->GetContentAsString()
                    : FString();

            Self->UnregisterRequest(RequestPtr);

            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Completion),
                 StatusCode,
                 BodyText]() mutable
                {
                    Completion(StatusCode, BodyText);
                });
        });

    const bool bStarted = Request->ProcessRequest();
    if (!bStarted)
    {
        UnregisterRequest(RequestPtr);
    }
    return bStarted;
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformInventorySnapshotCompletion Completion)
{
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/inventory"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, Body));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
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
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body) mutable
        {
            if (StatusCode == 404)
            {
                Completion(
                    {},
                    EGamePlatformInventoryError::OperationNotFound);
                return;
            }

            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, Body));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
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
    Body->SetStringField(TEXT("operation_id"), GuidString(Request.OperationId));
    Body->SetNumberField(TEXT("expected_revision"), static_cast<double>(Request.ExpectedRevision));
    Body->SetStringField(TEXT("item_instance_id"), Request.ItemInstanceId);
    Body->SetStringField(TEXT("target_container_id"), Request.TargetContainerId.ToString());
    Body->SetNumberField(TEXT("target_slot_index"), Request.TargetSlotIndex);
    return BeginMutation(TEXT("/v1/inventory/move"), Body, MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginSplit(
    const FGamePlatformInventorySplitRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("operation_id"), GuidString(Request.OperationId));
    Body->SetNumberField(TEXT("expected_revision"), static_cast<double>(Request.ExpectedRevision));
    Body->SetStringField(TEXT("source_item_instance_id"), Request.SourceItemInstanceId);
    Body->SetNumberField(TEXT("split_quantity"), Request.SplitQuantity);
    Body->SetStringField(TEXT("target_container_id"), Request.TargetContainerId.ToString());
    Body->SetNumberField(TEXT("target_slot_index"), Request.TargetSlotIndex);
    return BeginMutation(TEXT("/v1/inventory/split"), Body, MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginMerge(
    const FGamePlatformInventoryMergeRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("operation_id"), GuidString(Request.OperationId));
    Body->SetNumberField(TEXT("expected_revision"), static_cast<double>(Request.ExpectedRevision));
    Body->SetStringField(TEXT("source_item_instance_id"), Request.SourceItemInstanceId);
    Body->SetStringField(TEXT("target_item_instance_id"), Request.TargetItemInstanceId);
    return BeginMutation(TEXT("/v1/inventory/merge"), Body, MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginSetQuickbar(
    const FGamePlatformInventoryQuickbarRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("operation_id"), GuidString(Request.OperationId));
    Body->SetNumberField(TEXT("expected_revision"), static_cast<double>(Request.ExpectedRevision));
    Body->SetNumberField(TEXT("slot_index"), Request.SlotIndex);
    Body->SetStringField(TEXT("item_instance_id"), Request.ItemInstanceId);
    return BeginMutation(TEXT("/v1/inventory/quickbar/set"), Body, MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginClearQuickbar(
    const FGamePlatformInventoryQuickbarRequest& Request,
    FGamePlatformInventoryMutationCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("operation_id"), GuidString(Request.OperationId));
    Body->SetNumberField(TEXT("expected_revision"), static_cast<double>(Request.ExpectedRevision));
    Body->SetNumberField(TEXT("slot_index"), Request.SlotIndex);
    return BeginMutation(TEXT("/v1/inventory/quickbar/clear"), Body, MoveTemp(Completion));
}

bool FGamePlatformInventoryGatewayHttpTransport::BeginMutation(
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    FGamePlatformInventoryMutationCompletion Completion)
{
    return StartJsonRequest(
        TEXT("POST"),
        Path,
        Body,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
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

    double Revision = 0.0;
    if (!Json->TryGetNumberField(
            TEXT("inventory_revision"),
            Revision) ||
        Revision <= 0.0)
    {
        return false;
    }

    OutSnapshot = {};
    OutSnapshot.InventoryRevision =
        static_cast<int64>(Revision);

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (Json->TryGetArrayField(TEXT("items"), Items) && Items)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Items)
        {
            const TSharedPtr<FJsonObject> ItemJson =
                Value.IsValid()
                    ? Value->AsObject()
                    : nullptr;

            if (!ItemJson.IsValid())
            {
                return false;
            }

            FGamePlatformInventoryItemInstance Item;
            FString ItemDefinitionId;
            FString ContainerId;
            FString InstanceState;
            double Quantity = 0.0;
            double SlotIndex = 0.0;
            double ItemRevision = 0.0;

            if (!ItemJson->TryGetStringField(TEXT("item_instance_id"), Item.ItemInstanceId) ||
                !ItemJson->TryGetStringField(TEXT("item_definition_id"), ItemDefinitionId) ||
                !ItemJson->TryGetNumberField(TEXT("quantity"), Quantity) ||
                !ItemJson->TryGetStringField(TEXT("container_id"), ContainerId) ||
                !ItemJson->TryGetNumberField(TEXT("slot_index"), SlotIndex) ||
                !ItemJson->TryGetNumberField(TEXT("revision"), ItemRevision) ||
                !ItemJson->TryGetStringField(TEXT("instance_state"), InstanceState))
            {
                return false;
            }

            Item.ItemDefinitionId = FName(*ItemDefinitionId);
            Item.Quantity = static_cast<int32>(Quantity);
            Item.ContainerId = FName(*ContainerId);
            Item.SlotIndex = static_cast<int32>(SlotIndex);
            Item.Revision = static_cast<int64>(ItemRevision);
            Item.InstanceState = FName(*InstanceState);

            if (!Item.IsValid())
            {
                return false;
            }

            OutSnapshot.Items.Add(MoveTemp(Item));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Quickbar = nullptr;
    if (Json->TryGetArrayField(TEXT("quickbar"), Quickbar) && Quickbar)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Quickbar)
        {
            const TSharedPtr<FJsonObject> SlotJson =
                Value.IsValid()
                    ? Value->AsObject()
                    : nullptr;

            if (!SlotJson.IsValid())
            {
                return false;
            }

            FGamePlatformInventoryQuickbarSlot Slot;
            double SlotIndex = 0.0;
            double SlotRevision = 0.0;

            if (!SlotJson->TryGetNumberField(TEXT("slot_index"), SlotIndex) ||
                !SlotJson->TryGetStringField(TEXT("item_instance_id"), Slot.ItemInstanceId) ||
                !SlotJson->TryGetNumberField(TEXT("revision"), SlotRevision))
            {
                return false;
            }

            Slot.SlotIndex = static_cast<int32>(SlotIndex);
            Slot.Revision = static_cast<int64>(SlotRevision);
            if (Slot.SlotIndex < 0 || Slot.Revision <= 0)
            {
                return false;
            }

            OutSnapshot.Quickbar.Add(MoveTemp(Slot));
        }
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

    if (!Json->TryGetStringField(TEXT("operation_id"), OperationId) ||
        !FGuid::Parse(OperationId, OutResult.OperationId) ||
        !Json->TryGetObjectField(TEXT("snapshot"), SnapshotJson) ||
        !SnapshotJson ||
        !JsonToSnapshot(*SnapshotJson, OutResult.Snapshot))
    {
        return false;
    }

    double Number = 0.0;
    if (Json->TryGetNumberField(TEXT("moved_quantity"), Number))
    {
        OutResult.MovedQuantity = static_cast<int32>(Number);
    }
    if (Json->TryGetNumberField(TEXT("remaining_quantity"), Number))
    {
        OutResult.RemainingQuantity = static_cast<int32>(Number);
    }

    bool bDuplicate = false;
    if (Json->TryGetBoolField(TEXT("duplicate"), bDuplicate))
    {
        OutResult.bDuplicate = bDuplicate;
    }

    return true;
}

EGamePlatformInventoryError
FGamePlatformInventoryGatewayHttpTransport::MapHttpError(
    int32 StatusCode,
    const FString& Body)
{
    const FString Lower = Body.ToLower();

    if (StatusCode == 0 || StatusCode >= 500)
    {
        return EGamePlatformInventoryError::BackendUnavailable;
    }
    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformInventoryError::Unauthorized;
    }
    if (StatusCode == 404)
    {
        return Lower.Contains(TEXT("definition"))
            ? EGamePlatformInventoryError::DefinitionNotFound
            : EGamePlatformInventoryError::ItemNotFound;
    }
    if (StatusCode == 409)
    {
        if (Lower.Contains(TEXT("revision")))
            return EGamePlatformInventoryError::RevisionConflict;
        if (Lower.Contains(TEXT("slot occupied")))
            return EGamePlatformInventoryError::SlotOccupied;
        if (Lower.Contains(TEXT("inventory full")))
            return EGamePlatformInventoryError::InventoryFull;
        if (Lower.Contains(TEXT("insufficient")))
            return EGamePlatformInventoryError::InsufficientQuantity;
        if (Lower.Contains(TEXT("consume")))
            return EGamePlatformInventoryError::ConsumeNotAllowed;
        if (Lower.Contains(TEXT("stack")))
            return EGamePlatformInventoryError::StackLimitExceeded;
        return EGamePlatformInventoryError::OutcomeUnknown;
    }
    if (StatusCode == 400)
    {
        if (Lower.Contains(TEXT("quantity")))
            return EGamePlatformInventoryError::InvalidQuantity;
        if (Lower.Contains(TEXT("slot")))
            return EGamePlatformInventoryError::SlotOutOfRange;
        if (Lower.Contains(TEXT("container")))
            return EGamePlatformInventoryError::ContainerNotFound;
    }

    return EGamePlatformInventoryError::OutcomeUnknown;
}
