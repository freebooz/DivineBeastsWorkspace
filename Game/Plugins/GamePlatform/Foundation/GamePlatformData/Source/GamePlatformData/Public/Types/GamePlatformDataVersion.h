#pragma once
#include "CoreMinimal.h"
#include "GamePlatformDataVersion.generated.h"

/** 数据格式与内容修订；兼容范围由具体定义类的CDO代码声明，资产不能扩大范围。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMDATA_API FGamePlatformDataVersion
{
    GENERATED_BODY()
    /** 序列化结构版本，从1开始；不等于逻辑身份版本或引擎版本。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Data", meta=(ClampMin="1"))
    int32 SchemaVersion = 1;
    /** 同结构的内容修订，从1开始；不隐含跨版本迁移。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Data", meta=(ClampMin="1"))
    int32 ContentRevision = 1;
};
