#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlatformCharacterStateView.generated.h"

/**
 * UGamePlatformCharacterStateView（平台角色状态只读接口标记）。
 * 该接口只公开跨游戏稳定、非敏感的运行状态，不公开CharacterId、账号或项目生肖类型。
 */
UINTERFACE(MinimalAPI)
class UGamePlatformCharacterStateView : public UInterface
{
    GENERATED_BODY()
};

/**
 * IGamePlatformCharacterStateView（平台角色状态只读接口）。
 * Combat、Ability、Animation、Presentation和Debug等上层能力可依赖本接口读取角色身份与就绪状态，
 * 无需反向依赖DivineBeasts项目模块。实现对象通常是挂在Pawn/Character上的复制组件。
 */
class GAMEPLATFORMCHARACTER_API IGamePlatformCharacterStateView
{
    GENERATED_BODY()

public:
    /** 返回服务器批准的稳定Hero Definition ID（英雄定义编号）；未绑定时为NAME_None。 */
    virtual FName GetCharacterStateHeroDefinitionId() const = 0;

    /** 返回当前Pawn出生代次；小于等于0表示尚未建立有效角色状态。 */
    virtual int32 GetCharacterStateSpawnGeneration() const = 0;

    /** 返回当前Avatar绑定代次；小于等于0表示尚未建立有效化身状态。 */
    virtual int32 GetCharacterStateAvatarGeneration() const = 0;

    /** 返回服务器认可的Definition结构版本；0表示尚未完成Definition加载与版本确认。 */
    virtual int32 GetCharacterStateDefinitionVersion() const = 0;

    /** 返回服务器认可的Definition内容修订号；空值表示尚未完成内容一致性确认。 */
    virtual FString GetCharacterStateContentRevision() const = 0;

    /** 返回本端是否已经同时满足服务器Ready事实与本地Definition配置要求。 */
    virtual bool IsCharacterStateReady() const = 0;
};
