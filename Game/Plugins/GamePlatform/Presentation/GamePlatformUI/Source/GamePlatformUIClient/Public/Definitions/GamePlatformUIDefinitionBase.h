#pragma once

#include "Definitions/GamePlatformDefinitionBase.h"
#include "GamePlatformUIDefinitionBase.generated.h"

/**
 * UGamePlatformUIDefinitionBase（游戏平台UI定义基类）。
 *
 * 统一复用GamePlatformData的稳定LogicalId、结构版本、依赖租约和AssetRegistry能力。
 * UI Definition只保存静态展示配置，不保存LocalPlayer/World运行状态。
 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIDefinitionBase
    : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /** 通用视觉样式身份；项目主题可在不修改C++的情况下替换具体资源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Definition")
    FName StyleId = NAME_None;

    /** 与该UI定义一起预加载的纯表现资源软引用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Definition")
    TArray<TSoftObjectPtr<UObject>> PreloadAssets;
};
