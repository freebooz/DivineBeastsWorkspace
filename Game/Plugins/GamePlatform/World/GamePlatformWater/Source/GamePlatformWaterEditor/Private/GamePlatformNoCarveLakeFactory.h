#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "GamePlatformNoCarveLakeFactory.generated.h"

/** 延迟完成湖体构造，避免原生默认值在替换期间触发地形雕刻。 */
UCLASS()
class UGamePlatformNoCarveLakeFactory : public UActorFactory
{
    GENERATED_BODY()
public:
    UGamePlatformNoCarveLakeFactory();
protected:
    virtual AActor* SpawnActor(UObject* Asset, ULevel* Level, const FTransform& Transform,
        const FActorSpawnParameters& Parameters) override;
};
