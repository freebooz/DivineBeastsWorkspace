#pragma once

#include "Types/MobaPresentationTypes.h"

/**
 * IMobaPresentationContextContributor（MOBA表现上下文贡献接口）。
 * DivineBeasts等上层可以单向补充Hero/Skin/World/ContentPack等项目上下文；
 * 本接口不认识任何项目层类型。
 */
class MOBAPRESENTATIONRUNTIME_API IMobaPresentationContextContributor
{
public:
    virtual ~IMobaPresentationContextContributor() = default;
    virtual void Contribute(FMobaPresentationContext& InOutContext) const = 0;
};
