#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationCatalog.h"

/** FDivineBeastsPresentationProjectCatalog（神兽联盟项目默认表现目录）。 */
class DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationProjectCatalog
{
public:
    static constexpr int32 CatalogRevision = 1;

    static FGamePlatformPresentationCatalogFragment BuildDefaultFragment();

    /** 天气VFX的项目片段；只有真实Definition/系统完成GamePlatformData预载后才可发布。 */
    static FGamePlatformPresentationCatalogFragment BuildWeatherVFXFragment();
    /** 天气SFX的项目片段；独立于Niagara，缺声音资源不阻断雨雪画面。 */
    static FGamePlatformPresentationCatalogFragment BuildWeatherSFXFragment();
};
