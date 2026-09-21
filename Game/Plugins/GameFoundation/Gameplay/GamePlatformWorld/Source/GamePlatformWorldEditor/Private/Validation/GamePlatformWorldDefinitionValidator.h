#pragma once

#include "EditorValidatorBase.h"
#include "GamePlatformWorldDefinitionValidator.generated.h"

/** 世界领域编辑器验证：检查已保存地图的存在与真实UWorld类型、区域定义类型及完整父链。
 * 只在游戏线程编辑器验证入口同步读取源资产，不创建运行子系统，不移动资产或更改用户对象。
 * DataEditor继续负责通用RequiredDefinitions依赖图；本验证器不访问其Private类或建立第二套加载器。
 */
UCLASS()
class UGamePlatformWorldDefinitionValidator final : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    /** 只接收World/Region定义及其派生类型，非本领域对象返回不适用。 */
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
        FDataValidationContext& InContext) const override;
    /** 使用Data公开唯一源解析及编辑器注册表；缺失、歧义、错类、父环或上限均返回Invalid。 */
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
        UObject* InAsset, FDataValidationContext& Context) override;
};
