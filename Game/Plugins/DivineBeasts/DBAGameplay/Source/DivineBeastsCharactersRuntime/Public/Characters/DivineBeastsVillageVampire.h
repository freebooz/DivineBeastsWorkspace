// SSLM新手村吸血鬼怪物：复用已验证的服务器AI/Combat生命周期，保持独立客户端外观身份。
#pragma once

#include "Characters/DivineBeastsVillageWolf.h"
#include "DivineBeastsVillageVampire.generated.h"

/** 人型近战怪物；当前期复用村庄怪物权威基类，未来可独立扩充物种能力。 */
UCLASS()
class DIVINEBEASTSCHARACTERSRUNTIME_API ADivineBeastsVillageVampire final
    : public ADivineBeastsVillageWolf
{
    GENERATED_BODY()
public:
    ADivineBeastsVillageVampire();
};
