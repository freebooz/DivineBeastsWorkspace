#include "Tests/GamePlatformVFXMotionTestSource.h"
#include "Components/SceneComponent.h"
AGamePlatformVFXMotionTestSource::AGamePlatformVFXMotionTestSource()
{SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MotionTestRoot")));}
FGamePlatformMotionSnapshot AGamePlatformVFXMotionTestSource::GetMotionSnapshot_Implementation() const
{if(OnQuery)OnQuery();return Sample;}
