#pragma once

#include "CoreMinimal.h"

/** FDivineBeastsUILocalization（项目UI本地化错误映射）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUILocalization
{
public:
    static FText ErrorCodeToText(FName ErrorCode);
};
