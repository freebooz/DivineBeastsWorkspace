#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXResult.h"

class UObject;

/**
 * GamePlatformPresentation 对接缝。
 * 当前源码包没有拿到真实 GamePlatformPresentation 接口头文件，因此不臆造继承关系；
 * 项目接入时仅需让该类实现真实 Provider Interface，并把中立表现请求转换为 FGamePlatformVFXRequest。
 */
class FGamePlatformVFXPresentationProvider
{
public:
    FGamePlatformVFXPlayResult Submit(const UObject* WorldContextObject, const FGamePlatformVFXRequest& Request) const;
};
