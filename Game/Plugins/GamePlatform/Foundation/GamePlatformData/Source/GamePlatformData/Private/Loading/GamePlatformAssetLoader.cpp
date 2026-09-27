#include "Loading/GamePlatformAssetLoader.h"

TSharedPtr<FStreamableHandle> FGamePlatformAssetLoader::RequestAsyncLoad(
    const TArray<FSoftObjectPath>& Assets,
    FStreamableDelegate Completion)
{
    if (Assets.IsEmpty())
    {
        return nullptr;
    }

    return UAssetManager::GetStreamableManager().RequestAsyncLoad(
        Assets,
        MoveTemp(Completion));
}

void FGamePlatformAssetLoader::Cancel(
    const TSharedPtr<FStreamableHandle>& Handle)
{
    if (Handle.IsValid())
    {
        Handle->CancelHandle();
    }
}
