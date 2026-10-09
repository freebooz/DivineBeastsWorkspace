#include "Loading/GamePlatformHeroDefinitionLoader.h"

#include "Definitions/GamePlatformHeroDefinition.h"
#include "Engine/AssetManager.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"

FGamePlatformDataLease FGamePlatformHeroDefinitionLoader::AcquireDefinitionResources(UGameInstance& GameInstance,
    const FPrimaryAssetId& PrimaryAssetId, EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
    TFunction<void(UGamePlatformHeroDefinition*, const FGamePlatformDataLease&, const FGamePlatformResult&)> Completion,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    auto* Data = IGamePlatformDataService::Get(GameInstance);
    if (!Data || !Completion)
    { OutResult = FGamePlatformResult::Failure(TEXT("HeroDataServiceUnavailable"), TEXT("英雄资源租约需要已初始化Data服务和完成回调。")); return {}; }
    // 这里只解析稳定主资产身份到软路径；加载、引用计数、取消和跨实例资源所有权全部交给Data。
    const FSoftObjectPath AssetPath = PrimaryAssetId.IsValid() ? UAssetManager::Get().GetPrimaryAssetPath(PrimaryAssetId) : FSoftObjectPath();
    return Data->AcquireResources({AssetPath}, Lifetime, WeakCaller,
        [AssetPath, WeakInstance = TWeakObjectPtr<UGameInstance>(&GameInstance), Completion = MoveTemp(Completion)](const auto& Lease, const auto& Result) mutable
        {
            auto* Service = WeakInstance.IsValid() ? IGamePlatformDataService::Get(*WeakInstance.Get()) : nullptr;
            const bool bReadable = Result.IsSuccess() && Service &&
                Service->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded;
            auto* Definition = bReadable ? Cast<UGamePlatformHeroDefinition>(AssetPath.ResolveObject()) : nullptr;
            const auto TypedResult = Result.IsSuccess() && !Definition
                ? FGamePlatformResult::Failure(TEXT("HeroDefinitionClassMismatch"), TEXT("英雄主资产的真实类型不兼容平台英雄定义。")) : Result;
            if (Result.IsSuccess() && !Definition && WeakInstance.IsValid())
            { if (Service) { Service->ReleaseResources(Lease); } }
            Completion(Definition, Lease, TypedResult);
        }, OutResult);
}

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
