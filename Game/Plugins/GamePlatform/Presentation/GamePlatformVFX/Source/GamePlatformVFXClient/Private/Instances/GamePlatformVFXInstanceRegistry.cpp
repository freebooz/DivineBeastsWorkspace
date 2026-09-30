#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "HAL/PlatformTime.h"
#include "NiagaraComponent.h"

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::Reserve(
    UWorld* World,
    const FGamePlatformVFXRequest* Request)
{
    FGamePlatformVFXHandle Handle;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = 1;
    Handle.World = World;

    FGamePlatformVFXInstanceRecord& Record = Records.Add(Handle.Id);
    Record.Handle = Handle;
    if (Request)
    {
        Record.Request = *Request;
    }
    Record.StartedAtSeconds = FPlatformTime::Seconds();
    Record.State = EGamePlatformVFXInstanceState::Pending;
    return Handle;
}

bool FGamePlatformVFXInstanceRegistry::SetDefinition(
    const FGamePlatformVFXHandle& Handle,
    UGamePlatformVFXDefinition* Definition)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !IsValid(Definition))
    {
        return false;
    }
    Record->Definition = Definition;
    Record->DefinitionPath = FSoftObjectPath(Definition->GetPathName());
    Record->MaxLifetimeSeconds = Definition->GetMaxLifetimeSeconds();
    return true;
}

bool FGamePlatformVFXInstanceRegistry::SetLoadLease(
    const FGamePlatformVFXHandle& Handle,
    const FGamePlatformVFXPreloadHandle& Lease)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !Lease.IsValid())
    {
        return false;
    }
    Record->LoadLease = Lease;
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AttachComponent(const FGamePlatformVFXHandle& Handle, UNiagaraComponent* Component, bool bPooled)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !IsValid(Component))
    {
        return false;
    }

    Record->Component = Component;
    Record->bPooled = bPooled;
    Record->State = EGamePlatformVFXInstanceState::Active;
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AddChild(const FGamePlatformVFXHandle& Parent, const FGamePlatformVFXHandle& Child)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Parent.Id);
    if (!Record || Record->Handle != Parent || !Child.IsValid())
    {
        return false;
    }

    Record->Children.AddUnique(Child);
    Record->State = EGamePlatformVFXInstanceState::Active;
    return true;
}

bool FGamePlatformVFXInstanceRegistry::Stop(const FGamePlatformVFXHandle& Handle, const bool bStopComponent)
{
    const FGamePlatformVFXInstanceRecord* Existing = Records.Find(Handle.Id);
    if (!Existing || Existing->Handle != Handle)
    {
        return false;
    }

    FGamePlatformVFXInstanceRecord Record = *Existing;
    Records.Remove(Handle.Id);

    if (bStopComponent)
    {
        if (UNiagaraComponent* Component = Record.Component.Get())
        {
            Component->DeactivateImmediate();
            if (!Record.bPooled)
            {
                Component->DestroyComponent();
            }
        }
    }

    for (const FGamePlatformVFXHandle& Child : Record.Children)
    {
        Stop(Child);
    }

    return true;
}

bool FGamePlatformVFXInstanceRegistry::IsActive(const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle)
    {
        return false;
    }

    if (Record->State == EGamePlatformVFXInstanceState::Pending)
    {
        return true;
    }

    if (const UNiagaraComponent* Component = Record->Component.Get())
    {
        if (Component->IsActive())
        {
            return true;
        }
    }

    for (const FGamePlatformVFXHandle& Child : Record->Children)
    {
        if (IsActive(Child))
        {
            return true;
        }
    }

    return false;
}

bool FGamePlatformVFXInstanceRegistry::IsActiveId(const FGuid& Id) const
{
    if (const FGamePlatformVFXInstanceRecord* Record = Records.Find(Id))
    {
        return IsActive(Record->Handle);
    }
    return false;
}

UNiagaraComponent* FGamePlatformVFXInstanceRegistry::GetComponent(const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle ? Record->Component.Get() : nullptr;
}

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::FindByComponent(const UNiagaraComponent* Component) const
{
    if (!IsValid(Component)) return {};
    for (const TPair<FGuid, FGamePlatformVFXInstanceRecord>& Pair : Records)
    {
        if (Pair.Value.Component.Get() == Component) return Pair.Value.Handle;
    }
    return {};
}

TArray<FGamePlatformVFXHandle> FGamePlatformVFXInstanceRegistry::GetChildren(const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle ? Record->Children : TArray<FGamePlatformVFXHandle>();
}

void FGamePlatformVFXInstanceRegistry::Prune()
{
    TArray<FGamePlatformVFXHandle> DeadHandles;
    for (TPair<FGuid, FGamePlatformVFXInstanceRecord>& Pair : Records)
    {
        FGamePlatformVFXInstanceRecord& Record = Pair.Value;
        Record.Children.RemoveAll([this](const FGamePlatformVFXHandle& Child)
        {
            return !IsActive(Child);
        });

        const UNiagaraComponent* Component = Record.Component.Get();
        const bool bComponentActive = IsValid(Component) && Component->IsActive();
        const bool bLifetimeExpired = Record.MaxLifetimeSeconds > 0.0f &&
            (FPlatformTime::Seconds() - Record.StartedAtSeconds) >= Record.MaxLifetimeSeconds;
        if (bLifetimeExpired ||
            (Record.State == EGamePlatformVFXInstanceState::Active &&
             !bComponentActive &&
             Record.Children.Num() == 0))
        {
            DeadHandles.Add(Record.Handle);
        }
    }

    for (const FGamePlatformVFXHandle& Handle : DeadHandles)
    {
        Stop(Handle);
    }
}

void FGamePlatformVFXInstanceRegistry::Reset()
{
    TArray<FGuid> Keys;
    Records.GetKeys(Keys);
    for (const FGuid& Id : Keys)
    {
        if (const FGamePlatformVFXInstanceRecord* Record = Records.Find(Id))
        {
            Stop(Record->Handle);
        }
    }
    Records.Reset();
}
