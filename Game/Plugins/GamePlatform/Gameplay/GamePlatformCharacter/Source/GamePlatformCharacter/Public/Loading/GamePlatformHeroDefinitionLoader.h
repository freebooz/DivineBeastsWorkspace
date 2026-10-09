#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformResult.h"

struct FStreamableHandle;
class UGamePlatformHeroDefinition;
class UGameInstance;

/**
 * FGamePlatformHeroDefinitionLoader（平台英雄定义加载器）。
 * 已发布英雄PrimaryAssetId与UPrimaryDataAsset父类保持兼容。新实例/世界消费者使用Data资源租约。
 * 旧StreamableHandle入口仅供尚未迁移的调用方兼容；不是实例/世界租约，不拥有跨世界释放语义。
 */
class GAMEPLATFORMCHARACTER_API FGamePlatformHeroDefinitionLoader
{
public:
    /**
     * 游戏线程申请英雄普通资源租约，不改主资产身份。成功时指针只在Lease仍Ready且未释放时有效。
     * Completion参数依次为定义、完整租约、真实结果；交给Data的请求即使同步拒绝，也可能延后通知失败。
     * 调用者必须以请求代次/取消状态抑制迟到完成；弱调用者失效时Data抑制通知。
     * 无已初始化Data服务为同步拒绝，OutResult说明失败且不调用Completion；消费者不得认为返回无效即成功。
     */
    static FGamePlatformDataLease AcquireDefinitionResources(UGameInstance& GameInstance,
        const FPrimaryAssetId& PrimaryAssetId, EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        TFunction<void(UGamePlatformHeroDefinition*, const FGamePlatformDataLease&, const FGamePlatformResult&)> Completion,
        FGamePlatformResult& OutResult);
    /** 旧兼容入口：调用方自行保存/取消句柄，可能同步完成；新生产消费者必须使用上面的作用域租约。 */
    static TSharedPtr<FStreamableHandle> RequestDefinition(
        const FPrimaryAssetId& PrimaryAssetId,
        TFunction<void(UGamePlatformHeroDefinition*)> Completion);
};
