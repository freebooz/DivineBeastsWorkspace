#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DBAFoundationHUD.generated.h"

/** 当前世界只读开发显示；不缓存旧实例或已释放定义，不用于正式UI。 */
UCLASS(Transient, NotBlueprintable)
class ADBAFoundationHUD final : public AHUD
{
    GENERATED_BODY()
public:
    /** 客户端绘制本GameInstance的值快照；服务器和非开发入口不绘制。 */
    virtual void DrawHUD() override;
};
