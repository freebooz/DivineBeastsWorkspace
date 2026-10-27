// SSLM演示菜单投影；项目开发UI只读消费，筛选不能改变编目总数或人工结果。
#pragma once
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#include "DivineBeastsDemoMenuTypes.generated.h"

/** 独立筛选值；None/空文字表示不限，不隐藏总量/待审计分母。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoFilter
{
    GENERATED_BODY()
    /** 中文名称、稳定ID或目的的大小写不敏感搜索。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FString SearchText;
    /** Platform/Moba/Project或None；内容包属于Project。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName Layer;
    /** code/content_pack或None。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName PluginKind;
    /** unreviewed/unimplemented/partial/implemented或None。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName ImplementationStatus;
    /** unregistered/registered/blocked/wired/launchable或None。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName IntegrationStatus;
    /** local/fixture/network/editor或None。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName RunMode;
    /** false不按运行状态筛选，true采用RunState。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") bool bFilterRunState = false;
    /** 只过滤当前世界最近真实运行；未运行功能为Idle。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") EDivineBeastsDemoRunState RunState = EDivineBeastsDemoRunState::Idle;
    /** 人工报告状态筛选或None；没有当前报告只能not_reviewed。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Demo") FName ReviewStatus;
};

/** 单功能四维状态投影；定义实现/接入、当前运行和人工记录相互独立。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoFeatureMenuEntry
{
    GENERATED_BODY()
    /** 经目录完整发布的功能值。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoFeatureEntry Feature;
    /** 当前世界真实运行状态，未运行为Idle。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") EDivineBeastsDemoRunState RunState = EDivineBeastsDemoRunState::Idle;
    /** 当前版本人工报告状态，未读取到有效签收为not_reviewed。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName ReviewStatus = TEXT("not_reviewed");
};

/** 案例详情与实际预检；禁用项保留具体原因，不能只隐藏按钮。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoScenarioMenuEntry
{
    GENERATED_BODY()
    /** 完整发布的独立案例值副本，刷新不改变活动运行合同。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoScenarioDefinition Scenario;
    /** 当前目标/真实插件/资源/适配注册的预检结果；不是执行成功。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoPreflightResult Preflight;
};

/** 事件驱动菜单快照；用户命令只有受理结果，真实执行由RunSnapshot更新。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoMenuSnapshot
{
    GENERATED_BODY()
    /** None表示中心；固定插件页面不因搜索、筛选或迟到通知改变此身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName FixedPluginId;
    /** 当前选中插件的独立页面已被完整目录登记；不代表资源已加载或能力可执行。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bCanOpenPluginPage = false;
    /** 筛选后可见插件/功能/案例；总数仍单独保存。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoPlugin> Plugins;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoFeatureMenuEntry> Features;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoScenarioMenuEntry> Scenarios;
    /** 选中身份不依赖中文文本；优先保持ID，不存在时只显示首项或空，不自动启动案例。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName SelectedPluginId;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName SelectedFeatureId;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName SelectedScenarioId;
    /** 完整编目分母，不受筛选影响。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 TotalPlugins = 0;
    /** 功能范围尚未完整审计的插件数，包括unreviewed/partial，不受搜索影响。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 UnreviewedPlugins = 0;
    /** 实际登记的功能数，不代表全项目功能分母。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 KnownFeatures = 0;
    /** wired/launchable的源码接线数；运行预检可因真实资源缺失阻塞。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 WiredFeatures = 0;
    /** 当前版本运行统计应来自独立有效报告；当前消费者尚未接历史报告，保持0，不冒充已验证。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 RuntimeVerifiedFeatures = 0;
    /** 当前未导入有效人工修订，保持0；待审对象或自动测试不得增加该数。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 HumanApprovedFeatures = 0;
    /** false时界面显示功能分母未知，不显示百分比。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bHasKnownDenominator = false;
    /** 当前/最近运行和目录真实结果，默认未执行。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoRunSnapshot Run;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGamePlatformResult CatalogResult;
    /** 中文详情包含目的/依据/步骤/期望/缺口，失败保留可读原因。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText Details;
    /** 作者内容指纹，不能冒充Editor已加载DLL版本。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FString SourceFingerprint;
};
