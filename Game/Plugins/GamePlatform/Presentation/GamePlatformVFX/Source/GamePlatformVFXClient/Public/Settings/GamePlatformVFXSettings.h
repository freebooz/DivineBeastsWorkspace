// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Settings/GamePlatformVFXWorldBudgetRule.h"
#include "GamePlatformVFXSettings.generated.h"

class UGamePlatformVFXCatalog;

/** GamePlatformVFX 可配置运行策略。 */
/** 独立可复用插件配置使用GamePlatformVFX类别；SSLM竞技场覆盖保留在项目Config目录。 */
UCLASS(Config=GamePlatformVFX, DefaultConfig, meta=(DisplayName="Game Platform VFX"))
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Combat（战斗）及以上普通软预算；Critical（关键）仅受HardMaxTrackedInstances限制。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxActiveInstances = 256;

    /**
     * Status（状态）及Ambient（环境）的累计软上限。
     * 低优先级请求只能使用预算前段，为Combat预留后段容量。
     */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxStatusInstances = 192;

    /** Ambient（环境）累计软上限；防止开放世界装饰效果抢占战斗预算。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 MaxAmbientInstances = 96;

    /** 绝对安全上限；Critical请求也不得突破。Pending与Active统一计入。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1"))
    int32 HardMaxTrackedInstances = 512;

    /** 同时等待共享Definition完成的VFX实例上限。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxPendingInstancePreloads = 64;

    /** World级共享Definition缓存上限；满载时只淘汰无实例、无Preload Pin的最久未使用项。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16", ClampMax="4096"))
    int32 MaxCachedDefinitions = 512;

    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="16", ClampMax="4096"))
    int32 MaxDedupeEntries = 512;
    /** 定义回退链结构安全深度；不是CPU/GPU预算。每次请求另以Visited身份集拒绝环。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxDefinitionFallbackDepth = 8;

    UPROPERTY(Config, EditAnywhere, Category="Catalog", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxRegisteredCatalogs = 64;

    UPROPERTY(Config, EditAnywhere, Category="Catalog", meta=(ClampMin="1", ClampMax="128"))
    int32 MaxStartupCatalogs = 32;

    /** 按WorldId前缀/效果画质启用可选场景级请求软预算；默认关闭，不影响其他世界。 */
    UPROPERTY(Config, EditAnywhere, Category="Scalability")
    bool bUseWorldBudgetRules = false;

    /** 场景级预算仅限制请求跟踪数量，Niagara System上限由Effect Type单独管理。 */
    UPROPERTY(Config, EditAnywhere, Category="Scalability")
    TArray<FGamePlatformVFXWorldBudgetRule> WorldBudgetRules;

    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bEnablePooling = true;

    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bEnableDiagnostics = true;

    UPROPERTY(Config, EditAnywhere, Category="Catalog")
    TArray<TSoftObjectPtr<UGamePlatformVFXCatalog>> StartupCatalogs;
};
