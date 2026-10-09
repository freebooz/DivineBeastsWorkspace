// 平台诊断注册表实现：进程级仅保存机制和弱目标，快照所属世界由每次调用的Context提供。
// 采集不改变权威状态，移除/失效目标不会延长Actor/World生命周期；输出统一过滤敏感字段并限长。
#include "Registry/GamePlatformDebugRegistry.h"
#include "Engine/World.h"

#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

FGamePlatformDebugRegistry& FGamePlatformDebugRegistry::Get()
{
    static FGamePlatformDebugRegistry Instance;
    return Instance;
}

bool FGamePlatformDebugRegistry::RegisterStateProvider(
    const TSharedRef<IGamePlatformDebugStateProvider>& Provider)
{
    const FName ProviderId = Provider->GetProviderId();
    if (ProviderId.IsNone())
    {
        return false;
    }

    FScopeLock Lock(&Mutex);
    if (Providers.Contains(ProviderId))
    {
        return false;
    }

    Providers.Add(ProviderId, Provider);
    ProviderEnabled.FindOrAdd(ProviderId, true);
    return true;
}

void FGamePlatformDebugRegistry::UnregisterStateProvider(FName ProviderId)
{
    FScopeLock Lock(&Mutex);
    Providers.Remove(ProviderId);
    ProviderEnabled.Remove(ProviderId);
}

TArray<FName> FGamePlatformDebugRegistry::GetProviderIds() const
{
    FScopeLock Lock(&Mutex);
    TArray<FName> Result;
    Providers.GetKeys(Result);
    Result.Sort(FNameLexicalLess());
    return Result;
}

bool FGamePlatformDebugRegistry::RegisterCommandDescriptor(
    const FGamePlatformDebugCommandDescriptor& Descriptor)
{
    if (Descriptor.Name.IsNone() || !Descriptor.Name.ToString().StartsWith(TEXT("gp.Debug.")))
    {
        return false;
    }

    FScopeLock Lock(&Mutex);
    if (Commands.Contains(Descriptor.Name))
    {
        return false;
    }

    Commands.Add(Descriptor.Name, Descriptor);
    return true;
}

void FGamePlatformDebugRegistry::UnregisterCommandDescriptor(FName CommandName)
{
    FScopeLock Lock(&Mutex);
    Commands.Remove(CommandName);
}

TArray<FGamePlatformDebugCommandDescriptor>
FGamePlatformDebugRegistry::GetCommandDescriptors() const
{
    FScopeLock Lock(&Mutex);
    TArray<FGamePlatformDebugCommandDescriptor> Result;
    Commands.GenerateValueArray(Result);
    Result.Sort([](
        const FGamePlatformDebugCommandDescriptor& A,
        const FGamePlatformDebugCommandDescriptor& B)
    {
        return A.Name.LexicalLess(B.Name);
    });
    return Result;
}

bool FGamePlatformDebugRegistry::SetProviderEnabled(FName ProviderId, bool bEnabled)
{
    FScopeLock Lock(&Mutex);
    if (!Providers.Contains(ProviderId))
    {
        return false;
    }

    ProviderEnabled.Add(ProviderId, bEnabled);
    return true;
}

bool FGamePlatformDebugRegistry::IsProviderEnabled(FName ProviderId) const
{
    FScopeLock Lock(&Mutex);
    const bool* Found = ProviderEnabled.Find(ProviderId);
    return Found && *Found;
}

bool FGamePlatformDebugRegistry::CollectSnapshot(
    FName ProviderId,
    const FGamePlatformDebugCollectContext& Context,
    FGamePlatformDebugSnapshot& OutSnapshot)
{
    TSharedPtr<IGamePlatformDebugStateProvider> Provider;
    bool bEnabled = false;

    {
        FScopeLock Lock(&Mutex);
        Provider = Providers.FindRef(ProviderId);
        bEnabled = ProviderEnabled.FindRef(ProviderId);
    }

    OutSnapshot = {};
    OutSnapshot.RequestId = Context.RequestId.IsValid()
        ? Context.RequestId
        : FGuid::NewGuid();
    OutSnapshot.CategoryId = ProviderId;
    OutSnapshot.SourceView = Context.SourceView;
    OutSnapshot.TimestampSeconds = FPlatformTime::Seconds();
    OutSnapshot.Revision = static_cast<uint64>(RevisionCounter.Increment());

    UWorld* World = Context.World.Get();
    if (World)
    {
        OutSnapshot.WorldGeneration = static_cast<int32>(World->GetUniqueID());
    }

    if (AActor* TargetActor = Context.Target.Get())
    {
        const FGamePlatformDebugTarget Target = ResolveDebugTarget(TargetActor);
        OutSnapshot.TargetId = Target.DebugTargetId;
        OutSnapshot.TargetGeneration = Target.TargetGeneration;
    }

    if (!Provider.IsValid())
    {
        OutSnapshot.FailureReason = TEXT("Provider not registered");
        SanitizeAndBoundSnapshot(OutSnapshot);
        return false;
    }

    if (!bEnabled)
    {
        OutSnapshot.FailureReason = TEXT("Provider disabled");
        SanitizeAndBoundSnapshot(OutSnapshot);
        return false;
    }

    if (Provider->GetEstimatedCost() == EGamePlatformDebugProviderCost::Expensive &&
        !Context.bAllowExpensive)
    {
        OutSnapshot.FailureReason = TEXT("Expensive provider requires manual/low-frequency collection");
        SanitizeAndBoundSnapshot(OutSnapshot);
        return false;
    }

    if (!Provider->CanCollect(Context))
    {
        OutSnapshot.FailureReason = TEXT("Provider cannot collect for this target");
        SanitizeAndBoundSnapshot(OutSnapshot);
        return false;
    }

    const bool bCollected = Provider->CollectSnapshot(Context, OutSnapshot);
    if (!bCollected && OutSnapshot.FailureReason.IsEmpty())
    {
        OutSnapshot.FailureReason = TEXT("Provider collection failed");
    }

    SanitizeAndBoundSnapshot(OutSnapshot);
    PruneExpiredTargets();
    return bCollected;
}

FGamePlatformDebugTarget FGamePlatformDebugRegistry::ResolveDebugTarget(AActor* Actor)
{
    FGamePlatformDebugTarget Empty;
    if (!IsValid(Actor))
    {
        return Empty;
    }

    FScopeLock Lock(&Mutex);
    const TWeakObjectPtr<AActor> Key(Actor);
    if (FGamePlatformDebugTarget* Existing = Targets.Find(Key))
    {
        if (Existing->World.Get() == Actor->GetWorld())
        {
            return *Existing;
        }
        Targets.Remove(Key);
    }

    FGamePlatformDebugTarget Result;
    Result.DebugTargetId = FGuid::NewGuid();
    Result.Actor = Actor;
    Result.World = Actor->GetWorld();
    Result.TargetGeneration = static_cast<int32>(Actor->GetUniqueID());
    Targets.Add(Key, Result);
    return Result;
}

void FGamePlatformDebugRegistry::PruneExpiredTargets()
{
    FScopeLock Lock(&Mutex);
    for (auto It = Targets.CreateIterator(); It; ++It)
    {
        const FGamePlatformDebugTarget& Target = It.Value();
        AActor* Actor = Target.Actor.Get();
        if (!Actor || !Target.World.IsValid() || Actor->GetWorld() != Target.World.Get())
        {
            It.RemoveCurrent();
        }
    }
}

bool FGamePlatformDebugRegistry::IsSensitiveField(
    const FGamePlatformDebugField& Field)
{
    if (Field.bSensitive)
    {
        return true;
    }

    FString Haystack = Field.Key.ToString() + TEXT(" ") + Field.DisplayName;
    Haystack = Haystack.ToLower();
    Haystack.ReplaceInline(TEXT("_"), TEXT(""));
    Haystack.ReplaceInline(TEXT("-"), TEXT(""));
    Haystack.ReplaceInline(TEXT(" "), TEXT(""));

    static const TCHAR* DenyTokens[] =
    {
        TEXT("accesstoken"),
        TEXT("refreshtoken"),
        TEXT("authorization"),
        TEXT("cookie"),
        TEXT("password"),
        TEXT("clientsecret"),
        TEXT("privatekey"),
        TEXT("signature"),
        TEXT("nonce"),
        TEXT("paymentreceipt"),
        TEXT("receipt"),
        TEXT("transferticket")
    };

    for (const TCHAR* Token : DenyTokens)
    {
        if (Haystack.Contains(Token))
        {
            return true;
        }
    }

    return false;
}

void FGamePlatformDebugRegistry::SanitizeAndBoundSnapshot(
    FGamePlatformDebugSnapshot& Snapshot) const
{
    TArray<FGamePlatformDebugField> SafeFields;
    SafeFields.Reserve(FMath::Min(
        Snapshot.Fields.Num(),
        GamePlatformDebugLimits::MaxFieldsPerSnapshot));

    int32 EstimatedCharacters = 0;
    for (FGamePlatformDebugField Field : Snapshot.Fields)
    {
        if (IsSensitiveField(Field))
        {
            Snapshot.bTruncated = true;
            continue;
        }

        if (SafeFields.Num() >= GamePlatformDebugLimits::MaxFieldsPerSnapshot)
        {
            Snapshot.bTruncated = true;
            break;
        }

        if (Field.Value.Len() > GamePlatformDebugLimits::MaxStringCharacters)
        {
            Field.Value = Field.Value.Left(GamePlatformDebugLimits::MaxStringCharacters);
            Field.Value += TEXT("…");
            Snapshot.bTruncated = true;
        }

        EstimatedCharacters +=
            Field.Key.ToString().Len() +
            Field.DisplayName.Len() +
            Field.Value.Len();

        if (EstimatedCharacters > GamePlatformDebugLimits::MaxPayloadCharacters)
        {
            Snapshot.bTruncated = true;
            break;
        }

        SafeFields.Add(MoveTemp(Field));
    }

    Snapshot.Fields = MoveTemp(SafeFields);
    if (Snapshot.FailureReason.Len() > GamePlatformDebugLimits::MaxStringCharacters)
    {
        Snapshot.FailureReason =
            Snapshot.FailureReason.Left(GamePlatformDebugLimits::MaxStringCharacters);
        Snapshot.bTruncated = true;
    }
}

FString FGamePlatformDebugRegistry::BuildSanitizedSummary(
    const FGamePlatformDebugSnapshot& Snapshot) const
{
    FGamePlatformDebugSnapshot SafeCopy = Snapshot;
    SanitizeAndBoundSnapshot(SafeCopy);

    FString Result = FString::Printf(
        TEXT("[%s] revision=%llu target=%s generation=%d world=%d"),
        *SafeCopy.CategoryId.ToString(),
        SafeCopy.Revision,
        *SafeCopy.TargetId.ToString(EGuidFormats::DigitsWithHyphens),
        SafeCopy.TargetGeneration,
        SafeCopy.WorldGeneration);

    if (!SafeCopy.FailureReason.IsEmpty())
    {
        Result += FString::Printf(TEXT("\nFailure: %s"), *SafeCopy.FailureReason);
    }

    for (const FGamePlatformDebugField& Field : SafeCopy.Fields)
    {
        Result += FString::Printf(
            TEXT("\n%s=%s"),
            *Field.Key.ToString(),
            *Field.Value);
    }

    if (SafeCopy.bTruncated)
    {
        Result += TEXT("\n[truncated]");
    }

    return Result;
}

uint64 FGamePlatformDebugRegistry::ForceRefreshRevision()
{
    return static_cast<uint64>(RevisionCounter.Increment());
}
