#include "Loading/GamePlatformHeroDefinitionLoader.h"

#include "Definitions/GamePlatformHeroDefinition.h"
#include "Engine/AssetManager.h"
#include "Loading/GamePlatformAssetLoader.h"

TSharedPtr<FStreamableHandle> FGamePlatformHeroDefinitionLoader::RequestDefinition(
    const FPrimaryAssetId& PrimaryAssetId,
    TFunction<void(UGamePlatformHeroDefinition*)> Completion)
{
    if (!PrimaryAssetId.IsValid())
    {
        if (Completion)
        {
            Completion(nullptr);
        }
        return nullptr;
    }

    const FSoftObjectPath AssetPath =
        UAssetManager::Get().GetPrimaryAssetPath(PrimaryAssetId);
    if (!AssetPath.IsValid())
    {
        if (Completion)
        {
            Completion(nullptr);
        }
        return nullptr;
    }

    if (UGamePlatformHeroDefinition* Existing =
        Cast<UGamePlatformHeroDefinition>(AssetPath.ResolveObject()))
    {
        if (Completion)
        {
            Completion(Existing);
        }
        return nullptr;
    }

    TArray<FSoftObjectPath> Assets;
    Assets.Add(AssetPath);
    return FGamePlatformAssetLoader::RequestAsyncLoad(
        Assets,
        FStreamableDelegate::CreateLambda(
            [AssetPath, Completion = MoveTemp(Completion)]() mutable
            {
                if (Completion)
                {
                    Completion(
                        Cast<UGamePlatformHeroDefinition>(
                            AssetPath.ResolveObject()));
                }
            }));
}
