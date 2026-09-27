#include "Initialization/DivineBeastsCharacterSpawnInitializer.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "GameFramework/Character.h"

bool FDivineBeastsCharacterSpawnInitializer::InitializeCharacter(
    ACharacter& Character,
    const FGamePlatformCharacterInitializationContext& Context,
    FString& OutError)
{
    if (!Character.HasAuthority())
    {
        OutError = TEXT("角色出生身份只能由服务器可信Spawn路径初始化。");
        return false;
    }
    if (!Context.IsValid(OutError))
    {
        return false;
    }

    UDivineBeastsCharacterComponent* Component =
        Character.FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!Component)
    {
        Component = NewObject<UDivineBeastsCharacterComponent>(
            &Character,
            TEXT("DivineBeastsCharacterComponent"));
        if (!Component)
        {
            OutError = TEXT("创建DivineBeastsCharacterComponent失败。");
            return false;
        }
        Character.AddInstanceComponent(Component);
        Component->RegisterComponent();
    }

    return Component->AuthorityBindTrustedContext(Context, OutError);
}

bool FDivineBeastsCharacterSpawnInitializer::TryGetLoadedSpawnEnvelope(
    FName HeroDefinitionId,
    FGamePlatformCharacterSpawnEnvelope& OutEnvelope)
{
    const FSoftObjectPath Path =
        FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroDefinitionId);
    const UDivineBeastsHeroDefinition* Definition =
        Path.IsValid()
            ? Cast<UDivineBeastsHeroDefinition>(Path.ResolveObject())
            : nullptr;
    if (!Definition)
    {
        return false;
    }

    FString Error;
    if (!Definition->SpawnEnvelope.IsValid(Error))
    {
        return false;
    }

    OutEnvelope = Definition->SpawnEnvelope;
    return true;
}
