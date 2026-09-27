# Architecture（架构）

依赖方向固定为 GamePlatform（游戏平台层） ← MobaCommon（MOBA通用层） ← DivineBeasts（项目层）。MobaPresentation保持独立两模块，竞技专用装配选择它；DBAClient的公共项目表现不反向依赖它。

表现链为 GamePlatformPresentation（平台表现协调） ← MobaPresentation（MOBA表现语义） ← 未来DivineBeastsPresentation（项目表现扩展）。GamePlatformArena（竞技插件）不反向依赖MobaPresentation。

MobaPresentationRuntime（共享语义模块）只依赖平台核心、平台表现核心和GameplayTags（玩法标签）；MobaPresentationClient（客户端适配模块）读取Arena/Combat等公共事实并提交平台表现请求。

GamePlatformVFX（平台VFX插件）仍是唯一VFX运行框架。MobaPresentation不选择具体Niagara、Sound、Widget或Camera资源。
