#include "Framework/GamePlatformGameStateBase.h"

#include "Components/GamePlatformExperienceComponent.h"

AGamePlatformGameStateBase::AGamePlatformGameStateBase()
{
    bReplicates = true;
    Experience = CreateDefaultSubobject<UGamePlatformExperienceComponent>(TEXT("Experience"));
}
