# HeroIdentity（英雄身份）

正式12个HeroDefinitionId：

Hero.Zodiac.Rat、Ox、Tiger、Rabbit、Dragon、Snake、Horse、Goat、Monkey、Rooster、Dog、Boar。

Stable ID只表达生肖玩法模板身份，不使用影牙、玄角、白君等显示名作为协议主键。显示名通过Localization Key，例如 Hero.Zodiac.Rat.Name，由未来本地化/表现内容解析。

GameplayTag（玩法标签）DBA.Character.Zodiac.*只用于查询与语义，不替代HeroDefinitionId数据库身份。
