#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformPCGEditorLibrary.generated.h"

class UGamePlatformPCGProfileDefinition;
class UGamePlatformPCGMeshSetDefinition;
class UPCGGraph;
class AGamePlatformPCGWorldDirector;

/** Python/蓝图编辑器入口；固定开发资产范围，拒绝正式地图和已有包覆盖，不是运行时服务。 */
UCLASS()
class UGamePlatformPCGEditorLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 游戏线程，仅首次创建固定开发图/配置；任一目标已占用则整批拒绝，不覆盖用户资产。不执行图或地图操作。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreateDevelopmentAssets(FString& Error);

    /**
     * 首次创建M0/M1 Foundation Template（基础模板）开发资产；全部位于/Game/Development，不覆盖已有包。
     * 只创建真实UPCGGraph模板并先通过Template Contract校验，不创建发布内容、不执行生成。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreateFoundationTemplateAssets(FString& Error);

    /** 首次创建七个M0/M1 Foundation Subgraph（基础公共子图）开发资产；拒绝覆盖已有包。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreateFoundationSubgraphAssets(FString& Error);

    /** 模板+子图统一入口；先对全部目标包做占用预检，再执行真实资产创建。Commandlet应优先调用本函数。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreateFoundationAssets(FString& Error);
    /**
     * P2～P5真实蓝图创作：在项目已注册内容包的PCG/Blueprints目录生成11个通用Actor子蓝图，
     * 不创建文本伪资产、不覆盖已有包；图与具体网格留由项目图实例配置，世界地图不自动修改。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreatePCGPlacementBlueprints(const FString& RootPackagePath, FString& Error);
    /**
     * 为已完成GamePlatformData租约加载的项目MeshSet生成真正带Weighted Spawner的开发图实例。
     * 仅创建于/Game/Development/Foundation/PCG/Realized/，不修改共享模板或Profile、不写正式地图；
     * 成功后使用编辑器将返回Graph与Profile.GraphReference关联并另行保存、验证与烘焙。
     * Profile中的MeshSetDefinitionId须列在RequiredDefinitions中，且所有Mesh均须已加载。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static UPCGGraph* CreateDevelopmentRealizedGraphAsset(const FString& PackageName,
        UGamePlatformPCGProfileDefinition* Profile, UGamePlatformPCGMeshSetDefinition* MeshSet,
        FString& Error);
    /**
     * 编辑器阶段从显式注册的WorldDirector提取确定性空间几何，注入已绑定的Graph Instance。
     * 不保存/执行、不扫描全世界Actor；调用者需要随后在UE编辑器明确保存真实资产。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool ConfigureRealizedGraphSpatialMasks(
        UPCGGraph* RealizedGraph, AGamePlatformPCGWorldDirector* Director,
        int32 SubjectPriority, FString& Error);
    /** 游戏线程；Profile及其图/网格必须已加载且保存。返回真实依赖字节指纹，不批准生成结果或重开状态。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool InspectProfileSource(UGamePlatformPCGProfileDefinition* Profile, FString& Fingerprint,
        TArray<FString>& Dependencies, FString& Error);

    /** 只读验证Profile（配置）当前图是否满足Legacy Fixture或1.0 Template Contract；不执行生成、不保存资产。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool ValidateProfileContract(UGamePlatformPCGProfileDefinition* Profile, FString& Error);

    /** 返回1.0已登记模板ID，供Editor工具/自动化创建入口使用；返回ID不代表对应.uasset已经存在。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|PCG|Editor")
    static TArray<FName> GetKnownTemplateIds();

    /** 返回M0/M1已登记公共子图ID；ID存在不代表对应.uasset已经落盘。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|PCG|Editor")
    static TArray<FName> GetKnownSubgraphIds();
};
