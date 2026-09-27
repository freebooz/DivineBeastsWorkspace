# DivineBeastsPresentation（神兽联盟项目表现插件）

DivineBeastsPresentation位于DivineBeasts/Presentation（神兽联盟项目表现层），固定两个模块：

- DivineBeastsPresentationRuntime（项目表现运行模块，Runtime双端）。
- DivineBeastsPresentationClient（项目表现客户端模块，ClientOnly仅客户端）。

本插件不是第三套VFX框架。GamePlatformVFX（游戏平台视觉特效）仍是唯一VFX执行框架；MobaPresentation（MOBA表现语义）继续负责MOBA语义；本插件只拥有项目稳定Context（上下文）、项目Semantic（表现语义）、Project Catalog（项目目录）、Content Pack（内容包）注册契约和非MOBA项目事实适配。

Runtime不引用Niagara、Sound、Widget、Camera、HTTP或Backend DTO（后端数据对象）。Client不硬依赖MobaPresentation、VFX/SFX/UI/Animation/Camera客户端模块；所有请求最终提交同一个UGamePlatformPresentationClientSubsystem（平台表现客户端协调器）。

当前真实Content Pack资产为0。DBAPresentationPack_Core、DBASFXPack_Core、DBAAnimationPack_Core、DBAHeroPack_*、DBAWorldPack_*、DBASkinPack_*均未在仓库中实现，因此包迁移/资源预加载/Cook只能标“未执行”，不能伪报完成。
