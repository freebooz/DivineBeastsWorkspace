#include "Initialization/GamePlatformCharacterInitializationExecutor.h"

#include "Features/IModularFeatures.h"
#include "GameFramework/Character.h"

IGamePlatformCharacterInitializer*
FGamePlatformCharacterInitializationExecutor::ResolveUniqueInitializer(FString& OutError)
{
    check(IsInGameThread());

    const TArray<IGamePlatformCharacterInitializer*> Initializers =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<IGamePlatformCharacterInitializer>(
                IGamePlatformCharacterInitializer::GetModularFeatureName());

    if (Initializers.Num() != 1 || Initializers[0] == nullptr)
    {
        OutError = FString::Printf(
            TEXT("平台角色初始化器必须唯一配置，当前数量=%d。"),
            Initializers.Num());
        return nullptr;
    }

    OutError.Reset();
    return Initializers[0];
}

bool FGamePlatformCharacterInitializationExecutor::InitializeCharacter(
    ACharacter& Character,
    const FGamePlatformCharacterInitializationContext& Context,
    FString& OutError)
{
    check(IsInGameThread());

    if (!Character.HasAuthority())
    {
        OutError = TEXT("平台角色初始化只能在服务器权威 ACharacter 上执行。");
        return false;
    }

    if (!Context.IsValid(OutError))
    {
        return false;
    }

    IGamePlatformCharacterInitializer* Initializer =
        ResolveUniqueInitializer(OutError);
    if (!Initializer)
    {
        return false;
    }

    return Initializer->InitializeCharacter(Character, Context, OutError);
}
