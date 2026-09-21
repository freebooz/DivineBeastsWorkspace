#pragma once

#include "CoreMinimal.h"
#include "Instances/GamePlatformVFXInstanceRecord.h"

class UNiagaraComponent;

class FGamePlatformVFXInstanceRegistry
{
public:
    FGamePlatformVFXInstanceRecord& Add(const FGamePlatformVFXInstanceRecord& Record);
    FGamePlatformVFXInstanceRecord* Find(const FGuid& InstanceId);
    const FGamePlatformVFXInstanceRecord* Find(const FGuid& InstanceId) const;
    FGamePlatformVFXInstanceRecord* FindByComponent(const UNiagaraComponent* Component);
    bool Remove(const FGuid& InstanceId);
    int32 Num() const { return Records.Num(); }
    void GetAllIds(TArray<FGuid>& OutIds) const;

private:
    TMap<FGuid, FGamePlatformVFXInstanceRecord> Records;
};
