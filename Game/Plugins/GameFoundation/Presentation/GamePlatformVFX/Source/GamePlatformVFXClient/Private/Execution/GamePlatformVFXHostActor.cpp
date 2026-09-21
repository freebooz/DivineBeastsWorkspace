#include "Execution/GamePlatformVFXHostActor.h"

AGamePlatformVFXHostActor::AGamePlatformVFXHostActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);
}
