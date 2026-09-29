#include "Subsystems/GamePlatformSaveSubsystem.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Misc/App.h"
#include "Policy/GamePlatformSavePolicy.h"
#include "Storage/GamePlatformSaveStorage.h"

namespace
{
    constexpr int32 MaxConcurrentSaveRequests = 16;

    FGamePlatformSaveOperationResult LoadFromStorage(
        const FGamePlatformSaveKey& Key,
        const FString& StableStorageId)
    {
        FGamePlatformSaveOperationResult Output;
        Output.Operation = EGamePlatformSaveOperation::Load;

        const FString PrimaryPath =
            FGamePlatformSaveStorage::BuildPrimaryPath(StableStorageId);
        const FString BackupPath =
            FGamePlatformSaveStorage::BuildBackupPath(PrimaryPath);

        TArray<uint8> Encoded;
        FGamePlatformResult PrimaryResult =
            FGamePlatformSaveStorage::ReadFile(PrimaryPath, Encoded);
        if (PrimaryResult.IsSuccess())
        {
            FGamePlatformSaveRecord Record;
            PrimaryResult = FGamePlatformSavePolicy::DecodeRecord(
                Key,
                Encoded,
                Record);
            if (PrimaryResult.IsSuccess())
            {
                Output.Record = MoveTemp(Record);
                Output.Result = FGamePlatformResult::Success();
                return Output;
            }
        }

        Encoded.Reset();
        FGamePlatformResult BackupResult =
            FGamePlatformSaveStorage::ReadFile(BackupPath, Encoded);
        if (BackupResult.IsSuccess())
        {
            FGamePlatformSaveRecord Record;
            BackupResult = FGamePlatformSavePolicy::DecodeRecord(
                Key,
                Encoded,
                Record);
            if (BackupResult.IsSuccess())
            {
                Output.Record = MoveTemp(Record);
                Output.bRecoveredFromBackup = true;
                Output.Result = FGamePlatformResult::Success();
                return Output;
            }
        }

        const bool bPrimaryMissing =
            PrimaryResult.Code == FName(TEXT("SaveFileNotFound"));
        const bool bBackupMissing =
            BackupResult.Code == FName(TEXT("SaveFileNotFound"));
        Output.Result = bPrimaryMissing && bBackupMissing
            ? FGamePlatformResult::Failure(
                TEXT("SaveRecordNotFound"),
                TEXT("本地存档主文件和备份均不存在。"))
            : FGamePlatformResult::Failure(
                TEXT("SaveRecordUnavailable"),
                TEXT("本地存档主文件与备份均无法通过读取或完整性校验。"));
        return Output;
    }
}

bool UGamePlatformSaveSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return !IsRunningDedicatedServer() &&
        !IsRunningCommandlet() &&
        Cast<UGameInstance>(Outer) != nullptr &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformSaveSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ScopeId = FGuid::NewGuid();
    Generation = 1;
}

void UGamePlatformSaveSubsystem::Deinitialize()
{
    // 先使旧后台完成回调失效，再同步向仍登记的调用方返回Cancelled。
    ++Generation;
    CancelPendingRequests();
    Providers.Reset();
    BusyStorageIds.Reset();
    ScopeId.Invalidate();
    Super::Deinitialize();
}

bool UGamePlatformSaveSubsystem::IsMutationEnvironmentAllowed() const
{
    if (IsRunningDedicatedServer() || IsRunningCommandlet() ||
        GetGameInstance() == nullptr)
    {
        return false;
    }

#if WITH_EDITOR
    // 普通PIE共享同一进程与Saved目录，禁止把编辑器多实例误当真实玩家存档环境。
    if (GIsEditor && !FApp::IsGame())
    {
        return false;
    }
#endif

    return true;
}

TSharedPtr<IGamePlatformSaveProvider>
UGamePlatformSaveSubsystem::FindProvider(const FName Namespace) const
{
    const TSharedPtr<IGamePlatformSaveProvider>* Found =
        Providers.Find(Namespace);
    return Found ? *Found : nullptr;
}

FGamePlatformResult UGamePlatformSaveSubsystem::RegisterProvider(
    const TSharedRef<IGamePlatformSaveProvider>& Provider)
{
    check(IsInGameThread());

    if (bDeliveringCallback)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveCallbackReentrancy"),
            TEXT("存档完成回调期间禁止同步注册Provider。"));
    }

    const FName Namespace = Provider->GetSaveNamespace();
    if (Namespace.IsNone() || Provider->GetCurrentSchemaVersion() <= 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveProviderInvalid"),
            TEXT("存档Provider必须提供非空Namespace和正整数SchemaVersion。"));
    }

    if (Providers.Contains(Namespace))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveProviderDuplicate"),
            TEXT("同一存档Namespace只允许注册一个Provider。"));
    }

    Providers.Add(Namespace, Provider);
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSaveSubsystem::UnregisterProvider(
    const FName Namespace)
{
    check(IsInGameThread());

    if (bDeliveringCallback)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveCallbackReentrancy"),
            TEXT("存档完成回调期间禁止同步注销Provider。"));
    }

    if (Namespace.IsNone() || Providers.Remove(Namespace) == 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveProviderNotRegistered"),
            TEXT("目标存档Namespace没有已注册Provider。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformSaveRequestHandle UGamePlatformSaveSubsystem::BeginRequest(
    const FGamePlatformSaveKey& Key,
    const EGamePlatformSaveOperation Operation,
    FGamePlatformSaveCallback Callback,
    TSharedPtr<IGamePlatformSaveProvider> ProviderSnapshot,
    FGamePlatformResult& OutStartResult)
{
    check(IsInGameThread());

    OutStartResult = FGamePlatformResult::Failure(
        TEXT("SaveRequestNotStarted"),
        TEXT("本地存档请求尚未启动。"));

    if (!IsMutationEnvironmentAllowed())
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Unsupported(
            TEXT("SaveEnvironmentUnsupported"),
            TEXT("当前环境不允许执行客户端本地存档IO。"));
        return {};
    }

    if (bDeliveringCallback)
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Failure(
            TEXT("SaveCallbackReentrancy"),
            TEXT("存档完成回调期间禁止同步反入Save服务。"));
        return {};
    }

    if (!Callback)
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Failure(
            TEXT("SaveCallbackRequired"),
            TEXT("异步本地存档请求必须提供完成回调。"));
        return {};
    }

    const FGamePlatformResult Validation =
        FGamePlatformSavePolicy::ValidateKey(Key);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = Validation;
        return {};
    }

    if (PendingRequests.Num() >= MaxConcurrentSaveRequests)
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Failure(
            TEXT("SaveRequestLimitReached"),
            TEXT("本地存档并发请求已达到平台安全上限。"));
        return {};
    }

    const FString StableStorageId =
        FGamePlatformSavePolicy::BuildStableStorageId(Key);
    if (BusyStorageIds.Contains(StableStorageId))
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Failure(
            TEXT("SaveSlotBusy"),
            TEXT("同一逻辑存档槽位已有IO请求进行中。"));
        return {};
    }

    FGamePlatformSaveRequestHandle Handle;
    Handle.ScopeId = ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = Generation;

    FPendingRequest Pending;
    Pending.Handle = Handle;
    Pending.Key = Key;
    Pending.StableStorageId = StableStorageId;
    Pending.Operation = Operation;
    Pending.Callback = MoveTemp(Callback);
    Pending.ProviderSnapshot = MoveTemp(ProviderSnapshot);

    PendingRequests.Add(Handle.Id, MoveTemp(Pending));
    BusyStorageIds.Add(StableStorageId);
    ++Diagnostics.StartedRequestCount;
    OutStartResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformSaveRequestHandle UGamePlatformSaveSubsystem::SaveRecordAsync(
    const FGamePlatformSaveRecord& Record,
    FGamePlatformSaveCallback Callback,
    FGamePlatformResult& OutStartResult)
{
    check(IsInGameThread());

    const FGamePlatformResult Validation =
        FGamePlatformSavePolicy::ValidateRecord(Record);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = Validation;
        return {};
    }

    const TSharedPtr<IGamePlatformSaveProvider> Provider =
        FindProvider(Record.Key.Namespace);
    if (Provider.IsValid() &&
        Record.SchemaVersion != Provider->GetCurrentSchemaVersion())
    {
        ++Diagnostics.RejectedRequestCount;
        OutStartResult = FGamePlatformResult::Failure(
            TEXT("SaveSchemaVersionNotCurrent"),
            TEXT("保存记录SchemaVersion必须与已注册Provider当前版本一致。"));
        return {};
    }

    const FGamePlatformSaveRequestHandle Handle = BeginRequest(
        Record.Key,
        EGamePlatformSaveOperation::Save,
        MoveTemp(Callback),
        nullptr,
        OutStartResult);
    if (!Handle.IsValid())
    {
        return Handle;
    }

    const TWeakObjectPtr<UGamePlatformSaveSubsystem> WeakThis(this);
    const FGamePlatformSaveRecord RecordCopy = Record;

    Async(EAsyncExecution::ThreadPool,
        [WeakThis, Handle, RecordCopy]() mutable
        {
            TArray<uint8> Encoded;
            FGamePlatformResult Result =
                FGamePlatformSavePolicy::EncodeRecord(RecordCopy, Encoded);
            if (Result.IsSuccess())
            {
                const FString StableStorageId =
                    FGamePlatformSavePolicy::BuildStableStorageId(RecordCopy.Key);
                const FString PrimaryPath =
                    FGamePlatformSaveStorage::BuildPrimaryPath(StableStorageId);
                Result = FGamePlatformSaveStorage::WriteAtomically(
                    PrimaryPath,
                    Encoded);
            }

            FGamePlatformSaveOperationResult OperationResult;
            OperationResult.Operation = EGamePlatformSaveOperation::Save;
            OperationResult.Result = MoveTemp(Result);
            if (OperationResult.Result.IsSuccess())
            {
                OperationResult.Record = MoveTemp(RecordCopy);
            }

            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, Handle,
                 OperationResult = MoveTemp(OperationResult)]() mutable
                {
                    if (UGamePlatformSaveSubsystem* This = WeakThis.Get())
                    {
                        This->CompleteRequest(
                            Handle,
                            MoveTemp(OperationResult));
                    }
                });
        });

    return Handle;
}

FGamePlatformSaveRequestHandle UGamePlatformSaveSubsystem::LoadRecordAsync(
    const FGamePlatformSaveKey& Key,
    FGamePlatformSaveCallback Callback,
    FGamePlatformResult& OutStartResult)
{
    check(IsInGameThread());

    const TSharedPtr<IGamePlatformSaveProvider> Provider =
        FindProvider(Key.Namespace);
    const FGamePlatformSaveRequestHandle Handle = BeginRequest(
        Key,
        EGamePlatformSaveOperation::Load,
        MoveTemp(Callback),
        Provider,
        OutStartResult);
    if (!Handle.IsValid())
    {
        return Handle;
    }

    const TWeakObjectPtr<UGamePlatformSaveSubsystem> WeakThis(this);
    const FString StableStorageId =
        FGamePlatformSavePolicy::BuildStableStorageId(Key);

    Async(EAsyncExecution::ThreadPool,
        [WeakThis, Handle, Key, StableStorageId]() mutable
        {
            FGamePlatformSaveOperationResult OperationResult =
                LoadFromStorage(Key, StableStorageId);

            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, Handle,
                 OperationResult = MoveTemp(OperationResult)]() mutable
                {
                    if (UGamePlatformSaveSubsystem* This = WeakThis.Get())
                    {
                        This->CompleteRequest(
                            Handle,
                            MoveTemp(OperationResult));
                    }
                });
        });

    return Handle;
}

FGamePlatformSaveRequestHandle UGamePlatformSaveSubsystem::DeleteRecordAsync(
    const FGamePlatformSaveKey& Key,
    FGamePlatformSaveCallback Callback,
    FGamePlatformResult& OutStartResult)
{
    check(IsInGameThread());

    const FGamePlatformSaveRequestHandle Handle = BeginRequest(
        Key,
        EGamePlatformSaveOperation::Delete,
        MoveTemp(Callback),
        nullptr,
        OutStartResult);
    if (!Handle.IsValid())
    {
        return Handle;
    }

    const TWeakObjectPtr<UGamePlatformSaveSubsystem> WeakThis(this);
    const FString StableStorageId =
        FGamePlatformSavePolicy::BuildStableStorageId(Key);

    Async(EAsyncExecution::ThreadPool,
        [WeakThis, Handle, StableStorageId]() mutable
        {
            const FString PrimaryPath =
                FGamePlatformSaveStorage::BuildPrimaryPath(StableStorageId);
            FGamePlatformSaveOperationResult OperationResult;
            OperationResult.Operation = EGamePlatformSaveOperation::Delete;
            OperationResult.Result =
                FGamePlatformSaveStorage::DeleteRecordFiles(PrimaryPath);

            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, Handle,
                 OperationResult = MoveTemp(OperationResult)]() mutable
                {
                    if (UGamePlatformSaveSubsystem* This = WeakThis.Get())
                    {
                        This->CompleteRequest(
                            Handle,
                            MoveTemp(OperationResult));
                    }
                });
        });

    return Handle;
}

void UGamePlatformSaveSubsystem::CompleteRequest(
    const FGamePlatformSaveRequestHandle& Handle,
    FGamePlatformSaveOperationResult OperationResult)
{
    check(IsInGameThread());

    if (Handle.ScopeId != ScopeId ||
        Handle.Generation != Generation ||
        !Handle.Id.IsValid())
    {
        return;
    }

    FPendingRequest* Pending = PendingRequests.Find(Handle.Id);
    if (!Pending)
    {
        return;
    }

    FPendingRequest Completed = MoveTemp(*Pending);
    PendingRequests.Remove(Handle.Id);
    BusyStorageIds.Remove(Completed.StableStorageId);

    if (OperationResult.Result.IsSuccess() &&
        OperationResult.Operation == EGamePlatformSaveOperation::Load &&
        Completed.ProviderSnapshot.IsValid())
    {
        // Provider属于外部领域实现；调用期间开启重入门禁，避免迁移代码同步反入Save服务。
        bDeliveringCallback = true;
        const int32 CurrentSchemaVersion =
            Completed.ProviderSnapshot->GetCurrentSchemaVersion();

        if (CurrentSchemaVersion <= 0)
        {
            OperationResult.Result = FGamePlatformResult::Failure(
                TEXT("SaveProviderSchemaInvalid"),
                TEXT("已注册Provider返回了非法当前SchemaVersion。"));
        }
        else if (OperationResult.Record.SchemaVersion > CurrentSchemaVersion)
        {
            OperationResult.Result = FGamePlatformResult::Failure(
                TEXT("SaveSchemaNewerThanProvider"),
                TEXT("本地存档Schema高于当前Provider支持版本，拒绝降级读取。"));
        }
        else if (OperationResult.Record.SchemaVersion < CurrentSchemaVersion)
        {
            const int32 StoredSchemaVersion =
                OperationResult.Record.SchemaVersion;
            TArray<uint8> MigratedPayload = OperationResult.Record.Payload;
            const FGamePlatformResult MigrationResult =
                Completed.ProviderSnapshot->MigratePayload(
                    StoredSchemaVersion,
                    CurrentSchemaVersion,
                    MigratedPayload);

            if (!MigrationResult.IsSuccess())
            {
                OperationResult.Result = MigrationResult;
            }
            else if (MigratedPayload.Num() >
                FGamePlatformSavePolicy::GetMaxPayloadBytes())
            {
                OperationResult.Result = FGamePlatformResult::Failure(
                    TEXT("SaveMigratedPayloadTooLarge"),
                    TEXT("迁移后的本地存档Payload超过平台安全上限。"));
            }
            else
            {
                OperationResult.Record.SchemaVersion = CurrentSchemaVersion;
                OperationResult.Record.Payload = MoveTemp(MigratedPayload);
                OperationResult.bMigrated = true;
            }
        }
        bDeliveringCallback = false;
    }

    if (OperationResult.bRecoveredFromBackup)
    {
        ++Diagnostics.RecoveredBackupCount;
    }
    if (OperationResult.bMigrated)
    {
        ++Diagnostics.MigratedRecordCount;
    }

    if (OperationResult.Result.IsSuccess())
    {
        ++Diagnostics.CompletedRequestCount;
    }
    else
    {
        ++Diagnostics.FailedRequestCount;
    }

    if (Completed.Callback)
    {
        bDeliveringCallback = true;
        Completed.Callback(OperationResult);
        bDeliveringCallback = false;
    }
}

void UGamePlatformSaveSubsystem::CancelPendingRequests()
{
    check(IsInGameThread());

    TArray<FPendingRequest> PendingSnapshot;
    PendingSnapshot.Reserve(PendingRequests.Num());
    for (TPair<FGuid, FPendingRequest>& Pair : PendingRequests)
    {
        PendingSnapshot.Add(MoveTemp(Pair.Value));
    }

    PendingRequests.Reset();
    BusyStorageIds.Reset();

    for (FPendingRequest& Pending : PendingSnapshot)
    {
        if (!Pending.Callback)
        {
            continue;
        }

        FGamePlatformSaveOperationResult Cancelled;
        Cancelled.Operation = Pending.Operation;
        Cancelled.Result = FGamePlatformResult::Cancelled(
            TEXT("GameInstance销毁，本地存档请求回调已取消。"));

        bDeliveringCallback = true;
        Pending.Callback(Cancelled);
        bDeliveringCallback = false;
    }
}

FGamePlatformSaveDiagnostics UGamePlatformSaveSubsystem::GetDiagnostics() const
{
    check(IsInGameThread());

    FGamePlatformSaveDiagnostics Result = Diagnostics;
    Result.ProviderCount = Providers.Num();
    Result.PendingRequestCount = PendingRequests.Num();
    return Result;
}
