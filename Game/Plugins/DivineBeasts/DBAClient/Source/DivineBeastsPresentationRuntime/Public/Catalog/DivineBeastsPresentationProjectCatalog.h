#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationCatalog.h"

/** FDivineBeastsPresentationProjectCatalog（神兽联盟项目默认表现目录）。 */
class DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationProjectCatalog
{
public:
    static constexpr int32 CatalogRevision = 1;

    static FGamePlatformPresentationCatalogFragment BuildDefaultFragment();
};
