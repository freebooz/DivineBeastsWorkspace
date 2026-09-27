#pragma once

#include "Resolution/GamePlatformVFXCatalogRegistry.h"

/** 语义请求解析器。解析保持纯确定性，不负责加载和执行。 */
class FGamePlatformVFXResolver
{
public:
    explicit FGamePlatformVFXResolver(const FGamePlatformVFXCatalogRegistry& InRegistry)
        : Registry(InRegistry)
    {
    }

    FGamePlatformVFXResolvedDefinition Resolve(const FGamePlatformVFXRequest& Request) const;

private:
    const FGamePlatformVFXCatalogRegistry& Registry;
};
