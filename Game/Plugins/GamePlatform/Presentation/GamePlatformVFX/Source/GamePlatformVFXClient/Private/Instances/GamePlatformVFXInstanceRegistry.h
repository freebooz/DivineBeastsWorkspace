#pragma once

#include "Instances/GamePlatformVFXInstanceRecord.h"

class UNiagaraComponent;
class UGamePlatformVFXDefinition;
class UWorld;

/** Handle -> 活动实例的唯一运行时索引。 */
class FGamePlatformVFXInstanceRegistry
{
public:
    FGamePlatformVFXHandle Reserve(UWorld* World = nullptr, const FGamePlatformVFXRequest* Request = nullptr);
    bool SetDefinition(const FGamePlatformVFXHandle& Handle, UGamePlatformVFXDefinition* Definition);
    bool SetLoadLease(const FGamePlatformVFXHandle& Handle, const FGamePlatformVFXPreloadHandle& Lease);
    bool AttachComponent(const FGamePlatformVFXHandle& Handle, UNiagaraComponent* Component, bool bPooled);
    bool AddChild(const FGamePlatformVFXHandle& Parent, const FGamePlatformVFXHandle& Child);
    bool Stop(const FGamePlatformVFXHandle& Handle, bool bStopComponent = true);
    bool IsActive(const FGamePlatformVFXHandle& Handle) const;
    bool IsActiveId(const FGuid& Id) const;
    UNiagaraComponent* GetComponent(const FGamePlatformVFXHandle& Handle) const;
    FGamePlatformVFXHandle FindByComponent(const UNiagaraComponent* Component) const;
    TArray<FGamePlatformVFXHandle> GetChildren(const FGamePlatformVFXHandle& Handle) const;
    void Prune();
    void Reset();
    int32 Num() const { return Records.Num(); }

private:
    TMap<FGuid, FGamePlatformVFXInstanceRecord> Records;
};
