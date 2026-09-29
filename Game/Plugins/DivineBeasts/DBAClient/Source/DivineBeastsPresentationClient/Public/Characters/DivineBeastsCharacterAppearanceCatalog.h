#pragma once

#include "CoreMinimal.h"

struct FStreamableHandle;
class UDivineBeastsCharacterAppearanceProfile;

/**
 * FDivineBeastsCharacterAppearanceCatalog（神兽联盟角色外观目录）。
 * 只负责稳定HeroDefinitionId到第三层英雄内容包Profile资产路径的确定性映射和异步加载，
 * 不保存全局实例、不应用材质、不拥有角色生命周期。
 */
class DIVINEBEASTSPRESENTATIONCLIENT_API FDivineBeastsCharacterAppearanceCatalog
{
public:
    /** 返回外观Profile逻辑ID；无效Hero返回NAME_None。 */
    static FName GetDefaultProfileId(FName HeroDefinitionId);

    /** 返回英雄内容包内固定的Profile软路径；后期替换真实模型时保持路径不变。 */
    static FSoftObjectPath GetDefaultProfileAssetPath(FName HeroDefinitionId);

    /** 使用GamePlatformData统一软加载入口异步加载默认外观Profile。 */
    static TSharedPtr<FStreamableHandle> RequestDefaultProfile(
        FName HeroDefinitionId,
        TFunction<void(UDivineBeastsCharacterAppearanceProfile*)> Completion);
};
