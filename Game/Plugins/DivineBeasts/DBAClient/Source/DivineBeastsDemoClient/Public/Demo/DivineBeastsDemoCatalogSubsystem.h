// 项目层演示目录/会话：基于实际插件描述及UE资产注册表生成只读快照。
// 无每帧轮询、无生产账号/权威写入，资源使用仍由项目既有数据服务管理。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "Contracts/DivineBeastsDemoCatalogTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "API/IDivineBeastsDemoScenarioAdapter.h"
#include "DivineBeastsDemoCatalogSubsystem.generated.h"

/** 插件架构层级；内容包仅属于项目内容的资产展示，不增加第四业务层。 */
UENUM(BlueprintType)
enum class EDivineBeastsDemoLayer : uint8
{
    Platform UMETA(DisplayName="平台层"),
    Moba UMETA(DisplayName="MOBA通用层"),
    Project UMETA(DisplayName="SSLM项目层"),
    Content UMETA(DisplayName="项目内容包")
};

/** 运行时发现的真实插件描述投影，不等于功能已经验收。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoPlugin
{
    GENERATED_BODY()
    /** 真实.uplugin名称，稳定身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName PluginId;
    /** 插件描述文件中的友好名称。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText DisplayName;
    /** 插件描述文件中的职责说明。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FString Description;
    /** 插件在三层架构中的归属。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") EDivineBeastsDemoLayer Layer = EDivineBeastsDemoLayer::Platform;
    /** 当前开发Target是否实际启用此插件。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bEnabled = false;
    /** 当前真实资产注册表中关联的审核/诊断地图数；不代表功能总数。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 DiscoveredMapCount = 0;
    /** 作者范围审计状态；未加载完整定义时unreviewed，不表示功能已经实现。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName FeatureAuditStatus = TEXT("unreviewed");
    /** 来源独立于三层归属；第三方Editor工具不会成为第四架构层。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName SourceKind = TEXT("first_party");
};

/** 从实际开发地图发现的演示候选，未完成视觉验收不能标记“通过”。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoScenario
{
    GENERATED_BODY()
    /** 正式功能案例使用稳定ScenarioId（案例标识）；未编目诊断地图按真实包路径登记且不计正式覆盖。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName ScenarioId;
    /** PluginId（插件身份）明确关联真实插件；诊断地图仅分组，不宣称业务功能所有权。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName PluginId;
    /** UI显示名称，可进一步由真实Definition覆盖。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText Title;
    /** UE AssetRegistry登记的真实地图软引用。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FSoftObjectPath MapAsset;
    /** 仅诊断/材质隔离的地图不得冒充正式功能演示。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bDiagnosticOnly = false;
    /** 当前Target已启用关联插件，且发现资产；不代表运行或人工验收通过。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bAvailable = false;
    /** 未验证状态给人工审核明确提示。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText VerificationStatus;
};

/** 游戏线程目录/预检状态变化；订阅者在失活时解绑，不轮询资产或登录状态。 */
DECLARE_MULTICAST_DELEGATE(FDivineBeastsDemoCatalogChanged);
struct FDivineBeastsDemoCatalogPrivateState;

/**
 * UDivineBeastsDemoCatalogSubsystem（项目开发演示目录）。
 * 仅在明确开发启动参数生效时创建，不配置认证服务；列表是UE实际插件/关卡发现结果。
 * 不持有世界Actor或业务权威，开放地图仅按经登记的ScenarioId操作。
 */
UCLASS()
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoCatalogSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    friend class FDivineBeastsIndependentPluginPageTest;
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 从真实插件管理器与资产注册表重新获取目录；仅在人工刷新时执行。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    void RefreshCatalog();

    /** 按架构分类的全部已发现项目插件，包括已禁用插件及内容包。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoPlugin> GetPlugins() const { return Plugins; }

    /** 按插件ID获取其真实地图候选；空表表示还没有可核查地图。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoScenario> GetScenarios(FName PluginId) const;

    /** 获取插件全部已审计功能，包括禁用/阻塞项；空集合表示范围仍待审计。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoFeatureEntry> GetFeatures(FName PluginId) const;
    /** 获取主/关联此功能的独立案例，不按地图或中文名称去重。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoScenarioDefinition> GetFeatureScenarios(FName FeatureId) const;
    /** 游戏线程预检实际目标、插件、资产和适配器；不生成对象或发送业务请求。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FDivineBeastsDemoPreflightResult PreflightScenario(FName ScenarioId) const;
    /** 目录发布或失败诊断的真实结果；未执行保持NotExecuted。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FGamePlatformResult GetCatalogResult() const;
    /** C++稳定案例读取；输出值副本，刷新后仍可安全用于本次运行的合同。 */
    bool FindScenarioDefinition(FName ScenarioId, FDivineBeastsDemoScenarioDefinition& OutScenario) const;
    /** 完整检查用例合同值副本，运行证据和签收不在此处修改。 */
    TArray<FDivineBeastsDemoReviewCaseDefinition> GetReviewCases() const;
    /** 当前完整目录作者源指纹，不冒充实际加载模块指纹。 */
    FString GetSourceFingerprint() const;
    /** 完整已发布目录中的独立页软类路径；仅查询，不执行同步加载；禁用插件仍可查看。 */
    bool FindPluginPageAsset(FName PluginId, FSoftObjectPath& OutPageAsset) const;
    /** 同一受控路由服务于中心按钮和DBAPluginTest；先结束当前运行，清理失败拒绝切页。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool OpenPluginPage(FName PluginId);
    /** 旅行后仍沿用请求页的固定上下文；None表示中心，不拥有世界运行或页面租约。 */
    FName GetPluginPageContext() const { return PluginPageContext; }
    /** 事件所有者为本游戏实例；调用者持委托句柄并在失活/退出解绑。 */
    FDivineBeastsDemoCatalogChanged OnCatalogChanged;
    /** 游戏线程开发组合根注册真实工厂；重复/关闭返回无效完整句柄。 */
    FDivineBeastsDemoAdapterRegistrationHandle RegisterScenarioAdapter(FName AdapterId, FDivineBeastsDemoAdapterFactory Factory);
    /** 完整实例句柄撤销；活跃关联案例先停止，新预检立即失效。 */
    FGamePlatformResult UnregisterScenarioAdapter(FDivineBeastsDemoAdapterRegistrationHandle Handle);
    /** 为本实例所属世界创建独立适配器；未注册/错世界返回空。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateScenarioAdapter(FName AdapterId, UWorld& World) const;
    /** 目标世界和UI资源就绪事件调用；仅消费本目录明确保存的待进入身份。 */
    void StartPendingScenario(UWorld& World);
    /** 活跃世界清理终态通知；只有清理成功才执行已受理的返程/切换请求。 */
    void OnScenarioStopped(UWorld& World, const FDivineBeastsDemoRunSnapshot& Snapshot);
    /** 读取待进入案例ID，供目标世界判断SampleWidget合同，未请求为空。 */
    FName GetPendingScenarioId() const;

    /**
     * 根据已登记场景请求切图。返回true只代表引擎已接受OpenLevel请求，
     * 并非地图装载、资源加载、功能运行或人工验收已成功。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool OpenScenario(FName ScenarioId);

    /** 返回真实开发总览地图，地图未生成时明确拒绝。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool ReturnToHub();

    /** 最新同步操作诊断，供演示页面显示，无敏感登录信息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FText GetLastOperationMessage() const { return LastOperationMessage; }

private:
    /** 清理等待中的页面意图；None在bPendingPluginPage为true时表示中心。 */
    FName PendingPluginPageId;
    FName PluginPageContext;
    bool bPendingPluginPage = false;
    bool bDirectPluginRouteConsumed = false;
    /** 私有快照、Data完整租约和请求代次；无进程全局缓存。 */
    TSharedPtr<FDivineBeastsDemoCatalogPrivateState> PrivateState;
    /** 在完整Data回调中验证候选后发布；旧代次只撤销自己的租约。 */
    void OnCatalogDefinitionReady(uint64 Generation, const FGamePlatformDataLease& Lease,
        const FGamePlatformResult& Result);
    /** 只刷新兼容投影，不猜正式归属；查询正式预检结果生成可用状态。 */
    void RebuildScenarioProjection();
    /** 在有效当前世界执行待导航；不把OpenLevel受理当成案例运行完成。 */
    bool ContinuePendingNavigation();
    UPROPERTY(Transient) TArray<FDivineBeastsDemoPlugin> Plugins;
    UPROPERTY(Transient) TArray<FDivineBeastsDemoScenario> Scenarios;
    UPROPERTY(Transient) FText LastOperationMessage;
};
