# ZodiacCatalog（生肖目录）

FDivineBeastsHeroCatalog（生肖英雄目录）CatalogRevision=1，固定维护12个Stable Hero ID、ZodiacIdentity、DisplayNameKey和预期Definition资产名。

预期资产名：DA_Hero_Zodiac_Rat、Ox、Tiger、Rabbit、Dragon、Snake、Horse、Goat、Monkey、Rooster、Dog、Boar。

核心逻辑Content Pack ID为 ContentPack.Hero.Zodiac.Core。Catalog通过UAssetManager的PrimaryAsset路径解析Definition，不建立第二套AssetManager。

12个Definition资产缺失时，运行时Catalog仍能提供Stable ID/本地化Key，但需要Definition数据的角色初始化与非空Appearance校验不能被视为完成。
