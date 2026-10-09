#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformCombatEvent.h"
#include "DivineBeastsArenaCombatFeedbackClientSubsystem.generated.h"

class UDivineBeastsCombatFeedbackCatalog;
class UGamePlatformHitFeedbackProfile;
class UMobaPresentationClientSubsystem;
class UWorld;
class IGamePlatformDataService;
struct FMobaResolvedHitFeedbackConfiguration;

/**
 * 神兽联盟竞技打击反馈客户端组合根。
 *
 * 职责：本地玩家独立持有Catalog/Profile的统一数据租约；按权威角色及技能ID解析已加载Profile，
 * 注入MobaPresentation提供的中立Resolver，不复制Niagara/SFX播放器或Gameplay裁决。
 * 端侧：ClientOnly；生命周期：当前World；没有真实目录资产时不订阅资源或伪造默认ID。
 */
UCLASS()
class UDivineBeastsArenaCombatFeedbackClientSubsystem final : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    /** 绑定当前世界；请求下一调度轮完成，不在首次命中同步加载。 */
    void BeginForWorld(UWorld* World);

    /** 切图、账号退出、世界清理时精确撤销本子系统自己的所有租约。 */
    void CancelWorldLeases();

    /** Moba每次命中时调用；只查询或发出异步预取，绝不访问项目客户端之外的权威状态。 */
    bool ResolveLoadedHitFeedback(
        const FGamePlatformCombatEvent& Event,
        FMobaResolvedHitFeedbackConfiguration& OutConfig);

    void BeginProfileLoad(const FPrimaryAssetId& ProfileDefinitionId);
    void HandlePostLoadMap(UWorld* World);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    IGamePlatformDataService* GetDataService() const;

    TWeakObjectPtr<UWorld> BoundWorld;
    TWeakObjectPtr<UMobaPresentationClientSubsystem> MobaPresentation;
    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle PostLoadMapHandle;

    /** 请求代次用于取消和跨世界异步回调过滤；递增后旧回调不可修改现存记录。 */
    /** 无资产配置/加载失败时，同一World不因多次命中反复重试Catalog IO。 */
    bool bCatalogLoadAttemptedForWorld = false;
    int32 WorldRequestGeneration = 0;
    FGamePlatformDataLease CatalogLease;
    TMap<FPrimaryAssetId, FGamePlatformDataLease> ProfileLeases;
    TSet<FPrimaryAssetId> FailedProfiles;    static constexpr int32 MaxActiveProfileLeases = 64;
};
