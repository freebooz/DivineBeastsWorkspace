#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "NiagaraComponent.h"

FGamePlatformVFXInstanceRecord& FGamePlatformVFXInstanceRegistry::Add(const FGamePlatformVFXInstanceRecord& Record)
{
    return Records.Add(Record.Handle.InstanceId, Record);
}

FGamePlatformVFXInstanceRecord* FGamePlatformVFXInstanceRegistry::Find(const FGuid& InstanceId)
{
    return Records.Find(InstanceId);
}

const FGamePlatformVFXInstanceRecord* FGamePlatformVFXInstanceRegistry::Find(const FGuid& InstanceId) const
{
    return Records.Find(InstanceId);
}

FGamePlatformVFXInstanceRecord* FGamePlatformVFXInstanceRegistry::FindByComponent(const UNiagaraComponent* Component)
{
    for (TPair<FGuid, FGamePlatformVFXInstanceRecord>& Pair : Records)
    {
        if (Pair.Value.NiagaraComponent.Get() == Component)
        {
            return &Pair.Value;
        }
    }
    return nullptr;
}

bool FGamePlatformVFXInstanceRegistry::Remove(const FGuid& InstanceId)
{
    return Records.Remove(InstanceId) > 0;
}

void FGamePlatformVFXInstanceRegistry::GetAllIds(TArray<FGuid>& OutIds) const
{
    Records.GetKeys(OutIds);
}
