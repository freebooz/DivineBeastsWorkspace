#pragma once

#include "CoreMinimal.h"

/**
 * GamePlatformVFXCommonParameters（平台VFX通用参数名）。
 *
 * 只定义跨项目稳定的 Niagara User Parameter（用户参数）名称，
 * 不携带技能、英雄、生肖或美术主题语义。
 */
namespace GamePlatformVFXCommonParameters
{
    GAMEPLATFORMVFXCLIENT_API const FName& PrimaryColor();
    GAMEPLATFORMVFXCLIENT_API const FName& SecondaryColor();
    GAMEPLATFORMVFXCLIENT_API const FName& CoreColor();
    GAMEPLATFORMVFXCLIENT_API const FName& Intensity();
    GAMEPLATFORMVFXCLIENT_API const FName& Duration();
    GAMEPLATFORMVFXCLIENT_API const FName& Radius();
    GAMEPLATFORMVFXCLIENT_API const FName& Length();
    GAMEPLATFORMVFXCLIENT_API const FName& Width();
    GAMEPLATFORMVFXCLIENT_API const FName& Speed();
    GAMEPLATFORMVFXCLIENT_API const FName& Seed();
    GAMEPLATFORMVFXCLIENT_API const FName& SourcePosition();
    GAMEPLATFORMVFXCLIENT_API const FName& TargetPosition();
    GAMEPLATFORMVFXCLIENT_API const FName& Direction();
    GAMEPLATFORMVFXCLIENT_API const FName& Scale();
}
