#pragma once
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "DivineBeastsArenaEntranceDefinition.generated.h"

/** 世界活动入口数据；项目模式来自Shared目录，不持有地址、票据、匹配器或客户端页面。 */
UCLASS(BlueprintType)
class DBAWORLDSRUNTIME_API UDivineBeastsArenaEntranceDefinition final : public UGamePlatformPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** 现有五种竞技模式的稳定身份；无效配置必须关闭交互。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Activity")
    FName ArenaModeId = NAME_None;
    /** 三维门楣文字；界面模式说明由既有竞技UI目录提供。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Activity")
    FText DisplayName;
    /** 游戏线程纯校验；不加载资源、不改变当前模式和报名。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Activity")
    bool IsConfigured() const;
};
