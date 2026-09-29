#pragma once

#include "Types/GamePlatformSFXTypes.h"
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"

/** GamePlatformSFX纯值策略；不访问UObject、World或磁盘，可直接自动化测试。 */
class FGamePlatformSFXPolicy final
{
public:
    static FGamePlatformResult ValidateRequest(const FGamePlatformSFXRequest& Request);

    /** 把规范逻辑DefinitionId转换为GamePlatformData统一主资产身份；拒绝资产路径和非法逻辑ID。 */
    static FGamePlatformResult BuildDefinitionAssetId(
        FName DefinitionId,
        FPrimaryAssetId& OutAssetId);
};
