# ProjectSemantics（项目表现语义）

本轮只新增两个真实项目专属表现语义：

- DBA.Presentation.World.Interaction.Committed：OpenWorld/Village中服务器确认交互结果复制到客户端后的项目反馈语义。
- DBA.Presentation.Village.Guidance.Ready：Village Tutorial/Training进入InWorld且可展示引导反馈时的项目语义。

没有批量制造Hero/Ability/Skin Tag；这些差异优先由Context匹配。

Arena/Combat/Ability等MOBA事实继续由MobaPresentation拥有语义转换。本插件不复制Moba Fact Adapter（MOBA事实适配器）。
