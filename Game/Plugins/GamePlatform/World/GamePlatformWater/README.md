# GamePlatformWater（平台通用水体插件）

> 2026-10-16复核：本插件正式纳入平台World（世界）分类的三层基线。水体独立审核地图`/Game/Development/Water/Maps/L_GP_WaterReview`现有真实`.umap`文件；静态严格资源检查通过，但还未取得本轮UE冷加载、Shader、效果截图、GPU及多人运行验收，下方2026-10-14描述是历史制作阶段记录。


插件职责：提供双端静态水域几何、服务器深度/流速判定和低频水域Presence（水域状态）复制；客户端根据Presence向已有GamePlatformPresentation（中立表现系统）提交水纹、飞溅语义并支持画质预算。GamePlatformWaterEditor（编辑器模块）只审查本地图的区域ID、规则和范围，不自动修改项目关卡。

依赖方向：GamePlatformWaterRuntime（双端权威数据）不依赖Niagara、Water插件GPU模拟、UI、Fluid Flux或第三层项目；GamePlatformWaterClient（客户端表现）对接UE5.8原生Water（湖泊与河流）和GamePlatformPresentation（中立表现入口），具体Niagara系统和播放归GamePlatformVFX（平台VFX）。项目层DBAWorldsRuntime（项目世界运行）只依赖Runtime，不可引用Client。服务器不Cook纯水面材质和特效。

## 已有真实代码和资产

- `Source/GamePlatformWaterRuntime`（共享运行模块）：`AGamePlatformWaterRegion`（权威水域Actor）、`UGamePlatformWaterWorldSubsystem`（本世界水域查询）、`UGamePlatformWaterInteractorComponent`（低频权威涉水同步）和自动化数学测试。
- `Source/GamePlatformWaterClient`（客户端表现模块）：水体本地请求、质量分级、事件驱动订阅，以及对应演示与画质配置接口。默认画质从UE现有视觉特效档位初始化，设置页变更可显式刷新，无Tick轮询。
- `Source/GamePlatformWaterEditor`（编辑器模块）：`gp.Water.Audit`（当前编辑器世界水域静态审查）控制台命令，不修改关卡。
- `/GamePlatformSurface/MaterialFunctions/Water`（通用材质函数）、`/GamePlatformSurface/Materials/Water`（母材质及实例）与`/GamePlatformVFX/Water`（Niagara水效和瀑布视觉蓝图）是**已有引擎保存的资产路径**，详见`Docs/Implementation/Water/WaterAssetRegistry_V1.json`（索引）。
- `Docs/Implementation/Water/SourceTextures`（4张RGBA PNG水体原稿）通过`Tools/Water/GenerateWaterSourceTextures.py`（贴图生成器）生成，**还未经UE导入为Texture2D**。

## 地图制作与验收状态

- `Docs/Implementation/Water/WaterReviewSceneManifest_V1.json`（水体审核地图待制作布置合同）明确湖泊、河流、瀑布与权威碰撞区域，但不是已生成的.umap（原生地图文件）。
- 正式新手村仍不改现有`L_Village_Start`（新手村地图），先通过独立审核地图和复制的地形测试后才能集成。
- 角色工作室`L_DBA_CharacterStudio`（角色预览）允许未来独立Fluid Flux，但当前项目未安装该插件，不能声明已支持。
- 截至2026-10-14，专门用于恢复编辑器的模块编译曾两次超时，缺少完整可加载的Runtime/Client模块DLL，Monolith未能制作新审核地图。不能将静态资源计数、局部编译或PNG源图冒充完整3目标构建、UE Shader/性能/双客户端验收。
