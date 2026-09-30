#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraComponent.h"

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::Reserve(UWorld* World)
{
    FGamePlatformVFXHandle Handle;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = 1;
    Handle.World = World;

    FGamePlatformVFXInstanceRecord& Record = Records.Add(Handle.Id);
    Record.Handle = Handle;
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
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AttachComponent(
    const FGamePlatformVFXHandle& Handle,
    UNiagaraComponent* Component,
    bool bPooled)
{
    FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    if (!Record || Record->Handle != Handle || !IsValid(Component))
    {
        return false;
    }

    if (UNiagaraComponent* Existing = Record->Component.Get())
    {
        ComponentHandles.Remove(Existing);
    }

    Record->Component = Component;
    Record->bPooled = bPooled;
    Record->State = EGamePlatformVFXInstanceState::Active;
    ComponentHandles.Add(Component, Handle);
    return true;
}

bool FGamePlatformVFXInstanceRegistry::AddChild(
    const FGamePlatformVFXHandle& Parent,
    const FGamePlatformVFXHandle& Child)
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

bool FGamePlatformVFXInstanceRegistry::Stop(
    const FGamePlatformVFXHandle& Handle,
    const bool bStopComponent,
    const bool bStopChildren)
{
    const FGamePlatformVFXInstanceRecord* Existing = Records.Find(Handle.Id);
    if (!Existing || Existing->Handle != Handle)
    {
        return false;
    }

    FGamePlatformVFXInstanceRecord Record = *Existing;
    Records.Remove(Handle.Id);

    if (UNiagaraComponent* Component = Record.Component.Get())
    {
        ComponentHandles.Remove(Component);
        if (bStopComponent)
        {
            Component->DeactivateImmediate();
            if (!Record.bPooled)
            {
                Component->DestroyComponent();
            }
        }
    }

    if (bStopChildren)
    {
        for (const FGamePlatformVFXHandle& Child : Record.Children)
        {
            Stop(Child);
        }
    }

    return true;
}

bool FGamePlatformVFXInstanceRegistry::IsActive(
    const FGamePlatformVFXHandle& Handle) const
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
        return Component->IsActive();
    }

    // 只有Composite父实例允许“Active但无Niagara Component”；普通实例若组件已失效应立即视为不活动。
    if (Record->State == EGamePlatformVFXInstanceState::Active)
    {
        if (const UGamePlatformVFXDefinition* Definition = Record->Definition.Get())
        {
            return Definition->GetBehavior() == EGamePlatformVFXBehavior::Composite;
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

UNiagaraComponent* FGamePlatformVFXInstanceRegistry::GetComponent(
    const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle ? Record->Component.Get() : nullptr;
}

FGamePlatformVFXHandle FGamePlatformVFXInstanceRegistry::FindByComponent(
    const UNiagaraComponent* Component) const
{
    if (!IsValid(Component))
    {
        return {};
    }

    if (const FGamePlatformVFXHandle* Handle = ComponentHandles.Find(Component))
    {
        return *Handle;
    }
    return {};
}

TArray<FGamePlatformVFXHandle> FGamePlatformVFXInstanceRegistry::GetChildren(
    const FGamePlatformVFXHandle& Handle) const
{
    const FGamePlatformVFXInstanceRecord* Record = Records.Find(Handle.Id);
    return Record && Record->Handle == Handle
        ? Record->Children
        : TArray<FGamePlatformVFXHandle>();
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
    ComponentHandles.Reset();
    Records.Reset();
}
