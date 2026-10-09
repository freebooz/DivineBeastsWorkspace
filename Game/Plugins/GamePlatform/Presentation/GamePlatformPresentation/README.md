# GamePlatformPresentation（游戏平台表现协调插件）

跨UI/VFX/SFX等表现系统的中立语义协调层。Core模块定义Presentation Request、Context、Catalog与解析契约；Client模块按LocalPlayer维护Provider、Context Contributor与Catalog Fragment，并通过WorldGeneration/RequestGeneration隔离旧世界和晚到请求。`GamePlatformSurface（游戏平台通用环境表面材质插件）`保持独立：需要跨表现系统语义编排时可以由项目适配层使用Presentation请求，但全局湿润／积雪等连续环境参数不要求逐次绕行Presentation总线。

该插件不包含Niagara、Sound或Widget具体资产类型。GamePlatformVFX已通过ProviderChannel=VFX正式接入，证明该协调层已进入实际组合使用。

Client 模块同时提供 `AGamePlatformCharacterPreviewStage（平台三维角色预览舞台）`：无 Tick、无复制，只负责已经加载完成的 SkeletalMesh / Material / AnimInstance 的本地展示、镜头距离和角色旋转。它不认识项目 Hero ID、生肖或后端角色身份，可供不同游戏项目的角色选择、捏脸、商城试穿等前端场景复用。


## 2026-09-30 设计审查修复

本次资源/生命周期与行为合同见 [设计修复说明](Docs/DesignRemediation-2026-09-30.md)。源码及新增回归不等于UE运行、真实资产或Cook验收；准确执行证据由任务修复报告记录。


2026-10-09本插件源码整改、中文API/所有权说明和待UE验收边界见 [本轮源码说明](Docs/AuditRemediation-2026-10-09.md)。
