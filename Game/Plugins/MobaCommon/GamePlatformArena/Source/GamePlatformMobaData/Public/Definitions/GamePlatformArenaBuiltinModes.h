#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformArenaModeDefinition.h"

/** FGamePlatformArenaBuiltinModes（五种正式竞技模式的原生可验证定义）。
 *  这些定义用于无资产环境、CI和服务器启动前交叉验证；正式项目可用同ID的Primary Asset覆盖展示/配置数据。
 */
class GAMEPLATFORMMOBADATA_API FGamePlatformArenaBuiltinModes
{
public:
    static const FName Duel1v1;
    static const FName Team2v2;
    static const FName Team3v3;
    static const FName Team4v4;
    static const FName Team5v5;
    static const FName MainArenaServerRole;

    static const TArray<FGamePlatformArenaModeSpec>& GetAll();
    static const FGamePlatformArenaModeSpec* Find(FName ArenaModeId);
    static bool ValidateAll(FString& OutError);
};
