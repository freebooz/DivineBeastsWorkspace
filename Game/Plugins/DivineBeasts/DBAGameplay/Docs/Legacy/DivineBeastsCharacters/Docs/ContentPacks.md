# ContentPacks（内容包）

核心逻辑Content Pack ID为 ContentPack.Hero.Zodiac.Core。

DivineBeastsCharactersRuntime只保存逻辑ContentPackId、AppearanceProfileId、PresentationProfileId和SkeletonCompatibilityId，不硬依赖可选视觉包。

12生肖高精模型、材质、动画、VFX、SFX、UI Portrait应位于客户端项目Hero Content Pack/Presentation层。服务器只加载Server-safe Hero Definition与碰撞/移动需要的数据。

当前仓库尚无正式视觉Content Pack资产，因此实际视觉Pack注册与Cook状态为“未执行”。
