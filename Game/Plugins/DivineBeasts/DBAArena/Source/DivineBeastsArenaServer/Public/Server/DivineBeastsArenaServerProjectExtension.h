#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaPolicies.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"
#include "Types/GamePlatformDataLease.h"

class UGameInstance;
class UWorld;

/**
 * FDivineBeastsArenaServerProjectExtension（神兽联盟竞技服务器项目扩展）。
 * 负责Production项目配置门禁与Hero资格；不重写平台GameMode生命周期。
 */
class DIVINEBEASTSARENASERVER_API FDivineBeastsArenaServerProjectExtension final
    : public IGamePlatformArenaServerProjectExtension
    , public IGamePlatformArenaHeroEligibilityProvider
{
public:
    /** 注册世界清理监听，不启动地图、连接或资源加载。 */
    FDivineBeastsArenaServerProjectExtension();
    /** 游戏线程销毁组合根，显式撤销自己持有的当前世界资源需求。 */
    virtual ~FDivineBeastsArenaServerProjectExtension() override;
    virtual bool ResolveModeSpec(
        const FGamePlatformArenaAssignment& Assignment,
        FGamePlatformArenaModeSpec& OutModeSpec,
        FString& OutError) const override;

    virtual bool ValidateAndConfigureGameMode(
        AGamePlatformArenaGameMode& GameMode,
        const FGamePlatformArenaAssignment& Assignment,
        const FGamePlatformArenaModeSpec& ModeSpec,
        FString& OutError) override;

    virtual bool IsHeroEligible(
        const FString& PlayerId,
        const FString& HeroDefinitionId,
        FName ArenaModeId,
        FString& OutReason) const override;
private:
    /** 模块Provider仅作为路由；每个真实GameMode拥有独立适配器、实例和预热需求。 */
    struct FWorldAssembly
    {
        TSharedPtr<IGamePlatformArenaGameplayLifecycleAdapter> GameplayLifecycleAdapter;
        TArray<FGamePlatformDataLease> HeroDefinitionWarmupLeases;
        TWeakObjectPtr<UGameInstance> Instance;
    };
    TMap<TWeakObjectPtr<AGamePlatformArenaGameMode>, FWorldAssembly> WorldAssemblies;
    FDelegateHandle WorldCleanupHandle;
    /** 只撤销指定世界的所有权；先从路由移除，再清GameMode借用指针和租约，允许重复调用。 */
    void ReleaseWorldAssembly(TWeakObjectPtr<AGamePlatformArenaGameMode> Mode);
    /** 世界退出及时回收桶，防止模块存活期间积累失效世界。 */
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    friend class FDivineBeastsArenaWorldOwnershipTest;
};
