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
    /**
     * 本项目世界采用的平台PCG配置定义身份（无需平台反向认识生肖或桃林）。
     * 只保存PrimaryAssetId，不硬引用Graph/Mesh；必须登记到RequiredDefinitions由GamePlatformData统一加载。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|World|PCG")
    TArray<FPrimaryAssetId> EnvironmentPCGProfileIds;

    /**
     * 经Editor Bake（编辑器烘焙）、独立重开和碰撞审查后的静态生成清单身份。
     * 本字段是世界内容发布依赖，不授权服务器运行实时PCG或动态改变导航。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|World|PCG")
    TArray<FPrimaryAssetId> PCGBakeManifestIds;

    /** 在游戏线程校验平台世界字段及项目目录关系；失败返回稳定错误码，不触发资产加载或网络操作。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
