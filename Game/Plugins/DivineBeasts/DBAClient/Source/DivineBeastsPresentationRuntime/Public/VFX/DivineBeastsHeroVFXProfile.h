#pragma once

#include "CoreMinimal.h"

/**
 * FDivineBeastsHeroVFXProfile（神兽联盟生肖英雄视觉特效配置）。
 *
 * 该结构只描述项目级视觉语言身份，不持有 UObject（虚幻对象）、资产路径或 Niagara（粒子特效）资源。
 * 具体资产由对应 DBAHeroPack_*（生肖英雄内容包）拥有，并通过项目 Catalog（表现目录）映射到平台 Definition（定义）。
 */
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsHeroVFXProfile
{
    /** Shared（共享契约）发布的稳定英雄定义编号。 */
    FName HeroDefinitionId = NAME_None;

    /** 默认项目 VFX 配置编号，例如 Presentation.VFX.Hero.Zodiac.Rat.Default。 */
    FName ProfileId = NAME_None;

    /** 形状语言编号。 */
    FName ShapeLanguageId = NAME_None;

    /** 运动语言编号。 */
    FName MotionLanguageId = NAME_None;

    /** 能量语言编号。 */
    FName EnergyLanguageId = NAME_None;

    /** 材质语言编号。 */
    FName MaterialLanguageId = NAME_None;

    /** 英雄标志性视觉符号编号。 */
    FName SignatureMotifId = NAME_None;

    /** 命中反馈语言编号。 */
    FName ImpactLanguageId = NAME_None;

    /** 消散语言编号。 */
    FName DissipationLanguageId = NAME_None;

    /** 只校验项目视觉语言字段完整性，不执行资产加载或 Gameplay（游戏逻辑）资格判断。 */
    bool IsValid() const;
};

/**
 * FDivineBeastsHeroVFXProfileCatalog（十二生肖英雄 VFX 配置目录）。
 *
 * HeroDefinitionId（英雄定义编号）唯一真源仍为 FDivineBeastsProjectCatalog；
 * 本目录只按稳定后缀附加项目视觉语言，不复制英雄身份真源。
 */
class DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsHeroVFXProfileCatalog
{
public:
    static constexpr int32 CatalogRevision = 1;

    /** 返回当前 Shared（共享契约）十二生肖集合对应的项目 VFX 配置。 */
    static const TArray<FDivineBeastsHeroVFXProfile>& GetProfiles();

    /** 按稳定 HeroDefinitionId（英雄定义编号）查找默认项目 VFX 配置。 */
    static const FDivineBeastsHeroVFXProfile* Find(FName HeroDefinitionId);

    /** 返回默认 ProfileId；未知英雄返回 NAME_None。 */
    static FName GetDefaultProfileId(FName HeroDefinitionId);
};
