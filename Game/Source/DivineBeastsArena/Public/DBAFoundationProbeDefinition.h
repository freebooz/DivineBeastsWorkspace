#pragma once

#include "Definitions/GamePlatformDefinitionBase.h"
#include "DBAFoundationProbeDefinition.generated.h"

/** 主工程开发探针；只用于显式FoundationStandalone，不是正式玩法数据或平台默认资产。 */
UCLASS(BlueprintType)
class DIVINEBEASTSARENA_API UDBAFoundationProbeDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 无单位的验证整数；编辑器保存后重新运行必须读取实际资产值，运行期不修改共享定义。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Foundation|Development")
    int32 ProbeValue = 42;
};
