# BirdFlock（通用鸟群）三层目录归属补充

2026-10-11；本说明是正式《解决方案总体目录规划说明》的独立增量，不修改可能被其他任务同时编辑且含混合换行符的V1.3.0源文件。

- GamePlatform（平台层）：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX`；公有/私有 `Environment/GamePlatformBirdFlock*` 用于客户端通用群飞、配置和数量边界；`Content/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master` 是独立于项目内容的通用母材质。
- MobaCommon（MOBA通用层）：不包含常规自然环境鸟群，不依赖项目海鸥/燕子/雄鹰资源。
- DivineBeasts（项目层）：`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/{SourceArt/BirdFlock,Content/VFX/Environment/BirdFlock}`；保存三鸟物种模型、纹理、材质，策划预设/共用蓝图属于该内容包；历史错误尺寸测试网格不允许使用。
- 开发工具：`Tools/Blender/BirdFlock` 生成与预览源模型，`Tools/Unreal/VFX/BirdFlock` 导入、回读与可选蓝图/地图生成；`Docs/Implementation/BirdFlockVFXImplementationPlan.md` 是执行及证据清单，`GamePlatformVFX/Docs/BirdFlock.md` 是最终功能接口规范。

P4–P7续接已实装真实平台Niagara模块/系统/EffectType、三个项目Niagara系统与WorldDefinition、统一场景蓝图和两张审核地图。正式运行通过项目场景组件→Presentation→Data租约→VFX执行器，不使用旧ISM原型。当前项目飞行网格位于`Content/VFX/Environment/BirdFlock/Meshes/Flight`，修正源位于`SourceArt/BirdFlock/RefinedMeshes`。GPU15组及正式PIE链路15组数量、取消/世界清理验证通过，3项客户端回归通过；P6全量构建/Cook与P7人工验收状态见`Docs/Implementation/BirdFlockP4P7Execution_20261011.md`。

新增工具均位于`Tools/Unreal/VFX/BirdFlock`：正式资产门禁、模板地图作者、真实GPU视口与PIE链路、10分钟稳定性审核；`Tools/Blender/BirdFlock/RepairBirdFlockFlightMeshes.py`修正源几何。证据位于`Saved/Validation/BirdFlockP4P7`，不把生成的临时日志逐件注册为正式资源。
