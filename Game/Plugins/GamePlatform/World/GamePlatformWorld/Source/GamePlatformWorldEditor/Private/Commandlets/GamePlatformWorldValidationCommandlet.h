#pragma once

#include "DataValidationCommandlet.h"
#include "EditorValidatorSubsystem.h"
#include "GamePlatformWorldValidationCommandlet.generated.h"

/** 世界定义专用命令行验证：只扫描已保存的World/Region定义及派生类，不生成或修改资产。
 * 运行于编辑器游戏线程，复用真实EditorValidatorSubsystem及既有World验证器。
 * -run=GamePlatformWorldEditor.GamePlatformWorldValidation；本类Main返回0仅表示非空完整验证通过，其他情况返回1。
 * 模块限定名让引擎在查找Commandlet时提前加载PostEngineInit编辑器模块，不必改插件加载阶段。
 * 引擎启动失败/全局错误仍以进程实际非零退出码为准，不能仅凭结果日志宣告成功。
 */
UCLASS()
class UGamePlatformWorldValidationCommandlet final : public UDataValidationCommandlet
{
    GENERATED_BODY()
public:
    /** 本次独立运行生成RunId并输出唯一汇总；不读取或复用历史日志。 */
    virtual int32 Main(const FString& FullCommandLine) override;
protected:
    /** 固定类过滤及磁盘扫描，拒绝用AssetType改变本入口的领域范围。 */
    virtual bool GetAssetsToValidate(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssetDataList) override;
    /** 保留引擎目录排除规则，并冻结实际待验证对象身份用于结果门禁。 */
    virtual void FilterAssetsToValidate(TArray<FAssetData>& AssetDataList) override;
    /** 强制加载验证、记录逐资产结果、不跳过配置排除目录或按用户数量截断。 */
    virtual void SetupValidationSettings(FValidateAssetsSettings& Settings) override;
    /** 原生实现固定true；此处以实际统计、World验证器执行数与本次对象明细决定成功。 */
    virtual bool ProcessValidationResults(FValidateAssetsResults& Results) override;
private:
    FGuid RunId;
    TArray<FString> SelectedObjectPaths;
    FValidateAssetsResults LastResults;
    bool bProcessedResults = false;
};
