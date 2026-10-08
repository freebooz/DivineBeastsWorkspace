#pragma once

#include "CoreMinimal.h"

/** FDivineBeastsUILocalization（项目UI本地化错误映射）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUILocalization
{
public:
    static FText ErrorCodeToText(FName ErrorCode);
    /** 项目英雄稳定ID的公共中文显示名；未知ID只显示通用名称，不暴露内部协议ID。 */
    static FText HeroNameToText(FName HeroDefinitionId);
};
