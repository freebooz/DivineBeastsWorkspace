#include "Definitions/GamePlatformVFXCompositeDefinition.h"

UGamePlatformVFXCompositeDefinition::UGamePlatformVFXCompositeDefinition()
{
    Behavior = EGamePlatformVFXBehavior::Composite;
    bAllowPooling = false;
}
