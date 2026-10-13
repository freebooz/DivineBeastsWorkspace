// SSLM开发目录的只读Data定义，项目层拥有编目，平台Data拥有加载与租约。
// JSON是唯一人工作者源，本资产只是经引擎生成的派生物；此类不加载资源、不启动世界或网络。
#pragma once

#include "Contracts/DivineBeastsDemoCatalogTypes.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "DivineBeastsDemoCatalogDefinition.generated.h"

/** 只供开发配置的Schema2目录；游戏线程加载/验证，共享定义发布后禁止原地修改。 */
UCLASS(BlueprintType)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoCatalogDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 默认Schema2但不填逻辑身份；身份必须由受控作者工具明确指定。 */
    UDivineBeastsDemoCatalogDefinition();
    /** 作者JSON原始字节的SHA256，64位小写十六进制；不是运行二进制指纹。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") FString SourceFingerprint;
    /** 全部真实插件及内容包，禁用者仍可查看，不由资产决定目标启用状态。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoPluginEntry> Plugins;
    /** 已审计独立功能；空表不表示完成全部功能。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoFeatureEntry> Features;
    /** 功能案例集合；共享地图不共享运行/签收身份。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoScenarioDefinition> Scenarios;
    /** 中立核验合同；实际人工结果留独立报告。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoReviewCaseDefinition> ReviewCases;
    /** 地图旧身份的显式兼容映射。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") TArray<FDivineBeastsDemoLegacyMapRoute> LegacyMapRoutes;
    /** 只读Schema2；类CDO范围不能被资产内容扩大。 */
    virtual int32 GetMinimumReadableSchemaVersion() const override { return 2; }
    /** 不做静默升级；未来结构必须有独立迁移。 */
    virtual int32 GetMaximumReadableSchemaVersion() const override { return 2; }
    /** 先检查平台身份/版本，再检查完整目录；失败返回具名诊断，无加载/写入副作用。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
