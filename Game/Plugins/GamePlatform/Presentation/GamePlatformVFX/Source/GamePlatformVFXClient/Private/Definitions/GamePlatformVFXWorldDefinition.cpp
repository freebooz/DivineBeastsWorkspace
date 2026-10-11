// 既有WorldDefinition的组合配置，定义资源仍由GamePlatformData提供租约。
#include "Definitions/GamePlatformVFXWorldDefinition.h"
UGamePlatformVFXWorldDefinition::UGamePlatformVFXWorldDefinition() { Behavior=EGamePlatformVFXBehavior::World; }
FGamePlatformResult UGamePlatformVFXWorldDefinition::ValidateDefinition() const
{
    const auto Base=Super::ValidateDefinition();
    if (!Base.IsSuccess() || !bEnableFauna) return Base;
    FString Error;
    return Fauna.Validate(Error) ? FGamePlatformResult::Success() :
        FGamePlatformResult::Failure(TEXT("VFX.FaunaConfigurationInvalid"),Error);
}
