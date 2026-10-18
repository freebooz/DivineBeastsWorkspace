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
    /** Water VFX（水体视觉表现）项目目录：只映射水纹/飞溅语义到逻辑定义身份，
     * 实际Niagara资产由统一Data与GamePlatformVFX加载；没有真实定义时不得发布。 */
    static FGamePlatformPresentationCatalogFragment BuildWaterVFXFragment();
    /** 天气SFX的项目片段；独立于Niagara，缺声音资源不阻断雨雪画面。 */
    static FGamePlatformPresentationCatalogFragment BuildWeatherSFXFragment();
    /** 首批客户端音频映射；Definition先经Data租约完整预载，禁止将未交付声音注册成假成功。 */
    static FGamePlatformPresentationCatalogFragment BuildStarterAudioFragment();
    /** 前端角色预览四类可替换自然飘落外观；只生成中立目录映射，无Niagara播放器。 */
    static FGamePlatformPresentationCatalogFragment BuildFrontEndFoliageFragment(FName StyleId);

    /** 鸟群三物种共享一份目录，通过不同语义映射到真实WorldDefinition。 */
    static FGamePlatformPresentationCatalogFragment BuildBirdFlockFragment();
    /** 项目物种名称转语义；未知名称返回无效标签，平台层不认识这些鸟种。 */
    static FGameplayTag GetBirdFlockTag(FName SpeciesId);
    /** 蝴蝶与鱼的项目配置；数据资产真实预载成功后才发布。 */
    static FGamePlatformPresentationCatalogFragment BuildFaunaFragment();
    static FGameplayTag GetFaunaTag(FName SpeciesId);

};
