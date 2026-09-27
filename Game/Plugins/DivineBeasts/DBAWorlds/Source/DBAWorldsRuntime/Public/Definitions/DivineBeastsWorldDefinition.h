#pragma once

#include "Definitions/GamePlatformWorldDefinition.h"
#include "DivineBeastsWorldDefinition.generated.h"

/**
 * UDivineBeastsWorldDefinition（神兽联盟项目世界定义）。
 * 在平台世界字段上补充项目ServerRole与可选ArenaMode一致性；纯字段校验不分配服务器、不加载地图。
 */
UCLASS(BlueprintType)
class DBAWORLDSRUNTIME_API UDivineBeastsWorldDefinition final : public UGamePlatformWorldDefinition
{
    GENERATED_BODY()

public:
    /** 必须是Shared正式服务器角色，且须与DefaultExperienceId映射到同一角色。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|World")
    FName ServerRoleId = NAME_None;

    /** 可选竞技模式；非空时必须映射到本定义相同的MainArena角色和体验。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|World")
    FName ArenaModeId = NAME_None;

    /** 在游戏线程校验平台世界字段及项目目录关系；失败返回稳定错误码，不触发资产加载或网络操作。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
