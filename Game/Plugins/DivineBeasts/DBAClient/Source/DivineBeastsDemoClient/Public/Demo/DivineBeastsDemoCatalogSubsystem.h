// 项目层演示目录/会话：基于实际插件描述及UE资产注册表生成只读快照。
// 无每帧轮询、无生产账号/权威写入，资源使用仍由项目既有数据服务管理。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "DivineBeastsDemoCatalogSubsystem.generated.h"

/** 插件架构层级；内容包仅属于项目内容的资产展示，不增加第四业务层。 */
UENUM(BlueprintType)
enum class EDivineBeastsDemoLayer : uint8
{
    Platform UMETA(DisplayName="平台层"),
    Moba UMETA(DisplayName="MOBA通用层"),
    Project UMETA(DisplayName="神兽联盟项目层"),
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
};

/** 从实际开发地图发现的演示候选，未完成视觉验收不能标记“通过”。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoScenario
{
    GENERATED_BODY()
    /** 地图长包路径同时充当候选唯一身份，避免相同文件名冲突。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName ScenarioId;
    /** 关联的真实插件，未归属地图挂载在开发工具项下。 */
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

/**
 * UDivineBeastsDemoCatalogSubsystem（项目开发演示目录）。
 * 仅在明确开发启动参数生效时创建，不配置认证服务；列表是UE实际插件/关卡发现结果。
 * 不持有世界Actor或业务权威，开放地图仅按经登记的ScenarioId操作。
 */
UCLASS()
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoCatalogSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
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
    UPROPERTY(Transient) TArray<FDivineBeastsDemoPlugin> Plugins;
    UPROPERTY(Transient) TArray<FDivineBeastsDemoScenario> Scenarios;
    UPROPERTY(Transient) FText LastOperationMessage;
};
