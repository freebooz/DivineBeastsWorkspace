# BirdFlock（通用鸟群）三层目录归属补充

2026-10-11；本说明是正式《解决方案总体目录规划说明》的独立增量，不修改可能被其他任务同时编辑且含混合换行符的V1.3.0源文件。

- GamePlatform（平台层）：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX`；公有/私有 `Environment/GamePlatformBirdFlock*` 用于客户端通用群飞、配置和数量边界；`Content/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master` 是独立于项目内容的通用母材质。
- MobaCommon（MOBA通用层）：不包含常规自然环境鸟群，不依赖项目海鸥/燕子/雄鹰资源。
- DivineBeasts（项目层）：`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/{SourceArt/BirdFlock,Content/VFX/Environment/BirdFlock}`；保存三鸟物种模型、纹理、材质，策划预设/共用蓝图属于该内容包；历史错误尺寸测试网格不允许使用。
- 开发工具：`Tools/Blender/BirdFlock` 生成与预览源模型，`Tools/Unreal/VFX/BirdFlock` 导入、回读与可选蓝图/地图生成；`Docs/Implementation/BirdFlockVFXImplementationPlan.md` 是执行及证据清单，`GamePlatformVFX/Docs/BirdFlock.md` 是最终功能接口规范。

本次确已创建Client通用代码和十件可正确回读的真实UE基础资产。编辑器因已有Live Coding会话锁定，真实蓝图/Niagara/审核地图/正式游戏场景接入仍待实现；不能以脚本存在声称资源已经生成。
