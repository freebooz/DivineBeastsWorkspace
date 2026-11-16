// SSLM新手村NPC的双端轻量实体；服务器只复制位置，客户端独立装配Arissa外观。
#pragma once

#include "GameFramework/Character.h"
#include "DivineBeastsVillageNPC.generated.h"

/** 新手村友好NPC，不具有怪物战斗资格；外观资源不进入服务器类默认对象。 */
UCLASS()
class DIVINEBEASTSCHARACTERSRUNTIME_API ADivineBeastsVillageNPC final : public ACharacter
{
    GENERATED_BODY()
public:
    ADivineBeastsVillageNPC();
};
