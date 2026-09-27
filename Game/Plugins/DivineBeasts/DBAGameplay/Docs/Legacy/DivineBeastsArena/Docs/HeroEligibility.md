# HeroEligibility（英雄资格）

项目Server实现IGamePlatformArenaHeroEligibilityProvider。

校验顺序：
1. PlayerId非空。
2. HeroDefinitionId属于DivineBeastsCharacters十二生肖核心Catalog。
3. ArenaModeId属于项目五模式。
4. Hero Definition资产可由AssetManager解析。
5. 唯一可信IDivineBeastsArenaTrustedHeroEligibilityProvider存在。
6. Provider执行最终Entitlement/Ownership/Maintenance校验。

客户端资格只用于显示，不能作为最终权威。当前12个Hero Definition真实资产和可信资格Provider均尚未运行配置，因此Dedicated Hero Selection状态为未执行/Fail Closed。
