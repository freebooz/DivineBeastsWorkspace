# HeroWorldSkinOverrides（英雄/世界/皮肤覆盖）

Catalog Context支持：

- Hero：HeroDefinitionId + AbilityId。
- World：WorldId + ExperienceId + RegionId。
- Skin：HeroDefinitionId + SkinId。
- Arena：ArenaModeId。
- Content Pack：ContentPackId。

Skin只能覆盖表现Definition，不改变Gameplay数值、技能、Entitlement或经济。

当前无实际Skin Pack/Hero Pack/World Pack，所以覆盖资源为“未执行”。平台Resolver已经支持ContentPack > Project > Moba > Platform的覆盖优先级，真实包出现后无需修改Dispatcher。
