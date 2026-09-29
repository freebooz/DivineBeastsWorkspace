#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GamePlatformSurfaceCoreAssetsCommandlet.generated.h"

/**
 * 生成/验证GamePlatformSurface核心MPC的编辑器命令。
 *
 * 默认只在资产不存在时首次创建；-ValidateOnly仅验证。命令不生成空母材质或伪材质函数。
 */
UCLASS()
class UGamePlatformSurfaceCoreAssetsCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UGamePlatformSurfaceCoreAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
